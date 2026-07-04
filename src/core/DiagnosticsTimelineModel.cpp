#include "DiagnosticsTimelineModel.h"

DiagnosticsTimelineModel::DiagnosticsTimelineModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DiagnosticsTimelineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant DiagnosticsTimelineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const DiagnosticsTimelineEntry &e = m_entries.at(index.row());
    switch (role) {
    case IdRole:         return e.id;
    case TimestampRole:  return e.timestamp;
    case CategoryRole:   return timelineCategoryName(e.category);
    case SeverityRole:   return timelineSeverityName(e.severity);
    case TitleRole:      return e.title;
    case DetailsRole:    return e.details;
    case CallIdRole:     return e.callId;
    case ProfileIdRole:  return e.profileId;
    case RemoteUriRole:  return e.remoteUri;
    case SipCodeRole:    return e.sipCode;
    case ColorHintRole:  return e.colorHint;
    case IconHintRole:   return e.iconHint;
    case EntryRole:      return QVariant::fromValue(e);
    case Qt::DisplayRole:
        return QStringLiteral("[%1] %2 — %3")
            .arg(e.timestamp.toString(QStringLiteral("hh:mm:ss")), timelineCategoryName(e.category), e.title);
    default:
        return {};
    }
}

QHash<int, QByteArray> DiagnosticsTimelineModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TimestampRole, "timestamp"},
        {CategoryRole, "category"},
        {SeverityRole, "severity"},
        {TitleRole, "title"},
        {DetailsRole, "details"},
        {CallIdRole, "callId"},
        {ProfileIdRole, "profileId"},
        {RemoteUriRole, "remoteUri"},
        {SipCodeRole, "sipCode"},
        {ColorHintRole, "colorHint"},
        {IconHintRole, "iconHint"},
        {EntryRole, "entry"},
    };
}

void DiagnosticsTimelineModel::append(const DiagnosticsTimelineEntry &entry)
{
    if (m_entries.size() >= kMaxEntries) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_entries.removeFirst();
        endRemoveRows();
    }

    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();

    emit entryAppended(entry);
}

void DiagnosticsTimelineModel::reset(const QList<DiagnosticsTimelineEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    if (m_entries.size() > kMaxEntries)
        m_entries = m_entries.mid(m_entries.size() - kMaxEntries);
    endResetModel();
}

void DiagnosticsTimelineModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

DiagnosticsTimelineEntry DiagnosticsTimelineModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    return m_entries.at(row);
}
