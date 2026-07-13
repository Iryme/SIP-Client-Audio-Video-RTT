#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "sip/InteropTraceExporter.h"
#include "sip/MessagingDiagnosticsStore.h"
#include "sip/PresenceDiagnosticsStore.h"
#include "sip/SipTraceLogger.h"
#include "msrp/MsrpRelayDiagnosticsEvent.h"
#include "msrp/MsrpSessionInfo.h"

// Tests for InteropTraceExporter — the server-compatible JSON export (Task
// W094, extended by Task W095) built on top of the existing, unmodified
// W090/W091 diagnostics pipeline. No parsing is duplicated here: every trace
// is logged through SipTraceLogger exactly like the other messaging tests,
// and MessagingDiagnosticsStore::buildEntry() (Task W090, extended by W095)
// does the actual CPIM/IMDN/is-composing/SDP-MSRP/RCS-FT-HTTP parsing and
// Content-Encoding decoding; this exporter only serializes the already-built
// MessagingTraceEntry.

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
} // namespace

class TestWindowsTraceJsonExport : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void exportSipMessageBasic();
    void exportCpimFields();
    void exportImdnFields();
    void exportIsComposingFields();
    void exportSdpMsrpFields();
    void rawSipRedacted();
    void sampleExportIsValidJson();
    void requiredFieldsPresent();

    void exportContentEncodingDecodedFields();
    void exportContentEncodingFailedFields();
    void exportRcsFileTransferFields();

    void exportPresenceEventsSection();
    void presenceEventsDoNotAffectMessagingEventsSchema();

    void exportMsrpSessionV3Fields();
    void exportMsrpRelayEventsFieldsRedacted();
    void exportMsrpCallPreparationEventsFieldsRedacted();

private:
    static void logMessage(const SipMessageTrace &t) { SipTraceLogger::instance().logMessage(t); }
    static QJsonObject firstEvent(const QString &json);
};

void TestWindowsTraceJsonExport::init()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
    PresenceDiagnosticsStore::instance().clear();
}

void TestWindowsTraceJsonExport::cleanup()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
    PresenceDiagnosticsStore::instance().clear();
}

QJsonObject TestWindowsTraceJsonExport::firstEvent(const QString &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    const QJsonArray events = doc.object().value(QStringLiteral("events")).toArray();
    return events.isEmpty() ? QJsonObject() : events.first().toObject();
}

void TestWindowsTraceJsonExport::exportSipMessageBasic()
{
    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Outbound;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("text/plain");
    t.callId      = QStringLiteral("call-basic-1");
    t.cSeq        = QStringLiteral("1 MESSAGE");
    t.fromUri     = QStringLiteral("sip:alice@example.com");
    t.toUri       = QStringLiteral("sip:bob@example.com");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: text/plain\r\n\r\nHello Bob!");
    logMessage(t);

    const QString json = InteropTraceExporter::exportToJson();
    const QJsonObject ev = firstEvent(json);

    QCOMPARE(ev.value(QStringLiteral("direction")).toString(), QStringLiteral("outbound"));
    QCOMPARE(ev.value(QStringLiteral("transport")).toString(), QStringLiteral("sip-message"));
    QCOMPARE(ev.value(QStringLiteral("payloadType")).toString(), QStringLiteral("plain"));
    QCOMPARE(ev.value(QStringLiteral("callId")).toString(), QStringLiteral("call-basic-1"));
    QCOMPARE(ev.value(QStringLiteral("cseq")).toString(), QStringLiteral("1 MESSAGE"));
    QCOMPARE(ev.value(QStringLiteral("from")).toString(), QStringLiteral("sip:alice@example.com"));
    QCOMPARE(ev.value(QStringLiteral("to")).toString(), QStringLiteral("sip:bob@example.com"));
    QCOMPARE(ev.value(QStringLiteral("contentType")).toString(), QStringLiteral("text/plain"));
    QCOMPARE(ev.value(QStringLiteral("bodyPreview")).toString(), QStringLiteral("Hello Bob!"));
}

