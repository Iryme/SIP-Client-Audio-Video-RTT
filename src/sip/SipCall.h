#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "sip/CallStateMachine.h"

// Manages a single SIP call. Owns a CallStateMachine that enforces valid
// call state transitions. In stub mode (ENABLE_PJSIP=OFF) all operations
// resolve locally via queued callbacks. In PJSIP mode (HAVE_PJSIP defined)
// operations are delegated to pjsua2 and driven by onCallState() callbacks.
//
// SipManager owns the active SipCall instance and provides the public call
// control API to the rest of the application.
class SipCall : public QObject
{
    Q_OBJECT
public:
    explicit SipCall(QObject *parent = nullptr);
    ~SipCall() override;

    // Initiate an outgoing call. Must be in Idle state.
    bool makeCall(const QString &remoteUri);

    // Binds the call to the active PJSIP account or existing incoming INVITE.
    // No-op in stub builds; the handle is a pj::Account* behind HAVE_PJSIP.
    void setPjsipAccountHandle(void *accountHandle);
    bool bindIncomingPjsipCall(void *accountHandle, int callId, const QString &remoteUri,
                               void *earlyCallHandle = nullptr);

    // Answer an incoming call. Must be in IncomingRinging state.
    bool answer();

    // Reject an incoming call (sends 4xx). Must be in IncomingRinging state.
    bool reject();

    // Hang up (any in-progress state except Idle and Failed).
    bool hangup();

    // Place the call on hold. Must be in Active state.
    bool hold();

    // Resume a held call. Must be in Held state.
    bool resume();

    // Mute / unmute the local microphone. Works in any call state.
    // In PJSIP mode adjusts capture device tx level immediately.
    bool setMuted(bool muted);
    bool isMuted() const;

    // Stop / resume sending video. Does not affect incoming video.
    // In PJSIP mode mutes the video capture stream immediately.
    bool setVideoMuted(bool muted);
    bool isVideoMuted() const;

    // True when a video stream is currently active (set/cleared alongside
    // videoMediaConnected / videoMediaDisconnected).
    bool isLocalVideoAvailable()  const;
    bool isRemoteVideoAvailable() const;

    // Force to Idle regardless of current state (shutdown / cleanup path).
    void reset(const QString &reason = QStringLiteral("Reset"));

    // Release the underlying pj::Call slot immediately (no-op in stub mode).
    // Call this before deleteLater() when the PJSIP account may be destroyed
    // soon after, to avoid the "deleting account while call active" warning.
    void releasePjsipCall();

    CallState state()      const;
    QString   statusText() const;
    QString   remoteUri()  const;
    QString   callId()     const;

    CallStateMachine &stateMachine();

signals:
    void callStateChanged(CallState state, const QString &statusText, int statusCode);
    void callConnected(const QString &remoteUri);
    void callDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void callFailed(const QString &remoteUri, const QString &reason, int statusCode);

    // Emitted when the PJSIP audio bridge is wired (CONFIRMED + media active).
    // In stub mode emitted when state reaches Active.
    void audioMediaConnected();
    void audioMediaDisconnected();

    // Mute state change (true = muted).
    void muteChanged(bool muted);

    // Audio level updates, ~10 Hz while media is active. Range 0–100.
    void inputLevelChanged(int level);
    void outputLevelChanged(int level);

    // Emitted when a video stream is activated / deactivated.
    // In stub mode emitted alongside audioMediaConnected/Disconnected.
    // In PJSIP mode emitted from onCallMediaState when PJMEDIA_TYPE_VIDEO
    // becomes PJSUA_CALL_MEDIA_ACTIVE or is removed.
    void videoMediaConnected();
    void videoMediaDisconnected();

    // Granular local/remote video availability signals.
    void localVideoStarted();
    void localVideoStopped();
    void remoteVideoStarted();
    void remoteVideoStopped();

    // Video mute state change (true = muted / not sending).
    void videoMuteChanged(bool muted);

private slots:
    void onStateMachineStateChanged(CallState state, const QString &statusText, int statusCode);
    void onStateMachineTimedOut(CallState stuckState);
    void onLevelTimerFired();

private:
    void postStubTransition(CallState to, const QString &reason, int statusCode = 0);

    CallStateMachine m_stateMachine;
    QString          m_remoteUri;
    QString          m_callId;
    bool             m_muted{false};
    bool             m_videoMuted{false};
    bool             m_localVideoAvailable{false};
    bool             m_remoteVideoAvailable{false};
    QTimer           m_levelTimer;

    struct Impl;
    Impl *m_impl{nullptr};
};
