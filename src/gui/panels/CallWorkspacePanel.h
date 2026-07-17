#pragma once

#include <QWidget>
#include <QString>
#include <QTimer>

#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyCallStateMachine.h"
#include "sip/CallStateMachine.h"
#include "sip/CallMediaOptions.h"
#include "media/RtpStats.h"

class AudioLevelMeter;
class QLabel;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class StatusCard;
class EmergencyCallController;
class StaticLocationProvider;
class CallInfoModel;

// Task W113 (Call Workspace). Consolidates every call control (header,
// identity, presence, duration, hold/mute/camera/video/RTT, media status
// grid, device status, jitter/loss/RTP stats, selected/negotiated media,
// a SIP Ladder deep link, and the emergency-call test-mode section) into one
// widget, replacing the ~1000 lines of ad hoc lambdas previously inlined in
// MainWindow::buildClientsPage().
//
// Internally driven by CallInfoModel (see gui/panels/call/CallInfoModel.h) —
// the panel never infers active media from a button's checked-state, it
// always reads back from that single source of truth.
//
// Supersedes the old, never-instantiated gui/panels/CallPanel — this class
// folds in CallPanel's emergency-call section (previously unreachable in the
// shipped app since CallPanel was dead code), audio-codec card, and
// initial-offer card.
//
// Uses the shared dial-target QLineEdit passed at construction (the same
// field the numeric dialpad, ConversationWorkspacePanel, and
// ClientMessagingView already read/write) instead of owning a private one,
// so a call started from any entrypoint (dialpad, conversation list, call
// history, contacts) always goes through the same placeCall() normalization
// + CallMediaOptions + status-bar-feedback path.
class CallWorkspacePanel : public QWidget
{
    Q_OBJECT
public:
    explicit CallWorkspacePanel(QLineEdit *targetInput, QWidget *parent = nullptr);

    CallInfoModel *callInfoModel() const { return m_callInfoModel; }

public slots:
    // Normalizes uri (via SipUriNormalizer, same as the dialpad), builds
    // CallMediaOptions from the current request-video/request-RTT button
    // state, and places the call. Surfaces failures via a status message
    // (statusMessageRequested) rather than only logging them — used by
    // every call-launch entrypoint (dialpad, conversation list, call
    // history redial, contacts dial).
    void placeCall(const QString &uri);

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);
    void openSipLadderRequested(const QString &callId);

private slots:
    void onCallStateChanged(CallState state, const QString &statusText, int statusCode);
    void onCallConnected(const QString &remoteUri);
    void onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void onCallFailed(const QString &remoteUri, const QString &reason, int statusCode);
    void onAudioMediaConnected();
    void onAudioMediaDisconnected();
    void onVideoMediaConnected();
    void onVideoMediaDisconnected();
    void onVideoRequested();
    void onLocalVideoStarted();
    void onLocalVideoStopped();
    void onRemoteVideoStarted();
    void onRemoteVideoStopped();
    void onRttMediaConnected();
    void onRttMediaDisconnected();
    void onRttRequested();
    void onVideoStatsUpdated(float fps, int dropsThisSec);
    void onDurationTick();
    void onVideoMutedChanged(bool muted);
    void onCameraEnabledChanged(bool enabled);
    void onPresenceUpdated(const QString &entity);

    // Emergency call slots
    void onEmergencyButtonClicked();
    void onEmergencyReadyToDial(const EmergencyCallProfile &profile);
    void onEmergencyStateChanged(EmergencyCallState state);
    void onEmergencyFailed(const QString &reason);
    void onGeneratePidfClicked();
    void onLocationUpdateClicked();
    void onAdvancedDiagnosticsToggled(bool expanded);

