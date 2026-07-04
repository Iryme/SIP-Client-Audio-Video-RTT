#include "DiagnosticsCenterPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/DiagnosticsBundleExporter.h"
#include "core/DiagnosticsCollector.h"

namespace {
QString yesNo(bool b) { return b ? QObject::tr("Yes") : QObject::tr("No"); }
QString connectedText(bool b) { return b ? QObject::tr("Connected") : QObject::tr("Not connected"); }
} // namespace

DiagnosticsCenterPanel::DiagnosticsCenterPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel(tr("<b>Diagnostics Center</b>"), this));
    topRow->addStretch(1);
    m_bundleBtn = new QPushButton(tr("Generate Diagnostics Bundle"), this);
    connect(m_bundleBtn, &QPushButton::clicked, this, &DiagnosticsCenterPanel::onGenerateBundle);
    topRow->addWidget(m_bundleBtn);
    root->addLayout(topRow);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildOverviewTab(), tr("Overview"));
    m_tabs->addTab(buildSipTab(),      tr("SIP"));
    m_tabs->addTab(buildMediaTab(),    tr("Media"));
    m_tabs->addTab(buildRtpTab(),      tr("RTP"));
    m_tabs->addTab(buildAudioTab(),    tr("Audio"));
    m_tabs->addTab(buildVideoTab(),    tr("Video"));
    m_tabs->addTab(buildNetworkTab(),  tr("Network"));
    m_tabs->addTab(buildSystemTab(),   tr("System"));
    root->addWidget(m_tabs, 1);

    connect(&DiagnosticsCollector::instance(), &DiagnosticsCollector::snapshotUpdated,
            this, &DiagnosticsCenterPanel::applySnapshot);
    applySnapshot(DiagnosticsCollector::instance().snapshot());
}

QLabel *DiagnosticsCenterPanel::addRow(QFormLayout *form, const QString &key, const QString &labelText)
{
    auto *value = new QLabel(this);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(labelText, value);
    m_values.insert(key, value);
    return value;
}

void DiagnosticsCenterPanel::setValue(const QString &key, const QString &text)
{
    if (auto *label = m_values.value(key, nullptr))
        label->setText(text);
}

QWidget *DiagnosticsCenterPanel::buildOverviewTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    addRow(form, QStringLiteral("ov.registration"), tr("Registration:"));
    addRow(form, QStringLiteral("ov.call"),         tr("Current Call:"));
    addRow(form, QStringLiteral("ov.profile"),      tr("Current SIP Profile:"));
    addRow(form, QStringLiteral("ov.remoteUri"),    tr("Remote URI:"));
    addRow(form, QStringLiteral("ov.audio"),        tr("Audio:"));
    addRow(form, QStringLiteral("ov.video"),        tr("Video:"));
    addRow(form, QStringLiteral("ov.rtt"),          tr("RTT:"));
    addRow(form, QStringLiteral("ov.microphone"),   tr("Microphone:"));
    addRow(form, QStringLiteral("ov.speaker"),      tr("Speaker:"));
    addRow(form, QStringLiteral("ov.camera"),       tr("Camera:"));
    addRow(form, QStringLiteral("ov.localIp"),      tr("Local IP:"));
    addRow(form, QStringLiteral("ov.transport"),    tr("Transport:"));
    addRow(form, QStringLiteral("ov.jitter"),       tr("Jitter:"));
    addRow(form, QStringLiteral("ov.loss"),         tr("Loss:"));
    addRow(form, QStringLiteral("ov.rttMs"),        tr("Round-trip time:"));
    addRow(form, QStringLiteral("ov.lastError"),    tr("Last SIP error:"));
    addRow(form, QStringLiteral("ov.version"),      tr("Version:"));
    addRow(form, QStringLiteral("ov.gitCommit"),    tr("Git commit:"));
    return page;
}

QWidget *DiagnosticsCenterPanel::buildSipTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout();
    addRow(form, QStringLiteral("sip.registration"),  tr("Current registration:"));
    addRow(form, QStringLiteral("sip.registrar"),     tr("Registrar:"));
    addRow(form, QStringLiteral("sip.outboundProxy"), tr("Outbound proxy:"));
    addRow(form, QStringLiteral("sip.transport"),     tr("Transport:"));
    addRow(form, QStringLiteral("sip.profile"),       tr("Current profile:"));
    addRow(form, QStringLiteral("sip.callId"),        tr("Call-ID:"));
    addRow(form, QStringLiteral("sip.dialogState"),   tr("Dialog state:"));
    addRow(form, QStringLiteral("sip.lastResponse"),  tr("Last SIP response:"));
    layout->addLayout(form);

    auto *btnRow = new QHBoxLayout();
    auto *ladderBtn = new QPushButton(tr("Open SIP Ladder"), page);
    auto *logsBtn = new QPushButton(tr("Open Logs"), page);
    connect(ladderBtn, &QPushButton::clicked, this, &DiagnosticsCenterPanel::openSipLadderRequested);
    connect(logsBtn,   &QPushButton::clicked, this, &DiagnosticsCenterPanel::openLogsRequested);
    btnRow->addWidget(ladderBtn);
    btnRow->addWidget(logsBtn);
    btnRow->addStretch(1);
    layout->addLayout(btnRow);
    layout->addStretch(1);
    return page;
}

