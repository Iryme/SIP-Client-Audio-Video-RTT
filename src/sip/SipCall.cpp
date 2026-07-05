#include "SipCall.h"

#include <QPointer>
#include <QUuid>

#include "core/Logger.h"
#include "core/AppSettings.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoQualityManager.h"
#include "media/RtpStats.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <pjsua-lib/pjsua.h>
#include <pj/errno.h>
#endif

#if defined(HAVE_PJSIP) && defined(_WIN32)
#include "media/PjsipGdiRenderer.h"
#include <windows.h>
#endif

#if defined(HAVE_PJSIP) && defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
static QString normalizeDeviceName(const QString &value)
{
    QString out;
    out.reserve(value.size());
    for (QChar ch : value.toLower()) {
        if (ch.isLetterOrNumber())
            out.append(ch);
    }
    return out;
}

static QString pjsipStatusText(pj_status_t st)
{
    char buf[PJ_ERR_MSG_SIZE] = {};
    pj_strerror(st, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

#endif

#ifdef HAVE_PJSIP
static QString mediaTypeName(pjmedia_type type)
{
    switch (type) {
    case PJMEDIA_TYPE_AUDIO: return QStringLiteral("audio");
    case PJMEDIA_TYPE_VIDEO: return QStringLiteral("video");
    case PJMEDIA_TYPE_TEXT:  return QStringLiteral("text");
    default:                 return QStringLiteral("unknown");
    }
}

static bool isSessionTerminatedError(const pj::Error &e)
{
    return e.status == PJSIP_ESESSIONTERMINATED;
}

static bool isTeardownState(CallState state)
{
    return state == CallState::Disconnecting
        || state == CallState::Idle
        || state == CallState::Failed;
}
#endif

#if defined(HAVE_PJSIP) && defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
static bool hasPjsipVideoCaptureDevice()
{
    try {
        pj::VidDevManager &vdm = pj::Endpoint::instance().vidDevManager();
        const unsigned count = vdm.getDevCount();
        for (unsigned i = 0; i < count; ++i) {
            try {
                pj::VideoDevInfo info = vdm.getDevInfo(static_cast<int>(i));
                if (info.dir == PJMEDIA_DIR_CAPTURE || info.dir == PJMEDIA_DIR_CAPTURE_RENDER)
                    return true;
            } catch (...) {}
        }
    } catch (...) {}
    return false;
}

static void applyVideoMediaDirectionIfNeeded(pj::CallSetting &setting)
{
    // Negotiate bidirectional video so the local camera can be transmitted
    // and the remote peer can still send video back.
    setting.mediaDir = {
        PJMEDIA_DIR_ENCODING_DECODING, // audio: bidirectional
        PJMEDIA_DIR_ENCODING_DECODING, // video: bidirectional
        PJMEDIA_DIR_ENCODING_DECODING  // text: bidirectional
    };
}

static unsigned pjsipVideoDeviceCount()
{
    try {
        return pj::Endpoint::instance().vidDevManager().getDevCount();
    } catch (...) {
        return 0;
    }
}

static QString preferredCameraDisplayName()
{
    const QString selectedId = AppSettings::loadSelectedCamera();
    if (!selectedId.isEmpty()) {
        const MediaDevice dev = MediaDeviceManager::instance().findDevice(
            MediaDeviceType::Camera, selectedId);
        if (!dev.isNull())
            return dev.displayName;
    }

    MediaDeviceSelectionModel selection(&MediaDeviceManager::instance());
    const MediaDevice dev = selection.selectedCamera();
    return dev.isNull() ? QString() : dev.displayName;
}

static pjmedia_vid_dev_index preferredPjsipVideoCaptureDevice()
{
    const QString preferredName = preferredCameraDisplayName();
    const QString preferredNorm = normalizeDeviceName(preferredName);
    try {
        pj::VidDevManager &vdm = pj::Endpoint::instance().vidDevManager();
        const unsigned count = vdm.getDevCount();
        for (unsigned i = 0; i < count; ++i) {
            try {
                pj::VideoDevInfo info = vdm.getDevInfo(static_cast<int>(i));
                if (info.dir != PJMEDIA_DIR_CAPTURE && info.dir != PJMEDIA_DIR_CAPTURE_RENDER)
                    continue;

                const QString name = QString::fromStdString(info.name);
                if (!preferredNorm.isEmpty()
                    && normalizeDeviceName(name) == preferredNorm) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("Selected Qt camera '%1' mapped to PJSIP video device '%2' (index %3)")
                            .arg(preferredName, name)
                            .arg(i));
                    return static_cast<pjmedia_vid_dev_index>(i);
                }
            } catch (...) {}
        }
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("No matching PJSIP video device found for Qt camera '%1'")
                .arg(preferredName));
    } catch (...) {}
    return PJMEDIA_VID_INVALID_DEV;
}
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
                if (isSessionTerminatedError(e)) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("onCallState: call already terminated; treating as normal teardown"));
                    QPointer<SipCall> self = m_impl->q;
                    QMetaObject::invokeMethod(self, [self]() {
                        if (!self)
                            return;
                        const CallState cur = self->m_stateMachine.state();
                        if (!isTeardownState(cur)) {
                            self->m_stateMachine.tryTransition(CallState::Idle,
                                                               QStringLiteral("Call ended"),
                                                               0);
                        }
                    }, Qt::QueuedConnection);
                    return;
                }
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
            const bool holdWasActive = m_impl->holdActive;
            QMetaObject::invokeMethod(self, [self, newState, reason, code, holdWasActive]() {
                if (!self)
                    return;
                const CallState cur = self->m_stateMachine.state();

                // In PJSIP mode, setHold() sends a re-INVITE with a=sendonly.
                // When the remote answers 200 OK, PJSIP_INV_STATE_CONFIRMED fires
                // again (the dialog stays CONFIRMED — there is no separate "held" PJSIP state).
                // We already transitioned the Qt state machine to Held when hold() was called,
                // so we must suppress this spurious Active transition while hold is in effect.
                if (newState == CallState::Active && holdWasActive) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("PJSIP hold re-INVITE 200 OK: suppressing Active "
                                       "transition — already in Held state; callId=%1")
                            .arg(self->m_callId));
                    return;
                }

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

            pj::CallInfo ci;
            try {
                ci = getInfo();
            } catch (const pj::Error &e) {
                if (isSessionTerminatedError(e)) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("PJSIP media state callback ignored: call already terminated during teardown"));
                    stopAudioBridge();
                    stopVideoBridge();
                    return;
                }
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("onCallMediaState: getInfo() threw: %1")
                        .arg(QString::fromStdString(e.reason)));
                return;
            }
            bool audioBridgeWired    = false;
            bool videoActive         = false;
            bool textMediaActive     = false;
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
                        // Log negotiated audio codec from SDP and store it so
                        // Diagnostics can display the real negotiated codec
                        // instead of only ever seeing it in the log.
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
                                if (m_impl->q) {
                                    m_impl->q->m_negotiatedAudioCodec =
                                        SipCall::formatAudioCodecSummary(encName, fmt.clock_rate, fmt.pt);
                                }
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
                        const bool localRequestPending = m_impl->videoRequestPendingLocal;
                        m_impl->videoRequestPendingLocal = false;
                        // Reset so a future remote video request after this one is detected.
                        if (localRequestPending)
                            m_impl->videoRequestNotified = false;
                        if (!localRequestPending && !m_impl->videoRequestNotified) {
                            m_impl->videoRequestNotified = true;
                            QPointer<SipCall> self = m_impl->q;
                            QMetaObject::invokeMethod(self, [self]() {
                                if (self)
                                    emit self->videoRequested();
                            }, Qt::QueuedConnection);
                        }
                        {
                            const bool captureAvail = hasPjsipVideoCaptureDevice();
                            Logger::instance().info(LogCategory::Sip,
                                QStringLiteral("PJSIP video media active: pjsipCallId=%1 "
                                               "mediaIndex=%2 winId=%3 capDev=%4 "
                                               "pjsipCaptureAvailable=%5 "
                                               "videoDir=ENCODING_DECODING(sendrecv)")
                                    .arg(getId())
                                    .arg(mi.index)
                                    .arg(mi.videoIncomingWindowId)
                                    .arg(mi.videoCapDev)
                                    .arg(captureAvail ? QStringLiteral("yes") : QStringLiteral("no")));
                            if (!captureAvail) {
                                Logger::instance().warn(LogCategory::Sip,
                                    QStringLiteral("Remote video negotiated but local transmit "
                                                   "may be unavailable: PJSIP capture backend missing "
                                                   "or no usable capture device is exposed"));
                            }
                        }
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
                } else if (mi.type == PJMEDIA_TYPE_TEXT) {
                    if (mi.status == PJSUA_CALL_MEDIA_ACTIVE) {
                        textMediaActive = true;
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("PJSIP RTT text stream active: pjsipCallId=%1 "
                                           "mediaIndex=%2 (RFC 4103 / T.140)")
                                .arg(getId()).arg(mi.index));
                        // Log negotiated text codec (red/t140) and whether RED
                        // was successfully negotiated with the remote peer.
                        try {
                            pjsua_stream_info si;
                            pj_bzero(&si, sizeof(si));
                            const pjsua_call_id cid =
                                static_cast<pjsua_call_id>(getId());
                            if (pjsua_call_get_stream_info(cid,
                                    static_cast<unsigned>(mi.index), &si) == PJ_SUCCESS
                                && si.type == PJMEDIA_TYPE_TEXT) {
                                const pjmedia_codec_info &fmt = si.info.txt.fmt;
                                const QString encName = fmt.encoding_name.slen > 0
                                    ? QString::fromLatin1(
                                          fmt.encoding_name.ptr,
                                          static_cast<int>(fmt.encoding_name.slen))
                                    : QStringLiteral("(unknown)");
                                const bool redNegotiated = encName.compare(
                                    QStringLiteral("red"),
                                    Qt::CaseInsensitive) == 0;
                                Logger::instance().info(LogCategory::Media,
                                    QStringLiteral("Negotiated text codec: %1/%2  pt=%3  "
                                                   "RED=%4")
                                        .arg(encName)
                                        .arg(fmt.clock_rate)
                                        .arg(fmt.pt)
                                        .arg(redNegotiated
                                             ? QStringLiteral("yes (RFC 4103 / RFC 2198)")
                                             : QStringLiteral("no (plain T.140 only)")));
                            }
                        } catch (...) {}
                    } else {
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("PJSIP RTT text stream not active: pjsipCallId=%1 "
                                           "mediaIndex=%2 status=%3")
                                .arg(getId()).arg(mi.index)
                                .arg(static_cast<int>(mi.status)));
                    }
                }
            }

            QPointer<SipCall> self = m_impl->q;
            const bool prevRttActive = m_impl->rttMediaActive;
            m_impl->rttMediaActive = textMediaActive;
            QMetaObject::invokeMethod(self,
                [self, audioBridgeWired, videoActive, videoIncomingWinId, videoCapDevId,
                 textMediaActive, prevRttActive]() {
                if (!self) return;
                if (audioBridgeWired)
                    emit self->audioMediaConnected();
                else
                    emit self->audioMediaDisconnected();
                if (textMediaActive && !prevRttActive) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("Text protocol active: protocol=RTT"));
                    self->m_impl->rttRequestNotified = false; // RTT accepted — reset pending flag
                    emit self->rttMediaConnected();
                } else if (!textMediaActive && prevRttActive) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("RTT text media gone — rttMediaDisconnected"));
                    emit self->rttMediaDisconnected();
                }
                if (videoActive) {
                    // Store on Qt main thread — read by attachVideoWindows on same thread.
                    self->m_impl->videoIncomingWinId =
                        static_cast<pjsua_vid_win_id>(videoIncomingWinId);
                    self->m_impl->videoCapDev = videoCapDevId;
                    self->m_localVideoAvailable  = true;
                    self->m_remoteVideoAvailable = true;
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("Video negotiation active"));
                    emit self->videoMediaConnected();
                    emit self->localVideoStarted();
                    emit self->remoteVideoStarted();
                } else if (self->m_localVideoAvailable || self->m_remoteVideoAvailable) {
                    // Video stream became inactive (re-negotiation removed it).
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("Video negotiation failed or inactive"));
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

        void onCallRxText(pj::OnCallRxTextParam &prm) override
        {
            if (!m_impl || !m_impl->q)
                return;
            const QString text = QString::fromStdString(prm.text);
            const unsigned seq = prm.seq;
            QPointer<SipCall> self = m_impl->q;
            // Logger and signal emission are both on the main thread — Logger is not
            // thread-safe (emits Qt signals) and must not be called from PJSIP callbacks.
            QMetaObject::invokeMethod(self, [self, text, seq]() {
                if (!self) return;
                if (!text.isEmpty()) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("RTT text received from remote: seq=%1 text=\"%2\"")
                            .arg(seq).arg(text));
                }
                // Emit for all packets (including empty keepalives) so RttSession
                // can maintain its suppression counter on the main thread.
                emit self->rttTextReceived(text);
            }, Qt::QueuedConnection);
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
                if (!self) return;
                self->m_negotiatedAudioCodec.clear();
                emit self->audioMediaDisconnected();
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

        void onCallRxReinvite(pj::OnCallRxReinviteParam &prm) override
        {
            if (!m_impl || !m_impl->q)
                return;

            const QString offerSdp = QString::fromStdString(prm.offer.wholeSdp);
            const bool hasVideoOffer = offerSdp.contains(QStringLiteral("m=video"), Qt::CaseInsensitive);
            const bool hasTextOffer  = offerSdp.contains(QStringLiteral("m=text"),  Qt::CaseInsensitive);

            // ── Video consent ───────────────────────────────────────────────
            if (hasVideoOffer && !m_impl->videoRequestPendingLocal) {
                // Remote requesting video — decline in auto-response; user must accept.
                prm.opt.videoCount = 0;
                if (!m_impl->videoRequestNotified) {
                    m_impl->videoRequestNotified = true;
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("Incoming video request pending — declined auto-response, "
                                       "awaiting user accept: callId=%1").arg(getId()));
                    QPointer<SipCall> self = m_impl->q;
                    QMetaObject::invokeMethod(self, [self]() {
                        if (self)
                            emit self->videoRequested();
                    }, Qt::QueuedConnection);
                }
            } else if (!hasVideoOffer && m_impl->videoRequestNotified && !m_impl->videoRequestPendingLocal) {
                // Peer withdrew the video offer — reset so UI can reflect "request video available".
                m_impl->videoRequestNotified = false;
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Peer withdrew video offer; resetting pending video state: callId=%1")
                        .arg(getId()));
                QPointer<SipCall> self = m_impl->q;
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) {
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("Peer video stopped; request video available again"));
                        emit self->videoMediaDisconnected();
                    }
                }, Qt::QueuedConnection);
            }

            // ── RTT/text consent ────────────────────────────────────────────
            if (hasTextOffer && !m_impl->rttMediaActive) {
                // Remote requesting RTT — decline in auto-response; user must accept.
                prm.opt.textCount = 0;
                if (!m_impl->rttRequestNotified) {
                    m_impl->rttRequestNotified = true;
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("Incoming text request pending: protocol=RTT — declined auto-response, "
                                       "awaiting user accept: callId=%1").arg(getId()));
                    QPointer<SipCall> self = m_impl->q;
                    QMetaObject::invokeMethod(self, [self]() {
                        if (self)
                            emit self->rttRequested();
                    }, Qt::QueuedConnection);
                }
            } else if (!hasTextOffer && m_impl->rttRequestNotified) {
                // Peer withdrew the RTT offer.
                m_impl->rttRequestNotified = false;
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Peer withdrew RTT offer: callId=%1").arg(getId()));
            }

            // If neither video nor text is being offered/changed, nothing further to do.
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
        videoRequestPendingLocal = false;
        videoRequestNotified = false;
    }

    // Release the DirectShow capture device without tearing down the call.
    // Called on Camera Off during an active video call. PJSIP manages the
    // call's capture through the preview subsystem — stopping it releases
    // the hardware (camera LED off). videoCapDev is preserved so the device
    // can be identified when Camera On calls resumeCapture().
    // Side-effect: stopLocalPreview() at call teardown will find
    // pjsua_vid_preview_get_win() == INVALID_ID and skip the stop, which
    // prevents the pjsua2 internal errors that appear when the session is
    // already terminated.
    void pauseCapture()
    {
        if (videoCapDev < PJMEDIA_VID_DEFAULT_CAPTURE_DEV)
            return;
        const auto capDev = static_cast<pjmedia_vid_dev_index>(videoCapDev);
        if (pjsua_vid_preview_get_win(capDev) == PJSUA_INVALID_ID)
            return;
        const pj_status_t st = pjsua_vid_preview_stop(capDev);
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Camera Off: capture released capDev=%1 status=%2")
                .arg(videoCapDev).arg(st));
    }

    // Re-open the DirectShow capture device after pauseCapture().
    // Called on Camera On during an active video call before resuming transmit.
    void resumeCapture()
    {
        if (videoCapDev < PJMEDIA_VID_DEFAULT_CAPTURE_DEV)
            return;
        const auto capDev = static_cast<pjmedia_vid_dev_index>(videoCapDev);
        if (pjsua_vid_preview_get_win(capDev) != PJSUA_INVALID_ID)
            return; // already open
        pjsua_vid_preview_param pvp;
        pjsua_vid_preview_param_default(&pvp);
        pvp.rend_id = PJMEDIA_VID_DEFAULT_RENDER_DEV;
        pvp.show    = PJ_FALSE;
        const pj_status_t st = pjsua_vid_preview_start(capDev, &pvp);
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Camera On: capture re-opened capDev=%1 status=%2")
                .arg(videoCapDev).arg(st));
    }

    PjCall             *pjCall{nullptr};
    pj::Call           *earlyCall{nullptr};           // EarlyCall from SipAccount; freed after pjCall
    void               *pjAccountHandle{nullptr};     // pj::Account* cast to void*
    pj::AudioMedia     *callAudioMedia{nullptr};      // valid only while audio media is active
    pj::VideoMedia     *callVideoMedia{nullptr};      // valid only while video media is active
    pjsua_vid_win_id    videoIncomingWinId{PJSUA_INVALID_ID}; // incoming video window id
    int                 videoCapDev{PJMEDIA_VID_INVALID_DEV}; // capture device; -1=default, >=0=specific
    bool                videoRequestPendingLocal{false}; // local user requested video and is awaiting completion
    bool                videoRequestNotified{false}; // first video negotiation notification emitted
    bool                rttRequestNotified{false};    // incoming RTT request notified to UI; reset on RTT active
    bool                rttMediaActive{false};         // true while T.140 text stream is active
    bool                holdActive{false};             // true while local hold is in effect (PJSIP mode)
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
    return makeCallWithOptions(remoteUri, SipCallOptions{});
}

