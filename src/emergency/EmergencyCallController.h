#pragma once

#include <QObject>

#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyCallStateMachine.h"
#include "emergency/EmergencyLocationProvider.h"

// Orchestrates emergency call preparation.
//
// Task 34 (this skeleton): validates profile, requests location, drives the SM,
// emits readyToDial() when preparation is complete.
// Does NOT place a real SIP INVITE — that is Task 36.
//
// Ownership:
//   - EmergencyCallController does NOT take ownership of the location provider.
//     The caller must ensure the provider outlives the controller, or call
//     setLocationProvider(nullptr) before the provider is destroyed.
class EmergencyCallController : public QObject
{
    Q_OBJECT
public:
    explicit EmergencyCallController(QObject *parent = nullptr);
    ~EmergencyCallController() override;

    // Replace the location provider. Pass nullptr to revert to NullLocationProvider.
    // The controller does NOT take ownership.
    void setLocationProvider(EmergencyLocationProvider *provider);
    EmergencyLocationProvider *locationProvider() const;

    void setProfile(const EmergencyCallProfile &profile);
    const EmergencyCallProfile &profile() const;

    // Begin preparation: validate profile → request location → emit readyToDial().
    // Returns false immediately if the profile is invalid (emits preparationFailed).
    // Safe to call when state is Idle; aborts and restarts if already in progress.
    bool prepare();

    // Abort any in-progress preparation. Transitions SM to Idle.
    void abort(const QString &reason = {});

    EmergencyCallState         state()        const;
    EmergencyCallStateMachine &stateMachine();

signals:
    void stateChanged(EmergencyCallState state);
    void readyToDial(const EmergencyCallProfile &profile);
    void preparationFailed(const QString &reason);
    void aborted(const QString &reason);

private slots:
    void onLocationStatusChanged(LocationStatus status);

private:
    void connectProvider(EmergencyLocationProvider *provider);
    void disconnectProvider(EmergencyLocationProvider *provider);
    void finishPreparation();

    EmergencyCallProfile       m_profile;
    EmergencyCallStateMachine  m_stateMachine;
    EmergencyLocationProvider *m_locationProvider{nullptr};
    NullLocationProvider      *m_nullProvider{nullptr};
};
