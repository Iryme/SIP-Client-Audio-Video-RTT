#pragma once

#include <QWidget>
#include <QString>
#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyCallStateMachine.h"
#include "sip/CallStateMachine.h"

class QLabel;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class EmergencyCallController;
class StaticLocationProvider;

class CallPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CallPanel(QWidget *parent = nullptr);

    void setRemoteName(const QString &name);
    void setRemoteUri(const QString &uri);
    void setCallState(const QString &state);
    void setDuration(const QString &duration);

signals:
    void muteToggled(bool muted);
    void videoToggled(bool on);
    void holdToggled(bool held);
    void hangupRequested();
    void answerRequested();
    void rejectRequested();
    void keypadToggled(bool visible);

public slots:
    // Populate the dial input with uri and give it focus (called from contacts/menu).
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
    void onMuteChanged(bool muted);
    void onVideoMuteChanged(bool muted);

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

    QLabel      *m_remoteName{nullptr};
    QLabel      *m_remoteUri{nullptr};
    QLabel      *m_callState{nullptr};
    QLabel      *m_duration{nullptr};

    // Audio device selectors (visible only during an active call)
    QWidget     *m_deviceRow{nullptr};
    QComboBox   *m_micSelector{nullptr};
    QComboBox   *m_spkSelector{nullptr};

    // Level meters
    QProgressBar *m_inputMeter{nullptr};
    QProgressBar *m_outputMeter{nullptr};

    QPushButton *m_btnMute{nullptr};
    QPushButton *m_btnVideo{nullptr};
    QPushButton *m_btnShare{nullptr};
    QPushButton *m_btnHold{nullptr};
    QPushButton *m_btnKeypad{nullptr};
    QPushButton *m_btnRecord{nullptr};
    QPushButton *m_btnAnswer{nullptr};
    QPushButton *m_btnReject{nullptr};
    QPushButton *m_btnHangup{nullptr};

    // Dial row — visible only when Idle and registered.
    QWidget     *m_dialRow{nullptr};
    QLabel      *m_regStatusLabel{nullptr};
    QLineEdit   *m_dialInput{nullptr};
    QPushButton *m_btnCall{nullptr};

    // Emergency test mode section (hidden unless emergency/testMode=true in settings).
    QWidget                 *m_emergencyRow{nullptr};
    QLabel                  *m_emergencyStateLabel{nullptr};
    QPushButton             *m_btnEmergency{nullptr};
    EmergencyCallController *m_emergencyController{nullptr};
    StaticLocationProvider  *m_staticLocationProvider{nullptr};
    bool                     m_emergencyCallActive{false};

    // Manual location input (inside emergency row)
    QLineEdit               *m_latInput{nullptr};
    QLineEdit               *m_lonInput{nullptr};
    QLineEdit               *m_uncertaintyInput{nullptr};
    QLabel                  *m_locationStatusLabel{nullptr};
    QPlainTextEdit          *m_pidfPreview{nullptr};
    QPushButton             *m_btnGeneratePidf{nullptr};
    QPushButton             *m_btnLocationUpdate{nullptr};
    bool                     m_manualLocationValid{false};
};
