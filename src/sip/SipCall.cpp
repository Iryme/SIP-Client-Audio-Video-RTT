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

            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP call state callback: pjsipCallId=%1 state=%2 lastCode=%3 reason=\"%4\" remote=%5")
                    .arg(getId())
                    .arg(static_cast<int>(ci.state))
                    .arg(code)
                    .arg(reason,
                         QString::fromStdString(ci.remoteUri)));

            switch (ci.state) {
            case PJSIP_INV_STATE_CALLING:     newState = CallState::OutgoingInit; break;
            case PJSIP_INV_STATE_EARLY:        newState = CallState::Ringing;      break;
            case PJSIP_INV_STATE_CONNECTING:   newState = CallState::Connecting;   break;
            case PJSIP_INV_STATE_CONFIRMED:    newState = CallState::Active;       break;
            case PJSIP_INV_STATE_DISCONNECTED:
                // Stop both bridges before the call object is torn down.
                stopAudioBridge();
                stopVideoBridge();
                newState = (code >= 400) ? CallState::Failed : CallState::Idle;
                break;
            default: return;
            }

            QPointer<SipCall> self = m_impl->q;
            QMetaObject::invokeMethod(self, [self, newState, reason, code]() {
                if (!self)
                    return;
                if (newState == CallState::Idle
                    && self->m_stateMachine.state() != CallState::Idle
                    && self->m_stateMachine.state() != CallState::IncomingRinging) {
                    self->m_stateMachine.tryTransition(CallState::Disconnecting,
                                                       reason.isEmpty()
                                                           ? QStringLiteral("Call disconnected")
                                                           : reason,
                                                       code);
                }
                self->m_stateMachine.tryTransition(newState, reason, code);
            }, Qt::QueuedConnection);
        }

        void onCallMediaState(pj::OnCallMediaStateParam &prm) override
        {
            Q_UNUSED(prm)
            if (!m_impl || !m_impl->q)
                return;

            pj::CallInfo ci = getInfo();
            bool audioBridgeWired = false;
            bool videoActive      = false;

            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP media state callback: pjsipCallId=%1 mediaCount=%2")
                    .arg(getId())
                    .arg(static_cast<int>(ci.media.size())));

            for (const auto &mi : ci.media) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("PJSIP media stream: index=%1 type=%2 status=%3")
                        .arg(mi.index)
                        .arg(static_cast<int>(mi.type))
                        .arg(static_cast<int>(mi.status)));
                if (mi.type == PJMEDIA_TYPE_AUDIO
                        && mi.status == PJSUA_CALL_MEDIA_ACTIVE
                        && !audioBridgeWired) {
                    try {
                        auto *aud = static_cast<pj::AudioMedia *>(getMedia(mi.index));
                        pj::AudDevManager &adm =
                            pj::Endpoint::instance().audDevManager();
                        adm.getCaptureDevMedia().startTransmit(*aud);
                        aud->startTransmit(adm.getPlaybackDevMedia());
                        m_impl->callAudioMedia = aud;
                        audioBridgeWired = true;
                        const pjsua_call_id cid = static_cast<pjsua_call_id>(getId());
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("PJSIP RTP audio bridge connected: pjsipCallId=%1 confSlot=%2 mediaIndex=%3")
                                .arg(getId())
                                .arg(pjsua_call_get_conf_port(cid))
                                .arg(mi.index));
                    } catch (...) {
                        m_impl->callAudioMedia = nullptr;
                        Logger::instance().warn(LogCategory::Sip,
                            QStringLiteral("PJSIP RTP audio bridge failed: pjsipCallId=%1 mediaIndex=%2")
                                .arg(getId()).arg(mi.index));
                    }
                } else if (mi.type == PJMEDIA_TYPE_VIDEO
                           && mi.status == PJSUA_CALL_MEDIA_ACTIVE
                           && !videoActive) {
                    try {
                        auto *vid = static_cast<pj::VideoMedia *>(getMedia(mi.index));
                        m_impl->callVideoMedia = vid;
                        videoActive = true;
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("PJSIP video media active: pjsipCallId=%1 "
                                           "mediaIndex=%2 winId=%3 capDev=%4")
                                .arg(getId())
                                .arg(mi.index)
                                .arg(mi.videoIncomingWindowId)
                                .arg(mi.videoCapDev));
                    } catch (...) {
                        m_impl->callVideoMedia = nullptr;
                        Logger::instance().warn(LogCategory::Sip,
                            QStringLiteral("PJSIP video media active but getMedia failed: "
                                           "pjsipCallId=%1 mediaIndex=%2")
                                .arg(getId()).arg(mi.index));
                    }
                } else if (mi.type == PJMEDIA_TYPE_VIDEO
                           && mi.status != PJSUA_CALL_MEDIA_ACTIVE) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("PJSIP video media not active: pjsipCallId=%1 "
                                       "mediaIndex=%2 status=%3")
                            .arg(getId())
                            .arg(mi.index)
                            .arg(static_cast<int>(mi.status)));
                }
            }

            QPointer<SipCall> self = m_impl->q;
            QMetaObject::invokeMethod(self, [self, audioBridgeWired, videoActive]() {
                if (!self) return;
                if (audioBridgeWired)
                    emit self->audioMediaConnected();
                else
                    emit self->audioMediaDisconnected();
                if (videoActive) {
                    self->m_localVideoAvailable  = true;
                    self->m_remoteVideoAvailable = true;
                    emit self->videoMediaConnected();
                    emit self->localVideoStarted();
                    emit self->remoteVideoStarted();
                } else if (self->m_localVideoAvailable || self->m_remoteVideoAvailable) {
                    self->m_localVideoAvailable  = false;
                    self->m_remoteVideoAvailable = false;
                    emit self->localVideoStopped();
                    emit self->remoteVideoStopped();
                    emit self->videoMediaDisconnected();
                }
            }, Qt::QueuedConnection);
        }

        void stopAudioBridge()
        {
            if (!m_impl->callAudioMedia)
                return;
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP RTP audio bridge disconnected: pjsipCallId=%1")
                    .arg(getId()));
            m_impl->callAudioMedia = nullptr;

            QPointer<SipCall> self = m_impl->q;
            QMetaObject::invokeMethod(self, [self]() {
                if (self) emit self->audioMediaDisconnected();
            }, Qt::QueuedConnection);
        }

        void stopVideoBridge()
        {
            if (!m_impl->callVideoMedia)
                return;
            m_impl->callVideoMedia = nullptr;

            QPointer<SipCall> self = m_impl->q;
            QMetaObject::invokeMethod(self, [self]() {
                if (!self) return;
                if (self->m_localVideoAvailable || self->m_remoteVideoAvailable) {
                    self->m_localVideoAvailable  = false;
                    self->m_remoteVideoAvailable = false;
                    emit self->localVideoStopped();
                    emit self->remoteVideoStopped();
                    emit self->videoMediaDisconnected();
                }
            }, Qt::QueuedConnection);
        }

    private:
        Impl *m_impl;
    };

    PjCall         *pjCall{nullptr};
    void           *pjAccountHandle{nullptr}; // pj::Account* cast to void*
    pj::AudioMedia *callAudioMedia{nullptr};  // valid only while audio media is active
    pj::VideoMedia *callVideoMedia{nullptr};  // valid only while video media is active
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
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP INVITE outbound: call=%1 target=%2")
                    .arg(m_callId, m_remoteUri));
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

