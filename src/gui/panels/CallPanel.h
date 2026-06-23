#pragma once

#include <QWidget>
#include "sip/CallStateMachine.h"

class QLabel;
class QComboBox;
class QProgressBar;
class QPushButton;

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

private slots:
    void onCallStateChanged(CallState state, const QString &statusText, int statusCode);
    void onIncomingCall(const QString &remoteUri);
    void onCallConnected(const QString &remoteUri);
    void onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void onCallFailed(const QString &remoteUri, const QString &reason, int statusCode);
    void onInputLevelChanged(int level);
    void onOutputLevelChanged(int level);
    void onMuteChanged(bool muted);

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
};
