#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

#include "core/CallHistoryStore.h"

// Tests for CallHistoryEntry / CallHistoryStore in isolation. Uses the
// testable CallHistoryStore(filePath) constructor so nothing touches the
// real user's AppData call history.

class TestCallHistory : public QObject
{
    Q_OBJECT

private slots:
    void jsonRoundTrip();
    void createOutgoingEntry();
    void createIncomingEntry();
    void completedCallDuration();
    void missedCall();
    void rejectedCall();
    void limitTo500Entries();
    void persistAndLoad();
};

void TestCallHistory::jsonRoundTrip()
{
    CallHistoryEntry e;
    e.id = QStringLiteral("abc-123");
    e.direction = CallDirection::Outgoing;
    e.remoteUri = QStringLiteral("sip:bob@example.com");
    e.displayName = QStringLiteral("Bob");
    e.profileId = QStringLiteral("profile-1");
    e.profileName = QStringLiteral("Work");
    e.startTime = QDateTime::currentDateTimeUtc();
    e.answerTime = e.startTime.addSecs(2);
    e.endTime = e.startTime.addSecs(62);
    e.durationSec = 60;
    e.result = CallResult::Completed;
    e.hadAudio = true;
    e.hadVideo = false;
    e.hadRtt = true;
    e.lastSipCode = 200;
    e.reason = QStringLiteral("Normal clearing");
    e.notes = QStringLiteral("test note");

    const CallHistoryEntry roundTripped = CallHistoryEntry::fromJson(e.toJson());

    QCOMPARE(roundTripped.id, e.id);
    QCOMPARE(int(roundTripped.direction), int(e.direction));
    QCOMPARE(roundTripped.remoteUri, e.remoteUri);
    QCOMPARE(roundTripped.displayName, e.displayName);
    QCOMPARE(roundTripped.profileId, e.profileId);
    QCOMPARE(roundTripped.profileName, e.profileName);
    QCOMPARE(roundTripped.durationSec, e.durationSec);
    QCOMPARE(int(roundTripped.result), int(e.result));
    QCOMPARE(roundTripped.hadAudio, e.hadAudio);
    QCOMPARE(roundTripped.hadVideo, e.hadVideo);
    QCOMPARE(roundTripped.hadRtt, e.hadRtt);
    QCOMPARE(roundTripped.lastSipCode, e.lastSipCode);
    QCOMPARE(roundTripped.reason, e.reason);
    QCOMPARE(roundTripped.notes, e.notes);
    QVERIFY(!roundTripped.answerTime.isNull());
    QVERIFY(!roundTripped.endTime.isNull());
}

void TestCallHistory::createOutgoingEntry()
{
    QTemporaryDir dir;
    CallHistoryStore store(dir.filePath("history.json"));

    CallHistoryEntry e;
    e.direction = CallDirection::Outgoing;
    e.remoteUri = QStringLiteral("sip:alice@example.com");
    e.startTime = QDateTime::currentDateTimeUtc();
    e.result = CallResult::Pending;

    const QString id = store.addEntry(e);
    QVERIFY(!id.isEmpty());

    const CallHistoryEntry stored = store.entry(id);
    QCOMPARE(stored.id, id);
    QCOMPARE(int(stored.direction), int(CallDirection::Outgoing));
    QCOMPARE(stored.remoteUri, e.remoteUri);
    QCOMPARE(int(stored.result), int(CallResult::Pending));
    QCOMPARE(store.entries().size(), 1);
}

void TestCallHistory::createIncomingEntry()
{
    QTemporaryDir dir;
    CallHistoryStore store(dir.filePath("history.json"));

    CallHistoryEntry e;
    e.direction = CallDirection::Incoming;
    e.remoteUri = QStringLiteral("sip:carol@example.com");
    e.startTime = QDateTime::currentDateTimeUtc();
    e.result = CallResult::Pending;

    const QString id = store.addEntry(e);
    const CallHistoryEntry stored = store.entry(id);
    QCOMPARE(int(stored.direction), int(CallDirection::Incoming));
    QCOMPARE(stored.remoteUri, e.remoteUri);
}

