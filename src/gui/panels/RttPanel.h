#pragma once
#include <QWidget>
#include <QTimer>
#include "rtt/RttSession.h"

class QLabel;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QTabWidget;

class RttPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RttPanel(QWidget *parent = nullptr);

    // Wire the panel to a session. Pass nullptr to disconnect.
    void setRttSession(RttSession *session);

signals:
    void rttMessageSent(const QString &text);

private slots:
    void onRttSend();
    void onRttStateChanged(RttState state);
    // Fires on every QLineEdit textChanged — computes T.140 delta and sends.
    void onRttInputChanged(const QString &newText);

private:
    void updateInputState();
    // Process one T.140 received block into m_remoteBuffer and update widgets.
    void processRemoteText(const QString &incoming);
    // Flush m_remoteBuffer to transcript and clear the live-typing area.
    void flushRemoteBuffer();
    void resetRttBuffers();

    // RTT tab
    QLabel      *m_rttState{nullptr};
    QTextEdit   *m_rttTranscript{nullptr};
    QTextEdit   *m_rttRemoteLive{nullptr};
    QLineEdit   *m_rttInput{nullptr};
    QPushButton *m_rttSend{nullptr};
    QPushButton *m_rttClear{nullptr};

    // LMPE tab (Task W113F: permanently unavailable, a single disabled
    // label -- no input/send/local-echo widgets anymore)
    QLabel      *m_lmpeState{nullptr};

    RttSession  *m_rttSession{nullptr};

    QString m_prevLocalText;
    QString m_remoteBuffer;

    // Defers m_rttRemoteLive repaints — batch rapid RTT packets into one update.
    QTimer  m_liveUpdateTimer;
};
