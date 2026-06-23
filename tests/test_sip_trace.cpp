#include <QtTest/QtTest>

#include "core/Logger.h"
#include "sip/SipMessageTrace.h"
#include "sip/SipTraceLogger.h"

// Tests for SipTraceLogger model in isolation.  No SipManager, SipCall, or
// network activity required — traces are injected directly via logMessage().

class TestSipTrace : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Model storage
    void registerTrace();
    void inviteTrace();
    void byeTrace();

    // Direction
    void directionInboundOutbound();

    // Security: credential redaction
    void authorizationRedacted();

    // Model lifecycle
    void clearAndExport();
};

// ---------------------------------------------------------------------------

void TestSipTrace::init()
{
    // Start each test with an empty trace store.
    SipTraceLogger::instance().clear();
}

void TestSipTrace::cleanup()
{
    SipTraceLogger::instance().clear();
}

// ---------------------------------------------------------------------------

void TestSipTrace::registerTrace()
{
    SipMessageTrace t;
    t.direction = SipMessageTrace::Direction::Outbound;
    t.method    = QStringLiteral("REGISTER");
    t.fromUri   = QStringLiteral("sip:alice@example.com");
    t.toUri     = QStringLiteral("sip:registrar.example.com");
    t.cSeq      = QStringLiteral("1 REGISTER");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(SipTraceLogger::instance().messages().size(), 1);
    const SipMessageTrace &stored = SipTraceLogger::instance().messages().first();
    QCOMPARE(stored.method, QStringLiteral("REGISTER"));
    QCOMPARE(stored.direction, SipMessageTrace::Direction::Outbound);
    QCOMPARE(stored.statusCode, 0);
    QVERIFY(!stored.timestamp.isNull());
}

void TestSipTrace::inviteTrace()
{
    SipMessageTrace t;
    t.direction = SipMessageTrace::Direction::Outbound;
    t.method    = QStringLiteral("INVITE");
    t.fromUri   = QStringLiteral("sip:alice@example.com");
    t.toUri     = QStringLiteral("sip:bob@example.com");
    t.callId    = QStringLiteral("abc-123-call-id");
    t.cSeq      = QStringLiteral("1 INVITE");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(SipTraceLogger::instance().messages().size(), 1);
    const SipMessageTrace &stored = SipTraceLogger::instance().messages().first();
    QCOMPARE(stored.method, QStringLiteral("INVITE"));
    QCOMPARE(stored.callId, QStringLiteral("abc-123-call-id"));
    QCOMPARE(stored.summary(), QStringLiteral("INVITE"));
}

void TestSipTrace::byeTrace()
{
    SipMessageTrace t;
    t.direction = SipMessageTrace::Direction::Outbound;
    t.method    = QStringLiteral("BYE");
    t.fromUri   = QStringLiteral("sip:alice@example.com");
    t.toUri     = QStringLiteral("sip:bob@example.com");
    t.cSeq      = QStringLiteral("3 BYE");

    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(SipTraceLogger::instance().messages().size(), 1);
    const SipMessageTrace &stored = SipTraceLogger::instance().messages().first();
    QCOMPARE(stored.method, QStringLiteral("BYE"));
    QCOMPARE(stored.direction, SipMessageTrace::Direction::Outbound);
}

void TestSipTrace::directionInboundOutbound()
{
    SipMessageTrace outbound;
    outbound.direction = SipMessageTrace::Direction::Outbound;
    outbound.method    = QStringLiteral("REGISTER");
    SipTraceLogger::instance().logMessage(outbound);

    SipMessageTrace inbound;
    inbound.direction  = SipMessageTrace::Direction::Inbound;
    inbound.statusCode = 200;
    inbound.statusText = QStringLiteral("OK");
    inbound.method     = QStringLiteral("REGISTER");
    SipTraceLogger::instance().logMessage(inbound);

    QCOMPARE(SipTraceLogger::instance().messages().size(), 2);

    const auto &m0 = SipTraceLogger::instance().messages().at(0);
    const auto &m1 = SipTraceLogger::instance().messages().at(1);

    QCOMPARE(m0.direction, SipMessageTrace::Direction::Outbound);
    QCOMPARE(m1.direction, SipMessageTrace::Direction::Inbound);

    QCOMPARE(m0.summary(), QStringLiteral("REGISTER"));
    QCOMPARE(m1.summary(), QStringLiteral("200 OK"));
}

