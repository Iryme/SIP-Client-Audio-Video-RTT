#pragma once

#include <QObject>
#include <QString>

#include "sip/CallStateMachine.h"

// Manages a single SIP call. Owns a CallStateMachine that enforces valid
// call state transitions. In stub mode (ENABLE_PJSIP=OFF) all operations
// resolve locally via queued callbacks. In PJSIP mode (HAVE_PJSIP defined)
// operations are delegated to pjsua2 and driven by onCallState() callbacks.
//
// SipManager owns the active SipCall instance and provides the public call
// control API to the rest of the application.
class SipCall : public QObject
{
    Q_OBJECT
public:
    explicit SipCall(QObject *parent = nullptr);
    ~SipCall() override;

    // Initiate an outgoing call. Must be in Idle state.
    bool makeCall(const QString &remoteUri);

    // Answer an incoming call. Must be in IncomingRinging state.
    bool answer();

    // Reject an incoming call (sends 4xx). Must be in IncomingRinging state.
    bool reject();

    // Hang up (any in-progress state except Idle and Failed).
    bool hangup();

    // Place the call on hold. Must be in Active state.
    bool hold();

    // Resume a held call. Must be in Held state.
    bool resume();

    // Force to Idle regardless of current state (shutdown / cleanup path).
    void reset(const QString &reason = QStringLiteral("Reset"));

    CallState state()      const;
    QString   statusText() const;
    QString   remoteUri()  const;
    QString   callId()     const;

    CallStateMachine &stateMachine();

signals:
    void callStateChanged(CallState state, const QString &statusText, int statusCode);
    void callConnected(const QString &remoteUri);
    void callDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void callFailed(const QString &remoteUri, const QString &reason, int statusCode);

private slots:
    void onStateMachineStateChanged(CallState state, const QString &statusText, int statusCode);
    void onStateMachineTimedOut(CallState stuckState);

private:
    void postStubTransition(CallState to, const QString &reason, int statusCode = 0);

    CallStateMachine m_stateMachine;
    QString          m_remoteUri;
    QString          m_callId;

    struct Impl;
    Impl *m_impl{nullptr};
};
