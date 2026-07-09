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
        void onInstantMessage(pj::OnInstantMessageParam &prm) override
        {
            if (!m_impl || !m_impl->owner)
                return;

            QString callId;
            try {
                if (prm.rdata.pjRxData) {
                    auto *rd = static_cast<pjsip_rx_data *>(prm.rdata.pjRxData);
                    if (rd && rd->msg_info.cid)
                        callId = QString::fromLatin1(rd->msg_info.cid->id.ptr,
                                                     static_cast<int>(rd->msg_info.cid->id.slen));
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
                [self, fromUri, toUri, contactUri, contentType, body, callId, profileId]() {
                    if (self)
                        emit self->instantMessageReceived(fromUri, toUri, contactUri, contentType,
                                                           body, callId, profileId);
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

    private:
        Impl *m_impl;
    };

    Account *account{nullptr};
    QMap<int, EarlyCall *> earlyCalls; // callId → EarlyCall*, keyed by PJSIP call_id
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

        // RTT / T.140 text media configuration (RFC 4103 + RFC 2198 RED).
        // redundancyLevel controls how many previous T.140 packets are
        // retransmitted with each PDU. PJSIP default is already
        // PJSUA_TXT_DEFAULT_REDUNDANCY_LEVEL=2, but we set it explicitly so
        // the intent is clear and logged. The negotiated level may be lower if
        // the remote peer SDP does not advertise red/90000.
        config.textConfig.redundancyLevel = kRttRedLevelDefault;
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