void TestWindowsTraceJsonExport::exportCpimFields()
{
    const QString cpimBody =
        QStringLiteral("From: <im:alice@example.com>\r\n"
                       "To: <im:bob@example.com>\r\n"
                       "DateTime: 2024-01-01T00:00:00Z\r\n"
                       "Subject: hello\r\n"
                       "Content-Type: text/plain\r\n\r\nHi there");

    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("message/cpim");
    t.callId      = QStringLiteral("call-cpim-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: message/cpim\r\n\r\n") + cpimBody;
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    QVERIFY(ev.contains(QStringLiteral("cpim")));
    const QJsonObject cpim = ev.value(QStringLiteral("cpim")).toObject();
    QCOMPARE(cpim.value(QStringLiteral("from")).toString(), QStringLiteral("<im:alice@example.com>"));
    QCOMPARE(cpim.value(QStringLiteral("to")).toString(), QStringLiteral("<im:bob@example.com>"));
    QCOMPARE(cpim.value(QStringLiteral("dateTime")).toString(), QStringLiteral("2024-01-01T00:00:00Z"));
    QCOMPARE(cpim.value(QStringLiteral("subject")).toString(), QStringLiteral("hello"));
    QCOMPARE(cpim.value(QStringLiteral("contentType")).toString(), QStringLiteral("text/plain"));
}

void TestWindowsTraceJsonExport::exportImdnFields()
{
    const QString imdnXml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">"
                       "<message-id>msg-42</message-id>"
                       "<original-recipient>sip:bob@example.com</original-recipient>"
                       "<final-recipient>sip:bob@example.com</final-recipient>"
                       "<delivery-notification><status><delivered/></status></delivery-notification>"
                       "</imdn>");

    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("message/imdn+xml");
    t.callId      = QStringLiteral("call-imdn-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                   "Content-Type: message/imdn+xml\r\n\r\n") + imdnXml;
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    QCOMPARE(ev.value(QStringLiteral("messageId")).toString(), QStringLiteral("msg-42"));
    QVERIFY(ev.contains(QStringLiteral("imdn")));
    const QJsonObject imdn = ev.value(QStringLiteral("imdn")).toObject();
    QCOMPARE(imdn.value(QStringLiteral("messageId")).toString(), QStringLiteral("msg-42"));
    QCOMPARE(imdn.value(QStringLiteral("originalRecipient")).toString(), QStringLiteral("sip:bob@example.com"));
    QCOMPARE(imdn.value(QStringLiteral("finalRecipient")).toString(), QStringLiteral("sip:bob@example.com"));
    QCOMPARE(imdn.value(QStringLiteral("disposition")).toString(), QStringLiteral("delivered"));
}

void TestWindowsTraceJsonExport::exportIsComposingFields()
{
    const QString isComposingXml =
        QStringLiteral("<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">"
                       "<state>active</state><refresh>60</refresh></isComposing>");

    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("application/im-iscomposing+xml");
    t.callId      = QStringLiteral("call-ic-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: application/im-iscomposing+xml\r\n\r\n") + isComposingXml;
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    QVERIFY(ev.contains(QStringLiteral("isComposing")));
    const QJsonObject ic = ev.value(QStringLiteral("isComposing")).toObject();
    QCOMPARE(ic.value(QStringLiteral("state")).toString(), QStringLiteral("active"));
    QCOMPARE(ic.value(QStringLiteral("refresh")).toString(), QStringLiteral("60"));
}

