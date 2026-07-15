#include "SipAccount.h"

#include <QMetaObject>
#include <QPointer>

#include "core/Logger.h"
#include "core/AppSettings.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <pjsua-lib/pjsua.h>
#if defined(_WIN32)
#include "media/PjsipGdiRenderer.h"
#endif
#endif

#include "rtt/RttConfig.h"
#include "sip/RtpPortRangeConfig.h"

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
static pjmedia_vid_dev_index firstPjsipVideoCaptureDevice()
{
    try {
        pj::VidDevManager &vdm = pj::Endpoint::instance().vidDevManager();
        const unsigned count = vdm.getDevCount();
        pjmedia_vid_dev_index fallback = PJMEDIA_VID_INVALID_DEV;
        for (unsigned i = 0; i < count; ++i) {
            try {
                pj::VideoDevInfo info = vdm.getDevInfo(static_cast<int>(i));
                if (info.dir != PJMEDIA_DIR_CAPTURE && info.dir != PJMEDIA_DIR_CAPTURE_RENDER)
                    continue;

                const QString name = QString::fromStdString(info.name);
                const QString lowerName = name.toLower();
                if (lowerName.contains(QStringLiteral("obs virtual camera")))
                    return static_cast<pjmedia_vid_dev_index>(i);
                if (lowerName.contains(QStringLiteral("colorbar generator")))
                    return static_cast<pjmedia_vid_dev_index>(i);
                if (lowerName.contains(QStringLiteral("colorbar-active")))
                    return static_cast<pjmedia_vid_dev_index>(i);

                if (fallback == PJMEDIA_VID_INVALID_DEV)
                    fallback = static_cast<pjmedia_vid_dev_index>(i);
            } catch (...) {}
        }
        if (fallback != PJMEDIA_VID_INVALID_DEV)
            return fallback;
    } catch (...) {}
    return PJMEDIA_VID_INVALID_DEV;
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

static unsigned pjsipVideoDeviceCount()
{
    try {
        return pj::Endpoint::instance().vidDevManager().getDevCount();
    } catch (...) {
        return 0;
    }
}
#endif

QString registrationStateName(RegistrationState state)
{
    switch (state) {
    case RegistrationState::Unregistered:       return QStringLiteral("Unregistered");
    case RegistrationState::Registering:         return QStringLiteral("Registering");
    case RegistrationState::Registered:          return QStringLiteral("Registered");
    case RegistrationState::Unregistering:       return QStringLiteral("Unregistering");
    case RegistrationState::RegistrationFailed:  return QStringLiteral("RegistrationFailed");
    }
    return QStringLiteral("Unregistered");
}

static QString ensureSipUri(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive)) {
        return trimmed;
    }
    return QStringLiteral("sip:") + trimmed;
}

struct SipAccount::Impl
{
    explicit Impl(SipAccount *accountOwner) : owner(accountOwner) {}

    void notify(RegistrationState state, const QString &text, int code, int expiry = 0)
    {
        if (owner)
            owner->postRegistrationResult(state, text, code, expiry);
    }

    QPointer<SipAccount> owner;
    pj::AccountConfig accountConfig;

#ifdef HAVE_PJSIP
    // Minimal pj::Call subclass created in onIncomingCall to claim the user_data
    // slot before returning from the callback. pjsua2 auto-rejects the call with
    // 500 Internal Server Error if user_data is still null when onIncomingCall
    // returns (see Endpoint::on_incoming_call in pjsua2/endpoint.cpp:956-959).
    class EarlyCall final : public pj::Call {
    public:
        EarlyCall(pj::Account &acc, int callId) : pj::Call(acc, callId) {}
        void onCallState(pj::OnCallStateParam &) override {}
        void onCallMediaState(pj::OnCallMediaStateParam &) override {}
    };

    class Account final : public pj::Account
    {
    public:
        explicit Account(Impl *implementation) : m_impl(implementation) {}

