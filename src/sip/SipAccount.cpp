#include "SipAccount.h"

#include <QMetaObject>
#include <QPointer>

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
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

#ifdef HAVE_PJSIP
    class Account final : public pj::Account
    {
    public:
        explicit Account(Impl *implementation) : m_impl(implementation) {}

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

            if (m_impl)
                m_impl->notify(state, text, code, expiry);
        }

    private:
        Impl *m_impl;
    };

    Account *account{nullptr};
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
#endif
    delete m_impl;
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
