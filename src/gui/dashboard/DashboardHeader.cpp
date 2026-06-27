#include "DashboardHeader.h"
#include "sip/SipManager.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr const char *kDotBase =
    "border-radius: 9px; min-width: 18px; max-width: 18px; "
    "min-height: 18px; max-height: 18px; background-color: %1;";
constexpr const char *kSectionLabel =
    "font-size: 10px; color: #50688a; background-color: transparent; border: none;";
constexpr const char *kStatusText =
    "font-size: 11px; font-weight: 600; color: #c0ccd8; "
    "background-color: transparent; border: none;";
}

DashboardHeader::DashboardHeader(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DashboardHeader");
    setFixedHeight(90);
    setStyleSheet(R"(
        QWidget#DashboardHeader {
            background-color: #0f1420;
            border-bottom: 1px solid #1e2a3a;
        }
    )");

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(24, 0, 24, 0);
    root->setSpacing(0);

    // --- Left: app identity ---
    auto *leftBlock = new QVBoxLayout();
    leftBlock->setSpacing(3);
    leftBlock->setAlignment(Qt::AlignVCenter);

    auto *appName = new QLabel(QStringLiteral("Iryme SIP Client"), this);
    appName->setStyleSheet("font-size: 18px; font-weight: 700; color: #d4d8e0; "
                           "background-color: transparent;");

    const QString subText = QStringLiteral("Audio / Video / RTT  ·  v0.1.0  ·  %1  ·  %2")
        .arg(SipManager::instance().backendName(), QSysInfo::prettyProductName());
    auto *appSub = new QLabel(subText, this);
    appSub->setStyleSheet("font-size: 10px; color: #506080; background-color: transparent;");

    m_clockLabel = new QLabel(this);
    m_clockLabel->setStyleSheet("font-size: 10px; color: #3a6090; background-color: transparent;");

    leftBlock->addStretch(1);
    leftBlock->addWidget(appName);
    leftBlock->addWidget(appSub);
    leftBlock->addWidget(m_clockLabel);
    leftBlock->addStretch(1);

    root->addLayout(leftBlock, 1);
    root->addStretch(1);

    // --- Divider ---
    auto *div = new QFrame(this);
    div->setFrameShape(QFrame::VLine);
    div->setFixedHeight(50);
    div->setStyleSheet("background-color: #1e2a3a; max-width: 1px; border: none;");
    root->addWidget(div);
    root->addSpacing(28);

    // --- Right: status indicators ---
    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(28);
    statusRow->setAlignment(Qt::AlignVCenter);

    statusRow->addWidget(makeStatusBlock(tr("SIP STATUS"),   this, &m_sipDot,   &m_sipText));
    statusRow->addWidget(makeStatusBlock(tr("MEDIA"),        this, &m_mediaDot, &m_mediaText));
    statusRow->addWidget(makeStatusBlock(tr("ACTIVE CALL"),  this, &m_callDot,  &m_callText));

    root->addLayout(statusRow);

    updateClock();
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &DashboardHeader::updateClock);
    timer->start();

    setSipStatus(RegistrationState::Unregistered);
    setMediaStatus(false);
    setCallStatus(CallState::Idle);
}

QWidget *DashboardHeader::makeStatusBlock(const QString &category, QWidget *parent,
                                          QLabel **dotOut, QLabel **statusTextOut)
{
    auto *w = new QWidget(parent);
    w->setStyleSheet("background-color: transparent;");
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    auto *dotRow = new QHBoxLayout();
    dotRow->setAlignment(Qt::AlignHCenter);
    auto *dot = new QLabel(w);
    dot->setFixedSize(18, 18);
    dotRow->addWidget(dot);

    auto *catLabel = new QLabel(category, w);
    catLabel->setStyleSheet(kSectionLabel);
    catLabel->setAlignment(Qt::AlignHCenter);

    auto *statusText = new QLabel(w);
    statusText->setStyleSheet(kStatusText);
    statusText->setAlignment(Qt::AlignHCenter);
    statusText->setMinimumWidth(90);

    lay->addStretch(1);
    lay->addLayout(dotRow);
    lay->addWidget(catLabel);
    lay->addWidget(statusText);
    lay->addStretch(1);

    if (dotOut)        *dotOut        = dot;
    if (statusTextOut) *statusTextOut = statusText;
    return w;
}

void DashboardHeader::applyDot(QLabel *dot, QLabel *text,
                               const QString &color, const QString &label)
{
    dot->setStyleSheet(QString::fromLatin1(kDotBase).arg(color));
    text->setText(label);
}

void DashboardHeader::updateClock()
{
    m_clockLabel->setText(QDateTime::currentDateTime()
        .toString(QStringLiteral("dddd, dd MMM yyyy  hh:mm:ss")));
}

void DashboardHeader::setSipStatus(RegistrationState state)
{
    switch (state) {
    case RegistrationState::Registered:
        applyDot(m_sipDot, m_sipText, "#4caf50", tr("Registered"));
        break;
    case RegistrationState::Registering:
    case RegistrationState::Unregistering:
        applyDot(m_sipDot, m_sipText, "#ffa726", tr("Registering…"));
        break;
    case RegistrationState::RegistrationFailed:
        applyDot(m_sipDot, m_sipText, "#ef5350", tr("Failed"));
        break;
    default:
        applyDot(m_sipDot, m_sipText, "#37474f", tr("Not Registered"));
        break;
    }
}

void DashboardHeader::setMediaStatus(bool connected)
{
    if (connected)
        applyDot(m_mediaDot, m_mediaText, "#4caf50", tr("Active"));
    else
        applyDot(m_mediaDot, m_mediaText, "#37474f", tr("Idle"));
}

void DashboardHeader::setCallStatus(CallState state)
{
    switch (state) {
    case CallState::Active:
        applyDot(m_callDot, m_callText, "#4caf50", tr("In Call"));
        break;
    case CallState::OutgoingInit:
    case CallState::Ringing:
    case CallState::IncomingRinging:
    case CallState::Connecting:
        applyDot(m_callDot, m_callText, "#ffa726", tr("Connecting…"));
        break;
    case CallState::Held:
        applyDot(m_callDot, m_callText, "#ff7043", tr("On Hold"));
        break;
    case CallState::Disconnecting:
        applyDot(m_callDot, m_callText, "#ffa726", tr("Ending…"));
        break;
    case CallState::Failed:
        applyDot(m_callDot, m_callText, "#ef5350", tr("Failed"));
        break;
    default:
        applyDot(m_callDot, m_callText, "#37474f", tr("No Active Call"));
        break;
    }
}
