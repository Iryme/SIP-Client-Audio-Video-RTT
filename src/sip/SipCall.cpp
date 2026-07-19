#include "SipCall.h"

#include <algorithm>

#include <QPointer>
#include <QUuid>

#include "core/Logger.h"
#include "core/AppSettings.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoQualityManager.h"
#include "media/RtpStats.h"
#include "msrp/MsrpSipIntegration.h"
#include "msrp/MsrpSipMediaInjector.h"
#include "msrp/MsrpSession.h"
#include "msrp/MsrpPath.h"
#include "msrp/MsrpSdpNegotiator.h"
#include "msrp/MsrpCallPreparation.h"
#include "msrp/MsrpRelayClient.h"
#include "msrp/MsrpTypes.h"
#include "sip/RtpPortDiagnostics.h"
#include "sip/VideoStreamGuard.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <pjsua-lib/pjsua.h>
#include <pj/errno.h>
#include <pj/pool.h>
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

// Security audit (2026-07-13): SipCallOptions::customHeaders is currently
// only ever populated with fixed literals or internally generated content
// (EmergencyInviteBuilder/EmergencyCallAdapter — never raw user text), but
// nothing on this path guarded against an embedded CRLF the way MSRP's own
// headerValueSafe() does for the same class of risk. A header value
// containing "\r\n" would inject an extra SIP header (or split the INVITE)
// once handed to pjsip. Reject rather than silently strip, so a caller
// mistake is visible in the log instead of producing a subtly wrong
// request.
static bool sipHeaderValueSafe(const QString &value)
{
    for (const QChar &c : value) {
        if (c == QLatin1Char('\r') || c == QLatin1Char('\n') || c == QLatin1Char('\0'))
            return false;
    }
    return true;
}

static void appendCustomHeaders(pj::CallOpParam &prm, const SipCallOptions &opts, const QString &callId)
{
    for (const auto &hdr : opts.customHeaders) {
        if (!sipHeaderValueSafe(hdr.first) || !sipHeaderValueSafe(hdr.second)) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("Rejected custom SIP header with embedded CR/LF/NUL "
                               "(possible header injection): callId=%1 name=%2")
                    .arg(callId, hdr.first));
            continue;
        }
        pj::SipHeader sh;
        sh.hName  = hdr.first.toStdString();
        sh.hValue = hdr.second.toStdString();
        prm.txOption.headers.push_back(sh);
    }
}

// Task W102 Phase 2: extracts every "m=message" section from a live/remote
// pjmedia_sdp_session, paired with its media index, reusing the already
// vetted MsrpSdpNegotiator text parser (per-section text obtained via the
// public pjmedia_sdp_media_print — no re-implementation of SDP attribute
// parsing, no vendor edit). Used against prm.remSdp when this client is
// answering a peer-initiated offer.
struct IndexedMessageBlock
{
    int index{-1};
    MsrpSdpNegotiator::MediaBlock block;
};

