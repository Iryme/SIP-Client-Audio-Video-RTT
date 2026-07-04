#include "CallHistoryListModel.h"

CallHistoryListModel::CallHistoryListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void CallHistoryListModel::setEntries(const QList<CallHistoryEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

CallHistoryEntry CallHistoryListModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    return m_entries.at(row);
}

int CallHistoryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant CallHistoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const CallHistoryEntry &e = m_entries.at(index.row());

    switch (role) {
    case Qt::DisplayRole: {
        const QString who = e.displayName.isEmpty() ? e.remoteUri
            : QStringLiteral("%1  <%2>").arg(e.displayName, e.remoteUri);
        const QString when = e.startTime.isNull() ? QString()
            : e.startTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        return QStringLiteral("[%1] %2 — %3 — %4 — %5 — %6")
            .arg(callDirectionName(e.direction), who, when,
                 formatCallDuration(e.durationSec), callResultName(e.result),
                 callHistoryBadges(e));
    }
    case EntryRole:        return QVariant::fromValue(e);
    case IdRole:           return e.id;
    case DirectionRole:    return int(e.direction);
    case ResultRole:       return int(e.result);
    case RemoteUriRole:    return e.remoteUri;
    case DisplayNameRole:  return e.displayName;
    case ProfileNameRole:  return e.profileName;
    case StartTimeRole:    return e.startTime;
    case DurationSecRole:  return e.durationSec;
    case HadAudioRole:     return e.hadAudio;
    case HadVideoRole:     return e.hadVideo;
    case HadRttRole:       return e.hadRtt;
    case LastSipCodeRole:  return e.lastSipCode;
    case ReasonRole:       return e.reason;
    default:               return {};
    }
}
