#include "SipCall.h"

#include <QPointer>
#include <QUuid>

#include "core/Logger.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <pjsua-lib/pjsua.h>
#endif

// ---------------------------------------------------------------------------
// Impl — pimpl holding pjsua2 Call subclass when PJSIP is available.
// In stub mode, Impl is an empty placeholder.
// ---------------------------------------------------------------------------
struct SipCall::Impl
{
    explicit Impl(SipCall *owner) : q(owner) {}
    QPointer<SipCall> q;

#ifdef HAVE_PJSIP
    // pjsua2 Call subclass — maps onCallState() and onCallMediaState() to the
    // SipCall state machine and audio bridge respectively.
    class PjCall final : public pj::Call
    {
    public:
        PjCall(Impl *impl, pj::Account &account, int callId = PJSUA_INVALID_ID)
            : pj::Call(account, callId), m_impl(impl) {}

        void onCallState(pj::OnCallStateParam &prm) override
        {
            Q_UNUSED(prm)
            if (!m_impl || !m_impl->q)
                return;

            pj::CallInfo ci = getInfo();
            CallState newState = CallState::Idle;
            QString   reason   = QString::fromStdString(ci.lastReason);
            int       code     = ci.lastStatusCode;

            switch (ci.state) {
            case PJSIP_INV_STATE_CALLING:     newState = CallState::OutgoingInit; break;
            case PJSIP_INV_STATE_EARLY:        newState = CallState::Ringing;      break;
            case PJSIP_INV_STATE_CONNECTING:   newState = CallState::Connecting;   break;
            case PJSIP_INV_STATE_CONFIRMED:    newState = CallState::Active;       break;
            case PJSIP_INV_STATE_DISCONNECTED:
                // Stop audio bridge before the call object is torn down.
                stopAudioBridge();
                newState = (code >= 400) ? CallState::Failed : CallState::Idle;
                break;
            default: return;
            }

            QPointer<SipCall> self = m_impl->q;
            QMetaObject::invokeMethod(self, [self, newState, reason, code]() {
                if (self)
                    self->m_stateMachine.tryTransition(newState, reason, code);
            }, Qt::QueuedConnection);
        }

        void onCallMediaState(pj::OnCallMediaStateParam &prm) override
        {
            Q_UNUSED(prm)
            if (!m_impl || !m_impl->q)
                return;

            pj::CallInfo ci = getInfo();
            bool bridgeWired = false;

            for (const auto &mi : ci.media) {
                if (mi.type == PJMEDIA_TYPE_AUDIO
                        && mi.status == PJSUA_CALL_MEDIA_ACTIVE) {
                    try {
                        auto *aud = static_cast<pj::AudioMedia *>(getMedia(mi.index));
                        pj::AudDevManager &adm =
                            pj::Endpoint::instance().audDevManager();
                        adm.getCaptureDevMedia().startTransmit(*aud);
                        aud->startTransmit(adm.getPlaybackDevMedia());
                        m_impl->callAudioMedia = aud;
                        bridgeWired = true;
                    } catch (...) {
                        m_impl->callAudioMedia = nullptr;
                    }
                    break;
                }
            }

            QPointer<SipCall> self = m_impl->q;
            QMetaObject::invokeMethod(self, [self, bridgeWired]() {
                if (!self) return;
                if (bridgeWired)
                    emit self->audioMediaConnected();
                else
                    emit self->audioMediaDisconnected();
            }, Qt::QueuedConnection);
        }

        void stopAudioBridge()
        {
            if (!m_impl->callAudioMedia)
                return;
            try {
                pj::AudDevManager &adm =
                    pj::Endpoint::instance().audDevManager();
                m_impl->callAudioMedia->stopTransmit(adm.getPlaybackDevMedia());
                adm.getCaptureDevMedia().stopTransmit(*m_impl->callAudioMedia);
            } catch (...) {}
            m_impl->callAudioMedia = nullptr;

            QPointer<SipCall> self = m_impl->q;
            QMetaObject::invokeMethod(self, [self]() {
                if (self) emit self->audioMediaDisconnected();
            }, Qt::QueuedConnection);
        }

    private:
        Impl *m_impl;
    };

    PjCall       *pjCall{nullptr};
    void         *pjAccountHandle{nullptr}; // pj::Account* cast to void*
    pj::AudioMedia *callAudioMedia{nullptr}; // valid only while media is active
#endif
};

// ---------------------------------------------------------------------------
// SipCall
// ---------------------------------------------------------------------------