void TestWindowsTraceJsonExport::exportSdpMsrpFields()
{
    const QString sdp =
        QStringLiteral("v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=-\r\nc=IN IP4 127.0.0.1\r\n"
                       "t=0 0\r\nm=message 12345 TCP/MSRP *\r\n"
                       "a=path:msrp://127.0.0.1:12345/session-xyz;tcp\r\n"
                       "a=accept-types:text/plain message/cpim\r\n"
                       "a=setup:active\r\na=connection:new\r\n");

    SipMessageTrace t;
    t.method      = QStringLiteral("INVITE");
    t.contentType = QStringLiteral("application/sdp");
    t.callId      = QStringLiteral("call-msrp-1");
    t.rawSip      = QStringLiteral("INVITE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: application/sdp\r\n\r\n") + sdp;
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    QCOMPARE(ev.value(QStringLiteral("transport")).toString(), QStringLiteral("msrp"));
    QVERIFY(ev.contains(QStringLiteral("msrp")));
    const QJsonObject msrp = ev.value(QStringLiteral("msrp")).toObject();
    QCOMPARE(msrp.value(QStringLiteral("transportProtocol")).toString(), QStringLiteral("TCP/MSRP"));
    QVERIFY(msrp.value(QStringLiteral("mediaLine")).toString().contains(QStringLiteral("m=message")));
    QVERIFY(msrp.value(QStringLiteral("path")).toString().contains(QStringLiteral("session-xyz")));
    QCOMPARE(msrp.value(QStringLiteral("acceptTypes")).toString(),
             QStringLiteral("text/plain message/cpim"));
    QCOMPARE(msrp.value(QStringLiteral("setup")).toString(), QStringLiteral("active"));
    QCOMPARE(msrp.value(QStringLiteral("connection")).toString(), QStringLiteral("new"));
    QCOMPARE(msrp.value(QStringLiteral("sessionId")).toString(), QStringLiteral("session-xyz"));
}

void TestWindowsTraceJsonExport::rawSipRedacted()
{
    SipMessageTrace t;
    t.method = QStringLiteral("MESSAGE");
    t.callId = QStringLiteral("call-redact-1");
    t.rawSip = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                              "Authorization: Digest username=\"bob\", response=\"secret-hash\"\r\n"
                              "\r\nHi");
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    const QString rawSipRedacted = ev.value(QStringLiteral("rawSipRedacted")).toString();
    QVERIFY(rawSipRedacted.contains(QStringLiteral("[REDACTED]")));
    QVERIFY(!rawSipRedacted.contains(QStringLiteral("secret-hash")));
}

void TestWindowsTraceJsonExport::sampleExportIsValidJson()
{
    SipMessageTrace t;
    t.method = QStringLiteral("MESSAGE");
    t.callId = QStringLiteral("call-valid-1");
    t.rawSip = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n\r\nHi");
    logMessage(t);

    const QString json = InteropTraceExporter::exportToJson();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    QCOMPARE(err.error, QJsonParseError::NoError);
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value(QStringLiteral("schemaVersion")).toInt(), InteropTraceExporter::kSchemaVersion);
    QCOMPARE(doc.object().value(QStringLiteral("source")).toString(), QStringLiteral("windows-client"));
    QVERIFY(!doc.object().value(QStringLiteral("exportedAt")).toString().isEmpty());
    QVERIFY(doc.object().value(QStringLiteral("events")).isArray());
}

void TestWindowsTraceJsonExport::requiredFieldsPresent()
{
    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("text/plain");
    t.callId      = QStringLiteral("call-req-1");
    t.cSeq        = QStringLiteral("1 MESSAGE");
    t.fromUri     = QStringLiteral("sip:a@x.com");
    t.toUri       = QStringLiteral("sip:b@x.com");
    t.rawSip      = QStringLiteral("MESSAGE sip:b@x.com SIP/2.0\r\n\r\nHi");
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    const QStringList requiredEventFields = {
        QStringLiteral("eventId"), QStringLiteral("timestamp"), QStringLiteral("direction"),
        QStringLiteral("status"), QStringLiteral("transport"), QStringLiteral("payloadType"),
        QStringLiteral("callId"), QStringLiteral("cseq"), QStringLiteral("from"),
        QStringLiteral("to"), QStringLiteral("contentType"), QStringLiteral("bodyPreview"),
        QStringLiteral("rawSipRedacted")
    };
    for (const QString &field : requiredEventFields)
        QVERIFY2(ev.contains(field), qPrintable(QStringLiteral("missing field: ") + field));
}

