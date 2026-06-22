#include "DiagnosticsLogger.h"
#include <QMutexLocker>
#include <QRegularExpression>

DiagnosticsLogger::DiagnosticsLogger()
{
    // Defaults: INFO/WARN/ERROR on; DEBUG/RAW off.
    // RAW must never be enabled without explicit user action.
    m_enabled[LogLevel::Info]  = true;
    m_enabled[LogLevel::Warn]  = true;
    m_enabled[LogLevel::Error] = true;
    m_enabled[LogLevel::Debug] = false;
    m_enabled[LogLevel::Raw]   = false;
}

DiagnosticsLogger &DiagnosticsLogger::instance()
{
    static DiagnosticsLogger s_instance;
    return s_instance;
}

void DiagnosticsLogger::log(LogLevel level, LogCategory category,
                             const QString &message, const QString &payload)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled.value(level, false))
        return;

    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level     = level;
    entry.category  = category;
    entry.message   = redact(message);
    entry.payload   = redact(payload);

    if (m_entries.size() >= m_maxEntries)
        m_entries.removeFirst();

    m_entries.append(entry);
}

void DiagnosticsLogger::setLevelEnabled(LogLevel level, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enabled[level] = enabled;
}

bool DiagnosticsLogger::isLevelEnabled(LogLevel level) const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled.value(level, false);
}

QList<LogEntry> DiagnosticsLogger::entries() const
{
    QMutexLocker locker(&m_mutex);
    return m_entries;
}

QList<LogEntry> DiagnosticsLogger::entriesForCategory(LogCategory category) const
{
    QMutexLocker locker(&m_mutex);
    QList<LogEntry> result;
    for (const auto &e : m_entries)
        if (e.category == category)
            result.append(e);
    return result;
}

QList<LogEntry> DiagnosticsLogger::entriesForLevel(LogLevel level) const
{
    QMutexLocker locker(&m_mutex);
    QList<LogEntry> result;
    for (const auto &e : m_entries)
        if (e.level == level)
            result.append(e);
    return result;
}

int DiagnosticsLogger::entryCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.size();
}

void DiagnosticsLogger::clear()
{
    QMutexLocker locker(&m_mutex);
    m_entries.clear();
}

void DiagnosticsLogger::setMaxEntries(int maxEntries)
{
    QMutexLocker locker(&m_mutex);
    m_maxEntries = maxEntries;
}

QString DiagnosticsLogger::exportAsText() const
{
    QMutexLocker locker(&m_mutex);
    QString out;
    out.reserve(m_entries.size() * 120);
    for (const auto &e : m_entries) {
        out += QStringLiteral("[%1] [%2] [%3] %4")
            .arg(e.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(levelName(e.level), -5)
            .arg(categoryName(e.category), -8)
            .arg(e.message);
        if (!e.payload.isEmpty())
            out += QStringLiteral(" | ") + e.payload;
        out += '\n';
    }
    return out;
}

QList<QVariantMap> DiagnosticsLogger::exportAsJsonReady() const
{
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> result;
    result.reserve(m_entries.size());
    for (const auto &e : m_entries) {
        QVariantMap map;
        map[QStringLiteral("timestamp")] = e.timestamp.toString(Qt::ISODateWithMs);
        map[QStringLiteral("level")]     = levelName(e.level);
        map[QStringLiteral("category")]  = categoryName(e.category);
        map[QStringLiteral("message")]   = e.message;
        map[QStringLiteral("payload")]   = e.payload;
        result.append(map);
    }
    return result;
}

// Redacts values following sensitive key names (case-insensitive).
// Patterns: key=value  or  key: value  → key=***
// Order matters: longer patterns (authorization, private key) before shorter substrings (auth).
QString DiagnosticsLogger::redact(const QString &text) const
{
    if (text.isEmpty())
        return text;

    static const QRegularExpression re(
        R"((password|passwd|secret|token|authorization|private[\s_-]?key|auth)\s*[:=]\s*\S+)",
        QRegularExpression::CaseInsensitiveOption
    );

    QString result = text;
    result.replace(re, QStringLiteral(R"(\1=***)"));
    return result;
}

QString DiagnosticsLogger::levelName(LogLevel l)
{
    switch (l) {
    case LogLevel::Info:  return QStringLiteral("INFO");
    case LogLevel::Warn:  return QStringLiteral("WARN");
    case LogLevel::Error: return QStringLiteral("ERROR");
    case LogLevel::Debug: return QStringLiteral("DEBUG");
    case LogLevel::Raw:   return QStringLiteral("RAW");
    }
    return QStringLiteral("?");
}

QString DiagnosticsLogger::categoryName(LogCategory c)
{
    switch (c) {
    case LogCategory::App:      return QStringLiteral("APP");
    case LogCategory::Sip:      return QStringLiteral("SIP");
    case LogCategory::Sdp:      return QStringLiteral("SDP");
    case LogCategory::Media:    return QStringLiteral("MEDIA");
    case LogCategory::Rtt:      return QStringLiteral("RTT");
    case LogCategory::Lmpe:     return QStringLiteral("LMPE");
    case LogCategory::Etsi:     return QStringLiteral("ETSI");
    case LogCategory::Platform: return QStringLiteral("PLATFORM");
    }
    return QStringLiteral("?");
}