bool SipCall::makeCallWithOptions(const QString &remoteUri, const SipCallOptions &opts)
{
    if (m_stateMachine.state() != CallState::Idle) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Call rejected: makeCallWithOptions called in state %1")
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
        QStringLiteral("Call started: id=%1 uri=%2 emergency=%3")
            .arg(m_callId, m_remoteUri,
                 opts.emergencyCall ? QStringLiteral("yes") : QStringLiteral("no")));

    m_stateMachine.tryTransition(CallState::OutgoingInit,
                                 QStringLiteral("Dialing %1").arg(m_remoteUri));

#ifdef HAVE_PJSIP
    if (m_impl->pjAccountHandle) {
        auto *account = static_cast<pj::Account *>(m_impl->pjAccountHandle);
        try {
            m_impl->pjCall = new Impl::PjCall(m_impl, *account);
            pj::CallOpParam prm(true);
            prm.opt.videoCount = opts.allowVideo ? 1 : 0;
            prm.opt.textCount  = opts.requireRtt ? 1 : 0;
            if (opts.allowVideo)
                applyVideoMediaDirectionIfNeeded(prm.opt);
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP INVITE outbound: RTT m=text offered (textCount=%1)")
                    .arg(prm.opt.textCount));
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP INVITE outbound video setup: call=%1 videoCount=%2 videoDevCount=%3 defaultCaptureDev=%4 hasCaptureDev=%5")
                    .arg(m_callId)
                    .arg(prm.opt.videoCount)
                    .arg(pjsipVideoDeviceCount())
                    .arg(preferredPjsipVideoCaptureDevice())
                    .arg(hasPjsipVideoCaptureDevice() ? QStringLiteral("true") : QStringLiteral("false")));
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP INVITE outbound: call=%1 target=%2")
                    .arg(m_callId, m_remoteUri));

            // Inject extra SIP headers (emergency path).
            for (const auto &hdr : opts.customHeaders) {
                pj::SipHeader sh;
                sh.hName  = hdr.first.toStdString();
                sh.hValue = hdr.second.toStdString();
                prm.txOption.headers.push_back(sh);
            }
            if (!opts.customHeaders.isEmpty())
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("PJSIP INVITE: %1 custom headers injected (emergency)")
                        .arg(opts.customHeaders.size()));

            // Attach PIDF-LO as multipart/mixed part (emergency path).
            // PJSIP merges the auto-generated SDP body with parts in
            // multipartParts — SDP is NOT replaced; it is prepended by PJSIP.
            if (opts.emergencyCall && !opts.body.isEmpty()) {
                pj::SipMultipartPart pidfPart;
                pidfPart.contentType.type    = "application";
                pidfPart.contentType.subType = "pidf+xml";
                pidfPart.body                = opts.body.toStdString();
                if (!opts.contentId.isEmpty()) {
                    pj::SipHeader cidHdr;
                    cidHdr.hName  = "Content-ID";
                    cidHdr.hValue = "<" + opts.contentId.toStdString() + ">";
                    pidfPart.headers.push_back(cidHdr);
                }
                prm.txOption.multipartParts.push_back(pidfPart);
                prm.txOption.multipartContentType.type    = "multipart";
                prm.txOption.multipartContentType.subType = "mixed";
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("PJSIP INVITE: PIDF-LO multipart/mixed part attached, contentId=%1")
                        .arg(opts.contentId));
            }

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
            pj::CallOpParam prm(true);
            prm.opt.videoCount = 1;
            prm.opt.textCount  = 1;  // Accept m=text (RFC 4103 T.140) if offered by remote
            applyVideoMediaDirectionIfNeeded(prm.opt);
            prm.statusCode = PJSIP_SC_OK;
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP INVITE answer: RTT m=text accepted (textCount=1)"));
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP INVITE answer video setup: call=%1 videoCount=%2 videoDevCount=%3 defaultCaptureDev=%4 hasCaptureDev=%5")
                    .arg(m_callId)
                    .arg(prm.opt.videoCount)
                    .arg(pjsipVideoDeviceCount())
                    .arg(preferredPjsipVideoCaptureDevice())
                    .arg(hasPjsipVideoCaptureDevice() ? QStringLiteral("true") : QStringLiteral("false")));
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
        QStringLiteral("SIP pause requested: callId=%1 currentState=%2")
            .arg(m_callId, callStateName(m_stateMachine.state())));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        // Log dialog state for transport diagnostics before sending re-INVITE.
        try {
            const pj::CallInfo ci = m_impl->pjCall->getInfo();
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("SIP pause pre-send: operation=Pause callId=%1 "
                               "pjsipCallId=%2 pjsipState=%3 "
                               "remoteUri=%4 localUri=%5 mediaCount=%6")
                    .arg(m_callId)
                    .arg(m_impl->pjCall->getId())
                    .arg(static_cast<int>(ci.state))
                    .arg(QString::fromStdString(ci.remoteUri))
                    .arg(QString::fromStdString(ci.localUri))
                    .arg(static_cast<int>(ci.media.size())));
        } catch (...) {}

        try {
            pj::CallOpParam prm;
            m_impl->pjCall->setHold(prm);
            // setHold() sends a re-INVITE with a=sendonly.  PJSIP stays in
            // CONFIRMED state after the 200 OK — there is no separate PJSIP
            // "held" state.  We transition the Qt SM to Held now (optimistic)
            // and suppress the CONFIRMED callback in onCallState via holdActive.
            m_impl->holdActive = true;
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("SIP pause re-INVITE sent: callId=%1 "
                               "sdpDirection=a=sendonly → transitioning to Held")
                    .arg(m_callId));
            m_stateMachine.tryTransition(CallState::Held,
                                         QStringLiteral("Pause re-INVITE sent"));
            return true;
        } catch (const pj::Error &e) {
            // Pause failed at transport level — UI must NOT transition to Held.
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("Pause/Resume failed due to unsuitable transport: "
                               "callId=%1 pjsipError=%2; UI remains Active")
                    .arg(m_callId, QString::fromStdString(e.reason)));
            return false;
        }
    }
