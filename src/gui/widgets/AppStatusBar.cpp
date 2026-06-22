#include "AppStatusBar.h"
#include <QLabel>
#include <QFrame>

static QLabel *makeStatusLabel(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet("color: #aaaaaa; font-size: 11px; padding: 0 6px;");
    return lbl;
}

static QFrame *makeSep(QWidget *parent)
{
    auto *sep = new QFrame(parent);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setStyleSheet("color: #444444;");
    return sep;
}

AppStatusBar::AppStatusBar(QWidget *parent)
    : QStatusBar(parent)
{
    setFixedHeight(28);
    setSizeGripEnabled(false);
    setObjectName("AppStatusBar");

    m_connState  = makeStatusLabel("● Disconnected", this);
    m_connState->setStyleSheet("color: #e05050; font-size: 11px; padding: 0 6px;");

    m_account    = makeStatusLabel("No account", this);
    m_transport  = makeStatusLabel("Transport: —", this);
    m_localIp    = makeStatusLabel("IP: —", this);
    m_jitter     = makeStatusLabel("Jitter: —", this);
    m_loss       = makeStatusLabel("Loss: —", this);
    m_rttLatency = makeStatusLabel("RTT: —", this);
    m_sipBackend = makeStatusLabel("SIP: —", this);

    addPermanentWidget(m_connState);
    addPermanentWidget(makeSep(this));
    addPermanentWidget(m_account);
    addPermanentWidget(makeSep(this));
    addPermanentWidget(m_transport);
    addPermanentWidget(makeSep(this));
    addPermanentWidget(m_localIp);
    addPermanentWidget(makeSep(this));
    addPermanentWidget(m_jitter);
    addPermanentWidget(makeSep(this));
    addPermanentWidget(m_loss);
    addPermanentWidget(makeSep(this));
    addPermanentWidget(m_rttLatency);
    addPermanentWidget(makeSep(this));
    addPermanentWidget(m_sipBackend);
}

void AppStatusBar::setConnectionState(const QString &s) { m_connState->setText(s); }
void AppStatusBar::setActiveAccount(const QString &s)   { m_account->setText(s); }
void AppStatusBar::setTransport(const QString &s)       { m_transport->setText("Transport: " + s); }
void AppStatusBar::setLocalIp(const QString &s)         { m_localIp->setText("IP: " + s); }
void AppStatusBar::setJitter(const QString &s)          { m_jitter->setText("Jitter: " + s); }
void AppStatusBar::setPacketLoss(const QString &s)      { m_loss->setText("Loss: " + s); }
void AppStatusBar::setRttLatency(const QString &s)      { m_rttLatency->setText("RTT: " + s); }

void AppStatusBar::setSipBackend(const QString &name, bool initialized)
{
    const QString state = initialized ? QStringLiteral("ready") : QStringLiteral("inactive");
    m_sipBackend->setText(QStringLiteral("SIP: %1 (%2)").arg(name, state));
}
