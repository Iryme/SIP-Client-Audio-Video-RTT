#include <QSignalSpy>
#include <QTest>

#include "sip/PresenceDiagnosticsStore.h"
#include "sip/SipRawMessageParser.h"
#include "sip/SipTraceLogger.h"

namespace {

SipMessageTrace makeTrace(const QString &rawSip, SipMessageTrace::Direction direction)
{
    return SipRawMessageParser::parse(rawSip, direction);
}

QString subscribeRequest()
{
    return QStringLiteral(
        "SUBSCRIBE sip:bob@example.com SIP/2.0\r\n"
        "From: <sip:alice@example.com>;tag=1\r\n"
        "To: <sip:bob@example.com>\r\n"
        "Call-ID: abc123\r\n"
        "CSeq: 1 SUBSCRIBE\r\n"
        "Event: presence\r\n"
        "Expires: 300\r\n"
        "Accept: application/pidf+xml\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

QString notifyWithPidf(const QString &subState)
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:bob@example.com\">"
        "<tuple id=\"t1\"><status><basic>open</basic></status></tuple></presence>");
    return QStringLiteral(
        "NOTIFY sip:alice@example.com SIP/2.0\r\n"
        "From: <sip:bob@example.com>;tag=2\r\n"
        "To: <sip:alice@example.com>;tag=1\r\n"
        "Call-ID: abc123\r\n"
        "CSeq: 1 NOTIFY\r\n"
        "Event: presence\r\n"
        "Subscription-State: %1\r\n"
        "Content-Type: application/pidf+xml\r\n"
        "Content-Length: %2\r\n"
        "\r\n"
        "%3").arg(subState).arg(body.toUtf8().size()).arg(body);
}

QString notifyTerminated(const QString &reason)
{
    return QStringLiteral(
        "NOTIFY sip:alice@example.com SIP/2.0\r\n"
        "From: <sip:bob@example.com>;tag=2\r\n"
        "To: <sip:alice@example.com>;tag=1\r\n"
        "Call-ID: abc123\r\n"
        "CSeq: 2 NOTIFY\r\n"
        "Event: presence\r\n"
        "Subscription-State: terminated;reason=%1\r\n"
        "Content-Length: 0\r\n"
        "\r\n").arg(reason);
}

} // namespace

class TestPresenceDiagnosticsStore : public QObject
{
    Q_OBJECT

private slots:
    void isPresenceRelevantMatchesSubscribeAndNotify();
    void isPresenceRelevantIgnoresOtherMethods();
    void subscribeRequestModel();
    void notifyActiveWithPidfBody();
    void notifyPendingState();
    void notifyTerminatedRejectedReason();
    void notifyTerminatedTimeoutReason();
    void expiresParsedFromSubscriptionStateParam();
    void expiresParsedFromExpiresHeaderWhenSubStateOmitsIt();
    void bodylessNotifyIsNotAnError();
    void rawSipRedactionPreserved();
    void duplicateNotifyProducesTwoEntries();
};

void TestPresenceDiagnosticsStore::isPresenceRelevantMatchesSubscribeAndNotify()
{
    QVERIFY(PresenceDiagnosticsStore::isPresenceRelevant(makeTrace(subscribeRequest(), SipMessageTrace::Direction::Outbound)));
    QVERIFY(PresenceDiagnosticsStore::isPresenceRelevant(makeTrace(notifyWithPidf(QStringLiteral("active")), SipMessageTrace::Direction::Inbound)));
}

void TestPresenceDiagnosticsStore::isPresenceRelevantIgnoresOtherMethods()
{
    const QString invite = QStringLiteral("INVITE sip:bob@example.com SIP/2.0\r\nCall-ID: x\r\nCSeq: 1 INVITE\r\nContent-Length: 0\r\n\r\n");
    QVERIFY(!PresenceDiagnosticsStore::isPresenceRelevant(makeTrace(invite, SipMessageTrace::Direction::Outbound)));
}

void TestPresenceDiagnosticsStore::subscribeRequestModel()
{
    const PresenceTraceEntry entry = PresenceDiagnosticsStore::buildEntry(
        makeTrace(subscribeRequest(), SipMessageTrace::Direction::Outbound));

    QCOMPARE(entry.method, QStringLiteral("SUBSCRIBE"));
    QCOMPARE(entry.eventPackage, QStringLiteral("presence"));
    QCOMPARE(entry.subscriptionExpires, 300);
    QCOMPARE(entry.callId, QStringLiteral("abc123"));
}

