#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <qwindowdefs.h>

#include "sip/CallStateMachine.h"
#include "sip/SipCallOptions.h"
#include "media/RtpStats.h"

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

    // Outgoing call with per-call SIP options (emergency headers, media policy).
    // When options.isEmpty(), behavior is identical to makeCall().
    bool makeCallWithOptions(const QString &remoteUri, const SipCallOptions &options);

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

    // Set microphone / speaker gain, 0-100 (100 = default/unity gain).
    // In PJSIP mode applies immediately via AudDevManager capture/playback
    // device media when the call's audio stream is active; otherwise the
    // value is stored and applied when audio media connects.
    bool setMicVolume(int percent);
    int  micVolume() const;
    bool setSpeakerVolume(int percent);
    int  speakerVolume() const;

    // Stop / resume sending video. Does not affect incoming video.
    // In PJSIP mode mutes the video capture stream immediately.
    bool setVideoMuted(bool muted);
    bool isVideoMuted() const;

    // Request / release negotiated video mid-call. Does not touch camera state.
    // In PJSIP mode attempts a re-INVITE with updated video media counts.
    bool requestVideo(bool enabled);

    // Request / release negotiated RTT/text mid-call.
    // In PJSIP mode attempts a re-INVITE with updated text media counts.
    bool requestRtt(bool enabled);

    // True when a video stream is currently active (set/cleared alongside
    // videoMediaConnected / videoMediaDisconnected).
    bool isLocalVideoAvailable()  const;
    bool isRemoteVideoAvailable() const;

    // Current RTP/RTCP snapshot for the active call. Returns an unavailable
    // snapshot when there is no active call or PJSIP cannot expose stats.
    RtpStatsSnapshot mediaRtpStats() const;

    // Embed PJSIP video windows into Qt native widget handles.
    // remoteWidget: winId() of the main remote video area.
    // localPreview:  winId() of the local PiP widget.
    // Call from the Qt main thread after videoMediaConnected is emitted.
    // No-op on non-Windows or stub builds.
    void attachVideoWindows(WId remoteWidget, WId localPreview);

    // Send a T.140 RTT text block via the active RTP text stream.
    // No-op in stub mode or when the text stream is not active.
    void sendRttText(const QString &text);

    // Send a SIP UPDATE with an updated PIDF-LO body during an active emergency call.
    // opts.emergencyCall must be true; call must be in Active state.
    // PJSIP mode: sends UPDATE with PIDF-LO as a multipart/mixed part alongside SDP.
    // Stub mode: returns false (no SIP stack).
    bool sendLocationUpdate(const SipCallOptions &opts);

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

    // Emitted when the call first negotiates a video media stream.
    // This is useful as a user-visible "video requested" notification.
    void videoRequested();

    // Emitted when the remote peer requests an RTT/text channel via re-INVITE.
    // The auto-response declines it (textCount=0); user must call requestRtt(true).
    void rttRequested();

    // Granular local/remote video availability signals.
    void localVideoStarted();
    void localVideoStopped();
    void remoteVideoStarted();
    void remoteVideoStopped();

    // Video mute state change (true = muted / not sending).
    void videoMuteChanged(bool muted);

    // Emitted when a T.140 / RFC 4103 text stream becomes active or inactive.
    // In PJSIP mode driven by onCallMediaState PJMEDIA_TYPE_TEXT detection.
    // In stub mode not emitted automatically (tests drive via direct signal).
    void rttMediaConnected();
    void rttMediaDisconnected();

    // Emitted from the PJSIP media thread (via queued connection) when the
    // remote peer sends an RTT text block.
    void rttTextReceived(const QString &text);

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
    int              m_micVolume{100};
    int              m_speakerVolume{100};
    bool             m_videoMuted{false};
    bool             m_localVideoAvailable{false};
    bool             m_remoteVideoAvailable{false};
    QTimer           m_levelTimer;

    struct Impl;
    Impl *m_impl{nullptr};
};
