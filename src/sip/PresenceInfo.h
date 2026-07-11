#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

// RFC 3856/3863 SIP Presence diagnostics/model info (Task W098). Pure
// Qt/text — never holds a pjsua2 type — so the UI and export layers never
// depend on pjsua2 objects directly.
struct PresenceInfo
{
    enum class BasicStatus { Unknown, Open, Closed };
    enum class ExtendedStatus { Unknown, Available, Away, Busy, DoNotDisturb, OnThePhone, Offline };
    enum class SubscriptionState { Unknown, Pending, Active, Terminated };
    enum class ParseStatus { Ok, Partial, Error };

    QString entityUri;
    QString contactUri;
    BasicStatus    basicStatus{BasicStatus::Unknown};
    ExtendedStatus extendedStatus{ExtendedStatus::Unknown};
    QString note;
    QString tupleId;
    QString priority; // verbatim decimal string, e.g. "0.8"; empty if absent
    QDateTime timestamp;
    int     expires{-1}; // -1 = unknown/not applicable
    SubscriptionState subscriptionState{SubscriptionState::Unknown};
    QString subscriptionReason; // normalized: timeout/deactivated/probation/rejected/noresource/giveup/invariant/unknown
    QString contentType;

    ParseStatus parseStatus{ParseStatus::Ok};
    QStringList parseWarnings;

    static QString basicStatusToString(BasicStatus s)
    {
        switch (s) {
        case BasicStatus::Open:   return QStringLiteral("open");
        case BasicStatus::Closed: return QStringLiteral("closed");
        case BasicStatus::Unknown: break;
        }
        return QStringLiteral("unknown");
    }

    static BasicStatus basicStatusFromString(const QString &s)
    {
        const QString v = s.trimmed().toLower();
        if (v == QLatin1String("open"))
            return BasicStatus::Open;
        if (v == QLatin1String("closed"))
            return BasicStatus::Closed;
        return BasicStatus::Unknown;
    }

    static QString extendedStatusToString(ExtendedStatus s)
    {
        switch (s) {
        case ExtendedStatus::Available:   return QStringLiteral("available");
        case ExtendedStatus::Away:        return QStringLiteral("away");
        case ExtendedStatus::Busy:        return QStringLiteral("busy");
        case ExtendedStatus::DoNotDisturb: return QStringLiteral("do-not-disturb");
        case ExtendedStatus::OnThePhone:  return QStringLiteral("on-the-phone");
        case ExtendedStatus::Offline:     return QStringLiteral("offline");
        case ExtendedStatus::Unknown: break;
        }
        return QStringLiteral("unknown");
    }

    static QString subscriptionStateToString(SubscriptionState s)
    {
        switch (s) {
        case SubscriptionState::Pending:    return QStringLiteral("pending");
        case SubscriptionState::Active:     return QStringLiteral("active");
        case SubscriptionState::Terminated: return QStringLiteral("terminated");
        case SubscriptionState::Unknown: break;
        }
        return QStringLiteral("unknown");
    }

    static SubscriptionState subscriptionStateFromString(const QString &s)
    {
        const QString v = s.trimmed().toLower();
        if (v.startsWith(QLatin1String("active")))
            return SubscriptionState::Active;
        if (v.startsWith(QLatin1String("pending")) || v.startsWith(QLatin1String("accepted"))
            || v.startsWith(QLatin1String("sent")))
            return SubscriptionState::Pending;
        if (v.startsWith(QLatin1String("terminated")))
            return SubscriptionState::Terminated;
        return SubscriptionState::Unknown;
    }

    static QString parseStatusToString(ParseStatus s)
    {
        switch (s) {
        case ParseStatus::Ok:      return QStringLiteral("ok");
        case ParseStatus::Partial: return QStringLiteral("partial");
        case ParseStatus::Error:   return QStringLiteral("error");
        }
        return QStringLiteral("error");
    }

    // Normalizes a raw "reason" token (e.g. from a Subscription-State header
    // reason= param, or pjsua2's BuddyInfo::subTermReason text) into one of
    // RFC 3265's well-known termination reasons. Unrecognized/empty input
    // maps to "unknown" rather than failing — termination handling always
    // has a defined outcome.
    static QString normalizeSubscriptionReason(const QString &raw)
    {
        static const char *const kKnown[] = {
            "timeout", "deactivated", "probation", "rejected",
            "noresource", "giveup", "invariant"
        };
        const QString v = raw.trimmed().toLower();
        for (const char *token : kKnown) {
            if (v.contains(QLatin1String(token)))
                return QString::fromLatin1(token);
        }
        return QStringLiteral("unknown");
    }
};

Q_DECLARE_METATYPE(PresenceInfo)
Q_DECLARE_METATYPE(PresenceInfo::SubscriptionState)
