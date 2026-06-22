#pragma once
#include <QWidget>

class QLabel;
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
    void keypadToggled(bool visible);

private:
    QLabel      *m_remoteName{nullptr};
    QLabel      *m_remoteUri{nullptr};
    QLabel      *m_callState{nullptr};
    QLabel      *m_duration{nullptr};

    QPushButton *m_btnMute{nullptr};
    QPushButton *m_btnVideo{nullptr};
    QPushButton *m_btnShare{nullptr};
    QPushButton *m_btnHold{nullptr};
    QPushButton *m_btnKeypad{nullptr};
    QPushButton *m_btnRecord{nullptr};
    QPushButton *m_btnHangup{nullptr};
};
