#pragma once
#include <QMetaType>
#include <QString>

// RFC 3994 "is-composing" indication diagnostics info.
struct IsComposingInfo
{
    enum class State { Unknown, Active, Idle, Gone };

    bool    present{false};
    State   state{State::Unknown};
    QString timeout;     // non-standard "timeout" element, if present
    QString refresh;     // RFC 3994 "refresh" element (seconds), if present
    QString contentType; // RFC 3994 "contenttype" element, if present (Task W097)

    static QString stateToString(State s)
    {
        switch (s) {
        case State::Active: return QStringLiteral("active");
        case State::Idle:   return QStringLiteral("idle");
        case State::Gone:   return QStringLiteral("gone");
        case State::Unknown: break;
        }
        return QStringLiteral("unknown");
    }
};

Q_DECLARE_METATYPE(IsComposingInfo::State)