static QList<IndexedMessageBlock> extractRemoteMessageBlocks(const pjmedia_sdp_session *sdp)
{
    QList<IndexedMessageBlock> result;
    if (!sdp)
        return result;
    char buf[4096];
    for (unsigned i = 0; i < sdp->media_count; ++i) {
        const pjmedia_sdp_media *m = sdp->media[i];
        if (!m || pj_stricmp2(&m->desc.media, "message") != 0)
            continue;
        const int len = pjmedia_sdp_media_print(m, buf, sizeof(buf));
        if (len <= 0)
            continue;
        const QString text = QString::fromLatin1(buf, len);
        const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(text);
        if (blocks.isEmpty())
            continue;
        IndexedMessageBlock entry;
        entry.index = static_cast<int>(i);
        entry.block = blocks.first();
        result.append(entry);
    }
    return result;
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
                // Held has no direct path to Failed (only Active/Disconnecting);
                // without this reroute a dialog that dies while on hold (e.g. a
                // dropped transport or a re-INVITE timeout) gets stuck at Held
                // forever, since tryTransition(Failed) below would be rejected.
                if (newState == CallState::Failed && cur == CallState::Held) {
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
                            // Honour an active mute: reconnecting the mic here
                            // would silently unmute the call on every
                            // renegotiation (hold/resume, video re-INVITE).
                            if (!m_impl->q->m_muted)
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
                                    AudioCodecInfo info;
                                    info.name = encName;
                                    info.clockRate = static_cast<int>(fmt.clock_rate);
                                    info.payloadType = static_cast<int>(fmt.pt);
                                    info.channels = static_cast<int>(fmt.channel_cnt);
                                    if (si.info.aud.param) {
                                        const pjmedia_codec_param *p = si.info.aud.param;
                                        info.ptime = static_cast<int>(p->info.frm_ptime)
                                                   * static_cast<int>(p->setting.frm_per_pkt);
                                        info.bitrate = static_cast<int>(p->info.avg_bps);
                                    }
                                    info.negotiated = true;
                                    m_impl->q->m_negotiatedAudioCodecInfo = info;
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
                        // !videoMediaActive: only notify on the inactive→active
                        // transition. Once video is running, every later media
                        // callback (e.g. triggered by an unrelated RTT
                        // re-INVITE) re-enters this branch with both flags
                        // cleared and used to re-emit videoRequested() — a
                        // spurious "peer requests video" popup that, because
                        // MediaRequestDialog is a single shared dialog, could
                        // overwrite a genuine pending RTT prompt milliseconds
                        // after it appeared (seen live: RTT prompt at .481
                        // replaced by a fake video prompt at .494).
                        if (!localRequestPending && !m_impl->videoRequestNotified
                            && !m_impl->videoMediaActive) {
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
                                if (m_impl->q) {
                                    VideoCodecInfo info;
                                    info.name = encName;
                                    info.payloadType = static_cast<int>(vfmt.pt);
                                    if (si.info.vid.codec_param) {
                                        const pjmedia_vid_codec_param *p = si.info.vid.codec_param;
                                        const pjmedia_video_format_detail &det = p->enc_fmt.det.vid;
                                        info.width = static_cast<int>(det.size.w);
                                        info.height = static_cast<int>(det.size.h);
                                        if (det.fps.denum > 0)
                                            info.fps = static_cast<int>(det.fps.num / det.fps.denum);
                                        info.bitrate = static_cast<int>(det.avg_bps);
                                    }
                                    info.negotiated = true;
                                    m_impl->q->m_negotiatedVideoCodecInfo = info;
                                }
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
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("RTT media stream status: index=%1 status=%2 direction=%3")
                            .arg(mi.index)
                            .arg(static_cast<int>(mi.status))
                            .arg(static_cast<int>(mi.dir)));
                    if (mi.status == PJSUA_CALL_MEDIA_ACTIVE) {
                        textMediaActive = true;
                        m_impl->rttRequestPendingLocal = false;
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
                        // Do NOT clear rttRequestPendingLocal / declare failure
                        // here: Model B (reject-then-renegotiate, see
                        // docs/rtt-offer-answer-state-machine.md) means the
                        // *first* response to our own offer is always m=text 0
                        // — the peer's real answer only arrives later, as its
                        // own separate accept-triggered re-INVITE, once their
                        // user clicks Accept. Treating this expected first
                        // bounce as a terminal failure gave up before the peer
                        // could ever respond, and (combined with the guard
                        // above) produced an endless Request/Accept ping-pong.
                        // RttSession's own negotiation timeout (12s,
                        // cancelPendingRttRequest()) is the sole give-up path.
                    }
                }
            }

            QPointer<SipCall> self = m_impl->q;
            const bool prevRttActive = m_impl->rttMediaActive;
            m_impl->rttMediaActive = textMediaActive;
            m_impl->videoMediaActive = videoActive;
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
                        QStringLiteral("RTT negotiated active: protocol=RTT"));
                    self->m_impl->rttRequestNotified = false; // RTT accepted — reset pending flag
                    emit self->rttMediaConnected();
                } else if (!textMediaActive && prevRttActive) {
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("RTT inactive/rejected/withdrawn: rttMediaDisconnected"));
                    emit self->rttMediaDisconnected();
                }
                // Note: a declined *first* response to our own RTT offer is
                // no longer treated as failure here — see the comment at the
                // PJSUA_CALL_MEDIA_ACTIVE check above. rttRequestPendingLocal
                // stays set until either this callback later sees the text
                // stream active, or RttSession's negotiation timeout gives up
                // via cancelPendingRttRequest().
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
            // Task W101 Phase 2: this call is the SDP *offerer* exactly when
            // no remote offer is present yet — pjsua2 leaves remSdp empty in
            // that case (see OnCallSdpCreatedParam's doc comment). Only the
            // offerer path is wired here (initial INVITE and any re-INVITE/
            // UPDATE this client itself originates); answering a
            // peer-initiated m=message offer is not yet implemented — see
            // docs/msrp-live-sdp-integration.md for why (PJSUA's own
            // handling of media types it doesn't recognize needs a separate
            // audit before an automatic accept/reject splice at the
            // matching index can be trusted not to corrupt the SDP).
            //
            // Bug fix (found via live manual testing, 2026-07-12): pjsua2's
            // pj::SdpSession::pjSdpSession is a plain "void *pjSdpSession;"
            // with no initializer (.deps/pjsip-msvc-install/include/pjsua2/
            // call.hpp) — it is only ever assigned by SdpSession::fromPj(),
            // never zeroed otherwise. On an outgoing call (no remote offer),
            // prm.remSdp is a default-constructed SdpSession: wholeSdp (a
            // std::string) is reliably empty, but pjSdpSession is
            // indeterminate stack garbage, NOT guaranteed null — comparing
            // it to nullptr is undefined behavior and, in this Debug build,
            // reliably false (MSVC fills uninitialized stack with 0xCC,
            // which is never exactly 0). That made isOfferer wrongly false
            // on outgoing calls, and the !isOfferer branch below then
            // dereferenced that garbage pointer via
            // extractRemoteMessageBlocks() — a real, reproducible crash
            // (SIPClient!extractRemoteMessageBlocks, access violation on an
            // 0xCCCCCCCC-pattern pointer) on any outgoing call once MSRP is
            // enabled. wholeSdp.empty() alone is the reliable signal — never
            // trust pjSdpSession's nullness on its own.
            const bool isOfferer = prm.remSdp.wholeSdp.empty();
            if (isOfferer && m_impl
                    && AppSettings::enableMsrp()
                    && (AppSettings::enableMsrpTcp() || AppSettings::enableMsrpTls())) {
                bool alreadyHasMessage = false;
                if (const auto *s = static_cast<const pjmedia_sdp_session *>(
                        prm.sdp.pjSdpSession)) {
                    for (unsigned i = 0; i < s->media_count; ++i) {
                        if (s->media[i] && pj_stricmp2(&s->media[i]->desc.media, "message") == 0) {
                            alreadyHasMessage = true;
                            break;
                        }
                    }
                }

                if (!alreadyHasMessage) {
                    if (!m_impl->msrpSdpPool)
                        m_impl->msrpSdpPool = pjsua_pool_create("msrp-sdp", 2048, 2048);

                    const QStringList acceptTypes = AppSettings::msrpAcceptTypes()
                        .split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    const QStringList acceptWrappedTypes = AppSettings::msrpAcceptWrappedTypes()
                        .split(QLatin1Char(' '), Qt::SkipEmptyParts);

                    int advertisedPort = 0;
                    MsrpUri advertisedUri;

                    // Task W108: a relay allocation obtained *before* this
                    // synchronous callback ran (see MsrpCallPreparationController)
                    // takes precedence — the allocated Use-Path is already real,
                    // wire-confirmed data; no I/O happens here, only reads of
                    // the prepared offer. No local passive listener is bound in
                    // this case (see Phase 9: the relay's own connection is
                    // adopted into msrpSession after the call is created).
                    if (m_impl->preparedMsrpOffer.ready
                            && m_impl->preparedMsrpOffer.mode == MsrpCallTransportMode::Relay
                            && m_impl->preparedMsrpOffer.advertisedUri.ok) {
                        m_impl->createMsrpSessionObject();
                        advertisedUri = m_impl->preparedMsrpOffer.advertisedUri;
                        advertisedPort = advertisedUri.port;
                        m_impl->msrpSession->setLocalUri(advertisedUri);
                        m_impl->msrpLocalUri = advertisedUri;
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("MSRP relay allocation applied to outbound SDP: callId=%1 "
                                           "allocationId=%2")
                                .arg(m_impl->q ? m_impl->q->callId() : QString())
                                .arg(m_impl->preparedMsrpOffer.relayAllocation.allocationId));
                    } else {
                        m_impl->startMsrpPassiveListener();
                        advertisedUri = m_impl->msrpLocalUri;
                        if (m_impl->msrpSession && !m_impl->msrpListenerFailed) {
                            advertisedPort = m_impl->msrpSession->transportLocalPort();
                            QString host = AppSettings::msrpAdvertisedHost();
                            if (host.isEmpty())
                                host = MsrpSipMediaInjector::resolveSessionConnectionHost(prm.sdp.pjSdpSession);
                            if (!host.isEmpty())
                                advertisedUri.host = host;
                        }
                    }

                    const auto injectResult = MsrpSipMediaInjector::injectMessageMedia(
                        prm.sdp.pjSdpSession, m_impl->msrpSdpPool, advertisedUri,
                        MsrpSetup::ActPass, acceptTypes, acceptWrappedTypes, advertisedPort);

                    if (injectResult.injected) {
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("MSRP m=message injected into outbound SDP: callId=%1 port=%2 host=%3")
                                .arg(m_impl->q ? m_impl->q->callId() : QString())
                                .arg(advertisedPort)
                                .arg(advertisedUri.host));

                        // Task W101 Phase 4: SIP<->MSRP dialog mapping — record
                        // the real SIP Call-ID (distinct from this app's internal
                        // callId()) and the media index this m=message section
                        // landed at, so the session can be correlated by more
                        // than IP/port even when a call renegotiates media order.
                        if (m_impl->msrpSession) {
                            if (const auto *s = static_cast<const pjmedia_sdp_session *>(prm.sdp.pjSdpSession))
                                m_impl->msrpSession->setMediaIndex(static_cast<int>(s->media_count) - 1);
                            try {
                                m_impl->msrpSession->setSipHeaderCallId(
                                    QString::fromStdString(getInfo().callIdString));
                            } catch (...) {
                                // getInfo() can throw during teardown races (see
                                // isSessionTerminatedError elsewhere in this
                                // file) — the mapping simply stays unset.
                            }
                        }
                    } else {
                        Logger::instance().warn(LogCategory::Sip,
                            QStringLiteral("MSRP m=message injection failed: callId=%1 reason=%2")
                                .arg(m_impl->q ? m_impl->q->callId() : QString())
                                .arg(injectResult.errorMessage));
                    }
                }
            }

            // Task W102 Phase 2: answer a peer-initiated m=message offer.
            // By this point PJSIP's own SDP negotiator (pjmedia_sdp_neg.c
            // create_answer) has already cloned every offered "message"
            // section into prm.sdp as a rejected (port 0) placeholder at
            // the SAME index (it never finds a local media capability for
            // a type PJSUA's media manager doesn't know about — see
            // MsrpSipMediaInjector::answerMessageMediaAtIndex's comment).
            // We only need to look at prm.remSdp (the offer) to decide
            // whether to keep that rejection or replace it with a real
            // accepted section — audio/video/RTT sections elsewhere in the
            // same SDP are untouched either way, so rejecting or accepting
            // MSRP here never affects the rest of the call.
            // Defense in depth alongside the isOfferer fix above: never
            // trust prm.remSdp.pjSdpSession at all unless wholeSdp is
            // actually non-empty (the only reliable "a real remote SDP was
            // supplied" signal — see the isOfferer comment).
            if (!isOfferer && !prm.remSdp.wholeSdp.empty() && m_impl
                    && AppSettings::enableMsrp()
                    && (AppSettings::enableMsrpTcp() || AppSettings::enableMsrpTls())) {
                const auto *remSdp = static_cast<const pjmedia_sdp_session *>(prm.remSdp.pjSdpSession);
                const auto remoteBlocks = extractRemoteMessageBlocks(remSdp);
                bool answeredOne = m_impl->msrpSession != nullptr; // single-session-per-call model (W101)
                for (const auto &entry : remoteBlocks) {
                    if (entry.block.rejected || entry.block.parseStatus == MsrpParseStatus::Error)
                        continue; // nothing to answer — leave PJSIP's own rejection in place
                    if (answeredOne) {
                        // Task W102 known limitation (documented, not silently
                        // dropped): this client's SipCall owns exactly one live
                        // MsrpSession per call. A second simultaneous
                        // m=message offer in the same SDP is explicitly
                        // rejected (left at port 0) rather than risking two
                        // sessions being multiplexed onto one mapping.
                        Logger::instance().warn(LogCategory::Sip,
                            QStringLiteral("Rejecting additional m=message section (index=%1): "
                                           "only one live MSRP session per call is supported")
                                .arg(entry.index));
                        continue;
                    }

                    if (m_impl->msrpSession) {
                        // Renegotiation (re-INVITE/UPDATE) of an already-accepted
                        // MSRP session: keep the existing session/role, just
                        // re-answer at whatever index this offer placed it at.
                    } else {
                        // Task W102 Phase 3: choose OUR role as the complement
                        // of what the remote proposed — this structurally
                        // rules out active/active and passive/passive
                        // collisions, since we never independently pick a
                        // fixed role; we only ever mirror or default.
                        if (entry.block.setup == MsrpSetup::Active) {
                            m_impl->startMsrpPassiveListener();
                        } else if (entry.block.setup == MsrpSetup::Passive) {
                            if (!m_impl->startMsrpActiveConnect(entry.block.path)) {
                                Logger::instance().warn(LogCategory::Sip,
                                    QStringLiteral("Remote requested MSRP active connect (setup:passive) "
                                                    "but no usable remote path was offered (index=%1)")
                                        .arg(entry.index));
                            }
                        } else {
                            // actpass or missing/unrecognized: default to
                            // passive (documented choice — this client always
                            // has a working passive-listen code path).
                            m_impl->startMsrpPassiveListener();
                        }
                    }

                    if (!m_impl->msrpSession) {
                        continue; // listener/connect failed — leave the port-0 rejection
                    }

                    if (!m_impl->msrpSdpPool)
                        m_impl->msrpSdpPool = pjsua_pool_create("msrp-sdp", 2048, 2048);

                    const QStringList acceptTypes = AppSettings::msrpAcceptTypes()
                        .split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    const QStringList acceptWrappedTypes = AppSettings::msrpAcceptWrappedTypes()
                        .split(QLatin1Char(' '), Qt::SkipEmptyParts);

                    const MsrpSetup ourSetup = (entry.block.setup == MsrpSetup::Active)
                        ? MsrpSetup::Passive
                        : (entry.block.setup == MsrpSetup::Passive ? MsrpSetup::Active : MsrpSetup::Passive);

                    // For the active-connect case the "port" we answer with is
                    // purely informational (see startMsrpActiveConnect); for
                    // passive it's the real bound listener port.
                    const int answerPort = (ourSetup == MsrpSetup::Active)
                        ? (m_impl->msrpLocalUri.port > 0 ? m_impl->msrpLocalUri.port : 2855)
                        : m_impl->msrpSession->transportLocalPort();

                    MsrpUri advertisedUri = m_impl->msrpLocalUri;
                    if (ourSetup == MsrpSetup::Passive) {
                        QString host = AppSettings::msrpAdvertisedHost();
                        if (host.isEmpty())
                            host = MsrpSipMediaInjector::resolveSessionConnectionHost(prm.sdp.pjSdpSession);
                        if (!host.isEmpty())
                            advertisedUri.host = host;
                    }

                    const auto answerResult = MsrpSipMediaInjector::answerMessageMediaAtIndex(
                        prm.sdp.pjSdpSession, m_impl->msrpSdpPool, entry.index,
                        advertisedUri, ourSetup, acceptTypes, acceptWrappedTypes, answerPort);

                    if (answerResult.injected) {
                        answeredOne = true;
                        m_impl->msrpSession->setMediaIndex(entry.index);
                        try {
                            m_impl->msrpSession->setSipHeaderCallId(
                                QString::fromStdString(getInfo().callIdString));
                        } catch (...) {
                        }
                        Logger::instance().info(LogCategory::Sip,
                            QStringLiteral("MSRP m=message answered (peer-initiated offer): callId=%1 "
                                           "index=%2 ourRole=%3 remoteRole=%4")
                                .arg(m_impl->q ? m_impl->q->callId() : QString())
                                .arg(entry.index)
                                .arg(msrpSetupToString(ourSetup))
                                .arg(msrpSetupToString(entry.block.setup)));
                    } else {
                        Logger::instance().warn(LogCategory::Sip,
                            QStringLiteral("MSRP m=message answer injection failed (index=%1): %2")
                                .arg(entry.index)
                                .arg(answerResult.errorMessage));
                    }
                }
            }

            // Extract m= lines from the created SDP offer/answer so callers can
            // verify that m=audio/m=video/m=message are present without needing
            // a live SIP trace capture. Prefer the parsed SDP session (which now
            // reflects our own pjSdpSession-level MSRP injection above) over
            // wholeSdp — wholeSdp is a separate, size-capped text snapshot
            // (PJSUA2_MAX_SDP_BUF_LEN) that our injection deliberately never
            // touches, and is empty in some pjsua2 callback paths anyway.
            QStringList mLines;
            if (const auto *s = static_cast<const pjmedia_sdp_session *>(prm.sdp.pjSdpSession)) {
                for (unsigned i = 0; i < s->media_count; ++i) {
                    const pjmedia_sdp_media *m = s->media[i];
                    if (!m)
                        continue;
                    mLines.append(QStringLiteral("m=%1 %2")
                        .arg(QString::fromLatin1(m->desc.media.ptr,
                                 static_cast<int>(m->desc.media.slen)))
                        .arg(m->desc.port));
                }
            }
            const QString sdp = QString::fromStdString(prm.sdp.wholeSdp);
            if (mLines.isEmpty()) {
                for (const QString &line : sdp.split(QLatin1Char('\n'))) {
                    const QString trimmed = line.trimmed();
                    if (trimmed.startsWith(QLatin1String("m=")))
                        mLines.append(trimmed);
                }
            }
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("SDP offer/answer m= lines (%1): %2")
                    .arg(mLines.size())
                    .arg(mLines.isEmpty() ? QStringLiteral("(none)")
                                          : mLines.join(QStringLiteral(", "))));

            // Task W100 (MSRP Foundation): still-useful read-only detection for
            // the peer's own SDP (e.g. remote-offered m=message on the answer
            // path, which this task does not yet act on — see comment above).
            if (m_impl && m_impl->q)
                MsrpSipIntegration::detectFromSdp(sdp, m_impl->q->callId());
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
                self->m_negotiatedAudioCodecInfo = AudioCodecInfo();
                emit self->audioMediaDisconnected();
            }, Qt::QueuedConnection);
        }

        void stopVideoBridge()
        {
            m_impl->videoMuteAppliedMediaIndex = -1;
            if (!m_impl->callVideoMedia)
                return;
            m_impl->callVideoMedia = nullptr;

            QPointer<SipCall> self = m_impl->q;
            QMetaObject::invokeMethod(self, [self]() {
                if (!self) return;
                self->m_negotiatedVideoCodecInfo = VideoCodecInfo();
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

            // Inspect the parsed SDP session rather than wholeSdp: wholeSdp is
            // empty in some pjsua2 callback paths, and a plain string search
            // cannot tell a real offer from a disabled stream ("m=video 0").
            bool hasVideoOffer = false;
            bool hasTextOffer  = false;
            if (const auto *sdp = static_cast<const pjmedia_sdp_session *>(
                    prm.offer.pjSdpSession)) {
                for (unsigned i = 0; i < sdp->media_count; ++i) {
                    const pjmedia_sdp_media *m = sdp->media[i];
                    if (!m || m->desc.port == 0)
                        continue; // disabled stream — not an offer
                    if (pj_stricmp2(&m->desc.media, "video") == 0)
                        hasVideoOffer = true;
                    else if (pj_stricmp2(&m->desc.media, "text") == 0)
                        hasTextOffer = true;
                }
            } else {
                const QString offerSdp = QString::fromStdString(prm.offer.wholeSdp);
                hasVideoOffer = offerSdp.contains(QStringLiteral("m=video"), Qt::CaseInsensitive);
                hasTextOffer  = offerSdp.contains(QStringLiteral("m=text"),  Qt::CaseInsensitive);
            }
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("re-INVITE offer received: callId=%1 video=%2 text=%3")
                    .arg(getId())
                    .arg(hasVideoOffer ? QStringLiteral("yes") : QStringLiteral("no"))
                    .arg(hasTextOffer  ? QStringLiteral("yes") : QStringLiteral("no")));

            // videoMediaActive/rttMediaActive are only true while PJSIP reports
            // PJSUA_CALL_MEDIA_ACTIVE; a held stream reports LOCAL_HOLD/
            // REMOTE_HOLD instead, so those flags go false for the whole
            // hold→resume window. Query the live (pre-answer) media status
            // here too so a resume re-INVITE — which still carries "m=video"/
            // "m=text" for a stream that was merely held, not withdrawn — is
            // not misread as a brand-new request. This affects BOTH sides of
            // a hold: the side that called hold()/resume() (its cached flags
            // go stale exactly the same way) and the peer, who never called
            // hold() locally and has no other way to know the stream was only
            // held (W110 live two-device follow-up: video consent popup
            // reappeared on resume even though video had never been turned
            // off, on both the resuming side and the peer).
            bool videoActiveOrHeld = false;
            bool textActiveOrHeld  = false;
            if (m_impl->pjCall) {
                try {
                    const pj::CallInfo ci = m_impl->pjCall->getInfo();
                    for (const auto &mi : ci.media) {
                        const bool activeOrHeld =
                            mi.status == PJSUA_CALL_MEDIA_ACTIVE
                            || mi.status == PJSUA_CALL_MEDIA_LOCAL_HOLD
                            || mi.status == PJSUA_CALL_MEDIA_REMOTE_HOLD;
                        if (mi.type == PJMEDIA_TYPE_VIDEO && activeOrHeld)
                            videoActiveOrHeld = true;
                        else if (mi.type == PJMEDIA_TYPE_TEXT && activeOrHeld)
                            textActiveOrHeld = true;
                    }
                } catch (...) {}
            }

            // ── Video consent ───────────────────────────────────────────────
            // The !videoMediaActive check mirrors the RTT guard below: without
            // it, any re-INVITE the peer sends for an unrelated reason (e.g.
            // toggling RTT) while video is already active still contains an
            // "m=video" line describing that already-active stream. Without
            // this check that was misread as a brand-new incoming video
            // request and auto-declined (prm.opt.videoCount = 0), which
            // genuinely answered the re-INVITE with video removed and tore
            // down video that was working fine (W110 live two-device
            // follow-up: "remote camera doesn't render"/video drops mid-call).
            if (hasVideoOffer && !m_impl->videoRequestPendingLocal
                && !m_impl->videoMediaActive && !videoActiveOrHeld) {
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
            // Mirrors the video guard above: if we ourselves are already
            // waiting on our own local RTT offer (rttRequestPendingLocal),
            // this incoming offer is the peer's accept-triggered re-INVITE
            // answering it, not a fresh remote request — accept it directly
            // instead of auto-declining and re-prompting, which previously
            // caused an endless "Accept RTT" ping-pong between both sides
            // (W110 live two-device follow-up).
            if (hasTextOffer && !m_impl->rttRequestPendingLocal
                && !m_impl->rttMediaActive && !textActiveOrHeld) {
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
                // Peer withdrew the RTT offer. Clearing the internal flag alone
                // is not enough: the UI prompt and RttSession were left in
                // RemoteOfferPending, so the user's later "Accept RTT" click
                // hit acceptIncomingRttRequest()'s "no pending incoming RTT
                // request" guard and silently did nothing (seen live during
                // hold/resume, where the hold re-INVITE drops m=text right
                // after an offer prompt was shown). Tell the Qt side too.
                m_impl->rttRequestNotified = false;
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Peer withdrew RTT offer: callId=%1").arg(getId()));
                QPointer<SipCall> self = m_impl->q;
                QMetaObject::invokeMethod(self, [self]() {
                    if (self)
                        emit self->rttRequestWithdrawn();
                }, Qt::QueuedConnection);
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
#if defined(_WIN32)
        // Use the same Qt GDI renderer as attachVideoWindows() so the preview
        // can be re-embedded into the local PiP widget after Camera On.
        if (PjsipGdiRenderer::deviceIndex() != PJMEDIA_VID_INVALID_DEV)
            pvp.rend_id = PjsipGdiRenderer::deviceIndex();
#endif
        pvp.show    = PJ_FALSE;
        const pj_status_t st = pjsua_vid_preview_start(capDev, &pvp);
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Camera On: capture re-opened capDev=%1 status=%2")
                .arg(videoCapDev).arg(st));
    }

    // Task W101 Phase 2/3: lazily starts this call's own MSRP passive
    // listener BEFORE the SDP offer/answer that advertises it is built —
    // called from PjCall::onCallSdpCreated, which fires while the SDP is
    // still being composed (i.e. strictly before anything is transmitted).
    // Idempotent: a re-INVITE/UPDATE later in the same call reuses the same
    // listener/session rather than opening a second one. On failure, leaves
    // msrpSession null and msrpListenerFailed set so the caller offers
    // m=message with port 0 (a valid "not available" answer, RFC 3264)
    // instead of an offer nobody could ever connect to.
    // Task W102 Phase 2/3 refactor: creation/wiring of the MsrpSession
    // object is split from actually starting a transport, so the same
    // object-creation code path serves both this call's own offer (always
    // passive-listens — see startMsrpPassiveListener) and an incoming
    // peer-initiated offer we are answering (role depends on the remote's
    // negotiated a=setup — see startMsrpPassiveListener/startMsrpActiveConnect
    // in onCallSdpCreated's answer branch).
    void createMsrpSessionObject()
    {
        if (msrpSession)
            return;

        const QString sessionKey = q ? q->callId() : QUuid::createUuid().toString();
        msrpSession = new MsrpSession(sessionKey, nullptr);
        if (q) {
            msrpSession->setSipCallId(q->callId());
            // Relay to SipManager, which owns message history/diagnostics —
            // mirrors the existing callConnected/callDisconnected pattern
            // rather than having SipCall reach into those stores directly.
            QPointer<SipCall> self = q;
            QObject::connect(msrpSession, &MsrpSession::payloadReceived, q,
                [self](const QString &, const QString &messageId,
                       const QString &contentType, const QByteArray &body) {
                    if (self)
                        emit self->msrpPayloadReceived(contentType, body, messageId);
                });
            QObject::connect(msrpSession, &MsrpSession::messageDeliveryStatusChanged, q,
                [self](const QString &, const QString &messageId, bool success,
                       const QString &statusText) {
                    if (self)
                        emit self->msrpDeliveryStatusChanged(messageId, success, statusText);
                });
            QObject::connect(msrpSession, &MsrpSession::fileTransferReceived, q,
                [self](const QString &, const QString &messageId, const QString &contentType,
                       const QString &suggestedFileName, const QByteArray &body) {
                    if (self)
                        emit self->msrpFileTransferReceived(contentType, suggestedFileName, body, messageId);
                });
        }
        msrpSession->setChunkSizeBytes(AppSettings::msrpChunkSizeBytes());
        msrpSession->setRequestReports(AppSettings::msrpRequestReports());
        msrpSession->setMaxFrameBytes(AppSettings::msrpMaxFrameBytes());
        msrpSession->setMaxMessageBytes(AppSettings::msrpMaxMessageBytes());
        msrpSession->setTlsVerifyPeer(AppSettings::msrpTlsVerifyPeer());
        msrpSession->setTlsCaCertificatePath(AppSettings::msrpTlsCaPath());
    }

    // Starts this call's own MSRP passive listener BEFORE the SDP offer/
    // answer that advertises it is built. Idempotent: a re-INVITE/UPDATE
    // later in the same call reuses the same listener/session rather than
    // opening a second one. On failure, leaves msrpSession null and
    // msrpListenerFailed set so the caller offers/answers m=message with
    // port 0 (a valid "not available" answer, RFC 3264) instead of a
    // section nobody could ever connect to.
    void startMsrpPassiveListener()
    {
        if (msrpSession || msrpListenerFailed)
            return; // already started, or already tried and failed this call

        createMsrpSessionObject();
        const QString sessionKey = q ? q->callId() : msrpSession->info().sessionKey;

        const bool useTls = AppSettings::enableMsrpTls();
        const QString bindAddress = AppSettings::msrpLocalBindAddress().isEmpty()
            ? QStringLiteral("0.0.0.0") : AppSettings::msrpLocalBindAddress();
        const int requestedPort = AppSettings::msrpPortMode() == QStringLiteral("fixed")
            ? AppSettings::msrpFixedPort() : 0;

        // Session-id is generated once, up front — random/unique per task
        // requirement — and reused for both the transport's own identity
        // and the advertised a=path so they never disagree.
        const QString localSessionId = MsrpPath::generateSessionId();

        // listenAsPassive() binds a QTcpServer synchronously — by the time
        // this call returns, transportLocalPort() reflects the real bound
        // port, so the SDP built right after this call always advertises a
        // listener that is genuinely already up (task requirement: "portul
        // advertised trebuie sa corespunda listener-ului real").
        msrpSession->setLocalUri(MsrpPath::buildUri(useTls, bindAddress, requestedPort, localSessionId));
        msrpSession->listenAsPassive(bindAddress, AppSettings::msrpConnectionTimeoutMs());

        const int boundPort = msrpSession->transportLocalPort();
        if (boundPort <= 0) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("MSRP passive listener failed to bind; offering m=message with port 0: callId=%1")
                    .arg(sessionKey));
            delete msrpSession;
            msrpSession = nullptr;
            msrpListenerFailed = true;
            return;
        }

        msrpLocalUri = MsrpPath::buildUri(useTls, bindAddress, boundPort, localSessionId);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("MSRP passive listener started: callId=%1 port=%2 tls=%3")
                .arg(sessionKey).arg(boundPort).arg(useTls));
    }

    // Task W102 Phase 2/3: used only when answering a peer-initiated
    // m=message offer whose negotiated setup role assigns *us* the active
    // (connecting) side — i.e. the remote offered "a=setup:passive". The
    // remote's own advertised a=path (already parsed into `remotePath`) is
    // where we connect to; our own advertised path uses an unspecified
    // port (RFC 4975 default 2855/2856) rather than probing/claiming a
    // real listening port, because nothing needs to dial back into us for
    // *this* session — the only connection this session ever uses is the
    // one we are about to open. This is a deliberate, documented protocol
    // choice, not a fabricated listener (see docs/msrp-offer-answer.md).
    bool startMsrpActiveConnect(const QList<MsrpUri> &remotePath)
    {
        if (msrpSession || msrpListenerFailed)
            return false;
        if (remotePath.isEmpty() || !remotePath.first().ok) {
            msrpListenerFailed = true;
            return false;
        }

        createMsrpSessionObject();
        const QString sessionKey = q ? q->callId() : msrpSession->info().sessionKey;

        const bool useTls = remotePath.first().transportProtocol() == MsrpTransportProtocol::Tls;
        const QString localSessionId = MsrpPath::generateSessionId();
        QString host = AppSettings::msrpAdvertisedHost();
        if (host.isEmpty())
            host = remotePath.first().host; // best available hint until c= is resolved by caller

        msrpSession->setLocalUri(MsrpPath::buildUri(useTls, host, /*port*/ -1, localSessionId));
        msrpSession->setRemotePath(remotePath);
        msrpSession->connectAsActive(AppSettings::msrpConnectionTimeoutMs());

        msrpLocalUri = msrpSession->info().localSessionId.isEmpty()
            ? MsrpPath::buildUri(useTls, host, -1, localSessionId)
            : MsrpPath::buildUri(useTls, host, -1, msrpSession->info().localSessionId);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("MSRP active connect started (answering as active): callId=%1 remoteHost=%2 remotePort=%3 tls=%4")
                .arg(sessionKey).arg(remotePath.first().host).arg(remotePath.first().port).arg(useTls));
        return true;
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
    bool                rttRequestPendingLocal{false}; // local user requested RTT and is awaiting completion
    bool                rttMediaActive{false};         // true while T.140 text stream is active
    bool                videoMediaActive{false};       // true while video stream is active (mirrors rttMediaActive)
    int                 videoMuteAppliedMediaIndex{-1}; // idempotency across media renegotiation
    bool                holdActive{false};             // true while local hold is in effect (PJSIP mode)
    bool                videoActiveBeforeHold{false};  // video was negotiated when local hold was sent
    bool                rttActiveBeforeHold{false};    // T.140 text was active when local hold was sent

    // Task W101 (Live MSRP SIP Integration): pj_pool_t* (via pjsua_pool_create,
    // public API — not the dialog's own pool, which pjsua2 does not expose).
    // Owns the memory for every m=message attribute spliced into this call's
    // SDP; must outlive any SDP that references it, so it lives for the
    // whole call and is released in ~SipCall.
    void        *msrpSdpPool{nullptr};
    MsrpSession *msrpSession{nullptr};   // this call's own MSRP listener/session, lazily started
    MsrpUri      msrpLocalUri;           // this call's advertised msrp(s):// URI once the listener is up
    bool         msrpListenerFailed{false}; // true once we've tried and failed — offer m=message with port 0

    // Task W108: async-prepared MSRP transport decision, supplied via
    // SipCall::setPreparedMsrpOffer() before makeCall()/makeCallWithOptions().
    // onCallSdpCreated only ever reads this — see the header doc comment.
    PreparedMsrpOffer preparedMsrpOffer;
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

    // Task W101: tear down this call's own MSRP listener/session before the
    // pool its advertised SDP attributes were allocated from is released —
    // the session itself doesn't reference the pool, but closing it first
    // keeps teardown order predictable (transport closed → then SDP memory
    // freed, never the reverse).
    delete m_impl->msrpSession;
    m_impl->msrpSession = nullptr;
    if (m_impl->msrpSdpPool) {
        pj_pool_release(static_cast<pj_pool_t *>(m_impl->msrpSdpPool));
        m_impl->msrpSdpPool = nullptr;
    }