#endif

    m_stateMachine.tryTransition(CallState::Held, QStringLiteral("Paused"));
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
        QStringLiteral("SIP resume requested: callId=%1 currentState=%2")
            .arg(m_callId, callStateName(m_stateMachine.state())));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        // Log dialog state for transport diagnostics and collect media counts.
        bool videoWasActive = m_localVideoAvailable || m_remoteVideoAvailable;
        const bool rttWasActive = m_impl->rttMediaActive;
        try {
            const pj::CallInfo ci = m_impl->pjCall->getInfo();
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("SIP resume pre-send: operation=Resume callId=%1 "
                               "pjsipCallId=%2 pjsipState=%3 "
                               "remoteUri=%4 localUri=%5 mediaCount=%6 "
                               "videoWasActive=%7 rttWasActive=%8")
                    .arg(m_callId)
                    .arg(m_impl->pjCall->getId())
                    .arg(static_cast<int>(ci.state))
                    .arg(QString::fromStdString(ci.remoteUri))
                    .arg(QString::fromStdString(ci.localUri))
                    .arg(static_cast<int>(ci.media.size()))
                    .arg(videoWasActive ? QStringLiteral("yes") : QStringLiteral("no"))
                    .arg(rttWasActive   ? QStringLiteral("yes") : QStringLiteral("no")));
            for (const auto &mi : ci.media) {
                if (mi.type == PJMEDIA_TYPE_VIDEO && mi.status == PJSUA_CALL_MEDIA_ACTIVE)
                    videoWasActive = true;
            }
        } catch (...) {}

        try {
            // Explicit media counts prevent PJSIP from generating empty SDP
            // (0 m= lines) which causes PJSIP_ETPNOTSUITABLE and a failed re-INVITE.
            // audioCount=1 keeps the audio stream; video/text preserve their current state.
            pj::CallOpParam prm(true);
            prm.opt.audioCount = 1;
            prm.opt.videoCount = videoWasActive ? 1 : 0;
            prm.opt.textCount  = rttWasActive   ? 1 : 0;
            prm.opt.mediaDir = {
                PJMEDIA_DIR_ENCODING_DECODING, // audio: sendrecv
                prm.opt.videoCount > 0 ? PJMEDIA_DIR_DECODING : PJMEDIA_DIR_NONE,
                prm.opt.textCount  > 0 ? PJMEDIA_DIR_ENCODING_DECODING : PJMEDIA_DIR_NONE
            };
            if (prm.opt.videoCount > 0)
                applyVideoMediaDirectionIfNeeded(prm.opt);
            prm.opt.flag |= PJSUA_CALL_UNHOLD;

            // Clear holdActive before reinvite so the CONFIRMED callback
            // (arriving when the remote answers the unhold re-INVITE) is no
            // longer suppressed and transitions the SM back to Active.
            m_impl->holdActive = false;
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("SIP resume re-INVITE: callId=%1 "
                               "audioCount=1 videoCount=%2 textCount=%3 "
                               "sdpDirection=a=sendrecv — waiting for PJSIP CONFIRMED")
                    .arg(m_callId)
                    .arg(prm.opt.videoCount)
                    .arg(prm.opt.textCount));
            m_impl->pjCall->reinvite(prm);
            m_stateMachine.tryTransition(CallState::Active, QStringLiteral("Call resumed"));
            return true;
        } catch (const pj::Error &e) {
            // Restore holdActive because the reinvite did not go out.
            m_impl->holdActive = true;
            // Resume failed at transport level — UI must NOT transition to Active.
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("Pause/Resume failed due to unsuitable transport: "
                               "callId=%1 pjsipError=%2; UI remains Held")
                    .arg(m_callId, QString::fromStdString(e.reason)));
            return false;
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
            adm.getCaptureDevMedia().adjustTxLevel(muted ? 0.0f : (m_micVolume / 100.0f));
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

