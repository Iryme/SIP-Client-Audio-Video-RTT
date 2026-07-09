#include <QtTest/QtTest>

#include "sip/MessagingDiagnosticsStore.h"
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

QTEST_GUILESS_MAIN(TestMessagingDiagnosticsStore)
#include "test_messaging_diagnostics_store.moc"
