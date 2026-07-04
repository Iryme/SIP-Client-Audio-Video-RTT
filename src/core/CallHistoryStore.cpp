#include "CallHistoryStore.h"
#include "core/Logger.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUuid>

static constexpr int kSaveDelayMs = 250;

CallHistoryStore &CallHistoryStore::instance()
{
    static CallHistoryStore s;
    return s;
}

CallHistoryStore::CallHistoryStore() : QObject(nullptr)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_filePath = dir + QStringLiteral("/call_history.json");

    m_saveTimer.setSingleShot(true);
    connect(&m_saveTimer, &QTimer::timeout, this, &CallHistoryStore::saveNow);

    load();
}

CallHistoryStore::CallHistoryStore(const QString &filePath)
    : QObject(nullptr), m_filePath(filePath)
{
    m_saveTimer.setSingleShot(true);
    connect(&m_saveTimer, &QTimer::timeout, this, &CallHistoryStore::saveNow);

    load();
}

QList<CallHistoryEntry> CallHistoryStore::entries() const
{
    return m_entries;
}

CallHistoryEntry CallHistoryStore::entry(const QString &id) const
{
    for (const CallHistoryEntry &e : m_entries) {
        if (e.id == id)
            return e;
    }
    return {};
}

QString CallHistoryStore::addEntry(const CallHistoryEntry &e)
{
    CallHistoryEntry copy = e;
    if (copy.id.isEmpty())
        copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_entries.prepend(copy);
    trimToLimit();
    scheduleSave();
    emit historyChanged();
    return copy.id;
}

bool CallHistoryStore::updateEntry(const QString &id, const std::function<void(CallHistoryEntry &)> &mutator)
{
    for (CallHistoryEntry &e : m_entries) {
        if (e.id == id) {
            mutator(e);
            scheduleSave();
            emit historyChanged();
            return true;
        }
    }
    return false;
}

void CallHistoryStore::clear()
{
    if (m_entries.isEmpty())
        return;
    m_entries.clear();
    scheduleSave();
    emit historyChanged();
}

bool CallHistoryStore::exportToJson(const QString &filePath) const
{
    QJsonArray arr;
    for (const CallHistoryEntry &e : m_entries)
        arr.append(e.toJson());

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Logger::instance().warn(LogCategory::App,
            QStringLiteral("CallHistoryStore: failed to open export file %1").arg(filePath));
        return false;
    }
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

int CallHistoryStore::callsToday() const
{
    const QDate today = QDateTime::currentDateTimeUtc().date();
    int count = 0;
    for (const CallHistoryEntry &e : m_entries) {
        if (!e.startTime.isNull() && e.startTime.toUTC().date() == today)
            ++count;
    }
    return count;
}

int CallHistoryStore::missedToday() const
{
    const QDate today = QDateTime::currentDateTimeUtc().date();
    int count = 0;
    for (const CallHistoryEntry &e : m_entries) {
        if (e.result == CallResult::Missed && !e.startTime.isNull()
            && e.startTime.toUTC().date() == today)
            ++count;
    }
    return count;
}

CallHistoryEntry CallHistoryStore::lastCall() const
{
    return m_entries.isEmpty() ? CallHistoryEntry{} : m_entries.first();
}

void CallHistoryStore::load()
{
    m_entries.clear();

    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return;

    for (const QJsonValue &v : doc.array()) {
        if (v.isObject())
            m_entries.append(CallHistoryEntry::fromJson(v.toObject()));
    }
}

void CallHistoryStore::scheduleSave()
{
    m_saveTimer.start(kSaveDelayMs);
}

void CallHistoryStore::saveNow()
{
    QJsonArray arr;
    for (const CallHistoryEntry &e : m_entries)
        arr.append(e.toJson());

    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Logger::instance().warn(LogCategory::App,
            QStringLiteral("CallHistoryStore: failed to write %1").arg(m_filePath));
        return;
    }
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void CallHistoryStore::trimToLimit()
{
    while (m_entries.size() > maxEntries())
        m_entries.removeLast();
}
