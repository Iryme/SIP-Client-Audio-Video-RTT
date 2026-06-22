#include "LogFilterModel.h"
#include "DiagnosticsLogger.h"

LogFilterModel::LogFilterModel(QObject *parent)
    : QObject(parent)
{
    m_levelVisible[LogLevel::Info]  = true;
    m_levelVisible[LogLevel::Warn]  = true;
    m_levelVisible[LogLevel::Error] = true;
    m_levelVisible[LogLevel::Debug] = false;
    m_levelVisible[LogLevel::Raw]   = false;
}

void LogFilterModel::addEntry(const LogEntry &entry)
{
    m_entries.append(entry);
}

void LogFilterModel::clear()
{
    m_entries.clear();
}

void LogFilterModel::setLevelVisible(LogLevel level, bool visible)
{
    if (m_levelVisible.value(level) == visible)
        return;
    m_levelVisible[level] = visible;
    emit filterChanged();
}

void LogFilterModel::setCategoryFilter(const QString &category)
{
    if (m_categoryFilter == category)
        return;
    m_categoryFilter = category;
    emit filterChanged();
}

void LogFilterModel::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;
    m_searchText = text;
    emit filterChanged();
}

void LogFilterModel::setRawVisible(bool visible)
{
    if (m_rawVisible == visible)
        return;
    m_rawVisible = visible;
    m_levelVisible[LogLevel::Raw] = visible;
    emit filterChanged();
}

bool LogFilterModel::isLevelVisible(LogLevel level) const
{
    return m_levelVisible.value(level, false);
}

bool LogFilterModel::matchesFilter(const LogEntry &entry) const
{
    // Level visibility
    if (!m_levelVisible.value(entry.level, false))
        return false;

    // RAW entries additionally require rawVisible
    if (entry.level == LogLevel::Raw && !m_rawVisible)
        return false;

    // Category filter
    if (m_categoryFilter != QLatin1String("All")) {
        if (DiagnosticsLogger::categoryName(entry.category) != m_categoryFilter)
            return false;
    }

    // Search text (case-insensitive, matches message or payload)
    if (!m_searchText.isEmpty()) {
        const bool inMessage = entry.message.contains(m_searchText, Qt::CaseInsensitive);
        const bool inPayload = entry.payload.contains(m_searchText, Qt::CaseInsensitive);
        if (!inMessage && !inPayload)
            return false;
    }

    return true;
}

QList<LogEntry> LogFilterModel::filteredEntries() const
{
    QList<LogEntry> result;
    for (const auto &e : m_entries) {
        if (matchesFilter(e))
            result.append(e);
    }
    return result;
}
