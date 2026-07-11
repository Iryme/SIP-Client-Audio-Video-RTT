#include <QSignalSpy>
#include <QTest>

#include "sip/PresenceStore.h"

class TestPresenceStore : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void updateByEntityOverwritesCurrentState();
    void snapshotContainsOneEntryPerEntity();
    void historyGrowsForRepeatedUpdates();
    void duplicateUpdateIsRecordedInHistory();
    void staleUpdateStillAppliesLatestWrite();
    void emptyEntityUriIsNoOp();
    void clearResetsCurrentAndHistory();
    void maxRetainedEventsBoundsHistory();
};

void TestPresenceStore::init()
{
    PresenceStore::instance().clear();
    PresenceStore::instance().setMaxRetainedEvents(500);
}

void TestPresenceStore::updateByEntityOverwritesCurrentState()
{
    PresenceInfo a;
    a.entityUri = QStringLiteral("sip:alice@example.com");
    a.basicStatus = PresenceInfo::BasicStatus::Open;
    PresenceStore::instance().upsert(a);

    PresenceInfo aUpdated = a;
    aUpdated.basicStatus = PresenceInfo::BasicStatus::Closed;
    PresenceStore::instance().upsert(aUpdated);

    const PresenceInfo current = PresenceStore::instance().current(a.entityUri);
    QCOMPARE(current.basicStatus, PresenceInfo::BasicStatus::Closed);
}

void TestPresenceStore::snapshotContainsOneEntryPerEntity()
{
    PresenceInfo a; a.entityUri = QStringLiteral("sip:alice@example.com");
    PresenceInfo b; b.entityUri = QStringLiteral("sip:bob@example.com");
    PresenceStore::instance().upsert(a);
    PresenceStore::instance().upsert(b);
    PresenceStore::instance().upsert(a); // update, not a new entity

    QCOMPARE(PresenceStore::instance().snapshot().size(), 2);
}

void TestPresenceStore::historyGrowsForRepeatedUpdates()
{
    PresenceInfo a; a.entityUri = QStringLiteral("sip:alice@example.com");
    PresenceStore::instance().upsert(a);
    PresenceStore::instance().upsert(a);
    PresenceStore::instance().upsert(a);

    QCOMPARE(PresenceStore::instance().history().size(), 3);
}

void TestPresenceStore::duplicateUpdateIsRecordedInHistory()
{
    QSignalSpy spy(&PresenceStore::instance(), &PresenceStore::presenceUpdated);
    PresenceInfo a; a.entityUri = QStringLiteral("sip:alice@example.com");
    PresenceStore::instance().upsert(a);
    PresenceStore::instance().upsert(a); // identical duplicate — still a valid update event
    QCOMPARE(spy.count(), 2);
}

void TestPresenceStore::staleUpdateStillAppliesLatestWrite()
{
    // PresenceStore does not itself sequence updates by timestamp — the
    // last upsert() call always wins, mirroring "last write wins" for a
    // current-state store. Ordering/staleness policy is the caller's
    // responsibility (see SipManager::onAccountBuddyPresenceChanged, which
    // only ever calls upsert() with the latest known pjsua2 state).
    PresenceInfo older;
    older.entityUri = QStringLiteral("sip:alice@example.com");
    older.timestamp = QDateTime::currentDateTimeUtc().addSecs(-60);
    older.note = QStringLiteral("older");

    PresenceInfo newer = older;
    newer.timestamp = QDateTime::currentDateTimeUtc();
    newer.note = QStringLiteral("newer");

    PresenceStore::instance().upsert(newer);
    PresenceStore::instance().upsert(older); // arrives "late" but store just applies it

    QCOMPARE(PresenceStore::instance().current(older.entityUri).note, QStringLiteral("older"));
}

void TestPresenceStore::emptyEntityUriIsNoOp()
{
    QSignalSpy spy(&PresenceStore::instance(), &PresenceStore::presenceUpdated);
    PresenceInfo info; // entityUri left empty
    PresenceStore::instance().upsert(info);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(PresenceStore::instance().snapshot().size(), 0);
}

void TestPresenceStore::clearResetsCurrentAndHistory()
{
    PresenceInfo a; a.entityUri = QStringLiteral("sip:alice@example.com");
    PresenceStore::instance().upsert(a);

    QSignalSpy spy(&PresenceStore::instance(), &PresenceStore::cleared);
    PresenceStore::instance().clear();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(PresenceStore::instance().snapshot().size(), 0);
    QCOMPARE(PresenceStore::instance().history().size(), 0);
}

void TestPresenceStore::maxRetainedEventsBoundsHistory()
{
    PresenceStore::instance().setMaxRetainedEvents(2);
    PresenceInfo a; a.entityUri = QStringLiteral("sip:alice@example.com");
    PresenceStore::instance().upsert(a);
    PresenceStore::instance().upsert(a);
    PresenceStore::instance().upsert(a);

    QCOMPARE(PresenceStore::instance().history().size(), 2);
}

QTEST_MAIN(TestPresenceStore)
#include "test_presence_store.moc"
