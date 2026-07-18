#pragma once
#include <QObject>
#include <QTimer>

// Drives the flashing/pulsing "you have an incoming media request" indicator
// shared by CallWorkspacePanel's Request Video and Request RTT buttons (see
// docs/incoming-media-request-alerts.md). Owns only a bool toggle + timer --
// no widget, style, or SIP/SDP knowledge. The caller connects to toggled()
// and re-applies its own dynamic property/QSS using isOn().
class RequestBlinker : public QObject
{
    Q_OBJECT
public:
    explicit RequestBlinker(QObject *parent = nullptr);

    // Idempotent: safe to call while already running (matches Qt's own
    // QTimer::start() semantics -- resets the interval, does not create a
    // second timer). Immediately turns the indicator on and emits toggled()
    // so the caller doesn't have to wait for the first tick.
    void start();

    // Idempotent: safe to call when not running. Always leaves isOn() false
    // and emits toggled() if it changed, so the caller's style is guaranteed
    // to end up back in its normal (non-alert) state.
    void stop();

    bool isOn() const { return m_on; }
    bool isActive() const { return m_timer.isActive(); }

signals:
    void toggled();

private:
    QTimer m_timer;
    bool   m_on{false};
};
