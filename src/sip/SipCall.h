#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <qwindowdefs.h>

#include "core/CodecInfo.h"
#include "sip/CallStateMachine.h"
#include "sip/SipCallOptions.h"
#include "media/RtpStats.h"
#include "msrp/MsrpCallPreparation.h"

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

    // Task W108: supplies a pre-resolved MSRP transport decision (Direct or
    // relay-allocated) obtained asynchronously by a
    // MsrpCallPreparationController *before* this call is created. Must be
    // called before makeCall()/makeCallWithOptions() to have any effect —
    // it is read once, synchronously, from inside the pjsip
    // onCallSdpCreated callback while building the offer; it is never used
    // to trigger any I/O of its own. A default-constructed (not-ready)
    // offer means "no relay decision was made" and preserves the exact
    // pre-W108 direct-MSRP behavior (lazy passive listener).
    void setPreparedMsrpOffer(const PreparedMsrpOffer &offer);

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

    // Request / release negotiated RTT/text mid-call. Local-initiated only:
    // refuses (logs a warning, returns false) while a remote RTT offer is
    // pending user consent (see rttRequested()/hasPendingIncomingRttRequest())
    // — use acceptIncomingRttRequest()/rejectIncomingRttRequest() for that
    // case instead. Also refuses while a previous local RTT re-INVITE is
    // still in flight (single-flight guard, prevents duplicate re-INVITEs
    // from repeated button clicks).
    // In PJSIP mode attempts a re-INVITE with updated text media counts.
    bool requestRtt(bool enabled);

    // True when a remote RTT/text offer was auto-declined (m=text 0) and is
    // awaiting the user's accept/reject decision.
    bool hasPendingIncomingRttRequest() const;

    // Accepts a pending incoming RTT offer. The offer itself was already
    // answered with m=text 0 by the synchronous auto-response in
    // onCallRxReinvite (pjsua2's onCallRxReinvite callback cannot defer the
    // answer) — this issues a *new* local re-INVITE offering m=text, which is
    // the only mechanism pjsua2 exposes for turning that decline into RTT
    // becoming active. Returns false (no-op) if no incoming request is
    // pending or a local RTT re-INVITE is already in flight.
    bool acceptIncomingRttRequest();

    // Rejects a pending incoming RTT offer. No new re-INVITE is sent — the
    // offer was already declined (m=text 0) by the auto-response; this only
    // clears the pending flag and emits rttRequestRejected() so the UI state
    // stops showing a contradictory "still pending" request. Returns false
    // if no incoming request is pending.
    bool rejectIncomingRttRequest();

    // True when a video stream is currently active (set/cleared alongside
    // videoMediaConnected / videoMediaDisconnected).
    bool isLocalVideoAvailable()  const;
    bool isRemoteVideoAvailable() const;

    // Current RTP/RTCP snapshot for the active call. Returns an unavailable
    // snapshot when there is no active call or PJSIP cannot expose stats.
    RtpStatsSnapshot mediaRtpStats() const;

    // Structured negotiated codec info for the active call's audio / video
    // streams, captured from PJSIP stream info when media becomes active.
    // Invalid (default-constructed) when nothing has been negotiated yet
    // (or in stub mode).
    AudioCodecInfo negotiatedAudioCodecInfo() const;
    VideoCodecInfo negotiatedVideoCodecInfo() const;

    // Negotiated audio codec for the active call's audio stream, formatted as
    // "<name>/<clockRateHz> pt=<payloadType>" (e.g. "PCMA/8000 pt=8").
    // Empty when no audio codec has been negotiated yet (or in stub mode).
    // Derived from negotiatedAudioCodecInfo(); kept for log/test compatibility.
    QString negotiatedAudioCodec() const;

    // Format a negotiated audio codec the same way it is stored/logged.
    // Exposed as a static utility so tests can verify the exact string
    // without needing a live PJSIP call.
    static QString formatAudioCodecSummary(const QString &name, int clockRateHz, int payloadType);

    // Embed PJSIP video windows into Qt native widget handles.
    // remoteWidget: winId() of the main remote video area.
    // localPreview:  winId() of the local PiP widget.
    // Call from the Qt main thread after videoMediaConnected is emitted.
    // No-op on non-Windows or stub builds.
    // Returns true when both windows were attached (or legitimately skipped);
    // false means the attach is incomplete and should be retried.
    bool attachVideoWindows(WId remoteWidget, WId localPreview);

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

    // PJSIP invite-session state text (e.g. "CONFIRMED"); empty in stub
    // mode or when no PJSIP call exists.
    QString sipDialogStateText() const;
    // Remote RTP address "ip:port" of the negotiated audio stream; empty
    // when unavailable (stub mode, no media, or call not connected).
    QString remoteMediaAddress() const;

    // Task W101 Phase 5/6: true only once this call's own MSRP session (see
    // onCallSdpCreated) has reached the Established state — never inferred
    // just from m=message appearing in negotiated SDP.
    bool isMsrpEstablished() const;

    // Sends contentType/body over this call's established MSRP session.
    // Returns the MSRP Message-ID on success, or an empty string if no
    // session is established — callers should check isMsrpEstablished()
    // first, or treat an empty return as failure.
    QString sendMsrpMessage(const QString &contentType, const QByteArray &body);

    CallStateMachine &stateMachine();

