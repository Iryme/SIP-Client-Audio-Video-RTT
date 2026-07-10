#pragma once
#include <QObject>
#include <QTimer>

#include "sip/IsComposingInfo.h"

// Drives the local outbound RFC 3994 typing-state machine (Task W097): pure
// Qt (QTimer), no PJSIP/network dependency, so it never sends anything
// itself — it only decides *when* an active/idle/gone notification should
// be sent, via sendIsComposingRequested(), and leaves the actual SIP
// MESSAGE composition/send to the caller (SipMessageComposer::
// composeIsComposing + SipManager::sendSipMessage).
//
// Debounce/throttle: onTextChanged() while already Active only restarts the
// idle timer (debounce) — it never re-emits "active" on every keystroke.
// "active" is re-sent only by the periodic refresh timer (throttle), which
// keeps a receiver's own is-composing display from expiring during a long
// typing burst. A "gone" is emitted at most once per composing session
// (guarded by the Stopped phase) even if stop() is called multiple times.
class TypingIndicatorController : public QObject
{
    Q_OBJECT
public:
    struct Config
    {
        int refreshSeconds{60};
        int idleSeconds{15};
        int goneDelaySeconds{30};
    };

    explicit TypingIndicatorController(QObject *parent = nullptr);

    void setConfig(const Config &config);
    Config config() const;

    // Call whenever the compose body's text changes. nonEmpty reflects
    // whether the body now has content (a growing/non-empty body is what
    // starts/continues a composing session).
    void onTextChanged(bool nonEmpty);

    // Call when the message is sent, or the editor/page is closing. Emits
    // "gone" if a composing session was in progress (no-op otherwise).
    void stop();

    // Current locally-tracked typing phase (Active/Idle; Unknown means no
    // composing session is in progress).
    IsComposingInfo::State currentState() const;

    // Test-only hooks: invoke what the internal timers would do on firing,
    // without waiting real wall-clock seconds — same pattern as
    // SipManager::scheduleRefresh (public specifically so tests can drive
    // timer-driven logic directly).
    void triggerRefreshTimeout();
    void triggerIdleTimeout();
    void triggerGoneTimeout();

signals:
    // refreshSeconds is the value to put in the outgoing notification's
    // <refresh> element (0 for Idle/Gone, config().refreshSeconds for
    // Active).
    void sendIsComposingRequested(IsComposingInfo::State state, int refreshSeconds);

private:
    void enterActive();
    void enterIdle();
    void enterGone();

    Config m_config;
    IsComposingInfo::State m_state{IsComposingInfo::State::Unknown}; // Unknown == Stopped
    QTimer m_refreshTimer;
    QTimer m_idleTimer;
    QTimer m_goneTimer;
};