#endif
    delete m_impl;
}

bool SipCall::isMsrpEstablished() const
{
#ifdef HAVE_PJSIP
    return m_impl->msrpSession && m_impl->msrpSession->info().isEstablished();
#else
    return false;
#endif
}

QString SipCall::sendMsrpMessage(const QString &contentType, const QByteArray &body)
{
#ifdef HAVE_PJSIP
    if (!isMsrpEstablished())
        return QString();
    return m_impl->msrpSession->sendMessage(contentType, body);
#else
    Q_UNUSED(contentType)
    Q_UNUSED(body)
    return QString();
#endif
}

SipCall::MsrpFileSendResult SipCall::sendMsrpFile(const QString &filePath, const QString &contentType)
{
#ifdef HAVE_PJSIP
    MsrpFileSendResult result;
    if (!isMsrpEstablished()) {
        result.error = QStringLiteral("No established MSRP session");
        return result;
    }
    const MsrpSession::FileSendResult sent = m_impl->msrpSession->sendFile(filePath, contentType);
    result.ok = sent.ok;
    result.error = sent.error;
    result.messageId = sent.messageId;
    result.fileSize = sent.fileSize;
    result.sha1Hex = sent.sha1Hex;
    return result;
#else
    Q_UNUSED(filePath)
    Q_UNUSED(contentType)
    MsrpFileSendResult result;
    result.error = QStringLiteral("MSRP not available in this build");
    return result;
#endif
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

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Initial call media offer: audio=%1 video=%2 rtt=%3")
            .arg(opts.requireAudio ? QStringLiteral("yes") : QStringLiteral("no"),
                 opts.allowVideo   ? QStringLiteral("yes") : QStringLiteral("no"),
                 opts.requireRtt   ? QStringLiteral("yes") : QStringLiteral("no")));

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
            appendCustomHeaders(prm, opts, m_callId);
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

            // Task W108 Phase 9: onCallSdpCreated (above) already created
            // msrpSession and populated the offer from the relay allocation
            // synchronously, without touching the relay's transport. Now
            // that the dialog exists, hand the relay's already-connected,
            // already-authenticated transport to msrpSession so SEND/
            // response/REPORT traffic for this call flows over it.
            if (m_impl->preparedMsrpOffer.ready
                    && m_impl->preparedMsrpOffer.mode == MsrpCallTransportMode::Relay
                    && m_impl->preparedMsrpOffer.relayClient
                    && m_impl->msrpSession) {
                if (auto transport = m_impl->preparedMsrpOffer.relayClient->takeTransport()) {
                    m_impl->msrpSession->adoptExternalTransport(std::move(transport),
                                                                  MsrpRole::ActiveConnector);
                    Logger::instance().info(LogCategory::Sip,
                        QStringLiteral("MSRP relay transport adopted into session: callId=%1")
                            .arg(m_callId));
                } else {
                    Logger::instance().warn(LogCategory::Sip,
                        QStringLiteral("MSRP relay transport unavailable at call-creation time "
                                       "(already taken or disconnected): callId=%1")
                            .arg(m_callId));
                }
            }

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