bool SipCall::setMicVolume(int percent)
{
    percent = qBound(0, percent, 100);
    m_micVolume = percent;

#ifdef HAVE_PJSIP
    if (m_impl->pjCall && m_impl->callAudioMedia && !m_muted) {
        try {
            pj::AudDevManager &adm = pj::Endpoint::instance().audDevManager();
            adm.getCaptureDevMedia().adjustTxLevel(percent / 100.0f);
        } catch (...) {}
    }
#endif
    return true;
}

int SipCall::micVolume() const
{
    return m_micVolume;
}

bool SipCall::setSpeakerVolume(int percent)
{
    percent = qBound(0, percent, 100);
    m_speakerVolume = percent;

#ifdef HAVE_PJSIP
    if (m_impl->pjCall && m_impl->callAudioMedia) {
        try {
            pj::AudDevManager &adm = pj::Endpoint::instance().audDevManager();
            adm.getPlaybackDevMedia().adjustRxLevel(percent / 100.0f);
        } catch (...) {}
    }
#endif
    return true;
}

int SipCall::speakerVolume() const
{
    return m_speakerVolume;
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
        if (!m_impl->callVideoMedia) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Video mute ignored: no active PJSIP video stream (call id=%1)")
                    .arg(m_callId));
        } else {
            try {
                pj::CallVidSetStreamParam prm;
                prm.medIdx = -1; // default video stream
                if (muted) {
                    m_impl->pjCall->vidSetStream(PJSUA_CALL_VID_STRM_STOP_TRANSMIT, prm);
                    m_impl->pauseCapture();
                } else {
                    m_impl->resumeCapture();
                    m_impl->pjCall->vidSetStream(PJSUA_CALL_VID_STRM_START_TRANSMIT, prm);
                }
            } catch (const pj::Error &e) {
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("vidSetStream error (call id=%1): %2")
                        .arg(m_callId, QString::fromStdString(e.reason)));
            } catch (...) {}
        }
    }