SipCall::SipCall(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this))
{
    connect(&m_stateMachine, &CallStateMachine::stateChanged,
            this, &SipCall::onStateMachineStateChanged);
    connect(&m_stateMachine, &CallStateMachine::transitionTimedOut,
            this, &SipCall::onStateMachineTimedOut);

    // Level timer: fires while audio bridge is active.
    m_levelTimer.setInterval(100);
    connect(&m_levelTimer, &QTimer::timeout, this, &SipCall::onLevelTimerFired);

    connect(this, &SipCall::audioMediaConnected, this, [this] {
        m_levelTimer.start();
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Audio media connected: id=%1").arg(m_callId));
    });
    connect(this, &SipCall::audioMediaDisconnected, this, [this] {
        m_levelTimer.stop();
        emit inputLevelChanged(0);
        emit outputLevelChanged(0);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Audio media disconnected: id=%1").arg(m_callId));
    });
}

SipCall::~SipCall()
{
    m_levelTimer.stop();
#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            if (m_impl->pjCall->isActive())
                m_impl->pjCall->hangup(pj::CallOpParam());
        } catch (...) {}
        delete m_impl->pjCall;
        m_impl->pjCall = nullptr;
    }
#endif
    delete m_impl;
}

bool SipCall::makeCall(const QString &remoteUri)
{
    if (m_stateMachine.state() != CallState::Idle) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Call rejected: makeCall called in state %1")
                .arg(callStateName(m_stateMachine.state())));
        return false;
    }
    if (remoteUri.trimmed().isEmpty()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Call rejected: empty remote URI"));
        return false;
    }

    m_remoteUri = remoteUri.trimmed();
    m_callId    = QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Call started: id=%1 uri=%2").arg(m_callId, m_remoteUri));

    m_stateMachine.tryTransition(CallState::OutgoingInit,
                                 QStringLiteral("Dialing %1").arg(m_remoteUri));

#ifdef HAVE_PJSIP
    if (m_impl->pjAccountHandle) {
        auto *account = static_cast<pj::Account *>(m_impl->pjAccountHandle);
        try {
            m_impl->pjCall = new Impl::PjCall(m_impl, *account);
            pj::CallOpParam prm(true);
            m_impl->pjCall->makeCall(m_remoteUri.toStdString(), prm);
            return true;
        } catch (const pj::Error &e) {
            delete m_impl->pjCall;
            m_impl->pjCall = nullptr;
            m_stateMachine.tryTransition(CallState::Failed,
                                         QString::fromStdString(e.reason),
                                         static_cast<int>(e.status));
            return false;
        }
    }
    // Fall through to stub path when pjAccountHandle not set.
#endif

    // Stub: advance to Ringing after a queued tick to allow OutgoingInit observers.
    postStubTransition(CallState::Ringing, QStringLiteral("Remote ringing"));
    return true;
}

bool SipCall::answer()
{
    if (m_stateMachine.state() != CallState::IncomingRinging) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("answer() rejected: not in IncomingRinging state (current: %1)")
                .arg(callStateName(m_stateMachine.state())));
        return false;
    }

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Answering call id=%1 from %2").arg(m_callId, m_remoteUri));

    m_stateMachine.tryTransition(CallState::Connecting, QStringLiteral("Answering"));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            pj::CallOpParam prm;
            prm.statusCode = PJSIP_SC_OK;
            m_impl->pjCall->answer(prm);
            return true;
        } catch (const pj::Error &e) {
            m_stateMachine.tryTransition(CallState::Failed,
                                         QString::fromStdString(e.reason),
                                         static_cast<int>(e.status));
            return false;
        }
    }
#endif

    postStubTransition(CallState::Active, QStringLiteral("Call connected"));
    return true;
}

bool SipCall::reject()
{
    if (m_stateMachine.state() != CallState::IncomingRinging) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("reject() rejected: not in IncomingRinging state (current: %1)")
                .arg(callStateName(m_stateMachine.state())));
        return false;
    }

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Rejecting call id=%1 from %2").arg(m_callId, m_remoteUri));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            pj::CallOpParam prm;
            prm.statusCode = PJSIP_SC_BUSY_HERE;
            m_impl->pjCall->hangup(prm);
            return true;
        } catch (const pj::Error &e) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("reject() PJSIP error: %1").arg(QString::fromStdString(e.reason)));
        }
    }
#endif

    m_stateMachine.tryTransition(CallState::Idle, QStringLiteral("Rejected by local user"), 486);
    return true;
}

bool SipCall::hangup()
{
    const CallState cur = m_stateMachine.state();
    if (cur == CallState::Idle || cur == CallState::Failed) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("hangup() ignored: call already in %1 state")
                .arg(callStateName(cur)));
        return false;
    }

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Hanging up call id=%1").arg(m_callId));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            m_impl->pjCall->hangup(pj::CallOpParam());
            return true;
        } catch (const pj::Error &e) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("hangup() PJSIP error: %1").arg(QString::fromStdString(e.reason)));
        }
    }
#endif

    m_stateMachine.tryTransition(CallState::Disconnecting, QStringLiteral("User hangup"));
    postStubTransition(CallState::Idle, QStringLiteral("Call ended"), 0);
    return true;
}