void TestWindowsTraceJsonExport::exportContentEncodingDecodedFields()
{
    const QString imdnXml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">"
                       "<message-id>msg-deflate-1</message-id>"
                       "<delivery-notification><status><displayed/></status></delivery-notification>"
                       "</imdn>");
    const QByteArray compressed = storedDeflateBlock(imdnXml.toUtf8());

    SipMessageTrace t;
    t.method          = QStringLiteral("MESSAGE");
    t.contentType     = QStringLiteral("message/imdn+xml");
    t.contentEncoding = QStringLiteral("deflate");
    t.callId          = QStringLiteral("call-deflate-1");
    t.rawSip          = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                       "Content-Type: message/imdn+xml\r\n"
                                       "Content-Encoding: deflate\r\n\r\n")
                       + QString::fromLatin1(compressed);
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    QCOMPARE(ev.value(QStringLiteral("contentEncoding")).toString(), QStringLiteral("deflate"));
    QCOMPARE(ev.value(QStringLiteral("decodeStatus")).toString(), QStringLiteral("decoded"));
    QCOMPARE(ev.value(QStringLiteral("decodeVariant")).toString(), QStringLiteral("raw-deflate"));
    QVERIFY(ev.value(QStringLiteral("compressedBodyLength")).toInt() > 0);
    QVERIFY(ev.value(QStringLiteral("decodedBodyLength")).toInt() > 0);
    QVERIFY(ev.value(QStringLiteral("decodedBodyPreview")).toString().contains(QStringLiteral("msg-deflate-1")));
    QCOMPARE(ev.value(QStringLiteral("parseStatus")).toString(), QStringLiteral("ok"));

    const QJsonObject imdn = ev.value(QStringLiteral("imdn")).toObject();
    QCOMPARE(imdn.value(QStringLiteral("messageId")).toString(), QStringLiteral("msg-deflate-1"));
    QCOMPARE(imdn.value(QStringLiteral("disposition")).toString(), QStringLiteral("displayed"));
}

void TestWindowsTraceJsonExport::exportContentEncodingFailedFields()
{
    SipMessageTrace t;
    t.method          = QStringLiteral("MESSAGE");
    t.contentType     = QStringLiteral("message/imdn+xml");
    t.contentEncoding = QStringLiteral("deflate");
    t.callId          = QStringLiteral("call-deflate-invalid-1");
    t.rawSip          = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                       "Content-Type: message/imdn+xml\r\n"
                                       "Content-Encoding: deflate\r\n\r\n"
                                       "not-a-valid-deflate-stream-at-all");
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    QCOMPARE(ev.value(QStringLiteral("contentEncoding")).toString(), QStringLiteral("deflate"));
    QCOMPARE(ev.value(QStringLiteral("decodeStatus")).toString(), QStringLiteral("failed"));
    QVERIFY(!ev.value(QStringLiteral("decodeError")).toString().isEmpty());
    QCOMPARE(ev.value(QStringLiteral("parseStatus")).toString(), QStringLiteral("partial"));

    const QJsonArray warnings = ev.value(QStringLiteral("parseWarnings")).toArray();
    bool sawDeflateWarning = false;
    for (const QJsonValue &w : warnings) {
        if (w.toString().contains(QStringLiteral("deflate decode failed")))
            sawDeflateWarning = true;
    }
    QVERIFY(sawDeflateWarning);
    QVERIFY(!ev.contains(QStringLiteral("imdn"))); // never XML-parsed the compressed bytes
}

