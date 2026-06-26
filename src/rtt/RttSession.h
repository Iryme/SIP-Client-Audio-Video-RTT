#pragma once
#include <QObject>
#include <QPointer>
#include <QString>

class SipCall;

enum class RttState {
    Disabled,   // No call or RTT not offered
    Offered,    // m=text offered in SDP, awaiting negotiation result
    Negotiated, // SDP negotiated but RTP text stream not yet active
    Active,     // RTP text stream active — send/receive enabled
    Failed      // Negotiation failed or stream error
};

QString rttStateName(RttState state);

// Manages RTT (Real-Time Text, RFC 4103 / T.140) state for a single call.
// Connects to SipCall RTT signals and translates them into a simple 5-state
// machine. Does not own any PJSIP resources directly.
class RttSession : public QObject
{
    Q_OBJECT
public:
    explicit RttSession(QObject *parent = nullptr);
    ~RttSession() override;

    // Attach to an active call. Disconnects any previous call first.
    void enableForCall(SipCall *call);

    // Detach from the current call and reset to Disabled.
    void disable();

    // Send text via RTP. Only works when state == Active.
    // Logs a warning and drops the text if RTT is not negotiated/active.
    void sendText(const QString &text);

    // Called by connected SipCall signals — public so tests can drive directly.
    void onCallMediaStateChanged(bool textMediaActive);
    void onCallEnded();

    RttState state() const;
    bool     isActive() const;

signals:
    void rttStateChanged(RttState state);
    void remoteTextReceived(const QString &text);
    void localTextQueued(const QString &text);

private:
    void setState(RttState newState);

    RttState          m_state{RttState::Disabled};
    QPointer<SipCall> m_call;  // weak — SipCall may outlive or pre-die this session
};
