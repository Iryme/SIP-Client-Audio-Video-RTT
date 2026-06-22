#include "CallPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

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
    m_btnHangup = makeBtn(tr("Hangup"),  false);
    m_btnHangup->setObjectName("HangupBtn");

    // Share and Record are placeholders - disabled until implemented
    m_btnShare->setEnabled(false);
    m_btnRecord->setEnabled(false);

    ctrlRow->addWidget(m_btnMute);
    ctrlRow->addWidget(m_btnVideo);
    ctrlRow->addWidget(m_btnShare);
    ctrlRow->addWidget(m_btnHold);
    ctrlRow->addWidget(m_btnKeypad);
    ctrlRow->addWidget(m_btnRecord);
    ctrlRow->addStretch();
    ctrlRow->addWidget(m_btnHangup);

    layout->addLayout(ctrlRow);

    connect(m_btnMute,   &QPushButton::toggled, this, &CallPanel::muteToggled);
    connect(m_btnVideo,  &QPushButton::toggled, this, &CallPanel::videoToggled);
    connect(m_btnHold,   &QPushButton::toggled, this, &CallPanel::holdToggled);
    connect(m_btnHangup, &QPushButton::clicked, this, &CallPanel::hangupRequested);
    connect(m_btnKeypad, &QPushButton::toggled, this, &CallPanel::keypadToggled);
}

void CallPanel::setRemoteName(const QString &name)  { m_remoteName->setText(name); }
void CallPanel::setRemoteUri(const QString &uri)    { m_remoteUri->setText(uri); }
void CallPanel::setCallState(const QString &state)  { m_callState->setText(state); }
void CallPanel::setDuration(const QString &dur)     { m_duration->setText(dur); }