void TestWindowsTraceJsonExport::exportRcsFileTransferFields()
{
    const QString rcsXml = QStringLiteral(
        "<file>"
        "  <file-info type=\"file\">"
        "    <file-size>4096</file-size>"
        "    <file-name>report.pdf</file-name>"
        "    <content-type>application/pdf</content-type>"
        "    <data url=\"https://files.example.test/dl/xyz?token=one-time-secret\" until=\"2030-01-01T00:00:00.000Z\"/>"
        "  </file-info>"
        "</file>");

    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("application/vnd.gsma.rcs-ft-http+xml");
    t.callId      = QStringLiteral("call-rcs-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: application/vnd.gsma.rcs-ft-http+xml\r\n\r\n") + rcsXml;
    logMessage(t);

    const QJsonObject ev = firstEvent(InteropTraceExporter::exportToJson());
    QCOMPARE(ev.value(QStringLiteral("payloadType")).toString(), QStringLiteral("rcs-ft-http"));
    QVERIFY(ev.contains(QStringLiteral("rcsFileTransfer")));

    const QJsonObject rcs = ev.value(QStringLiteral("rcsFileTransfer")).toObject();
    QCOMPARE(rcs.value(QStringLiteral("fileName")).toString(), QStringLiteral("report.pdf"));
    QCOMPARE(rcs.value(QStringLiteral("fileSize")).toDouble(), 4096.0);
    QCOMPARE(rcs.value(QStringLiteral("contentType")).toString(), QStringLiteral("application/pdf"));
    QCOMPARE(rcs.value(QStringLiteral("expiresAt")).toString(), QStringLiteral("2030-01-01T00:00:00.000Z"));
    QVERIFY(!rcs.value(QStringLiteral("thumbnailPresent")).toBool());

    const QString redacted = rcs.value(QStringLiteral("dataUrlRedacted")).toString();
    QVERIFY(!redacted.contains(QStringLiteral("one-time-secret")));
    QVERIFY(redacted.startsWith(QStringLiteral("https://files.example.test")));
}

void TestWindowsTraceJsonExport::exportPresenceEventsSection()
{
    const QString pidf = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:bob@example.com\">"
        "<tuple id=\"t1\"><status><basic>open</basic></status></tuple></presence>");

    SipMessageTrace t;
    t.method      = QStringLiteral("NOTIFY");
    t.contentType = QStringLiteral("application/pidf+xml");
    t.callId      = QStringLiteral("call-presence-1");
    t.rawSip      = QStringLiteral(
        "NOTIFY sip:alice@example.com SIP/2.0\r\n"
        "Call-ID: call-presence-1\r\n"
        "CSeq: 1 NOTIFY\r\n"
        "Event: presence\r\n"
        "Subscription-State: active;expires=300\r\n"
        "Content-Type: application/pidf+xml\r\n\r\n") + pidf;
    logMessage(t);

    const QString json = InteropTraceExporter::exportToJson();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    QCOMPARE(err.error, QJsonParseError::NoError);
    QVERIFY(doc.object().value(QStringLiteral("presenceEvents")).isArray());

    const QJsonArray presenceEvents = doc.object().value(QStringLiteral("presenceEvents")).toArray();
    QCOMPARE(presenceEvents.size(), 1);
    const QJsonObject ev = presenceEvents.first().toObject();
    QCOMPARE(ev.value(QStringLiteral("method")).toString(), QStringLiteral("NOTIFY"));
    QCOMPARE(ev.value(QStringLiteral("eventPackage")).toString(), QStringLiteral("presence"));
    QCOMPARE(ev.value(QStringLiteral("subscriptionState")).toString(), QStringLiteral("active"));
    QCOMPARE(ev.value(QStringLiteral("subscriptionExpires")).toInt(), 300);
    QCOMPARE(ev.value(QStringLiteral("parseStatus")).toString(), QStringLiteral("ok"));

    const QJsonObject presence = ev.value(QStringLiteral("presence")).toObject();
    QCOMPARE(presence.value(QStringLiteral("entity")).toString(), QStringLiteral("sip:bob@example.com"));
    QCOMPARE(presence.value(QStringLiteral("tupleId")).toString(), QStringLiteral("t1"));
    QCOMPARE(presence.value(QStringLiteral("basicStatus")).toString(), QStringLiteral("open"));
}

