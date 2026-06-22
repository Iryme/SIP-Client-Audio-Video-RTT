#pragma once
#include "LogEntry.h"
#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QVariantMap>

// Maximum entries kept in memory before oldest are dropped.
static constexpr int kDefaultMaxEntries = 10000;

class DiagnosticsLogger {
public:
    static DiagnosticsLogger &instance();

    // --- Logging ---
    void log(LogLevel level, LogCategory category,
             const QString &message, const QString &payload = {});

    void info (LogCategory c, const QString &m, const QString &p = {}) { log(LogLevel::Info,  c, m, p); }
    void warn (LogCategory c, const QString &m, const QString &p = {}) { log(LogLevel::Warn,  c, m, p); }
    void error(LogCategory c, const QString &m, const QString &p = {}) { log(LogLevel::Error, c, m, p); }
    void debug(LogCategory c, const QString &m, const QString &p = {}) { log(LogLevel::Debug, c, m, p); }
    void raw  (LogCategory c, const QString &m, const QString &p = {}) { log(LogLevel::Raw,   c, m, p); }

    // --- Level control ---
    void setLevelEnabled(LogLevel level, bool enabled);
    bool isLevelEnabled(LogLevel level) const;

    // --- Entry access ---
    QList<LogEntry> entries() const;
    QList<LogEntry> entriesForCategory(LogCategory category) const;
    QList<LogEntry> entriesForLevel(LogLevel level) const;
    int             entryCount() const;

    // --- Management ---
    void clear();
    void setMaxEntries(int maxEntries);

    // --- Export ---
    QString            exportAsText() const;
    QList<QVariantMap> exportAsJsonReady() const;

    // --- Helpers ---
    static QString levelName(LogLevel l);
    static QString categoryName(LogCategory c);

private:
    DiagnosticsLogger();
    ~DiagnosticsLogger() = default;
    DiagnosticsLogger(const DiagnosticsLogger &) = delete;
    DiagnosticsLogger &operator=(const DiagnosticsLogger &) = delete;

    QString redact(const QString &text) const;

    mutable QMutex        m_mutex;
    QHash<LogLevel, bool> m_enabled;
    QList<LogEntry>       m_entries;
    int                   m_maxEntries = kDefaultMaxEntries;
};
