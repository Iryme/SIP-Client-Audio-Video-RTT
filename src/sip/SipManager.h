#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include "sip/SipAccount.h"
#include "sip/SipCall.h"
#include "sip/SipCallOptions.h"
#include "sip/CallMediaOptions.h"
#include "sip/RegistrationStateMachine.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/RegistrationRefreshConfig.h"
#include "sip/ImdnInfo.h"
#include "sip/SipMessageComposer.h"
#include "rtt/RttSession.h"
#include "media/RtpStats.h"
#include "msrp/MsrpCallPreparationController.h"

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

    // Initiate an outgoing call with explicit media options (call type, audio/video/RTT).
    // Sets codec priorities accordingly before placing the call.
    bool makeCall(const QString &remoteUri, const CallMediaOptions &opts);

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

    // Request or release video / RTT mid-call via renegotiation when possible.
    bool requestCallVideo(bool enabled);
    // Local-initiated RTT request only — see SipCall::requestRtt(). Refuses
    // while an incoming RTT request is pending (see acceptIncomingRtt()/
    // rejectIncomingRtt() below).
    bool requestCallRtt(bool enabled);

    // Accepts/rejects a pending incoming RTT request (SipManager::rttRequested
    // was emitted). Distinct from requestCallRtt(): accepting sends a new
    // local re-INVITE (the incoming offer was already auto-declined and
    // cannot be un-declined); rejecting only clears the pending UI state
    // (Task W109A — see docs/rtt-offer-answer-state-machine.md).
    bool acceptIncomingRtt();
    bool rejectIncomingRtt();

    // Send RTT text via the active call's T.140 text stream.
    // No-op if no call is active or RTT is not negotiated.
    void sendRttText(const QString &text);

    // Send a SIP UPDATE with an updated PIDF-LO body on the active emergency call.
    // opts.emergencyCall must be true; active call must be in Active state.
    // Returns false if no active call, call is not Active, or PJSIP is unavailable.
    bool sendEmergencyLocationUpdate(const SipCallOptions &opts);

    // Sends an already-composed SIP MESSAGE (see SipMessageComposer). Logs a
    // synthetic outbound SipMessageTrace unconditionally (same pattern as
    // makeCall's INVITE trace) so the attempt is always visible in the SIP
    // Ladder / Messaging Diagnostics even if the underlying PJSIP send
    // fails. Non-blocking. Returns false (with `error` set) if msg is
    // invalid, no account is active, or PJSIP is unavailable.
    bool sendSipMessage(const ComposedSipMessage &msg, QString &error);

    // Task W096: sends a Displayed IMDN report for a previously-received
    // inbound Message History entry (manual "mark as read" UI action, used
    // when AppSettings::autoSendDisplayedImdn is OFF). No-op (returns
    // false) if the entry is not found, has no Message-ID, did not request
    // a display notification, or a Displayed report was already sent for
    // it.
    bool sendDisplayedImdnForEntry(qint64 inboundEntryId, QString &error);

    // ---- SIP Presence (Task W098) --------------------------------------
    // Starts/stops/refreshes a presence subscription to targetUri via the
    // active account, gated by AppSettings::enablePresence()/
    // enablePresenceSubscribe(). Returns false (with `error` set) if
    // presence/subscribe is disabled, no account is active, or PJSIP
    // rejects the request.
    bool subscribePresence(const QString &targetUri, QString &error);
    bool unsubscribePresence(const QString &targetUri, QString &error);
    bool refreshPresenceSubscription(const QString &targetUri, QString &error);

    // Sets this account's own presence status (experimental — see
    // docs/presence.md). basicStatus is "open"/"closed"; activity is one of
    // available/away/busy/do-not-disturb/offline.
    bool setOwnPresenceState(const QString &basicStatus, const QString &activity,
                             const QString &note, QString &error);

    // Access the RTT session for the current call (never null).
    RttSession *rttSession();
    RtpStatsSnapshot currentRtpStats() const;

    // Structured negotiated codec info for the active call. Invalid
    // (default-constructed, isValid() == false) when no call is active or
    // the codec has not been negotiated yet.
    AudioCodecInfo activeAudioCodecInfo() const;
    VideoCodecInfo activeVideoCodecInfo() const;

    CallState callState()          const;
    QString   callStatusText()     const;
    QString   activeCallRemoteUri() const;
    // Diagnostics accessors for the active call / SIP transport. All return
    // an empty string when the information is unavailable (no call, stub
    // mode, or transport not created yet).
    QString   activeCallSipId()    const;           // SIP Call-ID header value
    QString   activeCallDialogState() const;        // e.g. "CONFIRMED"
    QString   activeCallRemoteMediaAddress() const; // remote RTP "ip:port"
    QString   localTransportAddress() const;        // bound SIP "ip:port"
    // -------------------------------------------------------------------------

    bool    isInitialized()        const;
    bool    isPjsipAvailable()     const;
    // Returns true only if PJSIP has a usable video capture device.
    // Always false when PJMEDIA_VIDEO_DEV_HAS_DSHOW was not compiled in.
    bool    hasPjsipVideoCapture() const;
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
    void videoRequested();
    void callVideoMuteChanged(bool muted);

    // RTT request signal (forwarded from the active SipCall)
    // Emitted when the remote peer requests an RTT channel via re-INVITE.
    void rttRequested();
    void localVideoStarted();
    void localVideoStopped();
    void remoteVideoStarted();
    void remoteVideoStopped();

    // RTT media signals (forwarded from the active SipCall)
    void rttMediaConnected();
    void rttMediaDisconnected();
    void rttTextReceived(const QString &text);
    void rtpStatsChanged(const RtpStatsSnapshot &stats);

    // Task W109A — forwarded from the active SipCall. rttRequestRejected:
    // rejectIncomingRtt() cleared a pending incoming request. rttNegotiationFailed:
    // a local RTT re-INVITE (request or accept) failed — transport/port error
    // or the remote declined; audio/video are unaffected.
    void rttRequestRejected();
    void rttNegotiationFailed(const QString &reason);
    // Forwarded from SipCall: the peer withdrew a pending incoming RTT offer
    // (re-INVITE without m=text) — pending prompts must be dismissed.
    void rttRequestWithdrawn();

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
    void refreshRtpStats();

    // Task W093: routes the account's dedicated incoming-message callback
    // into MessageHistoryStore only (never back into SipTraceLogger — the
    // existing raw-trace pipeline already captures the same wire message
    // independently for the SIP Ladder / Messaging Diagnostics).
    void onAccountInstantMessageReceived(const QString &fromUri, const QString &toUri,
                                         const QString &contactUri, const QString &contentType,
                                         const QString &body, const QString &callId,
                                         const QString &profileId, const QString &messageId,
                                         const QString &dispositionNotification);
    void onAccountInstantMessageStatusReceived(qint64 correlationId, bool success,
                                               int statusCode, const QString &reason);

    // Task W098: routes the account's dedicated Buddy::onBuddyState()
    // callback into PresenceStore only. Also drives auto-resubscribe
    // backoff on subscription termination.
    void onAccountBuddyPresenceChanged(const QString &entityUri, const QString &contactUri,
                                       const QString &basicStatus, const QString &activity,
                                       const QString &statusText, const QString &note,
                                       const QString &subscriptionState, const QString &subscriptionReason,
                                       const QString &profileId);

