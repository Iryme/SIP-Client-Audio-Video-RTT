#include "CallPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

#include "sip/SipManager.h"

CallPanel::CallPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("CallPanel");
    setFixedHeight(110);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);

    // Call info row
    auto *infoRow = new QHBoxLayout();

    m_remoteName = new QLabel(tr("No active call"), this);
    m_remoteName->setObjectName("RemoteName");
    m_remoteName->setStyleSheet("font-size: 16px; font-weight: bold;");

    m_callState = new QLabel(tr("Idle"), this);
    m_callState->setObjectName("CallState");
    m_callState->setStyleSheet("color: #aaaaaa; font-size: 12px;");

    m_duration = new QLabel(tr("00:00:00"), this);
    m_duration->setObjectName("Duration");
    m_duration->setStyleSheet("color: #aaaaaa; font-size: 12px; font-family: monospace;");

    infoRow->addWidget(m_remoteName);
    infoRow->addStretch();
    infoRow->addWidget(m_callState);
    infoRow->addSpacing(12);
    infoRow->addWidget(m_duration);

    m_remoteUri = new QLabel(tr("—"), this);
    m_remoteUri->setObjectName("RemoteUri");
    m_remoteUri->setStyleSheet("color: #888888; font-size: 11px;");

    layout->addLayout(infoRow);
    layout->addWidget(m_remoteUri);

    // Separator
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep);

    // Call control buttons
    auto *ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(8);

    auto makeBtn = [&](const QString &label, bool checkable = false) -> QPushButton* {
        auto *btn = new QPushButton(label, this);
        btn->setObjectName("CallCtrlBtn");
        btn->setCheckable(checkable);
        btn->setFixedHeight(32);
        btn->setMinimumWidth(72);
        return btn;
    };

    m_btnMute   = makeBtn(tr("Mute"),    true);
    m_btnVideo  = makeBtn(tr("Video"),   true);
    m_btnShare  = makeBtn(tr("Share"),   false);
    m_btnHold   = makeBtn(tr("Hold"),    true);
    m_btnKeypad = makeBtn(tr("Keypad"),  true);
    m_btnRecord = makeBtn(tr("Record"),  true);
    m_btnAnswer = makeBtn(tr("Answer"),  false);
    m_btnReject = makeBtn(tr("Reject"),  false);
    m_btnHangup = makeBtn(tr("Hangup"),  false);

    m_btnAnswer->setObjectName("AnswerBtn");
    m_btnReject->setObjectName("RejectBtn");
    m_btnHangup->setObjectName("HangupBtn");

    // Share and Record are placeholders
    m_btnShare->setEnabled(false);
    m_btnRecord->setEnabled(false);

    ctrlRow->addWidget(m_btnMute);
    ctrlRow->addWidget(m_btnVideo);
    ctrlRow->addWidget(m_btnShare);
    ctrlRow->addWidget(m_btnHold);
    ctrlRow->addWidget(m_btnKeypad);
    ctrlRow->addWidget(m_btnRecord);
    ctrlRow->addStretch();
    ctrlRow->addWidget(m_btnAnswer);
    ctrlRow->addWidget(m_btnReject);
    ctrlRow->addWidget(m_btnHangup);

    layout->addLayout(ctrlRow);

    connect(m_btnMute,   &QPushButton::toggled, this, &CallPanel::muteToggled);
    connect(m_btnVideo,  &QPushButton::toggled, this, &CallPanel::videoToggled);
    connect(m_btnHold,   &QPushButton::toggled, this, &CallPanel::holdToggled);
    connect(m_btnHangup, &QPushButton::clicked, this, &CallPanel::hangupRequested);
    connect(m_btnAnswer, &QPushButton::clicked, this, &CallPanel::answerRequested);
    connect(m_btnReject, &QPushButton::clicked, this, &CallPanel::rejectRequested);
    connect(m_btnKeypad, &QPushButton::toggled, this, &CallPanel::keypadToggled);

    // Wire to Answer/Reject/Hangup button signals → SipManager
    connect(this, &CallPanel::answerRequested,
            []{ SipManager::instance().answerCall(); });
    connect(this, &CallPanel::rejectRequested,
            []{ SipManager::instance().rejectCall(); });
    connect(this, &CallPanel::hangupRequested,
            []{ SipManager::instance().hangupCall(); });
    connect(this, &CallPanel::holdToggled,
            [](bool held){
                if (held) SipManager::instance().holdCall();
                else      SipManager::instance().resumeCall();
            });

    // Wire to SipManager call signals
    connect(&SipManager::instance(), &SipManager::callStateChanged,
            this, &CallPanel::onCallStateChanged);
    connect(&SipManager::instance(), &SipManager::incomingCall,
            this, &CallPanel::onIncomingCall);
    connect(&SipManager::instance(), &SipManager::callConnected,
            this, &CallPanel::onCallConnected);
    connect(&SipManager::instance(), &SipManager::callDisconnected,
            this, &CallPanel::onCallDisconnected);
    connect(&SipManager::instance(), &SipManager::callFailed,
            this, &CallPanel::onCallFailed);

    // Set initial idle state
    applyCallState(CallState::Idle);
}