void SipCall::setPreparedMsrpOffer(const PreparedMsrpOffer &offer)
{
#ifdef HAVE_PJSIP
    m_impl->preparedMsrpOffer = offer;
#else
    Q_UNUSED(offer)
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
            // Remember the negotiated video state now: the media-state callback
            // that follows the hold re-INVITE will clear m_localVideoAvailable /
            // m_remoteVideoAvailable, so resume() cannot rely on them.
            m_impl->videoActiveBeforeHold =
                m_localVideoAvailable || m_remoteVideoAvailable;
            m_impl->rttActiveBeforeHold = m_impl->rttMediaActive;
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
        // videoActiveBeforeHold was captured in hold(): the hold renegotiation
        // clears the live availability flags, so they alone cannot be trusted.
        bool videoWasActive = m_impl->videoActiveBeforeHold
            || m_localVideoAvailable || m_remoteVideoAvailable;
        // The hold renegotiation deactivates the text stream, so the live
        // rttMediaActive flag alone would drop RTT from the unhold re-INVITE.
        bool rttWasActive = m_impl->rttActiveBeforeHold || m_impl->rttMediaActive;
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
                // A held video stream reports LOCAL_HOLD / REMOTE_HOLD, not
                // ACTIVE — all three mean video was negotiated and must be
                // restored by the unhold re-INVITE.
                if (mi.type == PJMEDIA_TYPE_VIDEO
                        && (mi.status == PJSUA_CALL_MEDIA_ACTIVE
                            || mi.status == PJSUA_CALL_MEDIA_LOCAL_HOLD
                            || mi.status == PJSUA_CALL_MEDIA_REMOTE_HOLD))
                    videoWasActive = true;
                if (mi.type == PJMEDIA_TYPE_TEXT
                        && (mi.status == PJSUA_CALL_MEDIA_ACTIVE
                            || mi.status == PJSUA_CALL_MEDIA_LOCAL_HOLD
                            || mi.status == PJSUA_CALL_MEDIA_REMOTE_HOLD))
                    rttWasActive = true;
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
                // video must resume bidirectional (camera transmit + remote
                // decode), not receive-only
                prm.opt.videoCount > 0 ? PJMEDIA_DIR_ENCODING_DECODING : PJMEDIA_DIR_NONE,
                prm.opt.textCount  > 0 ? PJMEDIA_DIR_ENCODING_DECODING : PJMEDIA_DIR_NONE
            };
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
            m_impl->videoActiveBeforeHold = false;
            m_impl->rttActiveBeforeHold   = false;
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
            // Deterministic mute: disconnect the microphone from the call on
            // the conference bridge instead of adjusting levels (level
            // adjustment is not honoured by every bridge backend and is
            // silently undone when the media session is rebuilt).
            pj::AudDevManager &adm = pj::Endpoint::instance().audDevManager();
            if (muted) {
                adm.getCaptureDevMedia().stopTransmit(*m_impl->callAudioMedia);
            } else {
                adm.getCaptureDevMedia().startTransmit(*m_impl->callAudioMedia);
                adm.getCaptureDevMedia().adjustTxLevel(m_micVolume / 100.0f);
            }
        } catch (const pj::Error &e) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("setMuted(%1) bridge error (call id=%2): %3")
                    .arg(muted).arg(m_callId, QString::fromStdString(e.reason)));
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
#ifdef HAVE_PJSIP
    VideoStreamGuard::Snapshot snapshot;
    snapshot.callObjectExists = m_impl && m_impl->pjCall;
    snapshot.teardownInProgress = isTeardownState(m_stateMachine.state());

    pj::CallInfo ci;
    if (snapshot.callObjectExists) {
        try {
            snapshot.callId = m_impl->pjCall->getId();
            ci = m_impl->pjCall->getInfo();
            if (ci.state == PJSIP_INV_STATE_NULL)
                snapshot.callState = VideoStreamGuard::CallState::Null;
            else if (ci.state == PJSIP_INV_STATE_DISCONNECTED)
                snapshot.callState = VideoStreamGuard::CallState::Disconnected;
            else if (ci.state == PJSIP_INV_STATE_CONFIRMED)
                snapshot.callState = VideoStreamGuard::CallState::Confirmed;
            else
                snapshot.callState = VideoStreamGuard::CallState::Connecting;

            snapshot.currentVideoStreamIndex =
                snapshot.callId >= 0
                    ? pjsua_call_get_vid_stream_idx(snapshot.callId)
                    : PJSUA_INVALID_ID;
            snapshot.media.reserve(ci.media.size());
            for (const pj::CallMediaInfo &mi : ci.media) {
                VideoStreamGuard::Media media;
                media.index = static_cast<int>(mi.index);
                media.type = mi.type == PJMEDIA_TYPE_VIDEO
                    ? VideoStreamGuard::MediaType::Video
                    : (mi.type == PJMEDIA_TYPE_AUDIO
                        ? VideoStreamGuard::MediaType::Audio
                        : VideoStreamGuard::MediaType::Other);
                switch (mi.status) {
                case PJSUA_CALL_MEDIA_ACTIVE:
                    media.status = VideoStreamGuard::MediaStatus::Active; break;
                case PJSUA_CALL_MEDIA_LOCAL_HOLD:
                    media.status = VideoStreamGuard::MediaStatus::LocalHold; break;
                case PJSUA_CALL_MEDIA_REMOTE_HOLD:
                    media.status = VideoStreamGuard::MediaStatus::RemoteHold; break;
                case PJSUA_CALL_MEDIA_ERROR:
                    media.status = VideoStreamGuard::MediaStatus::Error; break;
                default:
                    media.status = VideoStreamGuard::MediaStatus::None; break;
                }
                media.canTransmit = (mi.dir & PJMEDIA_DIR_ENCODING) != 0;
                snapshot.media.push_back(media);
            }
        } catch (const pj::Error &e) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("Camera %1: PJSIP call inspection failed; local capture changed only. "
                               "status=%2 title=%3 reason=%4 callState=%5 mediaIndex=-1")
                    .arg(muted ? QStringLiteral("Off") : QStringLiteral("On"))
                    .arg(e.status)
                    .arg(QString::fromStdString(e.title), QString::fromStdString(e.reason),
                         callStateName(m_stateMachine.state())));
            return true;
        }
    }

    const VideoStreamGuard::Decision decision = VideoStreamGuard::evaluate(snapshot);
    if (!decision.mayOperate) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Camera %1: no active PJSIP video stream; local capture %2 only. "
                           "reason=%3 callId=%4 callState=%5 mediaCount=%6 videoMediaCount=%7 "
                           "videoStreamIndex=%8")
                .arg(muted ? QStringLiteral("Off") : QStringLiteral("On"),
                     muted ? QStringLiteral("stopped") : QStringLiteral("preview state changed"),
                     QString::fromLatin1(decision.reason))
                .arg(snapshot.callId)
                .arg(callStateName(m_stateMachine.state()))
                .arg(snapshot.media.size())
                .arg(std::count_if(snapshot.media.cbegin(), snapshot.media.cend(),
                    [](const VideoStreamGuard::Media &media) {
                        return media.type == VideoStreamGuard::MediaType::Video;
                    }))
                .arg(snapshot.currentVideoStreamIndex));
        return true;
    }

    if (m_videoMuted == muted
            && m_impl->videoMuteAppliedMediaIndex == decision.mediaIndex) {
        return true;
    }

    try {
        pj::CallVidSetStreamParam prm;
        prm.medIdx = decision.mediaIndex;
        if (muted) {
            m_impl->pjCall->vidSetStream(PJSUA_CALL_VID_STRM_STOP_TRANSMIT, prm);
            m_impl->pauseCapture();
        } else {
            m_impl->resumeCapture();
            m_impl->pjCall->vidSetStream(PJSUA_CALL_VID_STRM_START_TRANSMIT, prm);
        }
        m_videoMuted = muted;
        m_impl->videoMuteAppliedMediaIndex = decision.mediaIndex;
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Video transmit %1 via vidSetStream: call id=%2 medIdx=%3")
                .arg(muted ? QStringLiteral("stopped") : QStringLiteral("started"),
                     m_callId)
                .arg(decision.mediaIndex));
        emit videoMuteChanged(muted);
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("vidSetStream error: status=%1 title=%2 reason=%3 "
                           "callState=%4 mediaIndex=%5")
                .arg(e.status)
                .arg(QString::fromStdString(e.title), QString::fromStdString(e.reason),
                     callStateName(m_stateMachine.state()))
                .arg(decision.mediaIndex));
        return false;
    }
