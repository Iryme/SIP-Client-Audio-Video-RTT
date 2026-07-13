#pragma once
#include <QObject>
#include <QString>

#include "msrp/MsrpCallPreparation.h"
#include "msrp/MsrpRelayConfig.h"

class QTimer;
class MsrpRelayClient;

// Task W108: prepares a call's MSRP transport *before* the SIP call is
// created, so the synchronous pjsip onCallSdpCreated callback never itself
// performs relay allocation, DNS, or any other I/O — it only reads the
// PreparedMsrpOffer this controller produces. See
// docs/msrp-relay-call-preparation.md for the full state machine and
// docs/msrp-relay-async-sdp-architecture.md for why this split is
// mandatory (pjsip callbacks must stay synchronous/non-blocking).
//
// Every start() call is asynchronous: even a trivial "MSRP not requested"
// or "relay disabled" resolution is deferred one event-loop turn (never
// emitted synchronously from inside start()), so callers never need a
// separate synchronous fast path.
//
// Cancellation safety: start() bumps an internal generation counter. Any
// relay-client signal that arrives after cancel()/a later start() call is
// silently dropped (checked via the captured generation at connect time) —
// a late allocationReady() can never resurrect a cancelled preparation into
// a "ready" call.
class MsrpCallPreparationController : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Idle,
        ResolvingPolicy,
        AllocatingRelay,
        PreparingDirectMsrp,
        Ready,
        Cancelled,
        Failed,
        Expired
    };
    Q_ENUM(State)

    explicit MsrpCallPreparationController(QObject *parent = nullptr);
    ~MsrpCallPreparationController() override;

    // wantsMsrp: whether this call attempt has MSRP enabled at all (mirrors
    // AppSettings::enableMsrp()+enableMsrpTcp()/enableMsrpTls() at the call
    // site). relayConfig/relayPassword: resolved by the caller from
    // AppSettings/CredentialStore — never hardcoded, never resolved by this
    // class itself. preparationTimeoutMs: overall wall-clock budget for
    // relay allocation before Automatic-mode falls back to Direct / Required
    // mode fails preparation.
    void start(bool wantsMsrp, const MsrpRelayConfig &relayConfig, const QString &relayPassword,
               const QString &preparationId, int preparationTimeoutMs);

    // Cancels an in-progress preparation. No signal from a relay client
    // created by this preparation attempt can produce ready()/failed()
    // afterward. Safe to call at any state, including after ready()/failed()
    // already fired (no-op then).
    void cancel();

    State state() const { return m_state; }

signals:
    void stateChanged(MsrpCallPreparationController::State state);
    void ready(const PreparedMsrpOffer &offer);
    void failed(const QString &reason);
    void cancelled();

private:
    void transitionTo(State s);
    void resolveDirect(MsrpCallTransportMode mode);
    void startRelayAllocation();
    void finishReady(MsrpCallTransportMode mode, const MsrpUri &uri,
                      const MsrpRelayAllocation &allocation);
    void finishFailed(const QString &reason);
    void teardownRelayClient();

    int m_generation{0};
    State m_state{State::Idle};
    bool m_wantsMsrp{false};
    MsrpRelayConfig m_relayConfig;
    QString m_relayPassword;
    QString m_preparationId;
    std::shared_ptr<MsrpRelayClient> m_relayClient;
    QTimer *m_timeoutTimer{nullptr};
};