#endif

    emit videoMuteChanged(muted);
    return true;
}

bool SipCall::requestVideo(bool enabled)
{
    const CallState state = m_stateMachine.state();
    if (state != CallState::Active && state != CallState::Held && state != CallState::Connecting) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("requestVideo() rejected: call not negotiable (state=%1)")
                .arg(callStateName(state)));
        return false;
    }

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Request video %1: call id=%2")
            .arg(enabled ? QStringLiteral("ON") : QStringLiteral("OFF"), m_callId));

    auto logMediaSummary = [this](const QString &phase) {
#if defined(HAVE_PJSIP)
        if (!m_impl || !m_impl->pjCall)
            return;
        try {
            const pj::CallInfo ci = m_impl->pjCall->getInfo();
            bool audioActive = false;
            bool videoActive = false;
            bool textActive = false;
            QString mediaState;
            for (const auto &mi : ci.media) {
                const bool active = (mi.status == PJSUA_CALL_MEDIA_ACTIVE);
                switch (mi.type) {
                case PJMEDIA_TYPE_AUDIO: audioActive |= active; break;
                case PJMEDIA_TYPE_VIDEO: videoActive |= active; break;
                case PJMEDIA_TYPE_TEXT:  textActive  |= active; break;
                default: break;
                }
                if (!mediaState.isEmpty())
                    mediaState += QStringLiteral(", ");
                mediaState += QStringLiteral("%1:%2")
                                  .arg(static_cast<int>(mi.type))
                                  .arg(static_cast<int>(mi.status));
            }

            const VideoSettings vs = VideoQualityManager::instance().current();
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("%1 summary: audio=%2 video=%3 rtt=%4 codecPriorities=%5 media=[%6]")
                    .arg(phase)
                    .arg(audioActive ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                    .arg(videoActive ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                    .arg(textActive ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                    .arg(vs.codecOrder.join(QStringLiteral(",")))
                    .arg(mediaState.isEmpty() ? QStringLiteral("(none)") : mediaState));
        } catch (const pj::Error &e) {
            if (isSessionTerminatedError(e)) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("%1 summary unavailable: call already terminated")
                        .arg(phase));
            } else {
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("%1 summary unavailable: %2")
                        .arg(phase, QString::fromStdString(e.reason)));
            }
        } catch (...) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("%1 summary unavailable: unknown error").arg(phase));
        }
#else
        Q_UNUSED(phase)
#endif
    };

    logMediaSummary(QStringLiteral("Before Request Video"));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            bool textActive = false;
            try {
                const pj::CallInfo ci = m_impl->pjCall->getInfo();
                for (const auto &mi : ci.media) {
                    if (mi.type == PJMEDIA_TYPE_TEXT
                        && mi.status == PJSUA_CALL_MEDIA_ACTIVE) {
                        textActive = true;
                        break;
                    }
                }
            } catch (...) {}

            pj::CallOpParam prm(true);
            prm.opt.videoCount = enabled ? 1 : 0;
            prm.opt.textCount = textActive ? 1 : 0;
            if (!enabled) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Request Video OFF ignored to preserve remote video stream; "
                                   "no re-INVITE sent"));
                logMediaSummary(QStringLiteral("After Request Video"));
                return true;
            }
            if (enabled) {
                applyVideoMediaDirectionIfNeeded(prm.opt);
                m_impl->videoRequestPendingLocal = true;
                if (m_impl->videoRequestNotified) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("Accepting pending incoming video request: callId=%1")
                            .arg(m_callId));
                    m_impl->videoRequestNotified = false;
                }
                const bool captureAvail = hasPjsipVideoCaptureDevice();
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Video negotiation started: callId=%1 "
                                   "videoCount=%2 pjsipVideoDevCount=%3 "
                                   "captureAvailable=%4 "
                                   "videoMediaDir=ENCODING_DECODING(sendrecv) textCount=%5")
                        .arg(m_callId)
                        .arg(prm.opt.videoCount)
                        .arg(pjsipVideoDeviceCount())
                        .arg(captureAvail ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(prm.opt.textCount));
                if (!captureAvail) {
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("PJSIP video capture backend not active; "
                                   "outgoing video may be unavailable if the selected camera "
                                   "cannot be opened by PJSIP"));
                }
            }
            m_impl->pjCall->reinvite(prm);
            logMediaSummary(QStringLiteral("After Request Video"));
            return true;
        } catch (const pj::Error &e) {
            if (isSessionTerminatedError(e)) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("requestVideo() ignored: call already terminated during teardown"));
                return false;
            }
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("requestVideo() PJSIP error: %1")
                    .arg(QString::fromStdString(e.reason)));
            m_impl->videoRequestPendingLocal = false;
            return false;
        }
    }