void TestWindowsTraceJsonExport::presenceEventsDoNotAffectMessagingEventsSchema()
{
    // A plain SIP MESSAGE export must still work unchanged ("events" is
    // unaffected by the new "presenceEvents" key) regardless of which
    // schemaVersion is current.
    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("text/plain");
    t.callId      = QStringLiteral("call-basic-2");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n\r\nHi");
    logMessage(t);

    const QString json = InteropTraceExporter::exportToJson();
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QCOMPARE(doc.object().value(QStringLiteral("schemaVersion")).toInt(), InteropTraceExporter::kSchemaVersion);
    QVERIFY(doc.object().value(QStringLiteral("events")).toArray().size() >= 1);
    QVERIFY(doc.object().contains(QStringLiteral("presenceEvents")));
}

void TestWindowsTraceJsonExport::exportMsrpSessionV3Fields()
{
    // Task W102 Phase 10 (schemaVersion 3): role/remoteSetup/negotiationState/
    // peerAssociation on msrpSessions entries, built directly through the
    // public exportToJson(entries, presence, xcap, msrpSessions, msrpEvents)
    // overload — no live SipCall/MsrpSession required.
    MsrpSessionInfo info;
    info.sessionKey = QStringLiteral("session-1");
    info.sipHeaderCallId = QStringLiteral("abc123@example.com");
    info.mediaIndex = 2;
    info.role = MsrpRole::PassiveListener;
    info.remoteSetup = MsrpSetup::Active;
    info.offerAnswerState = QStringLiteral("answer");
    info.state = MsrpSessionState::Established;

    const QString json = InteropTraceExporter::exportToJson(
        {}, {}, {}, {info}, {});
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QCOMPARE(doc.object().value(QStringLiteral("schemaVersion")).toInt(), 3);

    const QJsonArray sessions = doc.object().value(QStringLiteral("msrpSessions")).toArray();
    QCOMPARE(sessions.size(), 1);
    const QJsonObject s = sessions.first().toObject();
    QCOMPARE(s.value(QStringLiteral("role")).toString(), QStringLiteral("passive-listener"));
    QCOMPARE(s.value(QStringLiteral("remoteSetup")).toString(), QStringLiteral("active"));
    QCOMPARE(s.value(QStringLiteral("negotiationState")).toString(), QStringLiteral("answer"));

    const QJsonObject peerAssociation = s.value(QStringLiteral("peerAssociation")).toObject();
    QCOMPARE(peerAssociation.value(QStringLiteral("method")).toString(), QStringLiteral("sip-dialog-mapping"));
    QCOMPARE(peerAssociation.value(QStringLiteral("mediaIndex")).toInt(), 2);
    QCOMPARE(peerAssociation.value(QStringLiteral("sipHeaderCallId")).toString(), QStringLiteral("abc123@example.com"));
    QCOMPARE(peerAssociation.value(QStringLiteral("confidence")).toString(), QStringLiteral("exact"));

    // No mediaIndex/sipHeaderCallId -> confidence must not falsely claim "exact".
    MsrpSessionInfo unmapped;
    unmapped.sessionKey = QStringLiteral("session-2");
    const QString json2 = InteropTraceExporter::exportToJson({}, {}, {}, {unmapped}, {});
    const QJsonObject s2 = QJsonDocument::fromJson(json2.toUtf8())
        .object().value(QStringLiteral("msrpSessions")).toArray().first().toObject();
    QCOMPARE(s2.value(QStringLiteral("peerAssociation")).toObject()
        .value(QStringLiteral("confidence")).toString(), QStringLiteral("unknown"));
}

