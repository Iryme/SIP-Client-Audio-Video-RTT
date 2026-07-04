#pragma once
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>

enum class CallDirection { Incoming, Outgoing };

enum class CallResult { Pending, Completed, Missed, Rejected, Failed, Cancelled };

QString callDirectionName(CallDirection dir);
QString callResultName(CallResult result);

// A single call history record. Never holds passwords, auth headers, or any
// other credential material — only call metadata needed for a history list.
struct CallHistoryEntry
{
    QString       id;
    CallDirection direction{CallDirection::Outgoing};
    QString       remoteUri;
    QString       displayName;
    QString       profileId;
    QString       profileName;
    QDateTime     startTime;
    QDateTime     answerTime;
    QDateTime     endTime;
    int           durationSec{0};
    CallResult    result{CallResult::Pending};
    bool          hadAudio{false};
    bool          hadVideo{false};
    bool          hadRtt{false};
    int           lastSipCode{0};
    QString       reason;
    QString       notes;

    bool isNull() const { return id.isEmpty(); }

    QJsonObject toJson() const;
    static CallHistoryEntry fromJson(const QJsonObject &obj);
};

Q_DECLARE_METATYPE(CallHistoryEntry)

// Shared display helpers used by both the list model and the details dialog.
QString formatCallDuration(int secs);
QString callHistoryBadges(const CallHistoryEntry &e);