private:
    void populateDeviceCombos();
    void refreshCards();
    void refreshHoldButton();
    void refreshRequestVideoButton();
    void refreshRequestRttButton();
    void refreshPresenceCard();
    void resetStatusCards();
    QString formatDuration(int seconds) const;

    QLineEdit *m_targetInput{nullptr}; // owned by MainWindow, not this panel

    // Call control buttons
    QPushButton *m_btnCall{nullptr};
    QPushButton *m_btnAnswer{nullptr};
    QPushButton *m_btnReject{nullptr};
    QPushButton *m_btnHangup{nullptr};
    QPushButton *m_btnMute{nullptr};
    QPushButton *m_btnHold{nullptr};
    QPushButton *m_btnRequestVideo{nullptr};
    QPushButton *m_btnRequestRtt{nullptr};

    // Video controls
    QPushButton *m_btnCameraToggle{nullptr};
    QPushButton *m_btnVideoMute{nullptr};

    // Device selectors + level meters
    QComboBox *m_micSelector{nullptr};
    QComboBox *m_spkSelector{nullptr};
    QComboBox *m_cameraSelector{nullptr};
    AudioLevelMeter *m_inputMeter{nullptr};
    AudioLevelMeter *m_outputMeter{nullptr};

    // Status cards
    StatusCard *m_cardState{nullptr};
    StatusCard *m_cardDuration{nullptr};
    StatusCard *m_cardAudio{nullptr};
    StatusCard *m_cardAudioCodec{nullptr};
    StatusCard *m_cardLocalVideo{nullptr};
    StatusCard *m_cardRemoteVideo{nullptr};
    StatusCard *m_cardRtt{nullptr};
    StatusCard *m_cardLmpe{nullptr};
    StatusCard *m_cardVideoCodec{nullptr};
    StatusCard *m_cardBitrate{nullptr};
    StatusCard *m_cardResolution{nullptr};
    StatusCard *m_cardFps{nullptr};
    StatusCard *m_cardRemoteUri{nullptr};
    StatusCard *m_cardPresence{nullptr};
    StatusCard *m_cardInitialOffer{nullptr};
    StatusCard *m_cardLocalAccount{nullptr};
    // RTP-stats packet loss (%) — never the video pipeline's own frame-drop
    // count (that's m_cardVideoDrops). Keeping these separate fixes a real
    // bug (see docs/call-state-and-media-model.md): the pre-W113 code wrote
    // both an RTCP packet-loss percentage and a raw video-drop count into
    // the same card.
    StatusCard *m_cardPacketLoss{nullptr};
    StatusCard *m_cardVideoDrops{nullptr};
    StatusCard *m_cardJitter{nullptr};
    StatusCard *m_cardLatency{nullptr};
    StatusCard *m_cardCamera{nullptr};

    // Task W113a layout pass: essential cards (state/duration/identity/
    // presence/audio/remote-video/RTT/camera) are always visible; everything
    // else (codecs, bitrate/resolution/fps, selected media, local account,
    // packet loss/video drops/jitter/latency, LMPE) lives in m_advancedHost,
    // shown only when the disclosure button is expanded (persisted via
    // AppSettings::callWorkspaceAdvancedDiagnosticsExpanded()).
    QWidget *m_advancedHost{nullptr};
    QToolButton *m_advancedToggle{nullptr};

    QPushButton *m_btnOpenSipLadder{nullptr};

    // Duration tracking
    QTimer m_durationTimer;
    int m_durationSeconds{0};
    QTimer m_holdConfirmTimer;
    QTimer m_videoRequestBlinkTimer;
    bool m_videoRequestBlinkOn{false};

    // Media state (mirrors the sources of truth in CallInfoModel; kept as
    // plain bools here too since several are also needed synchronously
    // inside button-toggle handlers before CallInfoModel would be updated).
    bool m_audioConnected{false};
    bool m_videoConnected{false};
    bool m_videoRequested{false};
    bool m_localVideoActive{false};
    bool m_remoteVideoActive{false};
    bool m_rttConnected{false};
    bool m_rttRequested{false};
    bool m_videoRequestFailed{false};
    bool m_rttRequestFailed{false};
    QString m_remoteUri;
    CallMediaOptions m_selectedMedia;

    CallInfoModel *m_callInfoModel{nullptr};

    // Emergency test mode section (hidden unless AppSettings::emergencyTestModeEnabled())
    QWidget *m_emergencyRow{nullptr};
    QLabel *m_emergencyStateLabel{nullptr};
    QPushButton *m_btnEmergency{nullptr};
    EmergencyCallController *m_emergencyController{nullptr};
    StaticLocationProvider *m_staticLocationProvider{nullptr};
    bool m_emergencyCallActive{false};

    QLineEdit *m_latInput{nullptr};
    QLineEdit *m_lonInput{nullptr};
    QLineEdit *m_uncertaintyInput{nullptr};
    QLabel *m_locationStatusLabel{nullptr};
    QPlainTextEdit *m_pidfPreview{nullptr};
    QPushButton *m_btnGeneratePidf{nullptr};
    QPushButton *m_btnLocationUpdate{nullptr};
    bool m_manualLocationValid{false};
};