QWidget *DiagnosticsCenterPanel::buildMediaTab()
{
    // Not itemized in the spec's per-tab sections — this is a compact combined
    // summary of the audio/video/RTT connection state already collected for
    // the Overview/RTP/Audio/Video tabs, kept here for a single "is media
    // flowing at all" glance.
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    addRow(form, QStringLiteral("media.audio"), tr("Audio:"));
    addRow(form, QStringLiteral("media.video"), tr("Video:"));
    addRow(form, QStringLiteral("media.rtt"),   tr("RTT:"));
    return page;
}

QWidget *DiagnosticsCenterPanel::buildRtpTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *audioBox = new QGroupBox(tr("Audio"), page);
    auto *audioForm = new QFormLayout(audioBox);
    addRow(audioForm, QStringLiteral("rtp.audio.rx"),     tr("Packets RX:"));
    addRow(audioForm, QStringLiteral("rtp.audio.tx"),     tr("Packets TX:"));
    addRow(audioForm, QStringLiteral("rtp.audio.loss"),   tr("Packet loss:"));
    addRow(audioForm, QStringLiteral("rtp.audio.jitter"), tr("Jitter:"));
    addRow(audioForm, QStringLiteral("rtp.audio.rtt"),    tr("RTT:"));
    addRow(audioForm, QStringLiteral("rtp.audio.codec"),  tr("Codec:"));
    addRow(audioForm, QStringLiteral("rtp.audio.ptime"),  tr("ptime:"));
    layout->addWidget(audioBox);

    auto *videoBox = new QGroupBox(tr("Video"), page);
    auto *videoForm = new QFormLayout(videoBox);
    addRow(videoForm, QStringLiteral("rtp.video.codec"),      tr("Codec:"));
    addRow(videoForm, QStringLiteral("rtp.video.resolution"), tr("Resolution:"));
    addRow(videoForm, QStringLiteral("rtp.video.fps"),        tr("FPS:"));
    addRow(videoForm, QStringLiteral("rtp.video.bitrate"),    tr("Bitrate:"));
    layout->addWidget(videoBox);
    layout->addStretch(1);
    return page;
}

QWidget *DiagnosticsCenterPanel::buildAudioTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    addRow(form, QStringLiteral("audio.capture"),  tr("Capture device:"));
    addRow(form, QStringLiteral("audio.playback"), tr("Playback device:"));
    addRow(form, QStringLiteral("audio.micVol"),   tr("Mic volume:"));
    addRow(form, QStringLiteral("audio.spkVol"),   tr("Speaker volume:"));
    addRow(form, QStringLiteral("audio.micLevel"), tr("Mic level:"));
    addRow(form, QStringLiteral("audio.spkLevel"), tr("Speaker level:"));
    addRow(form, QStringLiteral("audio.mute"),     tr("Mute state:"));
    return page;
}

QWidget *DiagnosticsCenterPanel::buildVideoTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    addRow(form, QStringLiteral("video.camera"),     tr("Camera:"));
    addRow(form, QStringLiteral("video.state"),      tr("Video state:"));
    addRow(form, QStringLiteral("video.muted"),      tr("Video muted:"));
    addRow(form, QStringLiteral("video.resolution"), tr("Resolution:"));
    addRow(form, QStringLiteral("video.fps"),        tr("FPS:"));
    addRow(form, QStringLiteral("video.codec"),      tr("Codec:"));
    return page;
}

QWidget *DiagnosticsCenterPanel::buildNetworkTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    addRow(form, QStringLiteral("net.localIp"),   tr("Local IP:"));
    addRow(form, QStringLiteral("net.remoteIp"),  tr("Remote IP:"));
    addRow(form, QStringLiteral("net.localPort"), tr("Local port:"));
    addRow(form, QStringLiteral("net.remotePort"),tr("Remote port:"));
    addRow(form, QStringLiteral("net.transport"), tr("Transport:"));
    addRow(form, QStringLiteral("net.ice"),       tr("ICE:"));
    addRow(form, QStringLiteral("net.stun"),      tr("STUN:"));
    addRow(form, QStringLiteral("net.turn"),      tr("TURN:"));
    return page;
}

