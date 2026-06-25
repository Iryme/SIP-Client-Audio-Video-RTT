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

            pj::CallInfo ci;
            try {
                ci = getInfo();
            } catch (const pj::Error &e) {
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("onCallState: getInfo() threw: %1")
                        .arg(QString::fromStdString(e.reason)));
                return;
            }
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
                const CallState cur = self->m_stateMachine.state();
                if (newState == CallState::Idle && cur != CallState::Idle
                        && cur != CallState::IncomingRinging) {
                    self->m_stateMachine.tryTransition(CallState::Disconnecting,
                                                       reason.isEmpty()
                                                           ? QStringLiteral("Call disconnected")
                                                           : reason,
                                                       code);
                }
                // Caller cancelled an incoming call: route through Disconnecting so
                // IncomingRinging → Idle is a valid path.
                if (cur == CallState::IncomingRinging
                        && (newState == CallState::Failed || newState == CallState::Idle)) {
                    self->m_stateMachine.tryTransition(CallState::Disconnecting,
                                                       reason.isEmpty()
                                                           ? QStringLiteral("Caller cancelled")
                                                           : reason,
                                                       code);
                    self->m_stateMachine.tryTransition(CallState::Idle,
                                                       reason, code);
                    return;
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
            bool audioBridgeWired    = false;
            bool videoActive         = false;
            int  videoIncomingWinId  = PJSUA_INVALID_ID;
            int  videoCapDevId       = -1;

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
                        // onCallMediaState can fire multiple times (e.g. when a
                        // text/RTT stream is added after the initial INVITE).
                        // When PJSIP rebuilds the media session the old conf port
                        // is already removed internally — do NOT call stopTransmit
                        // on the old pointer (it would assert with slot=-1).
                        // Just wire to the new media object if it changed.
                        if (m_impl->callAudioMedia != aud) {
                            adm.getCaptureDevMedia().startTransmit(*aud);
                            aud->startTransmit(adm.getPlaybackDevMedia());
                            m_impl->callAudioMedia = aud;
                        }
                        audioBridgeWired = true;
                        const pjsua_call_id cid = static_cast<pjsua_call_id>(getId());
                        try {
                            pj::AudDevManager &admLog = pj::Endpoint::instance().audDevManager();
                            const int capIdx = admLog.getCaptureDev();
                            const int plbIdx = admLog.getPlaybackDev();
                            QString capName = QStringLiteral("(default)");
                            QString plbName = QStringLiteral("(default)");
                            if (capIdx >= 0) {
                                try { capName = QString::fromStdString(admLog.getDevInfo(capIdx).name); } catch (...) {}
                            }
                            if (plbIdx >= 0) {
                                try { plbName = QString::fromStdString(admLog.getDevInfo(plbIdx).name); } catch (...) {}
                            }
                            Logger::instance().info(LogCategory::Sip,
                                QStringLiteral("PJSIP RTP audio bridge connected: pjsipCallId=%1 confSlot=%2 mediaIndex=%3 capDev=[%4]\"%5\" plbDev=[%6]\"%7\"")
                                    .arg(getId())
                                    .arg(pjsua_call_get_conf_port(cid))
                                    .arg(mi.index)
                                    .arg(capIdx).arg(capName)
                                    .arg(plbIdx).arg(plbName));
                        } catch (...) {
                            Logger::instance().info(LogCategory::Sip,
                                QStringLiteral("PJSIP RTP audio bridge connected: pjsipCallId=%1 confSlot=%2 mediaIndex=%3")
                                    .arg(getId())
                                    .arg(pjsua_call_get_conf_port(cid))
                                    .arg(mi.index));
                        }
                        // Log negotiated audio codec from SDP.
                        try {
                            pjsua_stream_info si;
                            pj_bzero(&si, sizeof(si));
                            if (pjsua_call_get_stream_info(cid,
                                    static_cast<unsigned>(mi.index), &si) == PJ_SUCCESS
                                && si.type == PJMEDIA_TYPE_AUDIO) {
                                const pjmedia_codec_info &fmt = si.info.aud.fmt;
                                const QString encName = fmt.encoding_name.slen > 0
                                    ? QString::fromLatin1(fmt.encoding_name.ptr,
                                                          static_cast<int>(fmt.encoding_name.slen))
                                    : QStringLiteral("(unknown)");
                                Logger::instance().info(LogCategory::Media,
                                    QStringLiteral("Negotiated audio codec: %1/%2  pt=%3")
                                        .arg(encName)
                                        .arg(fmt.clock_rate)
                                        .arg(fmt.pt));
                            }
                        } catch (...) {}
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
                        videoActive        = true;
                        videoIncomingWinId = mi.videoIncomingWindowId;
                        videoCapDevId      = mi.videoCapDev;
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("PJSIP video media active: pjsipCallId=%1 "
                                           "mediaIndex=%2 winId=%3 capDev=%4")
                                .arg(getId())
                                .arg(mi.index)
                                .arg(mi.videoIncomingWindowId)
                                .arg(mi.videoCapDev));
                        // Log negotiated video codec.
                        try {
                            pjsua_stream_info si;
                            pj_bzero(&si, sizeof(si));
                            const pjsua_call_id cid = static_cast<pjsua_call_id>(getId());
                            if (pjsua_call_get_stream_info(cid,
                                    static_cast<unsigned>(mi.index), &si) == PJ_SUCCESS
                                && si.type == PJMEDIA_TYPE_VIDEO) {
                                const pjmedia_vid_codec_info &vfmt = si.info.vid.codec_info;
                                const QString encName = vfmt.encoding_name.slen > 0
                                    ? QString::fromLatin1(vfmt.encoding_name.ptr,
                                                          static_cast<int>(vfmt.encoding_name.slen))
                                    : QStringLiteral("(unknown)");
                                Logger::instance().info(LogCategory::Media,
                                    QStringLiteral("Negotiated video codec: %1  pt=%2")
                                        .arg(encName)
                                        .arg(vfmt.pt));
                            }
                        } catch (...) {}
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
            QMetaObject::invokeMethod(self,
                [self, audioBridgeWired, videoActive, videoIncomingWinId, videoCapDevId]() {
                if (!self) return;
                if (audioBridgeWired)
                    emit self->audioMediaConnected();
                else
                    emit self->audioMediaDisconnected();
                if (videoActive) {
                    // Store on Qt main thread — read by attachVideoWindows on same thread.
                    self->m_impl->videoIncomingWinId =
                        static_cast<pjsua_vid_win_id>(videoIncomingWinId);
                    self->m_impl->videoCapDev = videoCapDevId;
                    self->m_localVideoAvailable  = true;
                    self->m_remoteVideoAvailable = true;
                    emit self->videoMediaConnected();
                    emit self->localVideoStarted();
                    emit self->remoteVideoStarted();
                } else if (self->m_localVideoAvailable || self->m_remoteVideoAvailable) {
                    // Video stream became inactive (re-negotiation removed it).
                    self->m_impl->stopLocalPreview(); // also resets videoIncomingWinId/videoCapDev
                    self->m_localVideoAvailable  = false;
                    self->m_remoteVideoAvailable = false;
                    emit self->localVideoStopped();
                    emit self->remoteVideoStopped();
                    emit self->videoMediaDisconnected();
                }
            }, Qt::QueuedConnection);
        }

        void onCallSdpCreated(pj::OnCallSdpCreatedParam &prm) override
        {
            // Extract m= lines from the created SDP offer/answer so callers can
            // verify that m=audio and m=video are both present without needing a
            // live SIP trace capture.
            const QString sdp = QString::fromStdString(prm.sdp.wholeSdp);
            QStringList mLines;
            for (const QString &line : sdp.split(QLatin1Char('\n'))) {
                const QString trimmed = line.trimmed();
                if (trimmed.startsWith(QLatin1String("m=")))
                    mLines.append(trimmed);
            }
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("SDP offer/answer m= lines (%1): %2")
                    .arg(mLines.size())
                    .arg(mLines.isEmpty() ? QStringLiteral("(none)")
                                          : mLines.join(QStringLiteral(", "))));
        }

        void stopAudioBridge()
        {
            if (!m_impl->callAudioMedia)
                return;
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP RTP audio bridge disconnected: pjsipCallId=%1")
                    .arg(getId()));
            // Do NOT call stopTransmit here: at DISCONNECTED time the conf port
            // is already removed by PJSIP, so stopTransmit would assert slot=-1.
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
                    // Stop the local preview (camera) before signalling disconnect.
                    // Without this the capture device stays active after hangup.
                    self->m_impl->stopLocalPreview();
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

    // Stop the local preview window (if running) and reset video state.
    // Must be called on the Qt main thread. Idempotent.
    void stopLocalPreview()
    {
        // PJMEDIA_VID_DEFAULT_CAPTURE_DEV = -1 is valid; only reject truly
        // invalid values (PJMEDIA_VID_INVALID_DEV = -3, render default = -2).
        if (videoCapDev < PJMEDIA_VID_DEFAULT_CAPTURE_DEV)
            return;
        const pjmedia_vid_dev_index capDev =
            static_cast<pjmedia_vid_dev_index>(videoCapDev);
        if (pjsua_vid_preview_get_win(capDev) != PJSUA_INVALID_ID) {
            const pj_status_t st = pjsua_vid_preview_stop(capDev);
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Local preview stopped: capDev=%1 status=%2")
                    .arg(videoCapDev).arg(st));
        }
        videoCapDev        = PJMEDIA_VID_INVALID_DEV;
        videoIncomingWinId = PJSUA_INVALID_ID;
    }

    PjCall             *pjCall{nullptr};
    pj::Call           *earlyCall{nullptr};           // EarlyCall from SipAccount; freed after pjCall
    void               *pjAccountHandle{nullptr};     // pj::Account* cast to void*
    pj::AudioMedia     *callAudioMedia{nullptr};      // valid only while audio media is active
    pj::VideoMedia     *callVideoMedia{nullptr};      // valid only while video media is active
    pjsua_vid_win_id    videoIncomingWinId{PJSUA_INVALID_ID}; // incoming video window id
    int                 videoCapDev{PJMEDIA_VID_INVALID_DEV}; // capture device; -1=default, >=0=specific
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
    // earlyCall must be deleted AFTER pjCall: pjCall's destructor sets user_data=null;
    // if earlyCall were deleted first, its destructor would also set user_data=null,
    // unregistering pjCall and silencing all remaining PJSIP callbacks.
    delete m_impl->earlyCall;
    m_impl->earlyCall = nullptr;
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

