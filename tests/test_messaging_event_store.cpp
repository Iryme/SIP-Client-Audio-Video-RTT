#include <QtTest/QtTest>

#include "sip/MessagingDiagnosticsStore.h"
#include "sip/MessagingEventStore.h"
#include "sip/SipTraceLogger.h"

// Tests for MessagingEventStore — the transport-independent messaging event
// model (Task W091) built on top of MessagingDiagnosticsStore (Task W090).
// No SipManager, SipCall, or network activity required.

class TestMessagingEventStore : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void appendClearCount();
    void sipMessageMapsToEvent();
    void cpimWrappedImdnMapsPayloadType();
    void malformedImdnProducesParseWarning();
    void maxEventsRetainedEvictsOldest();
    void exportJsonContainsExpectedFields();
    void exportTextContainsExpectedFields();

private:
    static void logMessage(const SipMessageTrace &t) { SipTraceLogger::instance().logMessage(t); }
};

void TestMessagingEventStore::init()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
    MessagingEventStore::instance().setMaxEventsRetained(1000);
}

void TestMessagingEventStore::cleanup()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
    MessagingEventStore::instance().setMaxEventsRetained(1000);
}

void TestMessagingEventStore::appendClearCount()
{
    QCOMPARE(MessagingEventStore::instance().count(), 0);

    MessagingEvent e;
    e.direction = MessagingEvent::Direction::Outbound;
    e.transport = MessagingEvent::Transport::SipMessage;
    MessagingEventStore::instance().append(e);
    MessagingEventStore::instance().append(e);

    QCOMPARE(MessagingEventStore::instance().count(), 2);
    QCOMPARE(MessagingEventStore::instance().snapshot().size(), 2);

    QSignalSpy clearSpy(&MessagingEventStore::instance(), &MessagingEventStore::cleared);
    MessagingEventStore::instance().clear();
    QCOMPARE(clearSpy.count(), 1);
    QCOMPARE(MessagingEventStore::instance().count(), 0);
}

void TestMessagingEventStore::sipMessageMapsToEvent()
{
    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Outbound;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("text/plain");
    t.callId      = QStringLiteral("call-1");
    t.fromUri     = QStringLiteral("sip:alice@example.com");
    t.toUri       = QStringLiteral("sip:bob@example.com");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: text/plain\r\n\r\n"
                                   "Hello Bob!");

    QSignalSpy appendSpy(&MessagingEventStore::instance(), &MessagingEventStore::eventAppended);
    logMessage(t);

    QCOMPARE(appendSpy.count(), 1);
    QCOMPARE(MessagingEventStore::instance().count(), 1);

    const MessagingEvent e = MessagingEventStore::instance().snapshot().first();
    QCOMPARE(e.transport, MessagingEvent::Transport::SipMessage);
    QCOMPARE(e.payloadType, MessagingEvent::PayloadType::Plain);
    QCOMPARE(e.direction, MessagingEvent::Direction::Outbound);
    QCOMPARE(e.from, QStringLiteral("sip:alice@example.com"));
    QCOMPARE(e.to, QStringLiteral("sip:bob@example.com"));
    QCOMPARE(e.callId, QStringLiteral("call-1"));
    QCOMPARE(e.bodyPreview, QStringLiteral("Hello Bob!"));
    QCOMPARE(e.parseStatus, MessagingEvent::ParseStatus::Ok);
    QVERIFY(e.parseWarnings.isEmpty());
    QVERIFY(e.id > 0);
}

void TestMessagingEventStore::cpimWrappedImdnMapsPayloadType()
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
    t.callId      = QStringLiteral("cpim-call-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                   "Content-Type: message/cpim\r\n\r\n") + cpimBody;

    logMessage(t);

    QCOMPARE(MessagingEventStore::instance().count(), 1);
    const MessagingEvent e = MessagingEventStore::instance().snapshot().first();
    // Outer Content-Type is CPIM; the inner IMDN kind is only visible via the
    // structured MessagingTraceEntry (still reachable via
    // MessagingDiagnosticsStore, not re-parsed here).
    QCOMPARE(e.payloadType, MessagingEvent::PayloadType::Cpim);
    QCOMPARE(e.parseStatus, MessagingEvent::ParseStatus::Ok);
}

void TestMessagingEventStore::malformedImdnProducesParseWarning()
{
    SipMessageTrace t;
    t.direction   = SipMessageTrace::Direction::Inbound;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("message/imdn+xml");
    t.rawSip      = QStringLiteral("MESSAGE sip:alice@example.com SIP/2.0\r\n"
                                   "Content-Type: message/imdn+xml\r\n\r\n"
                                   "not xml at all");

    logMessage(t);

    QCOMPARE(MessagingEventStore::instance().count(), 1);
    const MessagingEvent e = MessagingEventStore::instance().snapshot().first();
    QCOMPARE(e.payloadType, MessagingEvent::PayloadType::Imdn);
    QCOMPARE(e.parseStatus, MessagingEvent::ParseStatus::Partial);
    QVERIFY(!e.parseWarnings.isEmpty());
}

void TestMessagingEventStore::maxEventsRetainedEvictsOldest()
{
    MessagingEventStore::instance().setMaxEventsRetained(3);

    for (int i = 0; i < 5; ++i) {
        SipMessageTrace t;
        t.method  = QStringLiteral("MESSAGE");
        t.callId  = QStringLiteral("call-%1").arg(i);
        t.rawSip  = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n\r\nbody %1").arg(i);
        logMessage(t);
    }

    QCOMPARE(MessagingEventStore::instance().count(), 3);
    const QList<MessagingEvent> events = MessagingEventStore::instance().snapshot();
    // Oldest (call-0, call-1) evicted first; newest three retained in order.
    QCOMPARE(events.at(0).callId, QStringLiteral("call-2"));
    QCOMPARE(events.at(1).callId, QStringLiteral("call-3"));
    QCOMPARE(events.at(2).callId, QStringLiteral("call-4"));
}

void TestMessagingEventStore::exportJsonContainsExpectedFields()
{
    SipMessageTrace t;
    t.method      = QStringLiteral("MESSAGE");
    t.contentType = QStringLiteral("text/plain");
    t.callId      = QStringLiteral("evt-call-1");
    t.rawSip      = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n"
                                   "Content-Type: text/plain\r\n\r\nHi");
    logMessage(t);

    const QString json = MessagingEventStore::instance().exportToJson();
    QVERIFY(json.contains(QStringLiteral("\"transport\"")));
    QVERIFY(json.contains(QStringLiteral("\"payloadType\"")));
    QVERIFY(json.contains(QStringLiteral("\"parseStatus\"")));
    QVERIFY(json.contains(QStringLiteral("\"parseWarnings\"")));
    QVERIFY(json.contains(QStringLiteral("evt-call-1")));
}

void TestMessagingEventStore::exportTextContainsExpectedFields()
{
    SipMessageTrace t;
    t.method = QStringLiteral("MESSAGE");
    t.callId = QStringLiteral("evt-call-2");
    t.rawSip = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\n\r\nHi");
    logMessage(t);

    const QString text = MessagingEventStore::instance().exportToText();
    QVERIFY(text.contains(QStringLiteral("transport=")));
    QVERIFY(text.contains(QStringLiteral("evt-call-2")));
}

QTEST_GUILESS_MAIN(TestMessagingEventStore)
#include "test_messaging_event_store.moc"