#endif

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Request video will apply on next call (stub or backend unavailable)"));
    return true;
}

bool SipCall::requestRtt(bool enabled)
{
    const CallState state = m_stateMachine.state();
    if (state != CallState::Active && state != CallState::Held && state != CallState::Connecting) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("requestRtt() rejected: call not negotiable (state=%1)")
                .arg(callStateName(state)));
        return false;
    }

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Request RTT %1: call id=%2")
            .arg(enabled ? QStringLiteral("ON") : QStringLiteral("OFF"), m_callId));

    auto logMediaSummary = [this](const QString &phase) {
#if defined(HAVE_PJSIP)
        if (!m_impl || !m_impl->pjCall)
            return;
        try {
            const pj::CallInfo ci = m_impl->pjCall->getInfo();
            bool audioActive = false;
            bool videoActive = false;
            bool textActive = false;
            QString mediaState;
            for (const auto &mi : ci.media) {
                const bool active = (mi.status == PJSUA_CALL_MEDIA_ACTIVE);
                switch (mi.type) {
                case PJMEDIA_TYPE_AUDIO: audioActive |= active; break;
                case PJMEDIA_TYPE_VIDEO: videoActive |= active; break;
                case PJMEDIA_TYPE_TEXT:  textActive  |= active; break;
                default: break;
                }
                if (!mediaState.isEmpty())
                    mediaState += QStringLiteral(", ");
                mediaState += QStringLiteral("%1:%2")
                                  .arg(static_cast<int>(mi.type))
                                  .arg(static_cast<int>(mi.status));
            }

            const VideoSettings vs = VideoQualityManager::instance().current();
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("%1 summary: audio=%2 video=%3 rtt=%4 codecPriorities=%5 media=[%6]")
                    .arg(phase)
                    .arg(audioActive ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                    .arg(videoActive ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                    .arg(textActive ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                    .arg(vs.codecOrder.join(QStringLiteral(",")))
                    .arg(mediaState.isEmpty() ? QStringLiteral("(none)") : mediaState));
        } catch (const pj::Error &e) {
            if (isSessionTerminatedError(e)) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("%1 summary unavailable: call already terminated")
                        .arg(phase));
            } else {
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("%1 summary unavailable: %2")
                        .arg(phase, QString::fromStdString(e.reason)));
            }
        } catch (...) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("%1 summary unavailable: unknown error").arg(phase));
        }
#else
        Q_UNUSED(phase)
#endif
    };

    logMediaSummary(QStringLiteral("Before Request RTT"));

#ifdef HAVE_PJSIP
    if (m_impl->pjCall) {
        try {
            bool videoActive = false;
            try {
                const pj::CallInfo ci = m_impl->pjCall->getInfo();
                for (const auto &mi : ci.media) {
                    if (mi.type == PJMEDIA_TYPE_VIDEO
                        && mi.status == PJSUA_CALL_MEDIA_ACTIVE) {
                        videoActive = true;
                        break;
                    }
                }
            } catch (...) {}

            pj::CallOpParam prm(true);
            prm.opt.videoCount = videoActive ? 1 : 0;
            prm.opt.textCount = enabled ? 1 : 0;
            if (videoActive)
                applyVideoMediaDirectionIfNeeded(prm.opt);
            m_impl->pjCall->reinvite(prm);
            logMediaSummary(QStringLiteral("After Request RTT"));
            return true;
        } catch (const pj::Error &e) {
            if (isSessionTerminatedError(e)) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("requestRtt() ignored: call already terminated during teardown"));
                return false;
            }
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("requestRtt() PJSIP error: %1")
                    .arg(QString::fromStdString(e.reason)));
            return false;
        }
    }
#endif

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT request will apply on next call (stub or backend unavailable)"));
    return true;
}

bool SipCall::isVideoMuted()           const { return m_videoMuted; }
bool SipCall::isLocalVideoAvailable()  const { return m_localVideoAvailable; }
bool SipCall::isRemoteVideoAvailable() const { return m_remoteVideoAvailable; }

