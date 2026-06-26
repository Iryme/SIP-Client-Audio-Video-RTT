#include "rtt/RttSession.h"
#include "sip/SipCall.h"
#include "core/Logger.h"

QString rttStateName(RttState state)
{
    switch (state) {
    case RttState::Disabled:   return QStringLiteral("Disabled");
    case RttState::Offered:    return QStringLiteral("Offered");
    case RttState::Negotiated: return QStringLiteral("Negotiated");
    case RttState::Active:     return QStringLiteral("Active");
    case RttState::Failed:     return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

RttSession::RttSession(QObject *parent)
    : QObject(parent)
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT session controller created"));
}

RttSession::~RttSession()
{
    // QPointer nulls automatically if SipCall was already destroyed.
    // Only disconnect if the object is still alive.
    if (!m_call.isNull())
        m_call->disconnect(this);
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT session controller destroyed"));
}

void RttSession::enableForCall(SipCall *call)
{
    if (m_call) {
        disable();
    }
    if (!call) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("RTT enableForCall: null call passed"));
        return;
    }
    m_call = call;

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT enable for call: id=%1").arg(call->callId()));

    connect(call, &SipCall::rttMediaConnected, this, [this]() {
        onCallMediaStateChanged(true);
    });
    connect(call, &SipCall::rttMediaDisconnected, this, [this]() {
        onCallMediaStateChanged(false);
    });
    connect(call, &SipCall::rttTextReceived, this, [this](const QString &text) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("RTT remote text received: \"%1\"").arg(text));
        emit remoteTextReceived(text);
    });
    connect(call, &SipCall::callDisconnected, this, [this](const QString &, const QString &, int) {
        onCallEnded();
    });
    connect(call, &SipCall::callFailed, this, [this](const QString &, const QString &, int) {
        onCallEnded();
    });

    setState(RttState::Offered);
}

void RttSession::disable()
{
    if (!m_call.isNull()) {
        m_call->disconnect(this);
    }
    m_call = nullptr;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT session disabled"));
    setState(RttState::Disabled);
}

void RttSession::sendText(const QString &text)
{
    if (m_state != RttState::Active) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("RTT sendText while not active (state=%1) — text dropped")
                .arg(rttStateName(m_state)));
        return;
    }
    if (m_call.isNull()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("RTT sendText: no active call"));
        return;
    }
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT sendText: \"%1\"").arg(text));
    emit localTextQueued(text);
    m_call->sendRttText(text);
}

void RttSession::onCallMediaStateChanged(bool textMediaActive)
{
    if (textMediaActive) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("RTT text media active — session Active"));
        setState(RttState::Active);
    } else {
        if (m_state == RttState::Active) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("RTT text media deactivated — session Negotiated"));
            setState(RttState::Negotiated);
        }
    }
}

void RttSession::onCallEnded()
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT call ended — cleaning up session"));
    if (!m_call.isNull()) {
        m_call->disconnect(this);
    }
    m_call = nullptr;
    setState(RttState::Disabled);
}

RttState RttSession::state() const
{
    return m_state;
}

bool RttSession::isActive() const
{
    return m_state == RttState::Active;
}

void RttSession::setState(RttState newState)
{
    if (m_state == newState)
        return;
    const RttState prev = m_state;
    m_state = newState;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT state: %1 -> %2")
            .arg(rttStateName(prev), rttStateName(newState)));
    emit rttStateChanged(newState);
}
