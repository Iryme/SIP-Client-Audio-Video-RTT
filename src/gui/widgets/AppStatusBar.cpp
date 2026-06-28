#include "AppStatusBar.h"
#include "sip/SipAccount.h"
#include <QLabel>
#include <QFrame>

static QLabel *makeStatusLabel(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet("color: #aaaaaa; font-size: 11px; padding: 0 6px;");
    return lbl;
}

static void setMetricLabel(QLabel *label, const QString &prefix, const QString &value,
                           const QString &tooltip)
{
    label->setText(prefix + value);
    label->setToolTip(tooltip);
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
void AppStatusBar::setLocalIp(const QString &s, const QString &tooltip)
{
    setMetricLabel(m_localIp, QStringLiteral("IP: "), s,
                   tooltip.isEmpty() ? QStringLiteral("Source: local network interface fallback")
                                     : tooltip);
}
void AppStatusBar::setRtpStats(const RtpStatsSnapshot &stats)
{
    if (!stats.available) {
        // No active call or call not in a state that can provide stats — reset to dash.
        m_jitter->setText(QStringLiteral("Jitter: —"));
        m_jitter->setToolTip(stats.reason.isEmpty()
            ? QStringLiteral("No active call")
            : stats.reason);
        m_loss->setText(QStringLiteral("Loss: —"));
        m_loss->setToolTip(m_jitter->toolTip());
        m_rttLatency->setText(QStringLiteral("RTT: —"));
        m_rttLatency->setToolTip(m_jitter->toolTip());
        return;
    }

    const QString baseTooltip = stats.reason.isEmpty()
        ? QStringLiteral("PJSIP RTCP statistics")
        : stats.reason;
    const QString streamHint = stats.streamIndex >= 0
        ? QStringLiteral("Stream %1 (%2)")
              .arg(stats.streamIndex)
              .arg(stats.streamType.isEmpty() ? QStringLiteral("unknown") : stats.streamType)
        : QString();

    const auto tooltipFor = [&](const QString &detail) {
        return streamHint.isEmpty()
            ? QStringLiteral("%1 | %2").arg(baseTooltip, detail)
            : QStringLiteral("%1 | %2 | %3").arg(baseTooltip, detail, streamHint);
    };

    if (stats.jitterAvailable) {
        setMetricLabel(m_jitter, QStringLiteral("Jitter: "),
                       QStringLiteral("%1 ms").arg(stats.jitterMs, 0, 'f', 1),
                       tooltipFor(QStringLiteral("PJSIP RTCP jitter")));
    } else {
        setMetricLabel(m_jitter, QStringLiteral("Jitter: "), QStringLiteral("N/A"),
                       tooltipFor(QStringLiteral("RTCP samples not available yet")));
    }

    if (stats.packetLossAvailable) {
        setMetricLabel(m_loss, QStringLiteral("Loss: "),
                       QStringLiteral("%1 %").arg(stats.packetLossPercent, 0, 'f', 1),
                       tooltipFor(QStringLiteral("packets received=%1 lost=%2")
                                  .arg(stats.packetReceivedPackets)
                                  .arg(stats.packetLossPackets)));
    } else {
        setMetricLabel(m_loss, QStringLiteral("Loss: "), QStringLiteral("N/A"),
                       tooltipFor(QStringLiteral("RTCP samples not available yet")));
    }

    if (stats.rttAvailable) {
        setMetricLabel(m_rttLatency, QStringLiteral("RTT: "),
                       QStringLiteral("%1 ms").arg(stats.rttMs, 0, 'f', 1),
                       tooltipFor(QStringLiteral("PJSIP RTCP RTT")));
    } else {
        setMetricLabel(m_rttLatency, QStringLiteral("RTT: "), QStringLiteral("N/A"),
                       tooltipFor(QStringLiteral("RTCP samples not available yet")));
    }
}
void AppStatusBar::setJitter(const QString &s, const QString &tooltip)
{
    m_jitter->setText("Jitter: " + s);
    m_jitter->setToolTip(tooltip.isEmpty() ? QStringLiteral("RTP statistics not available in this build")
                                           : tooltip);
}
void AppStatusBar::setPacketLoss(const QString &s, const QString &tooltip)
{
    m_loss->setText("Loss: " + s);
    m_loss->setToolTip(tooltip.isEmpty() ? QStringLiteral("RTP statistics not available in this build")
                                         : tooltip);
}
void AppStatusBar::setRttLatency(const QString &s, const QString &tooltip)
{
    m_rttLatency->setText("RTT: " + s);
    m_rttLatency->setToolTip(tooltip.isEmpty() ? QStringLiteral("RTT real-time text state")
                                               : tooltip);
}

void AppStatusBar::setSipBackend(const QString &name, bool initialized)
{
    const QString state = initialized ? QStringLiteral("ready") : QStringLiteral("inactive");
    m_sipBackend->setText(QStringLiteral("SIP: %1 (%2)").arg(name, state));
}

void AppStatusBar::setRegistrationStatus(RegistrationState state,
                                         const QString &statusText,
                                         int statusCode)
{
    QString text;
    QString color = QStringLiteral("#e05050");
    switch (state) {
    case RegistrationState::Unregistered:
        text = QStringLiteral("Unregistered");
        break;
    case RegistrationState::Registering:
        text  = QStringLiteral("Registering");
        color = QStringLiteral("#e0b850");
        break;
    case RegistrationState::Unregistering:
        text  = QStringLiteral("Unregistering");
        color = QStringLiteral("#e0b850");
        break;
    case RegistrationState::Registered:
        text = QStringLiteral("Registered");
        color = QStringLiteral("#50c878");
        break;
    case RegistrationState::RegistrationFailed:
        text = QStringLiteral("Registration failed");
        break;
    }

    m_connState->setText(text);
    m_connState->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; padding: 0 6px;").arg(color));
    m_connState->setToolTip(statusCode == 0
        ? statusText
        : QStringLiteral("%1 (status %2)").arg(statusText).arg(statusCode));
}