bool SipCall::bindIncomingPjsipCall(void *accountHandle, int callId, const QString &remoteUri,
                                    void *earlyCallHandle)
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
        // EarlyCall was created in onIncomingCall to prevent pjsua2 auto-reject.
        // Adopt it here — PjCall constructor will overwrite user_data to itself;
        // earlyCall is kept alive and deleted AFTER pjCall (see ~SipCall and
        // releasePjsipCall) because earlyCall's destructor would clear user_data.
        m_impl->earlyCall = static_cast<pj::Call *>(earlyCallHandle);

        m_impl->pjCall = new Impl::PjCall(m_impl, *account, callId);
        // PjCall constructor: pjsua_call_set_user_data(callId, pjCall) — overwrites
        // EarlyCall's user_data slot. All subsequent PJSIP callbacks go to PjCall.

        m_remoteUri = remoteUri.trimmed().isEmpty()
            ? QStringLiteral("sip:unknown@unknown")
            : remoteUri.trimmed();
        m_callId = QStringLiteral("pjsip-%1").arg(callId);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP INVITE inbound bound: call=%1 pjsipCallId=%2 remote=%3")
                .arg(m_callId).arg(callId).arg(m_remoteUri));

        // 180 Ringing was already sent in onIncomingCall (after EarlyCall was created).
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
        delete m_impl->earlyCall;
        m_impl->earlyCall = nullptr;
    }
    // Safety net: stop preview if stopVideoBridge callback hasn't fired yet.
    m_impl->stopLocalPreview();
