#include "rtt/RttSession.h"
#include "sip/SipCall.h"
#include "core/Logger.h"

QString rttStateName(RttState state)
{
    switch (state) {
    case RttState::Disabled:           return QStringLiteral("Disabled");
    case RttState::RemoteOfferPending: return QStringLiteral("RemoteOfferPending");
    case RttState::LocalOfferPending:  return QStringLiteral("LocalOfferPending");
    case RttState::Negotiating:        return QStringLiteral("Negotiating");
    case RttState::Active:             return QStringLiteral("Active");
    case RttState::Rejected:           return QStringLiteral("Rejected");
    case RttState::Failed:             return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

RttSession::RttSession(QObject *parent)
    : QObject(parent)
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT session controller created"));

    m_suppressedLogTimer.setInterval(5000);
    m_suppressedLogTimer.setSingleShot(false);
    connect(&m_suppressedLogTimer, &QTimer::timeout, this, &RttSession::flushSuppressedLog);

    m_negotiationTimeoutTimer.setSingleShot(true);
    m_negotiationTimeoutTimer.setInterval(kNegotiationTimeoutMs);
    connect(&m_negotiationTimeoutTimer, &QTimer::timeout, this, [this]() {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("RTT negotiation timed out (state=%1) after %2 ms")
                .arg(rttStateName(m_state)).arg(kNegotiationTimeoutMs));
        // SipCall no longer clears its own "offer in flight" guard on the
        // expected first auto-decline (see SipCall::onCallState) — this
        // timeout is now the only place that gives up on an unanswered local
        // RTT offer, so it must tell SipCall to release the guard too, or a
        // subsequent requestRtt()/acceptIncomingRttRequest() would stay
        // permanently refused as "already in flight".
        if (m_call)
            m_call->cancelPendingRttRequest();
        setState(RttState::Failed);
    });
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
    connect(call, &SipCall::rttRequested, this, [this]() {
        onIncomingRttRequest();
    });
    connect(call, &SipCall::rttRequestRejected, this, [this]() {
        onIncomingRttRejected();
    });
    connect(call, &SipCall::rttNegotiationFailed, this, [this](const QString &reason) {
        onNegotiationFailed(reason);
    });
    connect(call, &SipCall::rttLocalOfferSent, this, [this]() {
        onLocalOfferSent();
    });
    connect(call, &SipCall::rttTextReceived, this, [this](const QString &text) {
        // Suppress only truly empty keepalive packets. Whitespace and control
        // characters (space, CR, LF, BS) are REAL T.140 payload — trimming
        // them here would swallow spaces between words and the CR that
        // flushes a message to the transcript.
        if (text.isEmpty()) {
            ++m_suppressedEmptyRtt;
            if (!m_suppressedLogTimer.isActive())
                m_suppressedLogTimer.start();
            return;
        }
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

    setState(RttState::LocalOfferPending);
}

void RttSession::disable()
{
    if (!m_call.isNull()) {
        m_call->disconnect(this);
    }
    m_call = nullptr;
    flushSuppressedLog();
    m_suppressedLogTimer.stop();
    m_negotiationTimeoutTimer.stop();
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
                QStringLiteral("RTT text media deactivated — session Negotiating"));
            setState(RttState::Negotiating);
        }
    }
}

void RttSession::onIncomingRttRequest()
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT incoming request — awaiting user accept/reject"));
    setState(RttState::RemoteOfferPending);
}

void RttSession::onIncomingRttRejected()
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT incoming request rejected by user"));
    setState(RttState::Rejected);
}

void RttSession::onNegotiationFailed(const QString &reason)
{
    Logger::instance().warn(LogCategory::Sip,
        QStringLiteral("RTT negotiation failed: %1").arg(reason));
    setState(RttState::Failed);
}

void RttSession::onLocalOfferSent()
{
    setState(RttState::LocalOfferPending);
}

void RttSession::onCallEnded()
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("RTT call ended — cleaning up session"));
    if (!m_call.isNull()) {
        m_call->disconnect(this);
    }
    m_call = nullptr;
    flushSuppressedLog();
    m_suppressedLogTimer.stop();
    m_negotiationTimeoutTimer.stop();
    setState(RttState::Disabled);
}

void RttSession::flushSuppressedLog()
{
    if (m_suppressedEmptyRtt > 0) {
        Logger::instance().debug(LogCategory::Sip,
            QStringLiteral("RTT: suppressed %1 empty keepalive packet(s)")
                .arg(m_suppressedEmptyRtt));
        m_suppressedEmptyRtt = 0;
    }
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

    // The negotiation timeout only applies while we are waiting on a result
    // (a request we made, local or remote-accept); any other state means the
    // outcome is already known.
    if (newState == RttState::LocalOfferPending)
        m_negotiationTimeoutTimer.start();
    else
        m_negotiationTimeoutTimer.stop();

    emit rttStateChanged(newState);
}