void SipCall::setPjsipAccountHandle(void *accountHandle)
{
#ifdef HAVE_PJSIP
    m_impl->pjAccountHandle = accountHandle;
#else
    Q_UNUSED(accountHandle)
#endif
}

bool SipCall::bindIncomingPjsipCall(void *accountHandle, int callId, const QString &remoteUri)
{
#ifdef HAVE_PJSIP
    if (!accountHandle || callId == PJSUA_INVALID_ID) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Incoming PJSIP call bind rejected: invalid account or call id"));
        return false;
    }
    if (m_stateMachine.state() != CallState::Idle) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Incoming PJSIP call bind rejected: current state is %1")
                .arg(callStateName(m_stateMachine.state())));
        return false;
    }

    try {
        auto *account = static_cast<pj::Account *>(accountHandle);
        m_impl->pjAccountHandle = accountHandle;
        m_impl->pjCall = new Impl::PjCall(m_impl, *account, callId);
        m_remoteUri = remoteUri.trimmed().isEmpty()
            ? QStringLiteral("sip:unknown@unknown")
            : remoteUri.trimmed();
        m_callId = QStringLiteral("pjsip-%1").arg(callId);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP INVITE inbound bound: call=%1 pjsipCallId=%2 remote=%3")
                .arg(m_callId).arg(callId).arg(m_remoteUri));
        return m_stateMachine.tryTransition(CallState::IncomingRinging,
                                            QStringLiteral("Incoming call from %1")
                                                .arg(m_remoteUri));
    } catch (const pj::Error &e) {
        delete m_impl->pjCall;
        m_impl->pjCall = nullptr;
        m_stateMachine.tryTransition(CallState::Failed,
                                     QString::fromStdString(e.reason),
                                     static_cast<int>(e.status));
        return false;
    }