        void onIncomingCall(pj::OnIncomingCallParam &prm) override
        {
            if (!m_impl || !m_impl->owner)
                return;
            // Retrieve remote URI from the invite dialog.
            QString remoteUri;
            try {
                pj::CallInfo ci;
                // pjsua2 does not expose CallInfo before the Call object exists;
                // use prm.rdata to extract the From header URI.
                const pjsip_msg *msg = static_cast<pjsip_msg *>(prm.rdata.pjRxData
                    ? static_cast<pjsip_rx_data *>(prm.rdata.pjRxData)->msg_info.msg
                    : nullptr);
                if (msg) {
                    const pjsip_from_hdr *from = static_cast<pjsip_from_hdr *>(
                        pjsip_msg_find_hdr(msg, PJSIP_H_FROM, nullptr));
                    if (from) {
                        char buf[256] = {};
                        pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR,
                                        from->uri, buf, sizeof(buf));
                        remoteUri = QString::fromUtf8(buf);
                    }
                }
            } catch (...) {}

            if (remoteUri.isEmpty())
                remoteUri = QStringLiteral("sip:unknown@unknown");

            QPointer<SipAccount> self = m_impl->owner;
            const QString uri = remoteUri;
            const int callId = prm.callId;
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP incoming INVITE: callId=%1 remote=%2")
                    .arg(callId).arg(uri));

            // Create EarlyCall BEFORE returning from this callback.
            // pjsua2 (endpoint.cpp:956) auto-rejects with 500 if user_data is null
            // when onIncomingCall returns. EarlyCall claims the slot so the session
            // survives until PjCall is created on the Qt main thread.
            auto *early = new EarlyCall(*this, callId);
            m_impl->earlyCalls.insert(callId, early);

