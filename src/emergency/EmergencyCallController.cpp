#include "emergency/EmergencyCallController.h"

EmergencyCallController::EmergencyCallController(QObject *parent)
    : QObject(parent)
    , m_nullProvider(new NullLocationProvider(this))
{
    m_locationProvider = m_nullProvider;

    connect(&m_stateMachine, &EmergencyCallStateMachine::stateChanged,
            this, [this](EmergencyCallState s, EmergencyCallState, const QString &) {
                emit stateChanged(s);
            });
}

EmergencyCallController::~EmergencyCallController() = default;

void EmergencyCallController::setLocationProvider(EmergencyLocationProvider *provider)
{
    if (m_locationProvider && m_locationProvider != m_nullProvider)
        disconnectProvider(m_locationProvider);

    m_locationProvider = provider ? provider : m_nullProvider;

    if (m_locationProvider != m_nullProvider)
        connectProvider(m_locationProvider);
}

EmergencyLocationProvider *EmergencyCallController::locationProvider() const
{
    return m_locationProvider;
}

void EmergencyCallController::setProfile(const EmergencyCallProfile &profile)
{
    m_profile = profile;
}

const EmergencyCallProfile &EmergencyCallController::profile() const
{
    return m_profile;
}

bool EmergencyCallController::prepare()
{
    if (m_stateMachine.state() != EmergencyCallState::Idle)
        abort(QStringLiteral("restarted"));

    if (!m_profile.isValid()) {
        const QString reason = m_profile.validationError();
        emit preparationFailed(reason);
        return false;
    }

    m_stateMachine.transition(EmergencyCallState::Preparing,
                              QStringLiteral("prepare() called"));

    if (m_locationProvider->status() == LocationStatus::NotImplemented
        || m_locationProvider->status() == LocationStatus::Unavailable)
    {
        // No location available — skip directly to ReadyToDial (location is optional)
        m_stateMachine.transition(EmergencyCallState::ReadyToDial,
                                  QStringLiteral("location not available — skipped"));
        finishPreparation();
    } else {
        m_stateMachine.transition(EmergencyCallState::LocationPending,
                                  QStringLiteral("requesting location"));
        m_locationProvider->requestLocation();
    }

    return true;
}

void EmergencyCallController::abort(const QString &reason)
{
    m_stateMachine.reset();
    emit aborted(reason);
}

EmergencyCallState EmergencyCallController::state() const
{
    return m_stateMachine.state();
}

EmergencyCallStateMachine &EmergencyCallController::stateMachine()
{
    return m_stateMachine;
}

void EmergencyCallController::onLocationStatusChanged(LocationStatus status)
{
    if (m_stateMachine.state() != EmergencyCallState::LocationPending)
        return;

    if (status == LocationStatus::NotImplemented || status == LocationStatus::Unavailable) {
        m_stateMachine.transition(EmergencyCallState::ReadyToDial,
                                  QStringLiteral("location unavailable — proceeding without"));
        finishPreparation();
    }
}

void EmergencyCallController::connectProvider(EmergencyLocationProvider *provider)
{
    connect(provider, &EmergencyLocationProvider::locationStatusChanged,
            this, &EmergencyCallController::onLocationStatusChanged);
    connect(provider, &EmergencyLocationProvider::locationAvailable,
            this, [this](const QString &pidfLo) {
                m_profile.pidfLo = pidfLo;
                if (m_stateMachine.state() == EmergencyCallState::LocationPending) {
                    m_stateMachine.transition(EmergencyCallState::ReadyToDial,
                                              QStringLiteral("location received"));
                    finishPreparation();
                }
            });
}

void EmergencyCallController::disconnectProvider(EmergencyLocationProvider *provider)
{
    disconnect(provider, nullptr, this, nullptr);
}

void EmergencyCallController::finishPreparation()
{
    emit readyToDial(m_profile);
}
