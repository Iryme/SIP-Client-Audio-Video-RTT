#include <QtTest/QtTest>

#include "sip/MessagingDiagnosticsStore.h"
#include "sip/MessagingEventStore.h"
#include "sip/SipTraceLogger.h"

// Tests for MessagingDiagnosticsStore — relevance filtering and structured
// entry building (CPIM/IMDN/is-composing/SDP-MSRP) on top of raw SipMessageTrace
// data. No SipManager, SipCall, or network activity required.

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

    void imdnPlainXmlIsParsed();
    void imdnDeflateValidIsDecodedAndParsed();
    void imdnDeflateInvalidMarksPartialWithWarning();
    void isComposingPlainIsUnaffectedByEncodingSupport();
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

// ---- Task fix/w092-imdn-deflate-decoding: Content-Encoding: deflate -------

void TestMessagingDiagnosticsStore::imdnPlainXmlIsParsed()
{
    const QString imdnXml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">"
                       "<message-id>plain-1</message-id>"
                       "<delivery-notification><status><delivered/></status></delivery-notification>"
                       "</imdn>");

    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Inbound;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("message/imdn+xml");
    t.rawSip      = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                   "Content-Type: message/imdn+xml\r\n\r\n") + imdnXml;

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QVERIFY(!e.contentEncodingDecodeFailed);
    QVERIFY(e.imdn.present);
    QCOMPARE(e.imdn.disposition, ImdnInfo::Disposition::Delivered);
    QCOMPARE(e.imdn.messageId, QStringLiteral("plain-1"));
    QCOMPARE(e.bodyPreview, e.decodedBodyPreview);
}

void TestMessagingDiagnosticsStore::imdnDeflateValidIsDecodedAndParsed()
{
    const QString imdnXml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">"
                       "<message-id>deflate-1</message-id>"
                       "<delivery-notification><status><delivered/></status></delivery-notification>"
                       "</imdn>");

    // qCompress() produces [4-byte length prefix][zlib stream]; stripping
    // the prefix leaves exactly the bare zlib/deflate bytes a UA would put
    // on the wire for "Content-Encoding: deflate".
    const QByteArray zlibStream = qCompress(imdnXml.toUtf8()).mid(4);
    const QString compressedBody = QString::fromLatin1(zlibStream);

    SipMessageTrace t;
    t.direction       = SipMessageTrace::Direction::Inbound;
    t.method          = QStringLiteral("MESSAGE");
    t.contentType     = QStringLiteral("message/imdn+xml");
    t.contentEncoding = QStringLiteral("deflate");
    t.callId          = QStringLiteral("call-deflate-valid-1");
    t.rawSip          = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                       "Content-Type: message/imdn+xml\r\n"
                                       "Content-Encoding: deflate\r\n\r\n") + compressedBody;

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QCOMPARE(e.contentEncoding, QStringLiteral("deflate"));
    QVERIFY(!e.contentEncodingDecodeFailed);
    QVERIFY(e.imdn.present);
    QCOMPARE(e.imdn.disposition, ImdnInfo::Disposition::Delivered);
    QCOMPARE(e.imdn.messageId, QStringLiteral("deflate-1"));
    QVERIFY(e.decodedBodyPreview.contains(QStringLiteral("deflate-1")));
    // rawSip must stay exactly the original (still-compressed) bytes.
    QVERIFY(e.rawSip.contains(compressedBody));
    QVERIFY(!e.rawSip.contains(QStringLiteral("deflate-1")));

    // MessagingEventStore reuses the same classification — deflate success
    // must not be flagged as a parsing problem.
    const MessagingEvent ev = MessagingEventStore::mapFromTraceEntry(e, 1);
    QCOMPARE(ev.parseStatus, MessagingEvent::ParseStatus::Ok);
    QVERIFY(ev.parseWarnings.isEmpty());
}

void TestMessagingDiagnosticsStore::imdnDeflateInvalidMarksPartialWithWarning()
{
    SipMessageTrace t;
    t.direction       = SipMessageTrace::Direction::Inbound;
    t.method          = QStringLiteral("MESSAGE");
    t.contentType     = QStringLiteral("message/imdn+xml");
    t.contentEncoding = QStringLiteral("deflate");
    t.rawSip          = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                       "Content-Type: message/imdn+xml\r\n"
                                       "Content-Encoding: deflate\r\n\r\n"
                                       "this is not a valid deflate stream");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(MessagingDiagnosticsStore::instance().entries().size(), 1);
    const MessagingTraceEntry &e = MessagingDiagnosticsStore::instance().entries().first();
    QCOMPARE(e.contentEncoding, QStringLiteral("deflate"));
    QVERIFY(e.contentEncodingDecodeFailed);
    QVERIFY(!e.imdn.present);
    QVERIFY(e.decodedBodyPreview.isEmpty());

    const MessagingEvent ev = MessagingEventStore::mapFromTraceEntry(e, 1);
    QCOMPARE(ev.parseStatus, MessagingEvent::ParseStatus::Partial);
    QVERIFY(ev.parseWarnings.contains(QStringLiteral("deflate decode failed")));
}

void TestMessagingDiagnosticsStore::isComposingPlainIsUnaffectedByEncodingSupport()
{
    // Regression check: adding Content-Encoding handling must not break the
    // existing plain (unencoded) is-composing path.
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
    QVERIFY(e.contentEncoding.isEmpty());
    QVERIFY(!e.contentEncodingDecodeFailed);
    QVERIFY(e.isComposing.present);
    QCOMPARE(e.isComposing.state, IsComposingInfo::State::Active);
}

QTEST_GUILESS_MAIN(TestMessagingDiagnosticsStore)
#include "test_messaging_diagnostics_store.moc"