#endif
}

void SipCall::attachVideoWindows(WId remoteWidget, WId localPreview)
{
#if defined(HAVE_PJSIP) && defined(Q_OS_WIN)
    if (!m_impl)
        return;

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("attachVideoWindows: remoteHwnd=0x%1 localHwnd=0x%2 "
                       "videoWinId=%3 capDev=%4")
            .arg(static_cast<quintptr>(remoteWidget), 0, 16)
            .arg(static_cast<quintptr>(localPreview), 0, 16)
            .arg(m_impl->videoIncomingWinId)
            .arg(m_impl->videoCapDev));

    // --- Remote incoming video ------------------------------------------------
    if (m_impl->videoIncomingWinId != PJSUA_INVALID_ID && remoteWidget != 0) {
        try {
            pj::VideoWindow vw(m_impl->videoIncomingWinId);
            pj::VideoWindowInfo info = vw.getInfo();
            HWND pjHwnd  = static_cast<HWND>(info.winHandle.handle.window);
            HWND qtHwnd  = reinterpret_cast<HWND>(static_cast<quintptr>(remoteWidget));

            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Remote video PJSIP hwnd=0x%1 isNative=%2")
                    .arg(reinterpret_cast<quintptr>(pjHwnd), 0, 16)
                    .arg(info.isNative));

            if (pjHwnd && qtHwnd) {
                // Reparent the PJSIP video window into our Qt remote-video widget.
                // Change window style from popup to child so it behaves correctly.
                LONG style = GetWindowLong(pjHwnd, GWL_STYLE);
                style = (style & ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME))
                        | WS_CHILD | WS_VISIBLE;
                SetWindowLong(pjHwnd, GWL_STYLE, style);
                SetParent(pjHwnd, qtHwnd);

                RECT rc{};
                GetClientRect(qtHwnd, &rc);
                MoveWindow(pjHwnd, 0, 0, rc.right, rc.bottom, TRUE);
                ShowWindow(pjHwnd, SW_SHOW);

                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Remote video embedded into widget: %1x%2")
                        .arg(rc.right).arg(rc.bottom));
            } else {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("Remote video attach skipped: pjHwnd or qtHwnd is null"));
            }
        } catch (const pj::Error &e) {
            Logger::instance().warn(LogCategory::Media,
                QStringLiteral("attachVideoWindows remote failed: %1")
                    .arg(QString::fromStdString(e.reason)));
        }
    } else {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("Remote video attach skipped: videoWinId=%1 remoteWidget=%2")
                .arg(m_impl->videoIncomingWinId)
                .arg(static_cast<quintptr>(remoteWidget)));
    }

    // --- Local preview --------------------------------------------------------
    if (m_impl->videoCapDev >= PJMEDIA_VID_DEFAULT_CAPTURE_DEV && localPreview != 0) {
        pjsua_vid_win_id previewWinId = pjsua_vid_preview_get_win(
            static_cast<pjmedia_vid_dev_index>(m_impl->videoCapDev));

        if (previewWinId == PJSUA_INVALID_ID) {
            pjsua_vid_preview_param pvp;
            pjsua_vid_preview_param_default(&pvp);
            pvp.show = PJ_FALSE;
            const pj_status_t st = pjsua_vid_preview_start(
                static_cast<pjmedia_vid_dev_index>(m_impl->videoCapDev), &pvp);
            if (st == PJ_SUCCESS) {
                previewWinId = pjsua_vid_preview_get_win(
                    static_cast<pjmedia_vid_dev_index>(m_impl->videoCapDev));
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Local preview started: capDev=%1 previewWinId=%2")
                        .arg(m_impl->videoCapDev).arg(previewWinId));
            } else {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("Local preview start failed: capDev=%1 status=%2")
                        .arg(m_impl->videoCapDev).arg(st));
            }
        } else {
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Local preview already running: capDev=%1 previewWinId=%2")
                    .arg(m_impl->videoCapDev).arg(previewWinId));
        }

        if (previewWinId != PJSUA_INVALID_ID) {
            try {
                pj::VideoWindow pvw(previewWinId);
                pj::VideoWindowInfo pinfo = pvw.getInfo();
                HWND pjPreviewHwnd = static_cast<HWND>(pinfo.winHandle.handle.window);
                HWND qtPreviewHwnd =
                    reinterpret_cast<HWND>(static_cast<quintptr>(localPreview));

                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Local preview PJSIP hwnd=0x%1")
                        .arg(reinterpret_cast<quintptr>(pjPreviewHwnd), 0, 16));

                if (pjPreviewHwnd && qtPreviewHwnd) {
                    LONG style = GetWindowLong(pjPreviewHwnd, GWL_STYLE);
                    style = (style & ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME))
                            | WS_CHILD | WS_VISIBLE;
                    SetWindowLong(pjPreviewHwnd, GWL_STYLE, style);
                    SetParent(pjPreviewHwnd, qtPreviewHwnd);

                    RECT rc{};
                    GetClientRect(qtPreviewHwnd, &rc);
                    MoveWindow(pjPreviewHwnd, 0, 0, rc.right, rc.bottom, TRUE);
                    ShowWindow(pjPreviewHwnd, SW_SHOW);

                    Logger::instance().info(LogCategory::Media,
                        QStringLiteral("Local preview embedded into widget: %1x%2")
                            .arg(rc.right).arg(rc.bottom));
                } else {
                    Logger::instance().warn(LogCategory::Media,
                        QStringLiteral("Local preview attach skipped: hwnd null"));
                }
            } catch (const pj::Error &e) {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("attachVideoWindows local preview failed: %1")
                        .arg(QString::fromStdString(e.reason)));
            }
        } else {
            Logger::instance().warn(LogCategory::Media,
                QStringLiteral("Local preview window not available: capDev=%1")
                    .arg(m_impl->videoCapDev));
        }
    } else {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Local preview attach skipped: capDev=%1 localWidget=%2")
                .arg(m_impl->videoCapDev)
                .arg(static_cast<quintptr>(localPreview)));
    }
#else
    Q_UNUSED(remoteWidget)
    Q_UNUSED(localPreview)
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
        if (slot < 0)
            return; // conf port temporarily invalid (e.g. media re-negotiate)
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