RtpStatsSnapshot SipCall::mediaRtpStats() const
{
    RtpStatsSnapshot snap;
    snap.source = QStringLiteral("pjsua2 Call::getStreamStat");

#ifdef HAVE_PJSIP
    if (!m_impl || !m_impl->pjCall) {
        snap.reason = QStringLiteral("No active call");
        return snap;
    }
    if (isTeardownState(m_stateMachine.state())) {
        snap.reason = QStringLiteral("Call teardown in progress");
        return snap;
    }

    try {
        const pj::CallInfo ci = m_impl->pjCall->getInfo();
        int activeStreamIndex = -1;
        pjmedia_type activeStreamType = PJMEDIA_TYPE_NONE;

        for (const auto &mi : ci.media) {
            if (mi.status != PJSUA_CALL_MEDIA_ACTIVE)
                continue;
            if (mi.type == PJMEDIA_TYPE_AUDIO) {
                activeStreamIndex = static_cast<int>(mi.index);
                activeStreamType = mi.type;
                break;
            }
            if (activeStreamIndex < 0 && mi.type == PJMEDIA_TYPE_VIDEO) {
                activeStreamIndex = static_cast<int>(mi.index);
                activeStreamType = mi.type;
            }
        }

        if (activeStreamIndex < 0) {
            snap.reason = QStringLiteral("No active RTP stream");
            return snap;
        }

        pj::StreamStat stat = m_impl->pjCall->getStreamStat(static_cast<unsigned>(activeStreamIndex));
        const auto rxTotal = stat.rtcp.rxStat.pkt + stat.rtcp.rxStat.loss;

        snap.available = true;
        snap.streamIndex = activeStreamIndex;
        snap.streamType = mediaTypeName(activeStreamType);
        snap.reason = QStringLiteral("PJSIP RTCP stats available");

        if (stat.rtcp.rxStat.jitterUsec.n > 0 && stat.rtcp.rxStat.jitterUsec.mean >= 0) {
            snap.jitterAvailable = true;
            snap.jitterMs = static_cast<double>(stat.rtcp.rxStat.jitterUsec.mean) / 1000.0;
        }

        if (rxTotal > 0) {
            snap.packetLossAvailable = true;
            snap.packetReceivedPackets = stat.rtcp.rxStat.pkt;
            snap.packetLossPackets = stat.rtcp.rxStat.loss;
            snap.packetLossPercent = static_cast<double>(stat.rtcp.rxStat.loss) * 100.0
                / static_cast<double>(rxTotal);
        }

        if (stat.rtcp.rttUsec.n > 0 && stat.rtcp.rttUsec.mean >= 0) {
            snap.rttAvailable = true;
            snap.rttMs = static_cast<double>(stat.rtcp.rttUsec.mean) / 1000.0;
        }
        return snap;
    } catch (const pj::Error &e) {
        snap.reason = QStringLiteral("PJSIP stream statistics unavailable: %1")
                          .arg(QString::fromStdString(e.reason));
    } catch (...) {
        snap.reason = QStringLiteral("PJSIP stream statistics unavailable");
    }
#else
    snap.reason = QStringLiteral("PJSIP backend unavailable");
#endif
    return snap;
}

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
#if defined(HAVE_PJSIP) && defined(_WIN32)
    if (!m_impl)
        return;

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("attachVideoWindows: remoteHwnd=0x%1 localHwnd=0x%2 "
                       "videoWinId=%3 capDev=%4")
            .arg(static_cast<quintptr>(remoteWidget), 0, 16)
            .arg(static_cast<quintptr>(localPreview), 0, 16)
            .arg(m_impl->videoIncomingWinId)
            .arg(m_impl->videoCapDev));

    // Helper: build a pjmedia_vid_dev_hwnd pointing to a Win32 HWND.
    auto makeWinHwnd = [](HWND hwnd) -> pjmedia_vid_dev_hwnd {
        pjmedia_vid_dev_hwnd h{};
        h.type          = PJMEDIA_VID_DEV_HWND_TYPE_WINDOWS;
        h.info.win.hwnd = hwnd;
        return h;
    };

    // --- Remote incoming video -----------------------------------------------
    auto isValidIncomingWinId = [](pjsua_vid_win_id id) {
        return id != PJSUA_INVALID_ID;
    };

    // Re-fetch the window ID from live call info when the stored ID is still
    // invalid. PJSIP assigns the incoming render window lazily and the first
    // onCallMediaState callback may return -1 or 0 depending on timing.
    if (!isValidIncomingWinId(m_impl->videoIncomingWinId) && m_impl->pjCall) {
        if (isTeardownState(m_stateMachine.state()))
            return;
        try {
            pj::CallInfo ci = m_impl->pjCall->getInfo();
            for (const auto &mi : ci.media) {
                if (mi.type == PJMEDIA_TYPE_VIDEO
                    && mi.status == PJSUA_CALL_MEDIA_ACTIVE
                    && isValidIncomingWinId(mi.videoIncomingWindowId)) {
                    m_impl->videoIncomingWinId =
                        static_cast<pjsua_vid_win_id>(mi.videoIncomingWindowId);
                    Logger::instance().info(LogCategory::Media,
                        QStringLiteral("Remote video winId re-fetched from live call: %1")
                            .arg(m_impl->videoIncomingWinId));
                    break;
                }
            }
        } catch (...) {}
    }

    if (isValidIncomingWinId(m_impl->videoIncomingWinId) && remoteWidget != 0) {
        HWND qtHwnd = reinterpret_cast<HWND>(static_cast<quintptr>(remoteWidget));
        pjsua_vid_win_info wi;
        pj_bzero(&wi, sizeof(wi));
        pj_status_t st = pjsua_vid_win_get_info(m_impl->videoIncomingWinId, &wi);
        if (st == PJ_SUCCESS && wi.is_native) {
            HWND nativeHwnd = reinterpret_cast<HWND>(wi.hwnd.info.win.hwnd);
            if (nativeHwnd) {
                SetParent(nativeHwnd, qtHwnd);
                ShowWindow(nativeHwnd, SW_SHOW);
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Remote native video HWND parented to Qt widget: winId=%1 nativeHwnd=0x%2")
                        .arg(m_impl->videoIncomingWinId)
                        .arg(reinterpret_cast<quintptr>(nativeHwnd), 0, 16));
            } else {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("Remote video native handle missing: winId=%1")
                        .arg(m_impl->videoIncomingWinId));
            }
        } else {
            pjmedia_vid_dev_hwnd h = makeWinHwnd(qtHwnd);
            st = pjsua_vid_win_set_win(m_impl->videoIncomingWinId, &h);
            if (st == PJ_SUCCESS) {
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Remote video GDI renderer pointed at Qt widget: winId=%1")
                        .arg(m_impl->videoIncomingWinId));
            } else {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("Remote video set_win failed: winId=%1 status=%2")
                        .arg(m_impl->videoIncomingWinId).arg(st));
            }
        }
    } else {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("Remote video attach skipped: videoWinId=%1 remoteWidget=%2")
                .arg(m_impl->videoIncomingWinId)
                .arg(static_cast<quintptr>(remoteWidget)));
    }

    // --- Local preview -------------------------------------------------------
    if (m_impl->videoCapDev >= PJMEDIA_VID_DEFAULT_CAPTURE_DEV && localPreview != 0) {
        const pjmedia_vid_dev_index capDev =
            static_cast<pjmedia_vid_dev_index>(m_impl->videoCapDev);
        HWND qtPreviewHwnd = reinterpret_cast<HWND>(static_cast<quintptr>(localPreview));
        try {
            const pj::VideoDevInfo info = pj::Endpoint::instance().vidDevManager()
                .getDevInfo(static_cast<int>(capDev));
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Local preview device info: index=%1 name='%2' driver='%3' dir=%4 caps=0x%5")
                    .arg(capDev)
                    .arg(QString::fromStdString(info.name))
                    .arg(QString::fromStdString(info.driver))
                    .arg(static_cast<int>(info.dir))
                    .arg(QString::number(static_cast<qulonglong>(info.caps), 16)));
        } catch (...) {}

        const pjmedia_vid_dev_index gdiRenderDev = PjsipGdiRenderer::deviceIndex();
        const pjmedia_vid_dev_index renderDev =
            (gdiRenderDev != PJMEDIA_VID_INVALID_DEV)
                ? gdiRenderDev
                : PJMEDIA_VID_DEFAULT_RENDER_DEV;

        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Local preview device selection: capDev=%1 gdiRenderDev=%2 renderDev=%3")
                .arg(m_impl->videoCapDev)
                .arg(gdiRenderDev)
                .arg(renderDev));

        pjsua_vid_win_id previewWinId = pjsua_vid_preview_get_win(capDev);

        if (previewWinId == PJSUA_INVALID_ID) {
            // Start preview with our GDI renderer and point it at the Qt widget.
            pjsua_vid_preview_param pvp;
            pjsua_vid_preview_param_default(&pvp);
            pvp.show    = PJ_FALSE;
            pvp.rend_id = renderDev;
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Local preview params: capDev=%1 rendId=%2 hwnd=0x%3")
                    .arg(capDev)
                    .arg(pvp.rend_id)
                    .arg(reinterpret_cast<quintptr>(qtPreviewHwnd), 0, 16));
            pj_status_t st = pjsua_vid_preview_start(capDev, &pvp);
            if (st == PJ_SUCCESS) {
                previewWinId = pjsua_vid_preview_get_win(capDev);
                if (renderDev != PJMEDIA_VID_DEFAULT_RENDER_DEV) {
                    pjmedia_vid_dev_hwnd h = makeWinHwnd(qtPreviewHwnd);
                    const pj_status_t bindSt = pjsua_vid_win_set_win(previewWinId, &h);
                    if (bindSt == PJ_SUCCESS) {
                        Logger::instance().info(LogCategory::Media,
                            QStringLiteral("Local preview embedded: capDev=%1 previewWinId=%2 hwnd=0x%3")
                                .arg(m_impl->videoCapDev)
                                .arg(previewWinId)
                                .arg(reinterpret_cast<quintptr>(qtPreviewHwnd), 0, 16));
                    } else {
                        Logger::instance().warn(LogCategory::Media,
                            QStringLiteral("Local preview embed failed: capDev=%1 winId=%2 status=%3 (%4)")
                                .arg(m_impl->videoCapDev)
                                .arg(previewWinId)
                                .arg(bindSt)
                                .arg(pjsipStatusText(bindSt)));
                    }
                }
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Local preview started: capDev=%1 previewWinId=%2")
                        .arg(m_impl->videoCapDev).arg(previewWinId));
            } else {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("Local preview start failed: capDev=%1 status=%2 (%3)")
                        .arg(m_impl->videoCapDev)
                        .arg(st)
                        .arg(pjsipStatusText(st)));
            }
        } else {
            // Preview already running — redirect its render target to our widget.
            pjmedia_vid_dev_hwnd h = makeWinHwnd(qtPreviewHwnd);
            pj_status_t st = PJ_SUCCESS;
            if (st == PJ_SUCCESS) {
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Local preview GDI renderer pointed at Qt widget: "
                                   "capDev=%1 previewWinId=%2")
                        .arg(m_impl->videoCapDev).arg(previewWinId));
            } else {
                // The preview was started with a non-GDI renderer (e.g. Null).
                // Stop it and restart with the GDI renderer.
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Local preview renderer not GDI (status=%1); "
                                   "restarting with GDI renderer").arg(st));
                pjsua_vid_preview_stop(capDev);
                previewWinId = PJSUA_INVALID_ID;

                pjsua_vid_preview_param pvp;
                pjsua_vid_preview_param_default(&pvp);
                pvp.show    = PJ_FALSE;
                pvp.rend_id = PJMEDIA_VID_DEFAULT_RENDER_DEV;
                pvp.wnd = makeWinHwnd(qtPreviewHwnd);
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Local preview restart params: capDev=%1 rendId=%2 hwnd=0x%3")
                        .arg(capDev)
                        .arg(pvp.rend_id)
                        .arg(reinterpret_cast<quintptr>(qtPreviewHwnd), 0, 16));
                pj_status_t st = pjsua_vid_preview_start(capDev, &pvp);
                if (st == PJ_SUCCESS) {
                    previewWinId = pjsua_vid_preview_get_win(capDev);
                    Logger::instance().info(LogCategory::Media,
                        QStringLiteral("Local preview restarted with GDI renderer: "
                                       "capDev=%1 previewWinId=%2")
                            .arg(m_impl->videoCapDev).arg(previewWinId));
                } else {
                    Logger::instance().warn(LogCategory::Media,
                        QStringLiteral("Local preview restart failed: capDev=%1 status=%2 (%3)")
                            .arg(m_impl->videoCapDev)
                            .arg(st)
                            .arg(pjsipStatusText(st)));
                }
            }
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
    m_negotiatedAudioCodec.clear();
    m_stateMachine.reset(reason);
}