void TestPresenceDiagnosticsStore::notifyActiveWithPidfBody()
{
    const PresenceTraceEntry entry = PresenceDiagnosticsStore::buildEntry(
        makeTrace(notifyWithPidf(QStringLiteral("active;expires=300")), SipMessageTrace::Direction::Inbound));

    QCOMPARE(entry.method, QStringLiteral("NOTIFY"));
    QCOMPARE(entry.subscriptionState, QStringLiteral("active"));
    QCOMPARE(entry.subscriptionExpires, 300);
    QCOMPARE(entry.pidf.basicStatus, PresenceInfo::BasicStatus::Open);
    QCOMPARE(entry.pidf.entityUri, QStringLiteral("sip:bob@example.com"));
    QCOMPARE(entry.pidf.parseStatus, PresenceInfo::ParseStatus::Ok);
}

void TestPresenceDiagnosticsStore::notifyPendingState()
{
    const PresenceTraceEntry entry = PresenceDiagnosticsStore::buildEntry(
        makeTrace(notifyWithPidf(QStringLiteral("pending;expires=300")), SipMessageTrace::Direction::Inbound));
    QCOMPARE(entry.subscriptionState, QStringLiteral("pending"));
}

void TestPresenceDiagnosticsStore::notifyTerminatedRejectedReason()
{
    const PresenceTraceEntry entry = PresenceDiagnosticsStore::buildEntry(
        makeTrace(notifyTerminated(QStringLiteral("rejected")), SipMessageTrace::Direction::Inbound));
    QCOMPARE(entry.subscriptionState, QStringLiteral("terminated"));
    QCOMPARE(entry.subscriptionReason, QStringLiteral("rejected"));
}

void TestPresenceDiagnosticsStore::notifyTerminatedTimeoutReason()
{
    const PresenceTraceEntry entry = PresenceDiagnosticsStore::buildEntry(
        makeTrace(notifyTerminated(QStringLiteral("timeout")), SipMessageTrace::Direction::Inbound));
    QCOMPARE(entry.subscriptionState, QStringLiteral("terminated"));
    QCOMPARE(entry.subscriptionReason, QStringLiteral("timeout"));
}

void TestPresenceDiagnosticsStore::expiresParsedFromSubscriptionStateParam()
{
    const PresenceTraceEntry entry = PresenceDiagnosticsStore::buildEntry(
        makeTrace(notifyWithPidf(QStringLiteral("active;expires=120")), SipMessageTrace::Direction::Inbound));
    QCOMPARE(entry.subscriptionExpires, 120);
}

void TestPresenceDiagnosticsStore::expiresParsedFromExpiresHeaderWhenSubStateOmitsIt()
{
    const PresenceTraceEntry entry = PresenceDiagnosticsStore::buildEntry(
        makeTrace(subscribeRequest(), SipMessageTrace::Direction::Outbound));
    QCOMPARE(entry.subscriptionExpires, 300); // from the "Expires:" header, not Subscription-State
}

void TestPresenceDiagnosticsStore::bodylessNotifyIsNotAnError()
{
    const PresenceTraceEntry entry = PresenceDiagnosticsStore::buildEntry(
        makeTrace(notifyTerminated(QStringLiteral("timeout")), SipMessageTrace::Direction::Inbound));
    QCOMPARE(entry.pidf.parseStatus, PresenceInfo::ParseStatus::Ok);
}

void TestPresenceDiagnosticsStore::rawSipRedactionPreserved()
{
    SipTraceLogger::instance().clear();
    PresenceDiagnosticsStore::instance().clear();

    QString rawWithAuth = subscribeRequest();
    rawWithAuth.insert(rawWithAuth.indexOf(QStringLiteral("\r\n\r\n")),
                        QStringLiteral("\r\nAuthorization: Digest username=\"alice\", response=\"deadbeef\""));

    QSignalSpy spy(&PresenceDiagnosticsStore::instance(), &PresenceDiagnosticsStore::entryLogged);
    SipTraceLogger::instance().logMessage(makeTrace(rawWithAuth, SipMessageTrace::Direction::Outbound));

    QCOMPARE(spy.count(), 1);
    const PresenceTraceEntry entry = spy.at(0).at(0).value<PresenceTraceEntry>();
    QVERIFY(!entry.rawSip.contains(QStringLiteral("deadbeef")));
    QVERIFY(entry.rawSip.contains(QStringLiteral("[REDACTED]")));
}

void TestPresenceDiagnosticsStore::duplicateNotifyProducesTwoEntries()
{
    SipTraceLogger::instance().clear();
    PresenceDiagnosticsStore::instance().clear();

    const QString notify = notifyWithPidf(QStringLiteral("active;expires=300"));
    SipTraceLogger::instance().logMessage(makeTrace(notify, SipMessageTrace::Direction::Inbound));
    SipTraceLogger::instance().logMessage(makeTrace(notify, SipMessageTrace::Direction::Inbound));

    QCOMPARE(PresenceDiagnosticsStore::instance().entries().size(), 2);
}

QTEST_MAIN(TestPresenceDiagnosticsStore)
#include "test_presence_diagnostics_store.moc"