#else
    Q_UNUSED(accountHandle)
    Q_UNUSED(callId)
    Q_UNUSED(remoteUri)
    return false;
#endif
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
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP INVITE answer 200 OK: call=%1 remote=%2")
                    .arg(m_callId, m_remoteUri));
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
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP INVITE reject 486 Busy Here: call=%1 remote=%2")
                    .arg(m_callId, m_remoteUri));
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
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP BYE/hangup requested: call=%1 remote=%2")
                    .arg(m_callId, m_remoteUri));
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
    if (m_impl->pjCall && m_impl->callAudioMedia) {
        try {
            pj::AudDevManager &adm = pj::Endpoint::instance().audDevManager();
            adm.getCaptureDevMedia().adjustTxLevel(muted ? 0.0f : 1.0f);
        } catch (...) {}
    }
#endif

    emit muteChanged(muted);
    return true;
}

bool SipCall::isMuted() const
{
    return m_muted;
}

bool SipCall::setVideoMuted(bool muted)
{
    if (m_videoMuted == muted)
        return true;

    m_videoMuted = muted;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Video mute %1: call id=%2")
            .arg(muted ? QStringLiteral("ON") : QStringLiteral("OFF"), m_callId));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            pj::CallVidSetStreamParam prm;
            prm.medIdx = -1; // default video stream
            if (muted)
                m_impl->pjCall->vidSetStream(PJSUA_CALL_VID_STRM_STOP_TRANSMIT, prm);
            else
                m_impl->pjCall->vidSetStream(PJSUA_CALL_VID_STRM_START_TRANSMIT, prm);
        } catch (...) {}
    }
#endif

    emit videoMuteChanged(muted);
    return true;
}

bool SipCall::isVideoMuted()           const { return m_videoMuted; }
bool SipCall::isLocalVideoAvailable()  const { return m_localVideoAvailable; }
bool SipCall::isRemoteVideoAvailable() const { return m_remoteVideoAvailable; }

void SipCall::releasePjsipCall()
{
#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            if (m_impl->pjCall->isActive())
                m_impl->pjCall->hangup(pj::CallOpParam());
        } catch (...) {}
        delete m_impl->pjCall;
        m_impl->pjCall = nullptr;
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP call slot released: id=%1").arg(m_callId));
    }
#endif
}

void SipCall::reset(const QString &reason)
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Call id=%1 reset: %2").arg(m_callId, reason));
    m_levelTimer.stop();
    m_localVideoAvailable  = false;
    m_remoteVideoAvailable = false;
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
        // In stub mode, emit audio/video media connected when call goes Active
        // (PJSIP emits these from onCallMediaState instead).
#ifndef HAVE_PJSIP
        emit audioMediaConnected();
        m_localVideoAvailable  = true;
        m_remoteVideoAvailable = true;
        emit videoMediaConnected();
        emit localVideoStarted();
        emit remoteVideoStarted();
#endif
    } else if (state == CallState::Held || state == CallState::Disconnecting) {
#ifndef HAVE_PJSIP
        // Stub: tear down audio/video when call leaves Active.
        if (m_levelTimer.isActive())
            emit audioMediaDisconnected();
        if (m_localVideoAvailable || m_remoteVideoAvailable) {
            m_localVideoAvailable  = false;
            m_remoteVideoAvailable = false;
            emit localVideoStopped();
            emit remoteVideoStopped();
            emit videoMediaDisconnected();
        }
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
