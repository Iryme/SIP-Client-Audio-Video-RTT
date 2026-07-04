#include "CallHistoryEntry.h"

#include <QStringList>

QString callDirectionName(CallDirection dir)
{
    switch (dir) {
    case CallDirection::Incoming: return QStringLiteral("Incoming");
    case CallDirection::Outgoing: return QStringLiteral("Outgoing");
    }
    return QStringLiteral("Unknown");
}

QString callResultName(CallResult result)
{
    switch (result) {
    case CallResult::Pending:   return QStringLiteral("Pending");
    case CallResult::Completed: return QStringLiteral("Completed");
    case CallResult::Missed:    return QStringLiteral("Missed");
    case CallResult::Rejected:  return QStringLiteral("Rejected");
    case CallResult::Failed:    return QStringLiteral("Failed");
    case CallResult::Cancelled: return QStringLiteral("Cancelled");
    }
    return QStringLiteral("Unknown");
}

static QString directionToKey(CallDirection dir)
{
    return dir == CallDirection::Incoming ? QStringLiteral("incoming") : QStringLiteral("outgoing");
}

static CallDirection directionFromKey(const QString &key)
{
    return key == QLatin1String("incoming") ? CallDirection::Incoming : CallDirection::Outgoing;
}

static QString resultToKey(CallResult result)
{
    switch (result) {
    case CallResult::Pending:   return QStringLiteral("pending");
    case CallResult::Completed: return QStringLiteral("completed");
    case CallResult::Missed:    return QStringLiteral("missed");
    case CallResult::Rejected:  return QStringLiteral("rejected");
    case CallResult::Failed:    return QStringLiteral("failed");
    case CallResult::Cancelled: return QStringLiteral("cancelled");
    }
    return QStringLiteral("pending");
}

static CallResult resultFromKey(const QString &key)
{
    if (key == QLatin1String("completed")) return CallResult::Completed;
    if (key == QLatin1String("missed"))    return CallResult::Missed;
    if (key == QLatin1String("rejected"))  return CallResult::Rejected;
    if (key == QLatin1String("failed"))    return CallResult::Failed;
    if (key == QLatin1String("cancelled")) return CallResult::Cancelled;
    return CallResult::Pending;
}

QJsonObject CallHistoryEntry::toJson() const
{
    QJsonObject o;
    o["id"] = id;
    o["direction"] = directionToKey(direction);
    o["remoteUri"] = remoteUri;
    o["displayName"] = displayName;
    o["profileId"] = profileId;
    o["profileName"] = profileName;
    o["startTime"] = startTime.toString(Qt::ISODateWithMs);
    o["answerTime"] = answerTime.isNull() ? QString() : answerTime.toString(Qt::ISODateWithMs);
    o["endTime"] = endTime.isNull() ? QString() : endTime.toString(Qt::ISODateWithMs);
    o["durationSec"] = durationSec;
    o["result"] = resultToKey(result);
    o["hadAudio"] = hadAudio;
    o["hadVideo"] = hadVideo;
    o["hadRtt"] = hadRtt;
    o["lastSipCode"] = lastSipCode;
    o["reason"] = reason;
    o["notes"] = notes;
    return o;
}

CallHistoryEntry CallHistoryEntry::fromJson(const QJsonObject &obj)
{
    CallHistoryEntry e;
    e.id = obj.value("id").toString();
    e.direction = directionFromKey(obj.value("direction").toString());
    e.remoteUri = obj.value("remoteUri").toString();
    e.displayName = obj.value("displayName").toString();
    e.profileId = obj.value("profileId").toString();
    e.profileName = obj.value("profileName").toString();
    e.startTime = QDateTime::fromString(obj.value("startTime").toString(), Qt::ISODateWithMs);
    const QString answerStr = obj.value("answerTime").toString();
    if (!answerStr.isEmpty())
        e.answerTime = QDateTime::fromString(answerStr, Qt::ISODateWithMs);
    const QString endStr = obj.value("endTime").toString();
    if (!endStr.isEmpty())
        e.endTime = QDateTime::fromString(endStr, Qt::ISODateWithMs);
    e.durationSec = obj.value("durationSec").toInt();
    e.result = resultFromKey(obj.value("result").toString());
    e.hadAudio = obj.value("hadAudio").toBool();
    e.hadVideo = obj.value("hadVideo").toBool();
    e.hadRtt = obj.value("hadRtt").toBool();
    e.lastSipCode = obj.value("lastSipCode").toInt();
    e.reason = obj.value("reason").toString();
    e.notes = obj.value("notes").toString();
    return e;
}

QString formatCallDuration(int secs)
{
    const int h = secs / 3600;
    const int m = (secs % 3600) / 60;
    const int s = secs % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h, 2, 10, QLatin1Char('0'))
                                          .arg(m, 2, 10, QLatin1Char('0'))
                                          .arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0'))
                                   .arg(s, 2, 10, QLatin1Char('0'));
}

QString callHistoryBadges(const CallHistoryEntry &e)
{
    QStringList b;
    if (e.hadAudio) b << QStringLiteral("Audio");
    if (e.hadVideo) b << QStringLiteral("Video");
    if (e.hadRtt)   b << QStringLiteral("RTT");
    return b.isEmpty() ? QStringLiteral("—") : b.join(QStringLiteral(" · "));
}