private:
    SipManager();
    ~SipManager() override;

    void destroyAccount();
    void scheduleRetryIfEligible(int statusCode);
    void completePendingSwitch();
    void destroyActiveCall();
    void wireActiveCall(SipCall *call);

    // Shared setup for makeCall() and makeEmergencyCall():
    // guards, creates m_activeCall, wires signals, sets PJSIP handle.
    // Returns false if rejected (active call, empty URI).
    bool prepareOutgoingCall(const QString &remoteUri);

    // Task W108: shared tail end of makeCall()/makeCall(opts)/
    // makeEmergencyCall() — called after prepareOutgoingCall() has already
    // created m_activeCall. Resolves MSRP transport policy from
    // AppSettings/CredentialStore (never hardcoded) and either calls
    // m_activeCall->makeCallWithOptions() immediately (relay disabled/not
    // configured — identical to pre-W108 behavior, the overwhelmingly
    // common case) or defers it until MsrpCallPreparationController
    // resolves a relay allocation (or its configured fallback). Always
    // returns true if prepareOutgoingCall() already accepted the request;
    // the eventual outcome is observed via callStateChanged/callFailed as
    // before.
    bool dispatchMakeCall(const QString &remoteUri, const SipCallOptions &callOpts);
    MsrpRelayConfig buildMsrpRelayConfigFromSettings() const;
    void cancelMsrpCallPreparation();

    // Reads the persisted mic/speaker selection from MediaDeviceSelectionModel
    // and applies it to PJSIP AudDevManager. No-op when PJSIP is not active.
    void applyPersistedAudioDevices();

    // Reads VideoQualityManager settings and applies codec priority, bitrate,
    // and format to PJSIP before each outgoing call. No-op without HAVE_PJSIP.
    void applyVideoSettingsForCall();

    // Task W096: builds and sends an IMDN report (delivered/displayed) back
    // to toUri, referencing originalMessageId, and marks it sent on the
    // given inbound Message History entry. Shared by the auto-Delivered
    // path, the auto-Displayed path, and the manual "mark as read" action.
    bool sendImdnReport(const QString &toUri, const QString &originalMessageId,
                        ImdnInfo::Disposition disposition, qint64 inboundEntryId, QString &error);

    // Task W098: schedules a backoff-delayed automatic re-SUBSCRIBE for
    // entityUri after its subscription terminated with a retryable reason
    // (see PresenceResubscribePolicy). No-op if auto-resubscribe/presence/
    // subscribe is disabled in AppSettings.
    void schedulePresenceResubscribe(const QString &entityUri);

#ifdef HAVE_PJSIP
public:
    struct PjEndpoint;

private:
    bool ensureTransport(SipTransport transport, int &transportId, QString &error);

    PjEndpoint *m_ep{nullptr};
#endif

    SipCall                 *m_activeCall{nullptr};
    MsrpCallPreparationController *m_msrpCallPreparation{nullptr}; // Task W108, lazily created
    RttSession               m_rttSession;
    QTimer                   m_rtpStatsTimer;
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

    // SIP Presence (Task W098) auto-resubscribe backoff state, one timer +
    // attempt counter per watched entity URI.
    QHash<QString, QTimer *> m_presenceBackoffTimers;
    QHash<QString, int>      m_presenceBackoffAttempts;
};