#else
    if (m_videoMuted == muted)
        return true;
    m_videoMuted = muted;
    emit videoMuteChanged(muted);
#endif
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
            // Preserve RTT across this video re-INVITE unless the user has
            // explicitly disabled it or the peer withdrew/rejected it. RTT
            // counts as "preserve" when: negotiated active or held (a held
            // text stream reports LOCAL_HOLD/REMOTE_HOLD, not ACTIVE), an
            // incoming RTT request is pending user accept, a local RTT
            // request is still negotiating, or RTT was active before hold.
            bool textActive = false;
            QString preserveReason = QStringLiteral("no active/pending/held RTT state");
            if (m_impl->rttRequestNotified) {
                textActive = true;
                preserveReason = QStringLiteral("incoming RTT request pending local accept");
            } else if (m_impl->rttRequestPendingLocal) {
                textActive = true;
                preserveReason = QStringLiteral("local RTT request still negotiating");
            } else if (m_impl->rttActiveBeforeHold) {
                textActive = true;
                preserveReason = QStringLiteral("RTT was active before hold");
            }
            if (!textActive) {
                try {
                    const pj::CallInfo ci = m_impl->pjCall->getInfo();
                    for (const auto &mi : ci.media) {
                        if (mi.type == PJMEDIA_TYPE_TEXT
                            && (mi.status == PJSUA_CALL_MEDIA_ACTIVE
                                || mi.status == PJSUA_CALL_MEDIA_LOCAL_HOLD
                                || mi.status == PJSUA_CALL_MEDIA_REMOTE_HOLD)) {
                            textActive = true;
                            preserveReason = QStringLiteral("RTT negotiated active/held");
                            break;
                        }
                    }
                } catch (...) {}
            }
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Preserve RTT during re-INVITE: %1 reason=%2")
                    .arg(textActive ? QStringLiteral("yes") : QStringLiteral("no"), preserveReason));

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