            // Send 180 Ringing now that user_data is set (call is "owned").
            const pj_status_t ring_st = pjsua_call_answer(
                static_cast<pjsua_call_id>(callId), PJSIP_SC_RINGING, nullptr, nullptr);
            if (ring_st == PJ_SUCCESS) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("PJSIP 180 Ringing sent: callId=%1").arg(callId));
            } else {
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("PJSIP 180 Ringing failed: callId=%1 status=%2")
                        .arg(callId).arg(ring_st));
            }

            QMetaObject::invokeMethod(self, [self, uri, callId]() {
                if (self)
                    emit self->incomingPjsipCallReceived(uri, callId);
            }, Qt::QueuedConnection);
        }

        void onRegState(pj::OnRegStateParam &prm) override
        {
            RegistrationState state = RegistrationState::RegistrationFailed;
            QString text = QString::fromStdString(prm.reason);
            int code = prm.code;
            int expiry = 0;

            if (prm.status != PJ_SUCCESS) {
                code = static_cast<int>(prm.status);
                if (text.isEmpty())
                    text = QStringLiteral("PJSIP registration operation failed");
            } else if (prm.code >= 200 && prm.code < 300) {
                try {
                    const pj::AccountInfo info = getInfo();
                    state = info.regIsActive
                        ? RegistrationState::Registered
                        : RegistrationState::Unregistered;
                    if (info.regIsActive) {
                        expiry = static_cast<int>(info.regExpiresSec);
                        if (text.isEmpty())
                            text = QStringLiteral("Registration succeeded");
                    } else {
                        if (text.isEmpty())
                            text = QStringLiteral("Unregistration succeeded");
                    }
                } catch (const pj::Error &e) {
                    state = RegistrationState::RegistrationFailed;
                    code = static_cast<int>(e.status);
                    text = QString::fromStdString(e.reason);
                }
            } else {
                if (text.isEmpty())
                    text = QStringLiteral("Registration rejected");
            }

            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP registration callback: state=%1 code=%2 status=%3 reason=\"%4\" expiry=%5")
                    .arg(registrationStateName(state))
                    .arg(code)
                    .arg(static_cast<int>(prm.status))
                    .arg(text)
                    .arg(expiry));
            if (m_impl)
                m_impl->notify(state, text, code, expiry);
        }

        // Task W093: dedicated incoming-SIP-MESSAGE callback. This is purely
        // additive to the existing raw-trace capture (PjsipTraceModule, Task
        // W090) — it feeds a separate signal (instantMessageReceived) that
        // SipManager routes only into MessageHistoryStore, never back into
        // SipTraceLogger/MessagingEventStore, so the same wire message is
        // never logged twice into the same store.
        // Task W096: reads a non-standard (generic) SIP header by name
        // directly from the parsed message struct — the same pattern
        // already used above for PJSIP_H_FROM, just via
        // pjsip_msg_find_hdr_by_name since Message-ID/Disposition-
        // Notification have no dedicated pjsip_hdr subtype.
        static QString findGenericHeader(const pjsip_msg *msg, const char *name)
        {
            if (!msg)
                return QString();
            pj_str_t hname = pj_str(const_cast<char *>(name));
            auto *hdr = static_cast<pjsip_generic_string_hdr *>(
                pjsip_msg_find_hdr_by_name(msg, &hname, nullptr));
            if (!hdr)
                return QString();
            return QString::fromUtf8(hdr->hvalue.ptr, static_cast<int>(hdr->hvalue.slen));
        }

        void onInstantMessage(pj::OnInstantMessageParam &prm) override
        {
            if (!m_impl || !m_impl->owner)
                return;

            QString callId;
            QString messageId;
            QString dispositionNotification;
            try {
                if (prm.rdata.pjRxData) {
                    auto *rd = static_cast<pjsip_rx_data *>(prm.rdata.pjRxData);
                    if (rd && rd->msg_info.cid)
                        callId = QString::fromLatin1(rd->msg_info.cid->id.ptr,
                                                     static_cast<int>(rd->msg_info.cid->id.slen));
                    if (rd && rd->msg_info.msg) {
                        messageId = findGenericHeader(rd->msg_info.msg, "Message-ID");
                        dispositionNotification =
                            findGenericHeader(rd->msg_info.msg, "Disposition-Notification");
                    }
                }
            } catch (...) {}

            QPointer<SipAccount> self = m_impl->owner;
            const QString fromUri     = QString::fromStdString(prm.fromUri);
            const QString toUri       = QString::fromStdString(prm.toUri);
            const QString contactUri  = QString::fromStdString(prm.contactUri);
            const QString contentType = QString::fromStdString(prm.contentType);
            const QString body        = QString::fromStdString(prm.msgBody);
            const QString profileId   = self ? self->profileId() : QString();

            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP incoming MESSAGE: from=%1 to=%2 contentType=%3 bodyBytes=%4")
                    .arg(fromUri, toUri, contentType).arg(body.toUtf8().size()));

            QMetaObject::invokeMethod(self,
                [self, fromUri, toUri, contactUri, contentType, body, callId, profileId,
                 messageId, dispositionNotification]() {
                    if (self)
                        emit self->instantMessageReceived(fromUri, toUri, contactUri, contentType,
                                                           body, callId, profileId, messageId,
                                                           dispositionNotification);
                }, Qt::QueuedConnection);
        }

        void onInstantMessageStatus(pj::OnInstantMessageStatusParam &prm) override
        {
            if (!m_impl || !m_impl->owner)
                return;

            // The correlation id was passed as sendMessage()'s userData Token
            // (a plain integer round-tripped through void*, no heap
            // allocation/ownership involved — see SipAccount::sendMessage).
            const qint64 correlationId =
                static_cast<qint64>(reinterpret_cast<intptr_t>(prm.userData));
            const bool success = prm.code >= 200 && prm.code < 300;
            const int code = static_cast<int>(prm.code);
            const QString reason = QString::fromStdString(prm.reason);

            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP SIP MESSAGE status: correlationId=%1 code=%2 reason=\"%3\"")
                    .arg(correlationId).arg(code).arg(reason));

            QPointer<SipAccount> self = m_impl->owner;
            QMetaObject::invokeMethod(self, [self, correlationId, success, code, reason]() {
                if (self)
                    emit self->instantMessageStatusReceived(correlationId, success, code, reason);
            }, Qt::QueuedConnection);
        }

        // SIP Presence (Task W098): the account-level presence-subscription
        // status callback. Left un-overridden — pjsua2's default behavior
        // (prm.code stays at its default 200) already auto-accepts an
        // incoming SUBSCRIBE, which is sufficient for this foundation task:
        // whatever this account's own status was last set to via
        // setOwnPresenceState() is what such a watcher will see on
        // subsequent NOTIFYs. See docs/presence.md for the exact rationale.

    private:
        Impl *m_impl;
    };

    // SIP Presence (Task W098): a long-lived pjsua2 Buddy used purely to
    // watch one entity's presence (subscribe=true). Per pjsua2's own
    // documentation (pjsua2/presence.hpp), the library does not keep buddy
    // instances alive on its own — the *original* instance that called
    // create() must be owned by the application for the life of the
    // subscription, and only its destructor unregisters/deletes the
    // underlying pjsua-lib buddy. That is why these are heap-allocated and
    // tracked in Impl::presenceBuddies rather than being transient/stack
    // objects (unlike the throwaway Buddy in sendMessage(), which never
    // subscribes).
    class PresenceBuddy final : public pj::Buddy
    {
    public:
        PresenceBuddy(Impl *implementation, QString entityUri)
            : m_impl(implementation), m_entityUri(std::move(entityUri)) {}

        void onBuddyState() override
        {
            if (!m_impl || !m_impl->owner)
                return;

            QString contactUri, basicStatus, activity, statusText, note, subState, subReason;
            try {
                pj::BuddyInfo info = getInfo();
                contactUri = QString::fromStdString(info.contact);

                switch (info.presStatus.status) {
                case PJSUA_BUDDY_STATUS_ONLINE:  basicStatus = QStringLiteral("open"); break;
                case PJSUA_BUDDY_STATUS_OFFLINE: basicStatus = QStringLiteral("closed"); break;
                default:                         basicStatus = QStringLiteral("unknown"); break;
                }

                switch (info.presStatus.activity) {
                case PJRPID_ACTIVITY_AWAY: activity = QStringLiteral("away"); break;
                case PJRPID_ACTIVITY_BUSY: activity = QStringLiteral("busy"); break;
                default: break; // pjsua2's PresenceStatus does not distinguish further RPID activities
                }

                statusText = QString::fromStdString(info.presStatus.statusText);
                note       = QString::fromStdString(info.presStatus.note);

                switch (info.subState) {
                case PJSIP_EVSUB_STATE_ACTIVE:
                    subState = QStringLiteral("active");
                    break;
                case PJSIP_EVSUB_STATE_PENDING:
                case PJSIP_EVSUB_STATE_ACCEPTED:
                case PJSIP_EVSUB_STATE_SENT:
                    subState = QStringLiteral("pending");
                    break;
                case PJSIP_EVSUB_STATE_TERMINATED:
                    subState = QStringLiteral("terminated");
                    break;
                default:
                    subState = QStringLiteral("unknown");
                    break;
                }
                subReason = QString::fromStdString(info.subTermReason);
            } catch (...) {}

            QPointer<SipAccount> self = m_impl->owner;
            const QString entityUri = m_entityUri;
            const QString profileId = self ? self->profileId() : QString();

            QMetaObject::invokeMethod(self,
                [self, entityUri, contactUri, basicStatus, activity, statusText, note,
                 subState, subReason, profileId]() {
                    if (self)
                        emit self->buddyPresenceChanged(entityUri, contactUri, basicStatus, activity,
                                                        statusText, note, subState, subReason, profileId);
                }, Qt::QueuedConnection);
        }

    private:
        Impl   *m_impl;
        QString m_entityUri;
    };

    Account *account{nullptr};
    QMap<int, EarlyCall *> earlyCalls; // callId → EarlyCall*, keyed by PJSIP call_id
    QMap<QString, PresenceBuddy *> presenceBuddies; // targetUri → PresenceBuddy*
