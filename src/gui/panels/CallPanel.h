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
class QProgressBar;
class QPushButton;
class QSlider;
class StatusCard;
class EmergencyCallController;
class StaticLocationProvider;

class CallPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CallPanel(QWidget *parent = nullptr);

signals:
    void muteToggled(bool muted);
    void holdToggled(bool held);
    void hangupRequested();
    void answerRequested();
    void rejectRequested();
    void requestVideoToggled(bool enabled);

public slots:
    void setDialTarget(const QString &uri);
    void placeCall(const QString &uri = QString());
    void focusDialInput();

private slots:
    void onCallStateChanged(CallState state, const QString &statusText, int statusCode);
    void onIncomingCall(const QString &remoteUri);
    void onCallConnected(const QString &remoteUri);
    void onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void onCallFailed(const QString &remoteUri, const QString &reason, int statusCode);
    void onInputLevelChanged(int level);
    void onOutputLevelChanged(int level);
    void onMicrophoneVolumeChanged(int percent);
    void onSpeakerVolumeChanged(int percent);
    void onMuteChanged(bool muted);
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
    void onVideoStatsUpdated(float fps, int dropsThisSec);
    void onDurationTick();
    void onVideoMutedChanged(bool muted);
    void onCameraEnabledChanged(bool enabled);

    // Emergency call slots
    void onEmergencyButtonClicked();
    void onEmergencyReadyToDial(const EmergencyCallProfile &profile);
    void onEmergencyStateChanged(EmergencyCallState state);
    void onEmergencyFailed(const QString &reason);
    void onGeneratePidfClicked();
    void onLocationUpdateClicked();

private:
    void applyCallState(CallState state);
    void populateDeviceCombos();
    void updateStatusCards();
    void updateRtpStatCards(const RtpStatsSnapshot &stats);
    void refreshVideoRequestButton();
    void resetStatusCards();
    void updateVideoMuteState();
    QString formatDuration(int seconds) const;

    // Dial row
    QLineEdit   *m_dialInput{nullptr};
    QComboBox   *m_callTypeCombo{nullptr};
    QPushButton *m_btnCall{nullptr};
    QLabel      *m_regStatusLabel{nullptr};

    // Audio device selectors
    QComboBox   *m_micSelector{nullptr};
    QComboBox   *m_spkSelector{nullptr};

    // Level meters (green→red color scale)
    AudioLevelMeter *m_inputMeter{nullptr};
    AudioLevelMeter *m_outputMeter{nullptr};

    // Volume sliders — backed by AudioMediaManager::setMicrophoneVolume/setSpeakerVolume.
    QSlider     *m_micVolumeSlider{nullptr};
    QSlider     *m_spkVolumeSlider{nullptr};

    // Video controls (moved from VideoPanel overlay)
    QPushButton *m_btnCameraToggle{nullptr};
    QPushButton *m_btnVideoMute{nullptr};

    // Call control buttons
    QPushButton *m_btnMute{nullptr};
    QPushButton *m_btnHold{nullptr};
    QPushButton *m_btnRequestVideo{nullptr};
    QPushButton *m_btnAnswer{nullptr};
    QPushButton *m_btnReject{nullptr};
    QPushButton *m_btnHangup{nullptr};

    // Status cards (17 tiles, replacing the old text info grid)
    StatusCard *m_cardState{nullptr};
    StatusCard *m_cardDuration{nullptr};
    StatusCard *m_cardAudio{nullptr};
    StatusCard *m_cardLocalVideo{nullptr};
    StatusCard *m_cardRemoteVideo{nullptr};
    StatusCard *m_cardRtt{nullptr};
    StatusCard *m_cardLmpe{nullptr};
    StatusCard *m_cardVideoCodec{nullptr};
    StatusCard *m_cardAudioCodec{nullptr};
    StatusCard *m_cardBitrate{nullptr};
    StatusCard *m_cardResolution{nullptr};
    StatusCard *m_cardFps{nullptr};
    StatusCard *m_cardRemoteUri{nullptr};
    StatusCard *m_cardLocalAccount{nullptr};
    StatusCard *m_cardPacketLoss{nullptr};
    StatusCard *m_cardJitter{nullptr};
    StatusCard *m_cardLatency{nullptr};

    // Duration tracking
    QTimer  m_durationTimer;
    int     m_durationSeconds{0};
    QTimer  m_holdConfirmTimer;
    QTimer  m_videoRequestBlinkTimer;
    bool    m_videoRequestBlinkOn{false};

    // Media state
    bool m_audioConnected{false};
    bool m_videoConnected{false};
    bool m_videoRequested{false};
    bool m_localVideoActive{false};
    bool m_remoteVideoActive{false};
    bool m_rttConnected{false};
    QString m_remoteUri;
    CallType m_activeCallType{CallType::AudioOnly};

    // Emergency test mode section
    QWidget                 *m_emergencyRow{nullptr};
    QLabel                  *m_emergencyStateLabel{nullptr};
    QPushButton             *m_btnEmergency{nullptr};
    EmergencyCallController *m_emergencyController{nullptr};
    StaticLocationProvider  *m_staticLocationProvider{nullptr};
    bool                     m_emergencyCallActive{false};

    QLineEdit               *m_latInput{nullptr};
    QLineEdit               *m_lonInput{nullptr};
    QLineEdit               *m_uncertaintyInput{nullptr};
    QLabel                  *m_locationStatusLabel{nullptr};
    QPlainTextEdit          *m_pidfPreview{nullptr};
    QPushButton             *m_btnGeneratePidf{nullptr};
    QPushButton             *m_btnLocationUpdate{nullptr};
    bool                     m_manualLocationValid{false};
};