void TestSipTrace::authorizationRedacted()
{
    const QString raw =
        QStringLiteral("REGISTER sip:registrar.example.com SIP/2.0\r\n"
                       "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds\r\n"
                       "Authorization: Digest username=\"alice\","
                       " realm=\"atlanta.com\","
                       " nonce=\"84a4cc6f3082121f32b42a2187831a9e\","
                       " response=\"7587245234b3434cc3412213e5f113a5432\"\r\n"
                       "Proxy-Authorization: Digest username=\"alice\","
                       " realm=\"atlanta.com\", response=\"deadbeefcafe\"\r\n"
                       "Content-Length: 0\r\n\r\n");

    SipMessageTrace t;
    t.direction = SipMessageTrace::Direction::Outbound;
    t.method    = QStringLiteral("REGISTER");
    t.rawSip    = raw;
    SipTraceLogger::instance().logMessage(t);

    QCOMPARE(SipTraceLogger::instance().messages().size(), 1);
    const QString &stored = SipTraceLogger::instance().messages().first().rawSip;

    // Credential values must be absent.
    QVERIFY(!stored.contains(QStringLiteral("7587245234b3434cc3412213e5f113a5432")));
    QVERIFY(!stored.contains(QStringLiteral("deadbeefcafe")));

    // [REDACTED] placeholder must be present.
    QVERIFY(stored.contains(QStringLiteral("[REDACTED]")));

    // Non-sensitive headers must be preserved.
    QVERIFY(stored.contains(QStringLiteral("Via:")));
    QVERIFY(stored.contains(QStringLiteral("Content-Length:")));
}

void TestSipTrace::clearAndExport()
{
    // Populate a few entries.
    {
        SipMessageTrace reg;
        reg.direction = SipMessageTrace::Direction::Outbound;
        reg.method    = QStringLiteral("REGISTER");
        reg.fromUri   = QStringLiteral("sip:alice@example.com");
        reg.toUri     = QStringLiteral("sip:registrar.example.com");
        reg.cSeq      = QStringLiteral("1 REGISTER");
        SipTraceLogger::instance().logMessage(reg);
    }
    {
        SipMessageTrace ok;
        ok.direction  = SipMessageTrace::Direction::Inbound;
        ok.statusCode = 200;
        ok.statusText = QStringLiteral("OK");
        ok.method     = QStringLiteral("REGISTER");
        ok.cSeq       = QStringLiteral("1 REGISTER");
        SipTraceLogger::instance().logMessage(ok);
    }

    QCOMPARE(SipTraceLogger::instance().messages().size(), 2);

    // Export to text must include method names.
    const QString text = SipTraceLogger::instance().exportToText();
    QVERIFY(text.contains(QStringLiteral("REGISTER")));
    QVERIFY(text.contains(QStringLiteral("200 OK")));
    QVERIFY(text.contains(QStringLiteral(">>>")));
    QVERIFY(text.contains(QStringLiteral("<<<")));

    // Export to JSON must include expected keys.
    const QString json = SipTraceLogger::instance().exportToJson();
    QVERIFY(json.contains(QStringLiteral("\"method\"")));
    QVERIFY(json.contains(QStringLiteral("\"direction\"")));
    QVERIFY(json.contains(QStringLiteral("outbound")));
    QVERIFY(json.contains(QStringLiteral("inbound")));

    // Signal emitted on clear.
    QSignalSpy clearSpy(&SipTraceLogger::instance(), &SipTraceLogger::cleared);
    SipTraceLogger::instance().clear();
    QCOMPARE(clearSpy.count(), 1);
    QCOMPARE(SipTraceLogger::instance().messages().size(), 0);
}

QTEST_GUILESS_MAIN(TestSipTrace)
#include "test_sip_trace.moc"
