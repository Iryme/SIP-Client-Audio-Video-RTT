#pragma once
#include "LogEntry.h"
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

// Holds all received LogEntry objects and applies level / category / text filters.
// Isolated from any GUI class so it can be unit-tested without a display.
class LogFilterModel : public QObject
{
    Q_OBJECT
public:
    explicit LogFilterModel(QObject *parent = nullptr);

    // --- Entry management ---
    void addEntry(const LogEntry &entry);
    void clear();
    const QList<LogEntry> &allEntries() const { return m_entries; }

    // --- Filter setters ---
    void setLevelVisible(LogLevel level, bool visible);
    void setCategoryFilter(const QString &category); // "All" or a category name
    void setSearchText(const QString &text);
    void setRawVisible(bool visible);

    // --- Filter getters ---
    bool isLevelVisible(LogLevel level) const;
    QString categoryFilter() const { return m_categoryFilter; }
    QString searchText() const     { return m_searchText; }
    bool isRawVisible() const      { return m_rawVisible; }

    // --- Query ---
    bool matchesFilter(const LogEntry &entry) const;
    QList<LogEntry> filteredEntries() const;

signals:
    void filterChanged();

private:
    QList<LogEntry>       m_entries;
    QHash<LogLevel, bool> m_levelVisible;
    QString               m_categoryFilter{ "All" };
    QString               m_searchText;
    bool                  m_rawVisible{ false };
};
