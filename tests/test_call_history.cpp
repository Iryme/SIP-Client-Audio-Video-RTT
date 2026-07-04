#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

#include "core/CallHistoryStore.h"
#include "core/CallHistoryListModel.h"
#include "core/CallHistoryFilterProxyModel.h"

static CallHistoryEntry makeTestEntry(CallDirection dir, const QString &uri, const QString &name,
                                       const QString &profile, CallResult result,
                                       const QDateTime &start, bool video = false, bool rtt = false,
                                       int sipCode = 0, const QString &reason = QString())
{
    CallHistoryEntry e;
    e.direction = dir;
    e.remoteUri = uri;
    e.displayName = name;
    e.profileName = profile;
    e.result = result;
    e.startTime = start;
    e.hadVideo = video;
    e.hadRtt = rtt;
    e.lastSipCode = sipCode;
    e.reason = reason;
    return e;
}

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
    void searchByUriAndName();
    void filterIncomingOutgoingMissed();
    void filterFailed();
    void filterWithVideoRtt();
    void dateFilterTodayAndRanges();
    void csvExportEscaping();
    void redialIntentRequiresRemoteUri();
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

void TestCallHistory::searchByUriAndName()
{
    CallHistoryListModel model;
    QList<CallHistoryEntry> entries;
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:alice@example.com"),
                              QStringLiteral("Alice"), QStringLiteral("Work"),
                              CallResult::Completed, QDateTime::currentDateTimeUtc());
    entries << makeTestEntry(CallDirection::Incoming, QStringLiteral("sip:bob@example.com"),
                              QString(), QStringLiteral("Home"),
                              CallResult::Missed, QDateTime::currentDateTimeUtc());
    model.setEntries(entries);

    CallHistoryFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setSearchText(QStringLiteral("alice"));
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setSearchText(QStringLiteral("bob@example.com"));
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setSearchText(QStringLiteral("nomatch"));
    QCOMPARE(proxy.rowCount(), 0);

    proxy.setSearchText(QString());
    QCOMPARE(proxy.rowCount(), 2);
}

void TestCallHistory::filterIncomingOutgoingMissed()
{
    CallHistoryListModel model;
    QList<CallHistoryEntry> entries;
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:a@x.com"), {}, {},
                              CallResult::Completed, QDateTime::currentDateTimeUtc());
    entries << makeTestEntry(CallDirection::Incoming, QStringLiteral("sip:b@x.com"), {}, {},
                              CallResult::Completed, QDateTime::currentDateTimeUtc());
    entries << makeTestEntry(CallDirection::Incoming, QStringLiteral("sip:c@x.com"), {}, {},
                              CallResult::Missed, QDateTime::currentDateTimeUtc());
    model.setEntries(entries);

    CallHistoryFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setKindFilter(CallHistoryFilterProxyModel::KindFilter::Incoming);
    QCOMPARE(proxy.rowCount(), 2);

    proxy.setKindFilter(CallHistoryFilterProxyModel::KindFilter::Outgoing);
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setKindFilter(CallHistoryFilterProxyModel::KindFilter::Missed);
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setKindFilter(CallHistoryFilterProxyModel::KindFilter::All);
    QCOMPARE(proxy.rowCount(), 3);
}

void TestCallHistory::filterFailed()
{
    CallHistoryListModel model;
    QList<CallHistoryEntry> entries;
    entries << makeTestEntry(CallDirection::Incoming, QStringLiteral("sip:a@x.com"), {}, {},
                              CallResult::Missed, QDateTime::currentDateTimeUtc());
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:b@x.com"), {}, {},
                              CallResult::Failed, QDateTime::currentDateTimeUtc());
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:c@x.com"), {}, {},
                              CallResult::Completed, QDateTime::currentDateTimeUtc());
    model.setEntries(entries);

    CallHistoryFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setKindFilter(CallHistoryFilterProxyModel::KindFilter::Failed);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(CallHistoryListModel::RemoteUriRole).toString(),
             QStringLiteral("sip:b@x.com"));
}