#endif
};

SipAccount::SipAccount(const QString &profileId, QObject *parent)
    : QObject(parent)
    , m_profileId(profileId)
    , m_impl(new Impl(this))
{
}

SipAccount::~SipAccount()
{
#ifdef HAVE_PJSIP
    // pjsua2 requires every Buddy owned by an account to be deleted before
    // the account itself is shut down (see pjsua2/presence.hpp) — do this
    // first, before account->shutdown().
    for (auto *buddy : qAsConst(m_impl->presenceBuddies))
        delete buddy;
    m_impl->presenceBuddies.clear();

    if (m_impl->account)
        m_impl->account->shutdown();
    delete m_impl->account;
    m_impl->account = nullptr;
    for (auto *ec : qAsConst(m_impl->earlyCalls))
        delete ec;
    m_impl->earlyCalls.clear();
#endif
    delete m_impl;
}

void *SipAccount::takeEarlyPjCall(int callId)
{
#ifdef HAVE_PJSIP
    return m_impl ? m_impl->earlyCalls.take(callId) : nullptr;
#else
    Q_UNUSED(callId)
    return nullptr;
#endif
}

QString SipAccount::profileId() const
{
    return m_profileId;
}

bool SipAccount::startRegistration(const SipProfile &profile, const QString &password,
                                   int transportId)
{
#ifdef HAVE_PJSIP
    if (m_impl->account) {
        postRegistrationResult(RegistrationState::RegistrationFailed,
                               QStringLiteral("An account is already active"), 0);
        return false;
    }

    try {
        pj::AccountConfig config;
        config.idUri = profile.effectiveSipUri().toStdString();
        config.regConfig.registrarUri = ensureSipUri(profile.registrar).toStdString();
        if (transportId >= 0)
            config.sipConfig.transportId = transportId;

        const QString username = profile.authUsername.isEmpty()
            ? profile.sipUsername : profile.authUsername;
        config.sipConfig.authCreds.push_back(
            pj::AuthCredInfo("digest", "*", username.toStdString(), 0,
                             password.toStdString()));

        const QString proxy = !profile.outboundProxy.isEmpty()
            ? profile.outboundProxy : profile.proxy;
        if (!proxy.isEmpty())
            config.sipConfig.proxies.push_back(ensureSipUri(proxy).toStdString());


#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP video devices available before account create: count=%1")
                .arg(pjsipVideoDeviceCount()));
        const bool hasCaptureDev = hasPjsipVideoCaptureDevice();
        const pjmedia_vid_dev_index defaultCaptureDev = preferredPjsipVideoCaptureDevice();
        if (defaultCaptureDev != PJMEDIA_VID_INVALID_DEV) {
            config.videoConfig.defaultCaptureDevice = defaultCaptureDev;
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP video default capture device set to index %1")
                    .arg(defaultCaptureDev));
        } else {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("No PJSIP video capture device could be selected; using default capture device"));
        }
        config.videoConfig.autoTransmitOutgoing = hasCaptureDev;
        // Keep incoming video window hidden; we paint via our GDI renderer.
        config.videoConfig.autoShowIncoming = false;
