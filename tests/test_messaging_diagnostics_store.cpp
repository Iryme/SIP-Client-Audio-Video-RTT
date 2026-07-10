#include <QtTest/QtTest>

#include "sip/MessagingDiagnosticsStore.h"
#include "sip/SipTraceLogger.h"

// Tests for MessagingDiagnosticsStore — relevance filtering and structured
// entry building (CPIM/IMDN/is-composing/SDP-MSRP/RCS-FT-HTTP, plus Task
// W095's Content-Encoding decode pipeline) on top of raw SipMessageTrace
// data. No SipManager, SipCall, or network activity required.

namespace {
// Same hand-built RFC 1951 "stored block" fixture technique as
// test_deflate_decoder.cpp — deterministic, no compression library needed.
QByteArray storedDeflateBlock(const QByteArray &literal)
{
    QByteArray out;
    out.append(char(0x01));
    const quint16 len = static_cast<quint16>(literal.size());
    const quint16 nlen = static_cast<quint16>(~len);
    out.append(char(len & 0xFF));
    out.append(char((len >> 8) & 0xFF));
    out.append(char(nlen & 0xFF));
    out.append(char((nlen >> 8) & 0xFF));
    out.append(literal);
    return out;
}

QByteArray zlibWrap(const QByteArray &rawDeflate)
{
    QByteArray out;
    out.append(char(0x78));
    out.append(char(0x9C));
    out.append(rawDeflate);
    out.append(QByteArray(4, '\0'));
    return out;
}
} // namespace

class TestMessagingDiagnosticsStore : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void plainMessageIsRelevant();
    void nonMessagingSipTrafficIsIgnored();
    void cpimWrappedImdnIsParsed();
    void isComposingBodyIsParsed();
    void inviteWithMsrpSdpIsRelevant();
    void clearEmitsSignalAndEmptiesStore();
    void exportContainsExpectedFields();

    void imdnDeflateZlibDecodesAndParsesDelivered();
    void imdnDeflateRawDecodesAndParsesDisplayed();
    void imdnDeflateInvalidMarksFailedAndSkipsXmlParsing();
    void rcsFtHttpCompleteDescriptorIsParsed();
    void rcsFtHttpIncompleteDescriptorHasNoFileInfo();
    void bodyWithNullBytesSurvivesThePipeline();
};

void TestMessagingDiagnosticsStore::init()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
}

void TestMessagingDiagnosticsStore::cleanup()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
}

void TestMessagingDiagnosticsStore::plainMessageIsRelevant()
{
    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Outbound;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("text/plain");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: text/plain\r\n\r\n"
                                   "Hello Bob!");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QCOMPARE(e.contentKind, MessagingContentKind::PlainText);
    QCOMPARE(e.bodyPreview, QStringLiteral("Hello Bob!"));
}

void TestMessagingDiagnosticsStore::nonMessagingSipTrafficIsIgnored()
{
    SipMessageTrace t;
    t.direction = SipMessageTrace::Direction::Outbound;
    t.method    = QStringLiteral("REGISTER");
    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 0);
}

void TestMessagingDiagnosticsStore::cpimWrappedImdnIsParsed()
{
    const QString imdnXml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">"
                       "<message-id>xyz</message-id>"
                       "<delivery-notification><status><delivered/></status></delivery-notification>"
                       "</imdn>");

    const QString cpimBody =
        QStringLiteral("From: <im:bob@example.com>\r\n"
                       "To: <im:alice@example.com>\r\n"
                       "DateTime: 2024-01-01T00:00:00Z\r\n"
                       "Content-Type: message/imdn+xml\r\n"
                       "\r\n") + imdnXml;

    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Inbound;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("message/cpim");
    t.rawSip      = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                   "Content-Type: message/cpim\r\n\r\n") + cpimBody;

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QCOMPARE(e.contentKind, MessagingContentKind::Cpim);
    QVERIFY(e.cpim.present);
    QCOMPARE(e.cpim.contentType, QStringLiteral("message/imdn+xml"));
    QVERIFY(e.imdn.present);
    QCOMPARE(e.imdn.disposition, ImdnInfo::Disposition::Delivered);
    QCOMPARE(e.imdn.messageId, QStringLiteral("xyz"));
}

