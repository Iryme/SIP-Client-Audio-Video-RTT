#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <QList>

enum class LogLevel {
    Info,
    Warn,
    Error,
    Debug,
    Raw
};

enum class LogCategory {
    App,
    Sip,
    Sdp,
    Media,
    Rtt,
    Lmpe,
    Etsi,
    Platform,
    Perf
};

struct LogEntry {
    QDateTime   timestamp;
    LogLevel    level;
    LogCategory category;
    QString     message;
    QString     payload;
};

class Logger : public QObject
{
    Q_OBJECT
public:
    static Logger &instance();

    void log(LogLevel level, LogCategory category,
             const QString &message, const QString &payload = {});

    void info (LogCategory c, const QString &m)                      { log(LogLevel::Info,  c, m); }
    void warn (LogCategory c, const QString &m)                      { log(LogLevel::Warn,  c, m); }
    void error(LogCategory c, const QString &m)                      { log(LogLevel::Error, c, m); }
    void debug(LogCategory c, const QString &m)                      { log(LogLevel::Debug, c, m); }
    void raw  (LogCategory c, const QString &m, const QString &p={}) { log(LogLevel::Raw,   c, m, p); }

    void setLevelEnabled(LogLevel level, bool enabled);
    bool isLevelEnabled(LogLevel level) const;

    // In-memory ring buffer of the most recent entries (capped at
    // maxRecentEntries), regardless of level filtering — used by the
    // Diagnostics Center bundle export. Not persisted to disk.
    QList<LogEntry> recentEntries() const;
    static int maxRecentEntries();

    static QString levelName(LogLevel l);
    static QString categoryName(LogCategory c);

signals:
    void entryAdded(const LogEntry &entry);

private:
    Logger();
    QHash<LogLevel, bool> m_enabled;
    QList<LogEntry> m_recent;
};
