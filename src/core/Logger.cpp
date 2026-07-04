#include "Logger.h"
#include <QDebug>

Logger::Logger()
{
    // Defaults: INFO/WARN/ERROR on; DEBUG/RAW off
    m_enabled[LogLevel::Info]  = true;
    m_enabled[LogLevel::Warn]  = true;
    m_enabled[LogLevel::Error] = true;
    m_enabled[LogLevel::Debug] = false;
    m_enabled[LogLevel::Raw]   = false;
}

Logger &Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

void Logger::log(LogLevel level, LogCategory category,
                 const QString &message, const QString &payload)
{
    if (!m_enabled.value(level, false))
        return;

    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level     = level;
    entry.category  = category;
    entry.message   = message;
    entry.payload   = payload;

    // Mirror to Qt debug output during development
    const QString line = QStringLiteral("[%1] [%2] [%3] %4")
        .arg(entry.timestamp.toString("hh:mm:ss.zzz"))
        .arg(levelName(level))
        .arg(categoryName(category))
        .arg(message);
    qDebug().noquote() << line;

    m_recent.append(entry);
    if (m_recent.size() > maxRecentEntries())
        m_recent.removeFirst();

    emit entryAdded(entry);
}

QList<LogEntry> Logger::recentEntries() const
{
    return m_recent;
}

int Logger::maxRecentEntries()
{
    return 1000;
}

void Logger::setLevelEnabled(LogLevel level, bool enabled)
{
    m_enabled[level] = enabled;
}

bool Logger::isLevelEnabled(LogLevel level) const
{
    return m_enabled.value(level, false);
}

QString Logger::levelName(LogLevel l)
{
    switch (l) {
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Raw:   return "RAW";
    }
    return "?";
}

QString Logger::categoryName(LogCategory c)
{
    switch (c) {
    case LogCategory::App:      return "APP";
    case LogCategory::Sip:      return "SIP";
    case LogCategory::Sdp:      return "SDP";
    case LogCategory::Media:    return "MEDIA";
    case LogCategory::Rtt:      return "RTT";
    case LogCategory::Lmpe:     return "LMPE";
    case LogCategory::Etsi:     return "ETSI";
    case LogCategory::Platform: return "PLATFORM";
    case LogCategory::Perf:     return "PERF";
    }
    return "?";
}