bool SipCall::hasPendingIncomingRttRequest() const
{
#ifdef HAVE_PJSIP
    return m_impl && m_impl->rttRequestNotified;
#else
    return false;
#endif
}

bool SipCall::requestRtt(bool enabled)
{
    // Local-initiated request only. A pending remote offer must go through
    // acceptIncomingRttRequest()/rejectIncomingRttRequest() instead — treating
    // "accept" as an ordinary local request is exactly the ambiguity that
    // caused the contradictory "still pending after already declined" state
    // (Task W109A).
    if (enabled && hasPendingIncomingRttRequest()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("requestRtt() rejected: an incoming RTT request is pending — "
                           "use acceptIncomingRttRequest()/rejectIncomingRttRequest() instead"));
        return false;
    }
#ifdef HAVE_PJSIP
    if (enabled && m_impl && m_impl->rttRequestPendingLocal) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("requestRtt() ignored: a local RTT re-INVITE is already in flight "
                           "(duplicate request suppressed)"));
        return false;
    }
    // Requesting RTT while the T.140 stream is already running must not
    // start a new negotiation: the peer's answer to that redundant offer
    // never produces an inactive→active transition, so RttSession sat in
    // LocalOfferPending until its 12 s timeout demoted a perfectly working
    // session to Failed (seen live: users re-clicked "Request RTT" exactly
    // because no Active confirmation was visible, making it worse each
    // time). Re-emit rttMediaConnected() instead so RttSession/the UI
    // re-sync to Active immediately.
    if (enabled && m_impl && m_impl->rttMediaActive) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("requestRtt() no-op: RTT text stream already active — "
                           "re-confirming Active to the session/UI"));
        emit rttMediaConnected();
        return true;
    }
