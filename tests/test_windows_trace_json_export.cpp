#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "sip/InteropTraceExporter.h"
#include "sip/MessagingDiagnosticsStore.h"
#include "sip/SipTraceLogger.h"

// Tests for InteropTraceExporter — the server-compatible JSON export (Task
// W094) built on top of the existing, unmodified W090/W091 diagnostics
// pipeline. No parsing is duplicated here: every trace is logged through
// SipTraceLogger exactly like the other messaging tests, and
// MessagingDiagnosticsStore::buildEntry() (Task W090) does the actual CPIM/
// IMDN/is-composing/SDP-MSRP parsing; this exporter only serializes the
// already-parsed MessagingTraceEntry.

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

private:
    static void logMessage(const SipMessageTrace &t) { SipTraceLogger::instance().logMessage(t); }
    static QJsonObject firstEvent(const QString &json);
};

void TestWindowsTraceJsonExport::init()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
}

void TestWindowsTraceJsonExport::cleanup()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
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

QTEST_GUILESS_MAIN(TestWindowsTraceJsonExport)
#include "test_windows_trace_json_export.moc"
