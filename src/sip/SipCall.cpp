#include "SipCall.h"

SipCall::SipCall(QObject *parent)
    : QObject(parent)
{
}

SipCall::~SipCall() = default;

SipCall::State SipCall::state()     const { return m_state;     }
QString        SipCall::remoteUri() const { return m_remoteUri; }
QString        SipCall::callId()    const { return m_callId;    }