void TestWindowsTraceJsonExport::exportMsrpRelayEventsFieldsRedacted()
{
    // Task W107: msrpRelayEvents is purely additive (schemaVersion stays 3)
    // and every field is already pre-redacted by MsrpRelayDiagnosticsEvent
    // itself — this exporter never sees a nonce/response/credential value in
    // the first place, so there is nothing here that could leak one.
    MsrpRelayDiagnosticsEvent ev;
    ev.timestamp = QDateTime::currentDateTimeUtc();
    ev.kind = MsrpRelayDiagnosticsEvent::Kind::AllocationSuccess;
    ev.relayConnectionId = QStringLiteral("conn-1");
    ev.allocationId = QStringLiteral("alloc-1");
    ev.sipCallIdRedacted = QStringLiteral("call-abc…");
    ev.mediaIndex = 1;
    ev.responseCode = 200;
    ev.responseComment = QStringLiteral("OK");
    ev.algorithm = QStringLiteral("MD5");
    ev.qopUsed = true;
    ev.allocatedPathRedacted = QStringLiteral("relay.example.com:2855");
    ev.expiresAt = QDateTime::currentDateTimeUtc().addSecs(600);

    const QString json = InteropTraceExporter::exportToJson({}, {}, {}, {}, {}, {ev});
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QCOMPARE(doc.object().value(QStringLiteral("schemaVersion")).toInt(), InteropTraceExporter::kSchemaVersion);

    const QJsonArray relayEvents = doc.object().value(QStringLiteral("msrpRelayEvents")).toArray();
    QCOMPARE(relayEvents.size(), 1);
    const QJsonObject r = relayEvents.first().toObject();
    QCOMPARE(r.value(QStringLiteral("kind")).toString(), QStringLiteral("allocation-success"));
    QCOMPARE(r.value(QStringLiteral("responseCode")).toInt(), 200);
    QCOMPARE(r.value(QStringLiteral("qopUsed")).toBool(), true);
    QCOMPARE(r.value(QStringLiteral("allocatedPathRedacted")).toString(), QStringLiteral("relay.example.com:2855"));

    // Structural guarantee that nothing sensitive can appear: the exported
    // object's keys are a fixed, known set — no free-form "path"/"nonce"/
    // "credentials" key exists for a caller to have accidentally populated.
    const QStringList keys = r.keys();
    for (const QString &forbidden : {QStringLiteral("nonce"), QStringLiteral("password"),
                                     QStringLiteral("digest"), QStringLiteral("credentials")}) {
        QVERIFY(!keys.contains(forbidden));
    }
}

void TestWindowsTraceJsonExport::exportMsrpCallPreparationEventsFieldsRedacted()
{
    // Task W108: msrpCallPreparationEvents is purely additive (schemaVersion
    // stays 3). Only the preparation id, resolved mode, and an internal
    // allocation id are ever recorded — no credential/nonce/full path.
    MsrpCallPreparationDiagnosticsEvent ev;
    ev.timestamp = QDateTime::currentDateTimeUtc();
    ev.preparationId = QStringLiteral("prep-1");
    ev.state = MsrpCallPreparationController::State::Ready;
    ev.mode = QStringLiteral("relay");
    ev.allocationId = QStringLiteral("alloc-1");
    ev.reason = QString();

    const QString json = InteropTraceExporter::exportToJson({}, {}, {}, {}, {}, {}, {ev});
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QCOMPARE(doc.object().value(QStringLiteral("schemaVersion")).toInt(), InteropTraceExporter::kSchemaVersion);

    const QJsonArray prepEvents = doc.object().value(QStringLiteral("msrpCallPreparationEvents")).toArray();
    QCOMPARE(prepEvents.size(), 1);
    const QJsonObject p = prepEvents.first().toObject();
    QCOMPARE(p.value(QStringLiteral("preparationId")).toString(), QStringLiteral("prep-1"));
    QCOMPARE(p.value(QStringLiteral("state")).toString(), QStringLiteral("ready"));
    QCOMPARE(p.value(QStringLiteral("mode")).toString(), QStringLiteral("relay"));

    const QStringList keys = p.keys();
    for (const QString &forbidden : {QStringLiteral("nonce"), QStringLiteral("password"),
                                     QStringLiteral("digest"), QStringLiteral("credentials"),
                                     QStringLiteral("path")}) {
        QVERIFY(!keys.contains(forbidden));
    }
}

QTEST_GUILESS_MAIN(TestWindowsTraceJsonExport)
#include "test_windows_trace_json_export.moc"