#if defined(_WIN32)
        const pjmedia_vid_dev_index gdiRenderDev = PjsipGdiRenderer::deviceIndex();
        if (gdiRenderDev != PJMEDIA_VID_INVALID_DEV) {
            config.videoConfig.defaultRenderDevice = gdiRenderDev;
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("PJSIP video default render device set to Qt GDI renderer: %1")
                    .arg(gdiRenderDev));
        } else {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("Qt GDI renderer not available yet; using default render device"));
        }
#endif
        if (!hasCaptureDev) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("No PJSIP video capture device detected; outgoing video disabled, incoming video still allowed"));
        }
#endif

        // RTP/RTCP media port range (Task W109A) — pj::EpConfig::MediaConfig
        // has no port-range field; the actual pjsua2 API for this is the
        // per-account AccountMediaConfig::transportConfig (applies to audio,
        // video, and text streams alike). Without this, pjsua2 defaults to
        // its own built-in unbounded start port, so two instances of this
        // app on the same host both try to bind the same port first and
        // collide (WSAEADDRINUSE) — see docs/rtp-port-range-configuration.md.
        {
            const RtpPortRangeConfig rtpRange = resolveEffectiveRtpPortRange();
            QString rangeError, rangeWarning;
            if (!validateRtpPortRange(rtpRange.start, rtpRange.end, &rangeError, &rangeWarning)) {
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("Configured RTP port range [%1-%2] is invalid (%3); "
                                   "falling back to pjsua2 default (unbounded from its "
                                   "built-in start port)")
                        .arg(rtpRange.start).arg(rtpRange.end).arg(rangeError));
            } else {
                config.mediaConfig.transportConfig.port = static_cast<unsigned>(rtpRange.start);
                config.mediaConfig.transportConfig.portRange = static_cast<unsigned>(rtpRange.portRange());
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Effective RTP/RTCP port range: [%1-%2] (%3 ports)")
                        .arg(rtpRange.start).arg(rtpRange.end).arg(rtpRange.end - rtpRange.start + 1));
                if (!rangeWarning.isEmpty()) {
                    Logger::instance().warn(LogCategory::Sip,
                        QStringLiteral("RTP port range warning: %1").arg(rangeWarning));
                }
            }
        }

        // RTT / T.140 text media configuration (RFC 4103 + RFC 2198 RED).
        // redundancyLevel controls how many previous T.140 packets are
        // retransmitted with each PDU. PJSIP default is already
        // PJSUA_TXT_DEFAULT_REDUNDANCY_LEVEL=2, but we set it explicitly so
        // the intent is clear and logged. The negotiated level may be lower if
        // the remote peer SDP does not advertise red/90000.
        config.textConfig.redundancyLevel = kRttRedLevelDefault;

        // SIP Presence (Task W098): pjsua2 only applies presConfig at
        // account-creation time — toggling AppSettings::enablePresencePublish
        // later requires a fresh registration (re-create the account) to
        // take effect. See docs/presence.md for why this is marked
        // experimental rather than a fully dynamic setting.
        config.presConfig.publishEnabled = AppSettings::enablePresencePublish();
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP RTT text config: redundancyLevel=%1 (RED RFC 4103/2198; "
                           "max=%2; negotiated level subject to SDP answer)")
                .arg(config.textConfig.redundancyLevel)
                .arg(kRttRedLevelMax));

        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP REGISTER create account: idUri=%1 registrar=%2 transportId=%3 authUser=%4 proxy=%5")
                .arg(QString::fromStdString(config.idUri),
                     QString::fromStdString(config.regConfig.registrarUri))
                .arg(transportId)
                .arg(username,
                     proxy.isEmpty() ? QStringLiteral("(none)") : ensureSipUri(proxy)));
        m_impl->accountConfig = config;
        m_impl->account = new Impl::Account(m_impl);
        m_impl->account->create(config, true);
        return true;
    } catch (const pj::Error &e) {
        delete m_impl->account;
        m_impl->account = nullptr;
        postRegistrationResult(RegistrationState::RegistrationFailed,
                               QString::fromStdString(e.reason),
                               static_cast<int>(e.status));
        return false;
    }