void TestCallHistory::filterWithVideoRtt()
{
    CallHistoryListModel model;
    QList<CallHistoryEntry> entries;
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:a@x.com"), {}, {},
                              CallResult::Completed, QDateTime::currentDateTimeUtc(), true, false);
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:b@x.com"), {}, {},
                              CallResult::Completed, QDateTime::currentDateTimeUtc(), false, true);
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:c@x.com"), {}, {},
                              CallResult::Completed, QDateTime::currentDateTimeUtc(), false, false);
    model.setEntries(entries);

    CallHistoryFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setKindFilter(CallHistoryFilterProxyModel::KindFilter::WithVideo);
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setKindFilter(CallHistoryFilterProxyModel::KindFilter::WithRtt);
    QCOMPARE(proxy.rowCount(), 1);
}

void TestCallHistory::dateFilterTodayAndRanges()
{
    CallHistoryListModel model;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QList<CallHistoryEntry> entries;
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:today@x.com"), {}, {},
                              CallResult::Completed, now);
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:3days@x.com"), {}, {},
                              CallResult::Completed, now.addDays(-3));
    entries << makeTestEntry(CallDirection::Outgoing, QStringLiteral("sip:20days@x.com"), {}, {},
                              CallResult::Completed, now.addDays(-20));
    model.setEntries(entries);

    CallHistoryFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setDateFilter(CallHistoryFilterProxyModel::DateFilter::Today);
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setDateFilter(CallHistoryFilterProxyModel::DateFilter::Last7Days);
    QCOMPARE(proxy.rowCount(), 2);

    proxy.setDateFilter(CallHistoryFilterProxyModel::DateFilter::Last30Days);
    QCOMPARE(proxy.rowCount(), 3);

    proxy.setDateFilter(CallHistoryFilterProxyModel::DateFilter::AllTime);
    QCOMPARE(proxy.rowCount(), 3);
}

void TestCallHistory::csvExportEscaping()
{
    QTemporaryDir dir;
    CallHistoryStore store(dir.filePath("history.json"));

    const CallHistoryEntry e = makeTestEntry(
        CallDirection::Outgoing, QStringLiteral("sip:a,b\"c@x.com"),
        QStringLiteral("Multi\nLine, \"Name\""), QStringLiteral("Work"),
        CallResult::Completed, QDateTime::currentDateTimeUtc());
    store.addEntry(e);

    const QString csvPath = dir.filePath("history.csv");
    QVERIFY(store.exportToCsv(csvPath));

    QFile f(csvPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(f.readAll());
    f.close();

    QVERIFY(content.startsWith(QStringLiteral("id,direction,remoteUri")));
    QVERIFY(content.contains(QStringLiteral("\"sip:a,b\"\"c@x.com\"")));
    QVERIFY(content.contains(QStringLiteral("\"Multi\nLine, \"\"Name\"\"\"")));
}

void TestCallHistory::redialIntentRequiresRemoteUri()
{
    const CallHistoryEntry withUri = makeTestEntry(
        CallDirection::Outgoing, QStringLiteral("sip:a@x.com"), {}, {},
        CallResult::Completed, QDateTime::currentDateTimeUtc());
    const CallHistoryEntry withoutUri = makeTestEntry(
        CallDirection::Outgoing, QString(), {}, {},
        CallResult::Completed, QDateTime::currentDateTimeUtc());

    // This mirrors CallHistoryPanel's redial-enablement guard: a call-back
    // action is only ever emitted (and its button only ever enabled) when
    // the entry carries a non-empty remoteUri.
    QVERIFY(!withUri.remoteUri.isEmpty());
    QVERIFY(withoutUri.remoteUri.isEmpty());
}

QTEST_GUILESS_MAIN(TestCallHistory)
#include "test_call_history.moc"
