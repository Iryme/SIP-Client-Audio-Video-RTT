#include "SipAccount.h"

SipAccount::SipAccount(const QString &profileId, QObject *parent)
    : QObject(parent)
    , m_profileId(profileId)
{
}

SipAccount::~SipAccount() = default;

QString SipAccount::profileId() const
{
    return m_profileId;
}