signals:
    void callStateChanged(CallState state, const QString &statusText, int statusCode);
    void callConnected(const QString &remoteUri);
    void callDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void callFailed(const QString &remoteUri, const QString &reason, int statusCode);

    // Task W101 Phase 6: relayed from this call's own MsrpSession — SipCall
    // owns the session, but message history/diagnostics live in SipManager,
    // matching the existing callConnected/callDisconnected relay pattern.
    void msrpPayloadReceived(const QString &contentType, const QByteArray &body,
                             const QString &msrpMessageId);
    void msrpDeliveryStatusChanged(const QString &msrpMessageId, bool success,
                                   const QString &statusText);

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
    // The auto-response declines it (textCount=0); user must call
    // acceptIncomingRttRequest() (to renegotiate RTT on) or
    // rejectIncomingRttRequest() (to clear the pending state without
    // renegotiating — the decline was already sent).
    void rttRequested();

    // Emitted when rejectIncomingRttRequest() clears a pending incoming
    // RTT offer without sending a new re-INVITE.
    void rttRequestRejected();

    // Emitted right before a local RTT re-INVITE is sent (requestRtt(true)
    // or acceptIncomingRttRequest()), so listeners (RttSession) can reflect
    // the "negotiating" state and start a bounded negotiation timeout.
    void rttLocalOfferSent();

    // Emitted when a local RTT re-INVITE (requestRtt()/
    // acceptIncomingRttRequest()) fails: either a PJSIP/transport error (e.g.
    // RTP port bind failure) or the remote peer's answer coming back with
    // RTT declined (m=text 0 / line removed). Audio/video are unaffected —
    // this only concerns the RTT/text media line. `reason` is a short,
    // human-readable, non-sensitive diagnostic string.
    void rttNegotiationFailed(const QString &reason);

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

    // Shared re-INVITE logic for requestRtt()/acceptIncomingRttRequest() —
    // callers are responsible for their own distinct precondition checks and
    // logging (see requestRtt() vs acceptIncomingRttRequest()).
    bool sendRttOffer(bool enabled);

    CallStateMachine m_stateMachine;
    QString          m_remoteUri;
    QString          m_callId;
    bool             m_muted{false};
    int              m_micVolume{100};
    int              m_speakerVolume{100};
    bool             m_videoMuted{false};
    AudioCodecInfo   m_negotiatedAudioCodecInfo;
    VideoCodecInfo   m_negotiatedVideoCodecInfo;
    bool             m_localVideoAvailable{false};
    bool             m_remoteVideoAvailable{false};
    QTimer           m_levelTimer;

    struct Impl;
    Impl *m_impl{nullptr};
};
