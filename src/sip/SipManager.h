#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "sip/SipAccount.h"
#include "sip/SipCall.h"
#include "sip/SipCallOptions.h"
#include "sip/RegistrationStateMachine.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/RegistrationRefreshConfig.h"
#include "rtt/RttSession.h"

// Owns the SIP endpoint lifecycle and the account for the active profile.
// Registration flow is governed by RegistrationStateMachine; invalid operations
// (e.g. register while Registering) are rejected without side effects.
// Credentials are fetched from CredentialStore only for a registration attempt
// and are never retained by SipManager, logged, or persisted.
class SipManager : public QObject
{
    Q_OBJECT
public:
    static SipManager &instance();

    bool initialize();
    void shutdown();

    bool registerActiveProfile();
    bool unregisterActiveProfile();

    // Safely switch the active profile. If currently registered, sends UNREGISTER
    // for the old account and waits for the callback before creating the new account.
    // Returns false if a switch is already pending (rejected).
    bool switchActiveProfile(const QString &newProfileId);
    bool isSwitchingProfile() const;

    // ---- Call control -------------------------------------------------------
    // Initiate an outgoing call. Returns false if a call is already active or
    // if remoteUri is empty.
    bool makeCall(const QString &remoteUri);

    // Emergency variant: outgoing call with per-call SIP options (custom headers,
    // media policy).  Use EmergencyCallAdapter::toSipCallOptions() to build the
    // options from an EmergencyInvite.  Returns false if a call is already active.
    bool makeEmergencyCall(const QString &remoteUri, const SipCallOptions &options);

    // Answer an incoming call. Returns false if no incoming call is pending.
    bool answerCall();

    // Reject an incoming call (sends 486 Busy Here). Returns false if no
    // incoming call is pending.
    bool rejectCall();

    // Hang up the active call. No-op if no call is active.
    bool hangupCall();

    // Place the active call on hold. No-op if call is not Active.
    bool holdCall();

    // Resume a held call. No-op if call is not Held.
    bool resumeCall();

    // Mute / unmute the local microphone on the active call.
    bool setCallMuted(bool muted);
    bool isCallMuted() const;

    // Stop / resume sending video on the active call.
    bool setCallVideoMuted(bool muted);
    bool isCallVideoMuted() const;

    // Send RTT text via the active call's T.140 text stream.
    // No-op if no call is active or RTT is not negotiated.
    void sendRttText(const QString &text);

    // Send a SIP UPDATE with an updated PIDF-LO body on the active emergency call.
    // opts.emergencyCall must be true; active call must be in Active state.
    // Returns false if no active call, call is not Active, or PJSIP is unavailable.
    bool sendEmergencyLocationUpdate(const SipCallOptions &opts);

    // Access the RTT session for the current call (never null).
    RttSession *rttSession();

    CallState callState()          const;
    QString   callStatusText()     const;
    QString   activeCallRemoteUri() const;
    // -------------------------------------------------------------------------

    bool    isInitialized()        const;
    bool    isPjsipAvailable()     const;
    QString backendName()          const;
    QString lastError()            const;

    RegistrationState registrationState()       const;
    QString           registrationStatusText()  const;
    int               registrationStatusCode()  const;
    QString           registeredProfileId()     const;

    void                         setRetryPolicy(const RegistrationRetryPolicy &policy);
    const RegistrationRetryPolicy &retryPolicy() const;
    int                          retryAttempt() const;

    // Schedules the re-REGISTER refresh timer for the given expiry interval (seconds).
    // expirySeconds <= 0 uses m_refreshConfig.defaultExpirySeconds.
    // Public so tests can drive it directly without a live PJSIP account.
    void scheduleRefresh(int expirySeconds);

    void                            setRefreshConfig(const RegistrationRefreshConfig &config);
    const RegistrationRefreshConfig &refreshConfig() const;
    int                             registrationExpirySeconds() const;

    // Exposes the state machine for testing (timeout injection, spy connections).
    RegistrationStateMachine &stateMachine();

signals:
    void initialized();
    void shutdownComplete();
    void initializationFailed(const QString &reason);
    void registrationStateChanged(RegistrationState state,
                                  const QString     &statusText,
                                  int                statusCode);
    void registrationExpiryChanged(int seconds);
    void retryScheduled(int attempt, int delayMs);
    void refreshScheduled(int delayMs);
    void refreshStarted();
    void profileSwitchStarted(const QString &newProfileId);
    void profileSwitchCompleted(const QString &newProfileId);
    void profileSwitchFailed(const QString &newProfileId, const QString &reason);

    // Call signals
    void callStateChanged(CallState state, const QString &statusText, int statusCode);
    void incomingCall(const QString &remoteUri);
    void callConnected(const QString &remoteUri);
    void callDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void callFailed(const QString &remoteUri, const QString &reason, int statusCode);

    // Audio media signals (forwarded from the active SipCall)
    void audioMediaConnected();
    void audioMediaDisconnected();
    void callMuteChanged(bool muted);
    void callInputLevelChanged(int level);
    void callOutputLevelChanged(int level);

    // Video media signals (forwarded from the active SipCall)
    void videoMediaConnected();
    void videoMediaDisconnected();
    void callVideoMuteChanged(bool muted);
    void localVideoStarted();
    void localVideoStopped();
    void remoteVideoStarted();
    void remoteVideoStopped();

    // RTT media signals (forwarded from the active SipCall)
    void rttMediaConnected();
    void rttMediaDisconnected();
    void rttTextReceived(const QString &text);

private slots:
    void onAccountRegistrationStateChanged(RegistrationState state,
                                           const QString     &statusText,
                                           int                statusCode);
    void onAccountRegistrationExpiryReceived(int seconds);
    void onStateMachineTimedOut(RegistrationState stuckState);
    void onRetryTimerFired();
    void onRefreshTimerFired();
    void onAccountIncomingCall(const QString &remoteUri);
    void onActiveCallStateChanged(CallState state, const QString &statusText, int statusCode);

private:
    SipManager();
    ~SipManager() override;

    void destroyAccount();
    void scheduleRetryIfEligible(int statusCode);
    void completePendingSwitch();
    void destroyActiveCall();

    // Shared setup for makeCall() and makeEmergencyCall():
    // guards, creates m_activeCall, wires signals, sets PJSIP handle.
    // Returns false if rejected (active call, empty URI).
    bool prepareOutgoingCall(const QString &remoteUri);

    // Reads the persisted mic/speaker selection from MediaDeviceSelectionModel
    // and applies it to PJSIP AudDevManager. No-op when PJSIP is not active.
    void applyPersistedAudioDevices();

#ifdef HAVE_PJSIP
public:
    struct PjEndpoint;

private:
    bool ensureTransport(SipTransport transport, int &transportId, QString &error);

    PjEndpoint *m_ep{nullptr};
#endif

    SipCall                 *m_activeCall{nullptr};
    RttSession               m_rttSession;
    bool                     m_initialized{false};
    QString                  m_lastError;
    SipAccount              *m_account{nullptr};
    QString                  m_registeredProfileId;
    QString                  m_pendingProfileId;
    RegistrationStateMachine m_stateMachine;
    RegistrationRetryPolicy  m_retryPolicy;
    QTimer                   m_retryTimer;
    int                      m_retryAttempt{0};
    RegistrationRefreshConfig m_refreshConfig;
    QTimer                    m_refreshTimer;
    int                       m_registrationExpirySeconds{0};
    bool                      m_refreshing{false};
};