void TestCallHistory::completedCallDuration()
{
    QTemporaryDir dir;
    CallHistoryStore store(dir.filePath("history.json"));

    CallHistoryEntry e;
    e.direction = CallDirection::Outgoing;
    e.remoteUri = QStringLiteral("sip:dave@example.com");
    e.startTime = QDateTime::currentDateTimeUtc();
    const QString id = store.addEntry(e);

    const QDateTime answered = e.startTime.addSecs(3);
    const QDateTime ended    = answered.addSecs(45);

    store.updateEntry(id, [&](CallHistoryEntry &entry) {
        entry.answerTime = answered;
    });
    store.updateEntry(id, [&](CallHistoryEntry &entry) {
        entry.endTime = ended;
        entry.durationSec = static_cast<int>(entry.answerTime.secsTo(ended));
        entry.result = CallResult::Completed;
        entry.lastSipCode = 200;
    });

    const CallHistoryEntry stored = store.entry(id);
    QCOMPARE(int(stored.result), int(CallResult::Completed));
    QCOMPARE(stored.durationSec, 45);
    QVERIFY(!stored.answerTime.isNull());
    QVERIFY(!stored.endTime.isNull());
}

void TestCallHistory::missedCall()
{
    QTemporaryDir dir;
    CallHistoryStore store(dir.filePath("history.json"));

    CallHistoryEntry e;
    e.direction = CallDirection::Incoming;
    e.remoteUri = QStringLiteral("sip:erin@example.com");
    e.startTime = QDateTime::currentDateTimeUtc();
    const QString id = store.addEntry(e);

    // Never answered; terminated by remote cancel (statusCode 487), not a
    // local reject (486) -> missed, zero duration.
    store.updateEntry(id, [&](CallHistoryEntry &entry) {
        entry.endTime = entry.startTime.addSecs(20);
        entry.lastSipCode = 487;
        entry.reason = QStringLiteral("Request Terminated");
        entry.result = entry.answerTime.isNull() ? CallResult::Missed : CallResult::Completed;
        entry.durationSec = 0;
    });

    const CallHistoryEntry stored = store.entry(id);
    QCOMPARE(int(stored.result), int(CallResult::Missed));
    QCOMPARE(stored.durationSec, 0);
    QVERIFY(stored.answerTime.isNull());
}

void TestCallHistory::rejectedCall()
{
    QTemporaryDir dir;
    CallHistoryStore store(dir.filePath("history.json"));

    CallHistoryEntry e;
    e.direction = CallDirection::Incoming;
    e.remoteUri = QStringLiteral("sip:frank@example.com");
    e.startTime = QDateTime::currentDateTimeUtc();
    const QString id = store.addEntry(e);

    // Local reject sends 486 Busy Here (see SipCall::reject()).
    store.updateEntry(id, [&](CallHistoryEntry &entry) {
        entry.endTime = entry.startTime.addSecs(1);
        entry.lastSipCode = 486;
        entry.reason = QStringLiteral("Rejected by local user");
        entry.result = CallResult::Rejected;
        entry.durationSec = 0;
    });

    const CallHistoryEntry stored = store.entry(id);
    QCOMPARE(int(stored.result), int(CallResult::Rejected));
    QCOMPARE(stored.lastSipCode, 486);
}

void TestCallHistory::limitTo500Entries()
{
    QTemporaryDir dir;
    CallHistoryStore store(dir.filePath("history.json"));

    for (int i = 0; i < 505; ++i) {
        CallHistoryEntry e;
        e.direction = CallDirection::Outgoing;
        e.remoteUri = QStringLiteral("sip:user%1@example.com").arg(i);
        e.startTime = QDateTime::currentDateTimeUtc();
        store.addEntry(e);
    }

    QCOMPARE(store.entries().size(), CallHistoryStore::maxEntries());

    // Newest entries are kept (prepended); the most recently added call
    // (user504) must still be present, oldest (user0) must be gone.
    const auto all = store.entries();
    QCOMPARE(all.first().remoteUri, QStringLiteral("sip:user504@example.com"));
    for (const CallHistoryEntry &e : all)
        QVERIFY(e.remoteUri != QStringLiteral("sip:user0@example.com"));
}

void TestCallHistory::persistAndLoad()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("history.json");

    {
        CallHistoryStore store(path);
        CallHistoryEntry e;
        e.direction = CallDirection::Outgoing;
        e.remoteUri = QStringLiteral("sip:persisted@example.com");
        e.startTime = QDateTime::currentDateTimeUtc();
        e.result = CallResult::Completed;
        e.durationSec = 30;
        store.addEntry(e);
        QVERIFY(store.exportToJson(path));
    }

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QVERIFY(doc.isArray());
    QCOMPARE(doc.array().size(), 1);

    // A freshly constructed store pointed at the same file loads it back.
    CallHistoryStore reloaded(path);
    QCOMPARE(reloaded.entries().size(), 1);
    QCOMPARE(reloaded.entries().first().remoteUri, QStringLiteral("sip:persisted@example.com"));
    QCOMPARE(int(reloaded.entries().first().result), int(CallResult::Completed));
}

QTEST_GUILESS_MAIN(TestCallHistory)
#include "test_call_history.moc"