#else
    Q_UNUSED(profile)
    Q_UNUSED(password)
    Q_UNUSED(transportId)
    postRegistrationResult(RegistrationState::RegistrationFailed,
                           QStringLiteral("PJSIP is unavailable; registration was not attempted"),
                           0);
    return false;
#endif
}

bool SipAccount::startUnregistration()
{
#ifdef HAVE_PJSIP
    if (!m_impl->account) {
        postRegistrationResult(RegistrationState::Unregistered,
                               QStringLiteral("No account is registered"), 0);
        return true;
    }

    try {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP REGISTER unregister account for profile %1").arg(m_profileId));
        m_impl->account->setRegistration(false);
        return true;
    } catch (const pj::Error &e) {
        postRegistrationResult(RegistrationState::RegistrationFailed,
                               QString::fromStdString(e.reason),
                               static_cast<int>(e.status));
        return false;
    }
#else
    postRegistrationResult(RegistrationState::Unregistered,
                           QStringLiteral("Stub backend is already unregistered"), 0);
    return true;
#endif
}

bool SipAccount::refreshRegistration()
{
#ifdef HAVE_PJSIP
    if (!m_impl->account) {
        postRegistrationResult(RegistrationState::RegistrationFailed,
                               QStringLiteral("No active account to refresh"), 0);
        return false;
    }
    try {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP REGISTER refresh account for profile %1").arg(m_profileId));
        m_impl->account->setRegistration(true);
        return true;
    } catch (const pj::Error &e) {
        postRegistrationResult(RegistrationState::RegistrationFailed,
                               QString::fromStdString(e.reason),
                               static_cast<int>(e.status));
        return false;
    }
#else
    postRegistrationResult(RegistrationState::RegistrationFailed,
                           QStringLiteral("PJSIP unavailable; registration refresh not attempted"),
                           0);
    return false;
#endif
}

