#pragma once
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class SipCall;

// Task W109A extended this from a 5-state machine to distinguish a remote-
// initiated pending offer from a local one, and to give "declined"/"failed"
// their own terminal states instead of silently staying Disabled (the bug
// this task fixes: the previous machine had no way to represent "we already
// declined the remote's offer" separately from "nothing is happening").
enum class RttState {
    Disabled,           // No call, RTT never offered, or call ended
    RemoteOfferPending, // Peer sent m=text via re-INVITE; auto-declined (m=text 0),
                        // awaiting acceptIncomingRttRequest()/rejectIncomingRttRequest()
    LocalOfferPending,  // We offered m=text (initial INVITE, requestRtt(), or
                        // acceptIncomingRttRequest()) and are awaiting the result
    Negotiating,        // SDP negotiated RTT before, text stream currently inactive
                        // (e.g. call on hold) — was named "Negotiated" pre-W109A
    Active,             // RTP text stream active — send/receive enabled
    Rejected,           // A pending request (local or remote) was declined
    Failed              // Negotiation failed: transport/port error or timeout
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
    void onIncomingRttRequest();
    void onIncomingRttRejected();
    void onNegotiationFailed(const QString &reason);
    void onLocalOfferSent();
    void onCallEnded();

    RttState state() const;
    bool     isActive() const;

signals:
    void rttStateChanged(RttState state);
    void remoteTextReceived(const QString &text);
    void localTextQueued(const QString &text);

private:
    void setState(RttState newState);
    void flushSuppressedLog();

    RttState          m_state{RttState::Disabled};
    QPointer<SipCall> m_call;

    int    m_suppressedEmptyRtt{0};
    QTimer m_suppressedLogTimer;

    // Task W109A anti-ping-pong / bounded-wait guard: a local RTT offer
    // (initial, requestRtt(), or acceptIncomingRttRequest()) must resolve
    // (Active/Rejected/Failed) within this window or the state machine gives
    // up and reports Failed — otherwise a re-INVITE that never gets a
    // response (or a stream that PJSIP never reports active) would leave the
    // UI showing "negotiating" forever with no way out but ending the call.
    static constexpr int kNegotiationTimeoutMs = 12000;
    QTimer m_negotiationTimeoutTimer;
};
