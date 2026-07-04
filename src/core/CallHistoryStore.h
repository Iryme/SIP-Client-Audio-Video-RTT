#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QTimer>
#include <functional>

#include "core/CallHistoryEntry.h"

// Persists call history as a JSON array in AppData (QStandardPaths::AppDataLocation).
// Newest entries are kept at the front of entries(); the list is capped at
// maxEntries() (oldest entries are dropped past that limit). Never stores
// passwords or auth headers — only the metadata in CallHistoryEntry.
//
// Writes are coalesced onto a short timer (see scheduleSave()) so bursts of
// updates (e.g. answer immediately followed by end) don't hit disk on every
// call; the write itself is a small JSON document and stays on the main
// thread, same as ContactStore/AppSettings elsewhere in this codebase.
class CallHistoryStore : public QObject
{
    Q_OBJECT
public:
    static CallHistoryStore &instance();

    // Testable constructor: persists to filePath instead of the default
    // AppData location, so tests don't touch the real user's call history.
    explicit CallHistoryStore(const QString &filePath);

    static int maxEntries() { return 500; }

    QList<CallHistoryEntry> entries() const;
    CallHistoryEntry        entry(const QString &id) const;

    // Adds a new entry at the front of the list. Returns the entry's id.
    QString addEntry(const CallHistoryEntry &e);

    // Applies mutator to the entry with the given id and persists the change.
    // No-op if no entry with that id exists.
    bool updateEntry(const QString &id, const std::function<void(CallHistoryEntry &)> &mutator);

    void clear();

    // Writes the full history as a JSON array to filePath. Returns false on
    // I/O failure.
    bool exportToJson(const QString &filePath) const;

    // Dashboard summary helpers.
    int              callsToday()  const;
    int              missedToday() const;
    CallHistoryEntry lastCall()    const;

signals:
    void historyChanged();

private:
    CallHistoryStore();

    void load();
    void scheduleSave();
    void saveNow();
    void trimToLimit();

    QList<CallHistoryEntry> m_entries;
    QTimer                  m_saveTimer;
    QString                 m_filePath;
};