void TestMessagingDiagnosticsStore::isComposingBodyIsParsed()
{
    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Outbound;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("application/im-iscomposing+xml");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: application/im-iscomposing+xml\r\n\r\n"
                                   "<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">"
                                   "<state>active</state><refresh>60</refresh></isComposing>");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QVERIFY(e.isComposing.present);
    QCOMPARE(e.isComposing.state, IsComposingInfo::State::Active);
    QCOMPARE(e.isComposing.refresh, QStringLiteral("60"));
}

void TestMessagingDiagnosticsStore::inviteWithMsrpSdpIsRelevant()
{
    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Outbound;
    t.method      = QStringLiteral("INVITE");
    t.contentType = QStringLiteral("application/sdp");
    t.rawSip      = QStringLiteral("INVITE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: application/sdp\r\n\r\n"
                                   "v=0\r\n"
                                   "m=message 7654 TCP/MSRP *\r\n"
                                   "a=accept-types:message/cpim\r\n"
                                   "a=path:msrp://198.51.100.10:7654/sess42;tcp\r\n");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QVERIFY(e.sdpMsrp.present);
    QCOMPARE(e.sdpMsrp.sessionId, QStringLiteral("sess42"));
}

void TestMessagingDiagnosticsStore::clearEmitsSignalAndEmptiesStore()
{
    SipMessageTrace t;
    t.method = QStringLiteral("MESSAGE");
    SipTraceLogger::instance().logMessage(t);
    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);

    QSignalSpy clearSpy(&MessagingDiagnosticsStore::instance(), &MessagingDiagnosticsStore::cleared);
    MessagingDiagnosticsStore::instance().clear();
    QCOMPARE(clearSpy.count(), 1);
    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 0);
}

void TestMessagingDiagnosticsStore::exportContainsExpectedFields()
{
    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Outbound;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("text/plain");
    t.callId      = QStringLiteral("msg-call-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: text/plain\r\n\r\nHi");
    SipTraceLogger::instance().logMessage(t);

    const QString json = MessagingDiagnosticsStore::instance().exportToJson();
    QVERIFY(json.contains(QStringLiteral("\"method\"")));
    QVERIFY(json.contains(QStringLiteral("\"contentKind\"")));
    QVERIFY(json.contains(QStringLiteral("msg-call-1")));

    const QString text = MessagingDiagnosticsStore::instance().exportToText();
    QVERIFY(text.contains(QStringLiteral("MESSAGE")));
}

void TestMessagingDiagnosticsStore::imdnDeflateZlibDecodesAndParsesDelivered()
{
    const QString imdnXml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">"
                       "<message-id>zlib-1</message-id>"
                       "<delivery-notification><status><delivered/></status></delivery-notification>"
                       "</imdn>");
    const QByteArray compressed = zlibWrap(storedDeflateBlock(imdnXml.toUtf8()));

    SipMessageTrace t;
    t.direction       = SipMessageTrace::Direction::Inbound;
    t.method          = QStringLiteral("MESSAGE");
    t.contentType     = QStringLiteral("message/imdn+xml");
    t.contentEncoding = QStringLiteral("deflate");
    t.callId          = QStringLiteral("call-deflate-zlib-1");
    t.rawSip          = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                       "Content-Type: message/imdn+xml\r\n"
                                       "Content-Encoding: deflate\r\n\r\n")
                       + QString::fromLatin1(compressed);

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QCOMPARE(e.decodeStatus, ContentDecodeStatus::Decoded);
    QCOMPARE(e.decodeVariant, DeflateDecoder::Variant::Zlib);
    QVERIFY(e.compressedBodyLength > 0);
    QVERIFY(e.decodedBodyLength > 0);
    QVERIFY(e.imdn.present);
    QCOMPARE(e.imdn.disposition, ImdnInfo::Disposition::Delivered);
    QCOMPARE(e.imdn.messageId, QStringLiteral("zlib-1"));
    QVERIFY(e.bodyPreview.contains(QStringLiteral("zlib-1")));
}