#endif
    return sendRttOffer(enabled);
}

bool SipCall::acceptIncomingRttRequest()
{
    if (!hasPendingIncomingRttRequest()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("acceptIncomingRttRequest() ignored: no pending incoming RTT request"));
        return false;
    }
#ifdef HAVE_PJSIP
    if (m_impl && m_impl->rttRequestPendingLocal) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("acceptIncomingRttRequest() ignored: a local RTT re-INVITE is "
                           "already in flight (duplicate accept suppressed)"));
        return false;
    }
#endif
    return sendRttOffer(true);
}

bool SipCall::rejectIncomingRttRequest()
{
    if (!hasPendingIncomingRttRequest()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("rejectIncomingRttRequest() ignored: no pending incoming RTT request"));
        return false;
    }
#ifdef HAVE_PJSIP
    m_impl->rttRequestNotified = false;
#endif
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT request rejected by user: call id=%1 (already declined via "
                       "auto-response m=text 0 — no new re-INVITE sent)").arg(m_callId));
    emit rttRequestRejected();
    return true;
}

void SipCall::cancelPendingRttRequest()
{
#ifdef HAVE_PJSIP
    if (m_impl)
        m_impl->rttRequestPendingLocal = false;
#endif
}

bool SipCall::sendRttOffer(bool enabled)
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
            // Preserve video across this RTT re-INVITE using the same
            // active/held/pending rules as requestVideo() uses for RTT.
            bool videoActive = m_impl->videoRequestPendingLocal || m_impl->videoRequestNotified
                || m_impl->videoActiveBeforeHold;
            if (!videoActive) {
                try {
                    const pj::CallInfo ci = m_impl->pjCall->getInfo();
                    for (const auto &mi : ci.media) {
                        if (mi.type == PJMEDIA_TYPE_VIDEO
                            && (mi.status == PJSUA_CALL_MEDIA_ACTIVE
                                || mi.status == PJSUA_CALL_MEDIA_LOCAL_HOLD
                                || mi.status == PJSUA_CALL_MEDIA_REMOTE_HOLD)) {
                            videoActive = true;
                            break;
                        }
                    }
                } catch (...) {}
            }

            pj::CallOpParam prm(true);
            prm.opt.videoCount = videoActive ? 1 : 0;
            prm.opt.textCount = enabled ? 1 : 0;
            if (videoActive)
                applyVideoMediaDirectionIfNeeded(prm.opt);

            if (enabled) {
                m_impl->rttRequestPendingLocal = true;
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("%1: textCount=%2")
                        .arg(m_impl->rttRequestNotified
                                 ? QStringLiteral("Accept RTT")
                                 : QStringLiteral("Request RTT ON"))
                        .arg(prm.opt.textCount));
                if (m_impl->rttRequestNotified)
                    m_impl->rttRequestNotified = false;
                emit rttLocalOfferSent();
            }

            m_impl->pjCall->reinvite(prm);
            logMediaSummary(QStringLiteral("After Request RTT"));
            return true;
        } catch (const pj::Error &e) {
            m_impl->rttRequestPendingLocal = false;
            if (isSessionTerminatedError(e)) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("requestRtt() ignored: call already terminated during teardown"));
                return false;
            }
            const QString reason = QString::fromStdString(e.reason);
            const RtpPortErrorKind kind = classifyRtpPortError(reason);
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("requestRtt() PJSIP error [%1]: %2")
                    .arg(rtpPortErrorKindName(kind), reason));
            emit rttNegotiationFailed(reason);
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

        if (stat.rtcp.txStat.pkt > 0) {
            snap.packetsTxAvailable = true;
            snap.packetsTx = static_cast<unsigned>(stat.rtcp.txStat.pkt);
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

bool SipCall::attachVideoWindows(WId remoteWidget, WId localPreview)
{
#if defined(HAVE_PJSIP) && defined(_WIN32)
    if (!m_impl)
        return false;
    bool remoteOk = false;
    bool localOk  = false;

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
            return false;
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
        if (st != PJ_SUCCESS) {
            // The incoming render window is not fully created yet (PJSIP
            // assigns it lazily right after negotiation). Do NOT call
            // set_win on an unverified id — report failure and let the
            // caller retry once the window exists.
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Remote video window not ready yet: winId=%1 status=%2 — deferring attach")
                    .arg(m_impl->videoIncomingWinId).arg(st));
        } else if (wi.is_native) {
            HWND nativeHwnd = reinterpret_cast<HWND>(wi.hwnd.info.win.hwnd);
            if (nativeHwnd) {
                SetParent(nativeHwnd, qtHwnd);
                ShowWindow(nativeHwnd, SW_SHOW);
                remoteOk = true;
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
                remoteOk = true;
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
                localOk = true;
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
                        localOk = false;
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
            // Preview already running (e.g. re-opened by Camera On) — rebind
            // its render target to our widget. Without this the preview frames
            // go nowhere after the capture device is re-opened mid-call.
            pjmedia_vid_dev_hwnd h = makeWinHwnd(qtPreviewHwnd);
            pj_status_t st = pjsua_vid_win_set_win(previewWinId, &h);
            if (st == PJ_SUCCESS) {
                localOk = true;
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
                pvp.rend_id = renderDev;
                pvp.wnd = makeWinHwnd(qtPreviewHwnd);
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Local preview restart params: capDev=%1 rendId=%2 hwnd=0x%3")
                        .arg(capDev)
                        .arg(pvp.rend_id)
                        .arg(reinterpret_cast<quintptr>(qtPreviewHwnd), 0, 16));
                pj_status_t st = pjsua_vid_preview_start(capDev, &pvp);
                if (st == PJ_SUCCESS) {
                    previewWinId = pjsua_vid_preview_get_win(capDev);
                    localOk = true;
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
        // No local preview widget was supplied — nothing left to attach.
        // An invalid capture device, however, means the attach is incomplete.
        localOk = (localPreview == 0);
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Local preview attach skipped: capDev=%1 localWidget=%2")
                .arg(m_impl->videoCapDev)
                .arg(static_cast<quintptr>(localPreview)));
    }
    return remoteOk && localOk;
#else
    Q_UNUSED(remoteWidget)
    Q_UNUSED(localPreview)
    return false;
#endif
}

bool SipCall::setVideoWindowVisible(bool visible)
{
#if defined(HAVE_PJSIP) && defined(_WIN32)
    if (!m_impl)
        return true;
    bool ok = true;

    // Only the non-native (our Qt GDI renderer) path supports set_show();
    // this app never registers a native-window video renderer, so that
    // branch is not expected in practice — attachVideoWindows()'s own
    // is_native handling is left untouched for that case.
    auto pauseResumeWindow = [&ok, visible](pjsua_vid_win_id wid, const char *label) {
        if (wid == PJSUA_INVALID_ID)
            return;
        pjsua_vid_win_info wi;
        pj_bzero(&wi, sizeof(wi));
        if (pjsua_vid_win_get_info(wid, &wi) != PJ_SUCCESS || wi.is_native)
            return;
        const pj_status_t st = pjsua_vid_win_set_show(wid, visible ? PJ_TRUE : PJ_FALSE);
        if (st != PJ_SUCCESS) {
            ok = false;
            // Debug, not warn: the Qt GDI renderer does not implement the
            // SHOW capability (set_show consistently returns 520008 /
            // invalid-capability there), so during video calls this fired
            // twice on every page switch — pure log spam for a harmless
            // no-op on that render path.
            Logger::instance().debug(LogCategory::Media,
                QStringLiteral("setVideoWindowVisible(%1): set_show(%2) failed winId=%3 status=%4")
                    .arg(QString::fromLatin1(label))
                    .arg(visible)
                    .arg(wid)
                    .arg(st));
        }
    };

    pauseResumeWindow(m_impl->videoIncomingWinId, "remote");
    if (m_impl->videoCapDev >= PJMEDIA_VID_DEFAULT_CAPTURE_DEV) {
        const auto capDev = static_cast<pjmedia_vid_dev_index>(m_impl->videoCapDev);
        pauseResumeWindow(pjsua_vid_preview_get_win(capDev), "local");
    }
    return ok;
#else
    Q_UNUSED(visible)
    return true;
#endif
}

void SipCall::reset(const QString &reason)
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Call id=%1 reset: %2").arg(m_callId, reason));
    m_levelTimer.stop();
    m_localVideoAvailable  = false;
    m_remoteVideoAvailable = false;
    m_negotiatedAudioCodecInfo = AudioCodecInfo();
    m_negotiatedVideoCodecInfo = VideoCodecInfo();
    m_stateMachine.reset(reason);
}