void CallPanel::setRemoteName(const QString &name)  { m_remoteName->setText(name); }
void CallPanel::setRemoteUri(const QString &uri)    { m_remoteUri->setText(uri); }
void CallPanel::setCallState(const QString &state)  { m_callState->setText(state); }
void CallPanel::setDuration(const QString &dur)     { m_duration->setText(dur); }

void CallPanel::applyCallState(CallState state)
{
    const bool isIdle     = (state == CallState::Idle);
    const bool isIncoming = (state == CallState::IncomingRinging);
    const bool isActive   = (state == CallState::Active || state == CallState::Held);
    const bool hasCall    = !isIdle && state != CallState::Failed;

    m_btnAnswer->setVisible(isIncoming);
    m_btnReject->setVisible(isIncoming);
    m_btnHangup->setVisible(!isIncoming && hasCall);
    m_btnHold->setEnabled(isActive);
    m_btnMute->setEnabled(state == CallState::Active);
    m_btnVideo->setEnabled(state == CallState::Active);
    m_btnKeypad->setEnabled(state == CallState::Active);

    m_callState->setText(callStateDisplayText(state));

    // Color-code the state label
    QString color = QStringLiteral("#aaaaaa"); // Idle / default
    if (state == CallState::Active)
        color = QStringLiteral("#50c878"); // green
    else if (state == CallState::IncomingRinging || state == CallState::Ringing
             || state == CallState::OutgoingInit || state == CallState::Connecting)
        color = QStringLiteral("#e0b850"); // amber
    else if (state == CallState::Failed)
        color = QStringLiteral("#e05050"); // red
    else if (state == CallState::Held)
        color = QStringLiteral("#5090e0"); // blue

    m_callState->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(color));

    if (isIdle) {
        m_remoteName->setText(tr("No active call"));
        m_remoteUri->setText(tr("—"));
        m_duration->setText(tr("00:00:00"));
    }
}

void CallPanel::onCallStateChanged(CallState state, const QString &statusText, int statusCode)
{
    Q_UNUSED(statusText)
    Q_UNUSED(statusCode)
    applyCallState(state);
}

void CallPanel::onIncomingCall(const QString &remoteUri)
{
    m_remoteName->setText(tr("Incoming Call"));
    m_remoteUri->setText(remoteUri);
    applyCallState(CallState::IncomingRinging);
}

void CallPanel::onCallConnected(const QString &remoteUri)
{
    Q_UNUSED(remoteUri)
    applyCallState(CallState::Active);
}

void CallPanel::onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode)
{
    Q_UNUSED(remoteUri)
    Q_UNUSED(reason)
    Q_UNUSED(statusCode)
    applyCallState(CallState::Idle);
}

void CallPanel::onCallFailed(const QString &remoteUri, const QString &reason, int statusCode)
{
    Q_UNUSED(remoteUri)
    Q_UNUSED(statusCode)
    m_callState->setText(tr("Call Failed: %1").arg(reason));
    m_callState->setStyleSheet(QStringLiteral("color: #e05050; font-size: 12px;"));
    m_btnHangup->setVisible(false);
    m_btnAnswer->setVisible(false);
    m_btnReject->setVisible(false);
    m_btnHold->setEnabled(false);
    m_btnMute->setEnabled(false);
    m_btnVideo->setEnabled(false);
    m_btnKeypad->setEnabled(false);
}
