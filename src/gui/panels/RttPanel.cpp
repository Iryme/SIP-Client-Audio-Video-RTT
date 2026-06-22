#include "RttPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QListWidget>
#include <QFrame>

RttPanel::RttPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("RttPanel");

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("RttTabs");
    outerLayout->addWidget(tabs);

    // -----------------------------------------------------------------------
    // RTT Tab
    // -----------------------------------------------------------------------
    auto *rttTab = new QWidget();
    auto *rttLayout = new QVBoxLayout(rttTab);
    rttLayout->setContentsMargins(8, 8, 8, 8);
    rttLayout->setSpacing(6);

    m_rttState = new QLabel(tr("RTT: Inactive"), rttTab);
    m_rttState->setObjectName("RttState");
    m_rttState->setStyleSheet("color: #aaaaaa; font-size: 11px;");
    rttLayout->addWidget(m_rttState);

    // Remote live typing area
    auto *liveLabel = new QLabel(tr("Remote typing:"), rttTab);
    liveLabel->setStyleSheet("color: #888888; font-size: 10px;");
    rttLayout->addWidget(liveLabel);

    m_rttRemoteLive = new QTextEdit(rttTab);
    m_rttRemoteLive->setObjectName("RttRemoteLive");
    m_rttRemoteLive->setReadOnly(true);
    m_rttRemoteLive->setFixedHeight(48);
    m_rttRemoteLive->setPlaceholderText(tr("(remote is not typing)"));
    rttLayout->addWidget(m_rttRemoteLive);

    // Transcript
    auto *transcriptLabel = new QLabel(tr("Transcript:"), rttTab);
    transcriptLabel->setStyleSheet("color: #888888; font-size: 10px;");
    rttLayout->addWidget(transcriptLabel);

    m_rttTranscript = new QTextEdit(rttTab);
    m_rttTranscript->setObjectName("RttTranscript");
    m_rttTranscript->setReadOnly(true);
    rttLayout->addWidget(m_rttTranscript, 1);

    // Input + buttons
    m_rttInput = new QLineEdit(rttTab);
    m_rttInput->setObjectName("RttInput");
    m_rttInput->setPlaceholderText(tr("Type RTT message..."));
    rttLayout->addWidget(m_rttInput);

    auto *rttBtnRow = new QHBoxLayout();
    m_rttClear = new QPushButton(tr("Clear"), rttTab);
    m_rttSend  = new QPushButton(tr("Send"), rttTab);
    m_rttSend->setObjectName("SendBtn");
    rttBtnRow->addWidget(m_rttClear);
    rttBtnRow->addStretch();
    rttBtnRow->addWidget(m_rttSend);
    rttLayout->addLayout(rttBtnRow);

    connect(m_rttSend,  &QPushButton::clicked, this, &RttPanel::onRttSend);
    connect(m_rttInput, &QLineEdit::returnPressed, this, &RttPanel::onRttSend);
    connect(m_rttClear, &QPushButton::clicked, m_rttTranscript, &QTextEdit::clear);

    tabs->addTab(rttTab, tr("RTT"));

    // -----------------------------------------------------------------------
    // LMPE Tab
    // -----------------------------------------------------------------------
    auto *lmpeTab = new QWidget();
    auto *lmpeLayout = new QVBoxLayout(lmpeTab);
    lmpeLayout->setContentsMargins(8, 8, 8, 8);
    lmpeLayout->setSpacing(6);

    m_lmpeState = new QLabel(tr("LMPE: Inactive"), lmpeTab);
    m_lmpeState->setObjectName("LmpeState");
    m_lmpeState->setStyleSheet("color: #aaaaaa; font-size: 11px;");
    lmpeLayout->addWidget(m_lmpeState);

    m_lmpeList = new QListWidget(lmpeTab);
    m_lmpeList->setObjectName("LmpeList");
    lmpeLayout->addWidget(m_lmpeList, 1);

    m_lmpeInput = new QLineEdit(lmpeTab);
    m_lmpeInput->setObjectName("LmpeInput");
    m_lmpeInput->setPlaceholderText(tr("Type LMPE message..."));
    lmpeLayout->addWidget(m_lmpeInput);

    auto *lmpeBtnRow = new QHBoxLayout();
    m_lmpeSend = new QPushButton(tr("Send"), lmpeTab);
    m_lmpeSend->setObjectName("SendBtn");
    lmpeBtnRow->addStretch();
    lmpeBtnRow->addWidget(m_lmpeSend);
    lmpeLayout->addLayout(lmpeBtnRow);

    connect(m_lmpeSend,  &QPushButton::clicked, this, &RttPanel::onLmpeSend);
    connect(m_lmpeInput, &QLineEdit::returnPressed, this, &RttPanel::onLmpeSend);

    tabs->addTab(lmpeTab, tr("LMPE"));
}

void RttPanel::onRttSend()
{
    const QString text = m_rttInput->text().trimmed();
    if (text.isEmpty())
        return;
    m_rttTranscript->append(tr("You: %1").arg(text));
    m_rttInput->clear();
    emit rttMessageSent(text);
}

void RttPanel::onLmpeSend()
{
    const QString text = m_lmpeInput->text().trimmed();
    if (text.isEmpty())
        return;
    m_lmpeList->addItem(tr("You: %1").arg(text));
    m_lmpeInput->clear();
    emit lmpeMessageSent(text);
}