QWidget *DiagnosticsCenterPanel::buildSystemTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    addRow(form, QStringLiteral("sys.qt"),        tr("Qt version:"));
    addRow(form, QStringLiteral("sys.pjsip"),     tr("PJSIP version:"));
    addRow(form, QStringLiteral("sys.appVersion"),tr("Application version:"));
    addRow(form, QStringLiteral("sys.gitCommit"), tr("Git commit:"));
    addRow(form, QStringLiteral("sys.platform"),  tr("Platform:"));
    addRow(form, QStringLiteral("sys.arch"),      tr("Architecture:"));
    addRow(form, QStringLiteral("sys.buildType"), tr("Build type:"));
    addRow(form, QStringLiteral("sys.compiler"),  tr("Compiler:"));
    return page;
}

void DiagnosticsCenterPanel::applySnapshot(const DiagnosticsSnapshot &s)
{
    const QString registration = QStringLiteral("%1 (%2)").arg(s.registrationState, s.registrationStatusText);
    const QString audioSummary = QStringLiteral("%1 — %2").arg(connectedText(s.audioConnected), s.audioCodec);
    const QString videoSummary = QStringLiteral("%1 — %2").arg(connectedText(s.videoConnected), s.videoCodec);
    const QString resolutionText = s.videoResolution.isValid()
        ? QStringLiteral("%1x%2").arg(s.videoResolution.width()).arg(s.videoResolution.height())
        : diagnosticsNotAvailable();

    // Overview
    setValue(QStringLiteral("ov.registration"), registration);
    setValue(QStringLiteral("ov.call"),         s.callState);
    setValue(QStringLiteral("ov.profile"),      s.currentProfileName.isEmpty() ? tr("(none)") : s.currentProfileName);
    setValue(QStringLiteral("ov.remoteUri"),    s.remoteUri.isEmpty() ? diagnosticsNotAvailable() : s.remoteUri);
    setValue(QStringLiteral("ov.audio"),        audioSummary);
    setValue(QStringLiteral("ov.video"),        videoSummary);
    setValue(QStringLiteral("ov.rtt"),          s.rttState);
    setValue(QStringLiteral("ov.microphone"),   s.microphoneName);
    setValue(QStringLiteral("ov.speaker"),      s.speakerName);
    setValue(QStringLiteral("ov.camera"),       s.cameraName);
    setValue(QStringLiteral("ov.localIp"),      s.localIp);
    setValue(QStringLiteral("ov.transport"),    s.transport.isEmpty() ? diagnosticsNotAvailable() : s.transport);
    setValue(QStringLiteral("ov.jitter"),       s.audioJitterAvailable ? QStringLiteral("%1 ms").arg(s.audioJitterMs, 0, 'f', 1) : diagnosticsNotAvailable());
    setValue(QStringLiteral("ov.loss"),         s.audioLossAvailable ? QStringLiteral("%1%").arg(s.audioLossPercent, 0, 'f', 2) : diagnosticsNotAvailable());
    setValue(QStringLiteral("ov.rttMs"),        s.audioRttAvailable ? QStringLiteral("%1 ms").arg(s.audioRttMs, 0, 'f', 1) : diagnosticsNotAvailable());
    setValue(QStringLiteral("ov.lastError"),    s.lastSipError.isEmpty() ? tr("(none)") : s.lastSipError);
    setValue(QStringLiteral("ov.version"),      s.appVersion);
    setValue(QStringLiteral("ov.gitCommit"),    s.gitCommit);

    // SIP
    setValue(QStringLiteral("sip.registration"),  registration);
    setValue(QStringLiteral("sip.registrar"),     s.registrar.isEmpty() ? diagnosticsNotAvailable() : s.registrar);
    setValue(QStringLiteral("sip.outboundProxy"), s.outboundProxy.isEmpty() ? diagnosticsNotAvailable() : s.outboundProxy);
    setValue(QStringLiteral("sip.transport"),     s.transport.isEmpty() ? diagnosticsNotAvailable() : s.transport);
    setValue(QStringLiteral("sip.profile"),       s.currentProfileName.isEmpty() ? tr("(none)") : s.currentProfileName);
    setValue(QStringLiteral("sip.callId"),        s.callId);
    setValue(QStringLiteral("sip.dialogState"),   s.dialogState);
    setValue(QStringLiteral("sip.lastResponse"),  s.lastSipResponse.isEmpty() ? diagnosticsNotAvailable() : s.lastSipResponse);

    // Media (summary)
    setValue(QStringLiteral("media.audio"), audioSummary);
    setValue(QStringLiteral("media.video"), videoSummary);
    setValue(QStringLiteral("media.rtt"),   s.rttState);

    // RTP
    setValue(QStringLiteral("rtp.audio.rx"),     s.audioPacketsRxAvailable ? QString::number(s.audioPacketsRx) : diagnosticsNotAvailable());
    setValue(QStringLiteral("rtp.audio.tx"),     s.audioPacketsTx);
    setValue(QStringLiteral("rtp.audio.loss"),   s.audioLossAvailable ? QStringLiteral("%1%").arg(s.audioLossPercent, 0, 'f', 2) : diagnosticsNotAvailable());
    setValue(QStringLiteral("rtp.audio.jitter"), s.audioJitterAvailable ? QStringLiteral("%1 ms").arg(s.audioJitterMs, 0, 'f', 1) : diagnosticsNotAvailable());
    setValue(QStringLiteral("rtp.audio.rtt"),    s.audioRttAvailable ? QStringLiteral("%1 ms").arg(s.audioRttMs, 0, 'f', 1) : diagnosticsNotAvailable());
    setValue(QStringLiteral("rtp.audio.codec"),  s.audioCodec);
    setValue(QStringLiteral("rtp.audio.ptime"),  s.audioPtime);
    setValue(QStringLiteral("rtp.video.codec"),      s.videoCodec);
    setValue(QStringLiteral("rtp.video.resolution"), resolutionText);
    setValue(QStringLiteral("rtp.video.fps"),        s.videoFps > 0 ? QString::number(s.videoFps) : diagnosticsNotAvailable());
    setValue(QStringLiteral("rtp.video.bitrate"),    s.videoBitrateKbps > 0 ? tr("%1 kbps").arg(s.videoBitrateKbps) : diagnosticsNotAvailable());

    // Audio
    setValue(QStringLiteral("audio.capture"),  s.microphoneName);
    setValue(QStringLiteral("audio.playback"), s.speakerName);
    setValue(QStringLiteral("audio.micVol"),   tr("%1%").arg(s.microphoneVolume));
    setValue(QStringLiteral("audio.spkVol"),   tr("%1%").arg(s.speakerVolume));
    setValue(QStringLiteral("audio.micLevel"), tr("%1%").arg(s.microphoneLevel));
    setValue(QStringLiteral("audio.spkLevel"), tr("%1%").arg(s.speakerLevel));
    setValue(QStringLiteral("audio.mute"),     yesNo(s.audioMuted));

    // Video
    setValue(QStringLiteral("video.camera"),     s.cameraName);
    setValue(QStringLiteral("video.state"),      s.cameraEnabled ? tr("Enabled") : tr("Disabled"));
    setValue(QStringLiteral("video.muted"),      yesNo(s.videoMuted));
    setValue(QStringLiteral("video.resolution"), resolutionText);
    setValue(QStringLiteral("video.fps"),        s.videoFps > 0 ? QString::number(s.videoFps) : diagnosticsNotAvailable());
    setValue(QStringLiteral("video.codec"),      s.videoCodec);

    // Network
    setValue(QStringLiteral("net.localIp"),    s.localIp);
    setValue(QStringLiteral("net.remoteIp"),   s.remoteIp);
    setValue(QStringLiteral("net.localPort"),  s.localPort);
    setValue(QStringLiteral("net.remotePort"), s.remotePort);
    setValue(QStringLiteral("net.transport"),  s.transport.isEmpty() ? diagnosticsNotAvailable() : s.transport);
    setValue(QStringLiteral("net.ice"),        s.ice);
    setValue(QStringLiteral("net.stun"),       s.stun);
    setValue(QStringLiteral("net.turn"),       s.turn);

    // System
    setValue(QStringLiteral("sys.qt"),         s.qtVersion);
    setValue(QStringLiteral("sys.pjsip"),      s.pjsipVersion);
    setValue(QStringLiteral("sys.appVersion"), s.appVersion);
    setValue(QStringLiteral("sys.gitCommit"),  s.gitCommit);
    setValue(QStringLiteral("sys.platform"),   s.platform);
    setValue(QStringLiteral("sys.arch"),       s.architecture);
    setValue(QStringLiteral("sys.buildType"),  s.buildType);
    setValue(QStringLiteral("sys.compiler"),   s.compiler);
}

void DiagnosticsCenterPanel::onGenerateBundle()
{
    QString error;
    const QString path = DiagnosticsBundleExporter::generateBundle(DiagnosticsCollector::instance().snapshot(), &error);
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Diagnostics Bundle"),
                              tr("Failed to generate diagnostics bundle.\n%1").arg(error));
        return;
    }
    QMessageBox::information(this, tr("Diagnostics Bundle"),
                              tr("Diagnostics bundle written to:\n%1\n\n"
                                 "(Folder only for now — zipping is a follow-up task.)").arg(path));
}