bool SipAccount::applyVideoSettings()
{
#ifdef HAVE_PJSIP
#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
    if (!m_impl->account) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Video settings not applied to PJSIP: account not ready"));
        return false;
    }

    try {
        pj::AccountConfig cfg = m_impl->accountConfig;
        const bool hasCaptureDev = hasPjsipVideoCaptureDevice();
        const pjmedia_vid_dev_index defaultCaptureDev = preferredPjsipVideoCaptureDevice();

        if (defaultCaptureDev != PJMEDIA_VID_INVALID_DEV)
            cfg.videoConfig.defaultCaptureDevice = defaultCaptureDev;
        cfg.videoConfig.autoTransmitOutgoing = hasCaptureDev;
        cfg.videoConfig.autoShowIncoming = false;
#if defined(_WIN32)
        const pjmedia_vid_dev_index gdiRenderDev = PjsipGdiRenderer::deviceIndex();
        if (gdiRenderDev != PJMEDIA_VID_INVALID_DEV)
            cfg.videoConfig.defaultRenderDevice = gdiRenderDev;
#endif

        m_impl->account->modify(cfg);
        m_impl->accountConfig = cfg;
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP video settings applied: defaultCaptureDevice=%1 autoTransmitOutgoing=%2")
                .arg(static_cast<int>(cfg.videoConfig.defaultCaptureDevice))
                .arg(cfg.videoConfig.autoTransmitOutgoing ? QStringLiteral("true") : QStringLiteral("false")));
        return true;
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Video settings not applied to PJSIP: %1")
                .arg(QString::fromStdString(e.reason)));
        return false;
    }
#else
    return false;
#endif
#else
    return false;
#endif
}

void *SipAccount::pjAccountHandle() const
{
#ifdef HAVE_PJSIP
    return m_impl->account; // Impl::Account IS-A pj::Account
#else
    return nullptr;
#endif
}

bool SipAccount::sendMessage(const QString &toUri, const QString &contentType, const QString &body,
                             const QList<QPair<QString, QString>> &extraHeaders,
                             qint64 correlationId, QString &error)
{
#ifdef HAVE_PJSIP
    if (!m_impl->account) {
        error = QStringLiteral("No active SIP account");
        return false;
    }

    try {
        // A transient, non-subscribing Buddy is pjsua2's only out-of-dialog
        // IM sender (Account itself has no sendInstantMessage). subscribe =
        // false means no presence/dialog-event SUBSCRIBE is ever started —
        // this object exists only to route+authenticate one MESSAGE request.
        pj::BuddyConfig cfg;
        cfg.uri = toUri.toStdString();
        cfg.subscribe = false;
        cfg.subscribe_dlg_event = false;

        pj::Buddy buddy;
        buddy.create(*m_impl->account, cfg);

        pj::SendInstantMessageParam prm;
        prm.contentType = contentType.toStdString();
        prm.content     = body.toStdString();
        // Round-tripped through onInstantMessageStatus's userData — a plain
        // integer id, not a heap pointer, so there is nothing to free.
        prm.userData = reinterpret_cast<void *>(static_cast<intptr_t>(correlationId));
        for (const auto &hdr : extraHeaders) {
            pj::SipHeader sh;
            sh.hName  = hdr.first.toStdString();
            sh.hValue = hdr.second.toStdString();
            prm.txOption.headers.push_back(sh);
        }

        buddy.sendInstantMessage(prm);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP SIP MESSAGE submitted: to=%1 contentType=%2 bodyBytes=%3 correlationId=%4")
                .arg(toUri, contentType).arg(body.toUtf8().size()).arg(correlationId));
        return true;
    } catch (const pj::Error &e) {
        error = QString::fromStdString(e.reason);
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("PJSIP SIP MESSAGE send failed: to=%1 error=%2").arg(toUri, error));
        return false;
    }
#else
    Q_UNUSED(toUri)
    Q_UNUSED(contentType)
    Q_UNUSED(body)
    Q_UNUSED(extraHeaders)
    Q_UNUSED(correlationId)
    error = QStringLiteral("PJSIP is unavailable; SIP MESSAGE was not sent");
    return false;
#endif
}

bool SipAccount::subscribePresence(const QString &targetUri, QString &error)
{
#ifdef HAVE_PJSIP
    if (!m_impl->account) {
        error = QStringLiteral("No active SIP account");
        return false;
    }
    if (m_impl->presenceBuddies.contains(targetUri)) {
        error = QStringLiteral("Already subscribed to this URI");
        return false;
    }

    try {
        pj::BuddyConfig cfg;
        cfg.uri = targetUri.toStdString();
        cfg.subscribe = true;
        cfg.subscribe_dlg_event = false;

        auto *buddy = new Impl::PresenceBuddy(m_impl, targetUri);
        buddy->create(*m_impl->account, cfg);
        m_impl->presenceBuddies.insert(targetUri, buddy);

        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP Presence SUBSCRIBE started: target=%1").arg(targetUri));
        return true;
    } catch (const pj::Error &e) {
        error = QString::fromStdString(e.reason);
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("PJSIP Presence SUBSCRIBE failed: target=%1 error=%2").arg(targetUri, error));
        return false;
    }