bool SipCall::hold()
{
    if (m_stateMachine.state() != CallState::Active) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("hold() rejected: not in Active state (current: %1)")
                .arg(callStateName(m_stateMachine.state())));
        return false;
    }

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Placing call id=%1 on hold").arg(m_callId));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            pj::CallOpParam prm;
            m_impl->pjCall->setHold(prm);
            return true;
        } catch (const pj::Error &e) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("hold() PJSIP error: %1").arg(QString::fromStdString(e.reason)));
        }
    }
#endif

    m_stateMachine.tryTransition(CallState::Held, QStringLiteral("On hold"));
    return true;
}

bool SipCall::resume()
{
    if (m_stateMachine.state() != CallState::Held) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("resume() rejected: not in Held state (current: %1)")
                .arg(callStateName(m_stateMachine.state())));
        return false;
    }

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Resuming call id=%1").arg(m_callId));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            pj::CallOpParam prm;
            m_impl->pjCall->reinvite(prm);
            return true;
        } catch (const pj::Error &e) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("resume() PJSIP error: %1").arg(QString::fromStdString(e.reason)));
        }
    }
#endif

    m_stateMachine.tryTransition(CallState::Active, QStringLiteral("Call resumed"));
    return true;
}

bool SipCall::setMuted(bool muted)
{
    if (m_muted == muted)
        return true;

    m_muted = muted;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Mute %1: call id=%2")
            .arg(muted ? QStringLiteral("ON") : QStringLiteral("OFF"), m_callId));

#ifdef HAVE_PJSIP
    try {
        pj::AudDevManager &adm = pj::Endpoint::instance().audDevManager();
        adm.getCaptureDevMedia().adjustTxLevel(muted ? 0.0f : 1.0f);
    } catch (...) {}
#endif

    emit muteChanged(muted);
    return true;
}

bool SipCall::isMuted() const
{
    return m_muted;
}

void SipCall::reset(const QString &reason)
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Call id=%1 reset: %2").arg(m_callId, reason));
    m_levelTimer.stop();
    m_stateMachine.reset(reason);
}

CallState SipCall::state()      const { return m_stateMachine.state(); }
QString   SipCall::statusText() const { return m_stateMachine.statusText(); }
QString   SipCall::remoteUri()  const { return m_remoteUri; }
QString   SipCall::callId()     const { return m_callId; }

CallStateMachine &SipCall::stateMachine() { return m_stateMachine; }

void SipCall::onStateMachineStateChanged(CallState state,
                                         const QString &statusText,
                                         int statusCode)
{
    emit callStateChanged(state, statusText, statusCode);

    if (state == CallState::Active) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Call connected: id=%1 remote=%2").arg(m_callId, m_remoteUri));
        emit callConnected(m_remoteUri);
        // In stub mode, emit audioMediaConnected when call goes Active
        // (PJSIP emits it from onCallMediaState instead).
#ifndef HAVE_PJSIP
        emit audioMediaConnected();
#endif
    } else if (state == CallState::Held || state == CallState::Disconnecting) {
#ifndef HAVE_PJSIP
        // Stub: tear down audio when call leaves Active.
        if (m_levelTimer.isActive())
            emit audioMediaDisconnected();
#endif
    } else if (state == CallState::Idle) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Call disconnected: id=%1 remote=%2 reason=\"%3\" status=%4")
                .arg(m_callId, m_remoteUri, statusText).arg(statusCode));
        emit callDisconnected(m_remoteUri, statusText, statusCode);
    } else if (state == CallState::Failed) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Call failed: id=%1 remote=%2 reason=\"%3\" status=%4")
                .arg(m_callId, m_remoteUri, statusText).arg(statusCode));
        emit callFailed(m_remoteUri, statusText, statusCode);
    }
}

void SipCall::onStateMachineTimedOut(CallState stuckState)
{
    Logger::instance().warn(LogCategory::Sip,
        QStringLiteral("Call state machine timed out in %1; id=%2")
            .arg(callStateName(stuckState), m_callId));
}

void SipCall::onLevelTimerFired()
{
#ifdef HAVE_PJSIP
    if (!m_impl || !m_impl->pjCall || !m_impl->callAudioMedia)
        return;
    try {
        const pjsua_call_id cid = static_cast<pjsua_call_id>(m_impl->pjCall->getId());
        const pjsua_conf_port_id slot = pjsua_call_get_conf_port(cid);
        unsigned txLvl = 0, rxLvl = 0;
        pjsua_conf_get_signal_level(slot, &txLvl, &rxLvl);
        emit inputLevelChanged(static_cast<int>(txLvl * 100 / 255));
        emit outputLevelChanged(static_cast<int>(rxLvl * 100 / 255));
    } catch (...) {}
#endif
}

void SipCall::postStubTransition(CallState to, const QString &reason, int statusCode)
{
    QPointer<SipCall> self(this);
    QMetaObject::invokeMethod(this, [self, to, reason, statusCode]() {
        if (self)
            self->m_stateMachine.tryTransition(to, reason, statusCode);
    }, Qt::QueuedConnection);
}