void TestMessagingDiagnosticsStore::imdnDeflateRawDecodesAndParsesDisplayed()
{
    const QString imdnXml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">"
                       "<message-id>raw-1</message-id>"
                       "<delivery-notification><status><displayed/></status></delivery-notification>"
                       "</imdn>");
    const QByteArray compressed = storedDeflateBlock(imdnXml.toUtf8()); // no zlib wrapper

    SipMessageTrace t;
    t.direction       = SipMessageTrace::Direction::Inbound;
    t.method          = QStringLiteral("MESSAGE");
    t.contentType     = QStringLiteral("message/imdn+xml");
    t.contentEncoding = QStringLiteral("deflate");
    t.callId          = QStringLiteral("call-deflate-raw-1");
    t.rawSip          = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                       "Content-Type: message/imdn+xml\r\n"
                                       "Content-Encoding: deflate\r\n\r\n")
                       + QString::fromLatin1(compressed);

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QCOMPARE(e.decodeStatus, ContentDecodeStatus::Decoded);
    QCOMPARE(e.decodeVariant, DeflateDecoder::Variant::RawDeflate);
    QVERIFY(e.imdn.present);
    QCOMPARE(e.imdn.disposition, ImdnInfo::Disposition::Displayed);
    QCOMPARE(e.imdn.messageId, QStringLiteral("raw-1"));
}

void TestMessagingDiagnosticsStore::imdnDeflateInvalidMarksFailedAndSkipsXmlParsing()
{
    SipMessageTrace t;
    t.direction       = SipMessageTrace::Direction::Inbound;
    t.method          = QStringLiteral("MESSAGE");
    t.contentType     = QStringLiteral("message/imdn+xml");
    t.contentEncoding = QStringLiteral("deflate");
    t.callId          = QStringLiteral("call-deflate-invalid-1");
    t.rawSip          = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                       "Content-Type: message/imdn+xml\r\n"
                                       "Content-Encoding: deflate\r\n\r\n"
                                       "not-a-valid-deflate-stream-at-all");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QCOMPARE(e.decodeStatus, ContentDecodeStatus::Failed);
    QVERIFY(!e.decodeError.isEmpty());
    QVERIFY(!e.imdn.present); // never attempted to XML-parse the compressed bytes
    QVERIFY(e.bodyPreview.isEmpty());
}

void TestMessagingDiagnosticsStore::rcsFtHttpCompleteDescriptorIsParsed()
{
    const QString rcsXml = QStringLiteral(
        "<file>"
        "  <file-info type=\"file\">"
        "    <file-size>1024</file-size>"
        "    <file-name>image.png</file-name>"
        "    <content-type>image/png</content-type>"
        "    <data url=\"https://files.example.test/dl/image?token=abc\" until=\"2030-01-01T00:00:00.000Z\"/>"
        "  </file-info>"
        "</file>");

    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("application/vnd.gsma.rcs-ft-http+xml");
    t.callId      = QStringLiteral("call-rcs-complete-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: application/vnd.gsma.rcs-ft-http+xml\r\n\r\n") + rcsXml;

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QCOMPARE(e.contentKind, MessagingContentKind::RcsFtHttp);
    QVERIFY(e.rcsFtHttp.present);
    QCOMPARE(e.rcsFtHttp.fileName, QStringLiteral("image.png"));
    QCOMPARE(e.rcsFtHttp.fileSize, static_cast<qint64>(1024));
    QCOMPARE(e.rcsFtHttp.contentType, QStringLiteral("image/png"));
    QVERIFY(!e.rcsFtHttp.thumbnailPresent);
}

void TestMessagingDiagnosticsStore::rcsFtHttpIncompleteDescriptorHasNoFileInfo()
{
    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("application/vnd.gsma.rcs-ft-http+xml");
    t.callId      = QStringLiteral("call-rcs-partial-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: application/vnd.gsma.rcs-ft-http+xml\r\n\r\n"
                                   "<file></file>");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QVERIFY(e.rcsFtHttp.present); // <file> root seen
    QVERIFY(e.rcsFtHttp.fileName.isEmpty());
    QCOMPARE(e.rcsFtHttp.fileSize, static_cast<qint64>(-1));
}

void TestMessagingDiagnosticsStore::bodyWithNullBytesSurvivesThePipeline()
{
    QByteArray body;
    body.append("plain-text-with-a-nul-byte:");
    body.append('\0');
    body.append(":end");

    QString rawSip = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "Content-Length: %1\r\n\r\n").arg(body.size());
    rawSip += QString::fromLatin1(body);

    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("text/plain");
    t.callId      = QStringLiteral("call-nul-1");
    t.rawSip      = rawSip;

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    // The preview collapses whitespace but must not crash/truncate at the NUL.
    QVERIFY(e.bodyPreview.contains(QStringLiteral("plain-text-with-a-nul-byte")));
    QVERIFY(e.bodyPreview.contains(QStringLiteral("end")));
}

QTEST_GUILESS_MAIN(TestMessagingDiagnosticsStore)
#include "test_messaging_diagnostics_store.moc"
