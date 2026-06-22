#pragma once
#include <QObject>
#include <QString>

// SipCall wraps a pjsua2 Call (future task).
// Manages audio/video/RTT streams for a single call.
//
// This header is a forward-declaration stub — implementation deferred to
// the SIP calls task. No PJSIP types appear here intentionally;
// pjsua2.hpp is included only in SipCall.cpp when HAVE_PJSIP is defined.
class SipCall : public QObject
{
    Q_OBJECT
public:
    enum class State { Idle, Calling, IncomingRinging, Connected, Ended };

    explicit SipCall(QObject *parent = nullptr);
    ~SipCall() override;

    State   state()      const;
    QString remoteUri()  const;
    QString callId()     const;

    // Future: makeCall(), answer(), hangup(), hold(), onCallState() callback
signals:
    void stateChanged(SipCall::State state);
    void callEnded(const QString &callId, const QString &reason);

private:
    State   m_state{State::Idle};
    QString m_remoteUri;
    QString m_callId;
};