CallState SipCall::state()      const { return m_stateMachine.state(); }
QString   SipCall::statusText() const { return m_stateMachine.statusText(); }
QString   SipCall::remoteUri()  const { return m_remoteUri; }
QString   SipCall::callId()     const { return m_callId; }

QString SipCall::negotiatedAudioCodec() const { return m_negotiatedAudioCodec; }

QString SipCall::formatAudioCodecSummary(const QString &name, int clockRateHz, int payloadType)
{
    return QStringLiteral("%1/%2 pt=%3").arg(name).arg(clockRateHz).arg(payloadType);
}

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
    if (isTeardownState(m_stateMachine.state()))
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

void SipCall::sendRttText(const QString &text)
{
#ifdef HAVE_PJSIP
    if (!m_impl || !m_impl->pjCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("RTT sendRttText: no active PJSIP call"));
        return;
    }
    try {
        pj::CallSendTextParam prm;
        prm.medIdx = -1;  // first text stream
        prm.text   = text.toStdString();
        m_impl->pjCall->sendText(prm);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("RTT text sent via RTP T.140: call=%1 text=\"%2\"")
                .arg(m_callId, text));
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("RTT sendRttText failed: call=%1 reason=%2")
                .arg(m_callId, QString::fromStdString(e.reason)));
    }
#else
    Q_UNUSED(text)
    Logger::instance().warn(LogCategory::Sip,
        QStringLiteral("RTT sendRttText: PJSIP not available (stub mode)"));
#endif
}

bool SipCall::sendLocationUpdate(const SipCallOptions &opts)
{
    if (!opts.emergencyCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendLocationUpdate rejected: opts.emergencyCall is false"));
        return false;
    }
    if (m_stateMachine.state() != CallState::Active) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendLocationUpdate rejected: call not Active (state=%1)")
                .arg(callStateName(m_stateMachine.state())));
        return false;
    }

#ifdef HAVE_PJSIP
    if (!m_impl || !m_impl->pjCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendLocationUpdate: no PJSIP call object"));
        return false;
    }
    try {
        pj::CallOpParam prm;
        for (const auto &hdr : opts.customHeaders) {
            pj::SipHeader sh;
            sh.hName  = hdr.first.toStdString();
            sh.hValue = hdr.second.toStdString();
            prm.txOption.headers.push_back(sh);
        }
        if (!opts.body.isEmpty()) {
            pj::SipMultipartPart pidfPart;
            pidfPart.contentType.type    = "application";
            pidfPart.contentType.subType = "pidf+xml";
            pidfPart.body                = opts.body.toStdString();
            if (!opts.contentId.isEmpty()) {
                pj::SipHeader cidHdr;
                cidHdr.hName  = "Content-ID";
                cidHdr.hValue = "<" + opts.contentId.toStdString() + ">";
                pidfPart.headers.push_back(cidHdr);
            }
            prm.txOption.multipartParts.push_back(pidfPart);
            prm.txOption.multipartContentType.type    = "multipart";
            prm.txOption.multipartContentType.subType = "mixed";
        }
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("[EMERGENCY] SIP UPDATE with PIDF-LO: call=%1 contentId=%2")
                .arg(m_callId, opts.contentId));
        m_impl->pjCall->update(prm);
        return true;
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendLocationUpdate PJSIP error: call=%1 reason=%2")
                .arg(m_callId, QString::fromStdString(e.reason)));
        return false;
    }
#else
    Q_UNUSED(opts)
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("sendLocationUpdate: PJSIP not available — "
                       "UPDATE not sent (stub mode limitation)"));
    return false;
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
