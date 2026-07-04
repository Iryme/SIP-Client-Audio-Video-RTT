#pragma once
#include <QAbstractListModel>
#include <QList>

#include "core/CallHistoryEntry.h"

// Read-only list model over a snapshot of CallHistoryEntry records. Kept
// separate from CallHistoryStore so it can be fed either the live store
// (via setEntries(store.entries())) or synthetic entries in unit tests,
// without depending on QWidget/QApplication.
class CallHistoryListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        EntryRole = Qt::UserRole + 1,
        IdRole,
        DirectionRole,
        ResultRole,
        RemoteUriRole,
        DisplayNameRole,
        ProfileNameRole,
        StartTimeRole,
        DurationSecRole,
        HadAudioRole,
        HadVideoRole,
        HadRttRole,
        LastSipCodeRole,
        ReasonRole,
    };

    explicit CallHistoryListModel(QObject *parent = nullptr);

    void setEntries(const QList<CallHistoryEntry> &entries);
    CallHistoryEntry entryAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QList<CallHistoryEntry> m_entries;
};