#else
    Q_UNUSED(targetUri)
    error = QStringLiteral("PJSIP is unavailable; presence subscription was not attempted");
    return false;
#endif
}

bool SipAccount::unsubscribePresence(const QString &targetUri, QString &error)
{
#ifdef HAVE_PJSIP
    auto it = m_impl->presenceBuddies.find(targetUri);
    if (it == m_impl->presenceBuddies.end()) {
        error = QStringLiteral("Not subscribed to this URI");
        return false;
    }

    try {
        (*it)->subscribePresence(false);
    } catch (const pj::Error &e) {
        // Still tear down our local Buddy even if the unsubscribe request
        // itself failed to send (e.g. transport already gone) — we must not
        // leak a Buddy the caller believes is gone.
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("PJSIP Presence unsubscribe request failed: target=%1 error=%2")
                .arg(targetUri, QString::fromStdString(e.reason)));
    }

    delete it.value();
    m_impl->presenceBuddies.erase(it);
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("PJSIP Presence SUBSCRIBE stopped: target=%1").arg(targetUri));
    return true;
#else
    Q_UNUSED(targetUri)
    error = QStringLiteral("PJSIP is unavailable; presence subscription was not attempted");
    return false;
#endif
}

bool SipAccount::refreshPresenceSubscription(const QString &targetUri, QString &error)
{
#ifdef HAVE_PJSIP
    auto it = m_impl->presenceBuddies.find(targetUri);
    if (it == m_impl->presenceBuddies.end()) {
        error = QStringLiteral("Not subscribed to this URI");
        return false;
    }
    try {
        (*it)->updatePresence();
        return true;
    } catch (const pj::Error &e) {
        error = QString::fromStdString(e.reason);
        return false;
    }
#else
    Q_UNUSED(targetUri)
    error = QStringLiteral("PJSIP is unavailable; presence refresh was not attempted");
    return false;
#endif
}

bool SipAccount::setOwnPresenceState(const QString &basicStatus, const QString &activity,
                                     const QString &note, QString &error)
{
#ifdef HAVE_PJSIP
    if (!m_impl->account) {
        error = QStringLiteral("No active SIP account");
        return false;
    }
    try {
        pj::PresenceStatus st;
        st.status = basicStatus.compare(QStringLiteral("open"), Qt::CaseInsensitive) == 0
            ? PJSUA_BUDDY_STATUS_ONLINE : PJSUA_BUDDY_STATUS_OFFLINE;
        st.statusText = activity.toStdString();
        st.note = note.toStdString();
        if (activity.compare(QStringLiteral("away"), Qt::CaseInsensitive) == 0)
            st.activity = PJRPID_ACTIVITY_AWAY;
        else if (activity.compare(QStringLiteral("busy"), Qt::CaseInsensitive) == 0
                 || activity.compare(QStringLiteral("do-not-disturb"), Qt::CaseInsensitive) == 0)
            st.activity = PJRPID_ACTIVITY_BUSY;
        else
            st.activity = PJRPID_ACTIVITY_UNKNOWN;

        m_impl->account->setOnlineStatus(st);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("PJSIP own presence status set: basic=%1 activity=%2 publishEnabled=%3")
                .arg(basicStatus, activity,
                     m_impl->accountConfig.presConfig.publishEnabled ? QStringLiteral("true") : QStringLiteral("false")));
        return true;
    } catch (const pj::Error &e) {
        error = QString::fromStdString(e.reason);
        return false;
    }
#else
    Q_UNUSED(basicStatus)
    Q_UNUSED(activity)
    Q_UNUSED(note)
    error = QStringLiteral("PJSIP is unavailable; presence status was not set");
    return false;
#endif
}

void SipAccount::postRegistrationResult(RegistrationState state,
                                        const QString &statusText,
                                        int statusCode,
                                        int expirySeconds)
{
    QPointer<SipAccount> self(this);
    QMetaObject::invokeMethod(this, [self, state, statusText, statusCode, expirySeconds]() {
        if (self) {
            if (expirySeconds > 0)
                emit self->registrationExpiryReceived(expirySeconds);
            emit self->registrationStateChanged(state, statusText, statusCode);
        }
    }, Qt::QueuedConnection);
}