CallState SipCall::state()      const { return m_stateMachine.state(); }
QString   SipCall::statusText() const { return m_stateMachine.statusText(); }
QString   SipCall::remoteUri()  const { return m_remoteUri; }
QString   SipCall::callId()     const { return m_callId; }

QString SipCall::sipDialogStateText() const
{
#ifdef HAVE_PJSIP
    if (m_impl && m_impl->pjCall) {
        try {
            const pj::CallInfo ci = m_impl->pjCall->getInfo();
            return QString::fromStdString(ci.stateText);
        } catch (...) {}
    }
#endif
    return {};
}

QString SipCall::remoteMediaAddress() const
{
#ifdef HAVE_PJSIP
    if (m_impl && m_impl->pjCall) {
        try {
            const pjsua_call_id cid =
                static_cast<pjsua_call_id>(m_impl->pjCall->getId());
            pjsua_call_info ci;
            if (pjsua_call_get_info(cid, &ci) == PJ_SUCCESS) {
                for (unsigned i = 0; i < ci.media_cnt; ++i) {
                    if (ci.media[i].type != PJMEDIA_TYPE_AUDIO)
                        continue;
                    pjsua_stream_info si;
                    pj_bzero(&si, sizeof(si));
                    if (pjsua_call_get_stream_info(cid, i, &si) == PJ_SUCCESS
                            && si.type == PJMEDIA_TYPE_AUDIO) {
                        char buf[PJ_INET6_ADDRSTRLEN + 16] = {};
                        // Flags 3 = address + port ("ip:port").
                        pj_sockaddr_print(&si.info.aud.rem_addr, buf,
                                          sizeof(buf), 3);
                        const QString addr = QString::fromLatin1(buf);
                        if (!addr.isEmpty())
                            return addr;
                    }
                }
            }
        } catch (...) {}
    }
#endif
    return {};
}

AudioCodecInfo SipCall::negotiatedAudioCodecInfo() const { return m_negotiatedAudioCodecInfo; }
VideoCodecInfo SipCall::negotiatedVideoCodecInfo() const { return m_negotiatedVideoCodecInfo; }

QString SipCall::negotiatedAudioCodec() const
{
    if (!m_negotiatedAudioCodecInfo.isValid())
        return {};
    return formatAudioCodecSummary(m_negotiatedAudioCodecInfo.name,
                                   m_negotiatedAudioCodecInfo.clockRate,
                                   m_negotiatedAudioCodecInfo.payloadType);
}

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
        appendCustomHeaders(prm, opts, m_callId);
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
