#include "msrp/MsrpCallPreparationController.h"

#include <QPointer>
#include <QTimer>

#include "msrp/MsrpCallPreparationDiagnosticsEvent.h"
#include "msrp/MsrpCallPreparationDiagnosticsStore.h"
#include "msrp/MsrpRelayClient.h"

namespace {
QString transportModeToString(MsrpCallTransportMode mode)
{
    switch (mode) {
    case MsrpCallTransportMode::Direct: return QStringLiteral("direct");
    case MsrpCallTransportMode::Relay:  return QStringLiteral("relay");
    default:                            return QStringLiteral("none");
    }
}

void logPreparationEvent(const QString &preparationId, MsrpCallPreparationController::State state,
                          const QString &mode = QString(), const QString &allocationId = QString(),
                          const QString &reason = QString())
{
    MsrpCallPreparationDiagnosticsEvent ev;
    ev.timestamp = QDateTime::currentDateTimeUtc();
    ev.preparationId = preparationId;
    ev.state = state;
    ev.mode = mode;
    ev.allocationId = allocationId;
    ev.reason = reason;
    MsrpCallPreparationDiagnosticsStore::instance().record(ev);
}
} // namespace

MsrpCallPreparationController::MsrpCallPreparationController(QObject *parent)
    : QObject(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
}

MsrpCallPreparationController::~MsrpCallPreparationController()
{
    teardownRelayClient();
}

void MsrpCallPreparationController::transitionTo(State s)
{
    m_state = s;
    emit stateChanged(s);
}

void MsrpCallPreparationController::teardownRelayClient()
{
    if (m_relayClient) {
        m_relayClient->close();
        m_relayClient.reset(); // deleteLater() deleter — see makeDeleteLaterSharedPtr
    }
}

void MsrpCallPreparationController::start(bool wantsMsrp, const MsrpRelayConfig &relayConfig,
                                           const QString &relayPassword, const QString &preparationId,
                                           int preparationTimeoutMs)
{
    // A new attempt always invalidates any preparation still in flight —
    // its relay client's signals will be ignored (generation mismatch) and
    // it is torn down here rather than left to finish unobserved.
    ++m_generation;
    teardownRelayClient();
    m_timeoutTimer->stop();

    m_wantsMsrp = wantsMsrp;
    m_relayConfig = relayConfig;
    m_relayPassword = relayPassword;
    m_preparationId = preparationId;

    transitionTo(State::ResolvingPolicy);
    logPreparationEvent(m_preparationId, State::ResolvingPolicy);

    const int generation = m_generation;
    QPointer<MsrpCallPreparationController> self(this);

    if (!wantsMsrp || relayConfig.mode == MsrpRelayMode::Disabled || !relayConfig.isUsable()) {
        // Deferred even though the outcome is already known — see class
        // comment: start() never emits synchronously.
        QTimer::singleShot(0, this, [self, generation]() {
            if (!self || generation != self->m_generation)
                return;
            self->resolveDirect(self->m_wantsMsrp ? MsrpCallTransportMode::Direct
                                                    : MsrpCallTransportMode::None);
        });
        return;
    }

    QTimer::singleShot(0, this, [self, generation]() {
        if (!self || generation != self->m_generation)
            return;
        self->startRelayAllocation();
    });

    if (preparationTimeoutMs > 0) {
        m_timeoutTimer->start(preparationTimeoutMs);
        connect(m_timeoutTimer, &QTimer::timeout, this, [self, generation]() {
            if (!self || generation != self->m_generation)
                return;
            if (self->m_state == State::Ready || self->m_state == State::Failed
                    || self->m_state == State::Cancelled)
                return;
            self->transitionTo(State::Expired);
            self->finishFailed(QStringLiteral("relay allocation timed out"));
        });
    }
}

void MsrpCallPreparationController::resolveDirect(MsrpCallTransportMode mode)
{
    PreparedMsrpOffer offer;
    offer.ready = true;
    offer.mode = mode;
    offer.preparationId = m_preparationId;
    if (mode == MsrpCallTransportMode::Direct)
        transitionTo(State::PreparingDirectMsrp);
    transitionTo(State::Ready);
    logPreparationEvent(m_preparationId, State::Ready, transportModeToString(mode));
    emit ready(offer);
}

void MsrpCallPreparationController::startRelayAllocation()
{
    transitionTo(State::AllocatingRelay);

    auto *client = new MsrpRelayClient();
    m_relayClient = makeDeleteLaterSharedPtr(client);
    client->setCorrelation(m_preparationId, -1);
    client->configure(m_relayConfig, m_relayPassword);

    const int generation = m_generation;
    QPointer<MsrpCallPreparationController> self(this);

    connect(client, &MsrpRelayClient::allocationReady, this,
        [self, generation](const MsrpRelayAllocation &allocation) {
            if (!self || generation != self->m_generation)
                return;
            self->finishReady(MsrpCallTransportMode::Relay, allocation.allocatedUri(), allocation);
        });
    connect(client, &MsrpRelayClient::allocationFailed, this,
        [self, generation](const QString &reason) {
            if (!self || generation != self->m_generation)
                return;
            self->finishFailed(reason);
        });

    client->start();
}

void MsrpCallPreparationController::finishReady(MsrpCallTransportMode mode, const MsrpUri &uri,
                                                 const MsrpRelayAllocation &allocation)
{
    m_timeoutTimer->stop();
    PreparedMsrpOffer offer;
    offer.ready = true;
    offer.mode = mode;
    offer.advertisedUri = uri;
    offer.relayAllocation = allocation;
    offer.relayClient = m_relayClient; // ownership shared with the caller from here on
    offer.preparationId = m_preparationId;
    transitionTo(State::Ready);
    logPreparationEvent(m_preparationId, State::Ready, transportModeToString(mode),
                        allocation.allocationId);
    emit ready(offer);
}

void MsrpCallPreparationController::finishFailed(const QString &reason)
{
    m_timeoutTimer->stop();
    teardownRelayClient();

    // Required: no valid allocation means preparation itself fails — the
    // caller must never build an MSRP offer around an unallocated relay
    // path. Automatic: relay is optional — fall back to direct MSRP so
    // audio/video/RTT/messaging are never blocked by a relay-only failure.
    if (m_relayConfig.mode == MsrpRelayMode::Required) {
        transitionTo(State::Failed);
        logPreparationEvent(m_preparationId, State::Failed, QString(), QString(), reason);
        emit failed(reason);
        return;
    }

    logPreparationEvent(m_preparationId, State::Failed, QString(), QString(),
                        QStringLiteral("automatic fallback: %1").arg(reason));
    resolveDirect(MsrpCallTransportMode::Direct);
}

void MsrpCallPreparationController::cancel()
{
    if (m_state == State::Ready || m_state == State::Failed || m_state == State::Cancelled)
        return;
    ++m_generation; // invalidates any relay-client signal still in flight
    m_timeoutTimer->stop();
    teardownRelayClient();
    transitionTo(State::Cancelled);
    logPreparationEvent(m_preparationId, State::Cancelled);
    emit cancelled();
}
