#include "DiagnosticsCenterPanel.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "core/DiagnosticsBundleExporter.h"
#include "core/DiagnosticsCollector.h"
#include "core/DiagnosticsTimelineExporter.h"
#include "core/DiagnosticsTimelineFilterProxyModel.h"
#include "core/DiagnosticsTimelineService.h"

namespace {
QString yesNo(bool b) { return b ? QObject::tr("Yes") : QObject::tr("No"); }
QString connectedText(bool b) { return b ? QObject::tr("Connected") : QObject::tr("Not connected"); }

using CategoryFilter = DiagnosticsTimelineFilterProxyModel::CategoryFilter;

CategoryFilter categoryFilterForIndex(int index)
{
    switch (index) {
    case 1:  return CategoryFilter::Registration;
    case 2:  return CategoryFilter::Call;
    case 3:  return CategoryFilter::Sip;
    case 4:  return CategoryFilter::Media;
    case 5:  return CategoryFilter::Rtp;
    case 6:  return CategoryFilter::Camera;
    case 7:  return CategoryFilter::Audio;
    case 8:  return CategoryFilter::Video;
    case 9:  return CategoryFilter::Rtt;
    case 10: return CategoryFilter::Warnings;
    case 11: return CategoryFilter::Errors;
    default: return CategoryFilter::All;
    }
}
} // namespace

DiagnosticsCenterPanel::DiagnosticsCenterPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel(tr("<b>Diagnostics Center</b>"), this));
    topRow->addStretch(1);
    m_bundleStatus = new QLabel(this);
    m_bundleStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    topRow->addWidget(m_bundleStatus);
    m_bundleOpenFolderBtn = new QPushButton(tr("Open Folder"), this);
    m_bundleOpenFolderBtn->setEnabled(false);
    connect(m_bundleOpenFolderBtn, &QPushButton::clicked, this, &DiagnosticsCenterPanel::onOpenBundleFolder);
    topRow->addWidget(m_bundleOpenFolderBtn);
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
    m_tabs->addTab(buildTimelineTab(), tr("Timeline"));
    root->addWidget(m_tabs, 1);

    connect(&DiagnosticsCollector::instance(), &DiagnosticsCollector::snapshotUpdated,
            this, &DiagnosticsCenterPanel::applySnapshot);
    applySnapshot(DiagnosticsCollector::instance().snapshot());

    connect(DiagnosticsTimelineService::instance().model(), &DiagnosticsTimelineModel::entryAppended,
            this, &DiagnosticsCenterPanel::onTimelineEntryAppended);
    refreshTimelineTable();
    for (const auto &e : DiagnosticsTimelineService::instance().model()->entries())
        refreshRecentActivity(e);
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
    auto *outer = new QVBoxLayout(page);

    auto *formHost = new QWidget(page);
    auto *form = new QFormLayout(formHost);
    outer->addWidget(formHost);
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

    // Recent Activity — last 5 Diagnostics Timeline entries (task M). Reads
    // only DiagnosticsTimelineService's model, incrementally updated via
    // entryAppended, never rebuilt from scratch except at construction.
    outer->addWidget(new QLabel(tr("<b>Recent Activity</b>"), page));
    m_recentActivity = new QListWidget(page);
    m_recentActivity->setMaximumHeight(140);
    m_recentActivity->setSelectionMode(QAbstractItemView::NoSelection);
    m_recentActivity->setFocusPolicy(Qt::NoFocus);
    outer->addWidget(m_recentActivity);

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
    addRow(form, QStringLiteral("audio.codec"),       tr("Codec:"));
    addRow(form, QStringLiteral("audio.payloadType"), tr("Payload type:"));
    addRow(form, QStringLiteral("audio.clockRate"),   tr("Clock rate:"));
    addRow(form, QStringLiteral("audio.channels"),    tr("Channels:"));
    addRow(form, QStringLiteral("audio.ptime"),       tr("ptime:"));
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
    addRow(form, QStringLiteral("video.codec"),       tr("Codec:"));
    addRow(form, QStringLiteral("video.payloadType"), tr("Payload type:"));
    addRow(form, QStringLiteral("video.resolution"),  tr("Resolution:"));
    addRow(form, QStringLiteral("video.fps"),         tr("FPS:"));
    addRow(form, QStringLiteral("video.bitrate"),     tr("Bitrate:"));
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

QWidget *DiagnosticsCenterPanel::buildTimelineTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *toolbar = new QHBoxLayout();
    m_timelineSearch = new QLineEdit(page);
    m_timelineSearch->setPlaceholderText(tr("Search title, details, URI, SIP code, category..."));
    m_timelineSearch->setMinimumWidth(220);
    toolbar->addWidget(m_timelineSearch, 1);

    m_timelineFilter = new QComboBox(page);
    m_timelineFilter->addItem(tr("All"));
    m_timelineFilter->addItem(tr("Registration"));
    m_timelineFilter->addItem(tr("Call"));
    m_timelineFilter->addItem(tr("SIP"));
    m_timelineFilter->addItem(tr("Media"));
    m_timelineFilter->addItem(tr("RTP"));
    m_timelineFilter->addItem(tr("Camera"));
    m_timelineFilter->addItem(tr("Audio"));
    m_timelineFilter->addItem(tr("Video"));
    m_timelineFilter->addItem(tr("RTT"));
    m_timelineFilter->addItem(tr("Warnings"));
    m_timelineFilter->addItem(tr("Errors"));
    toolbar->addWidget(m_timelineFilter);

    m_timelineAutoScroll = new QCheckBox(tr("Auto Scroll"), page);
    m_timelineAutoScroll->setChecked(true);
    toolbar->addWidget(m_timelineAutoScroll);

    auto *exportJsonBtn = new QPushButton(tr("Export Timeline JSON"), page);
    auto *exportTxtBtn  = new QPushButton(tr("Export Timeline TXT"), page);
    toolbar->addWidget(exportJsonBtn);
    toolbar->addWidget(exportTxtBtn);
    layout->addLayout(toolbar);

    m_timelineTable = new QTableWidget(0, 4, page);
    m_timelineTable->setHorizontalHeaderLabels({tr("Time"), tr("Category"), tr("Severity"), tr("Title / Details")});
    m_timelineTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_timelineTable->verticalHeader()->setVisible(false);
    m_timelineTable->setSelectionBehavior(QTableWidget::SelectRows);
    m_timelineTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_timelineTable->setAlternatingRowColors(true);
    m_timelineTable->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_timelineTable, 1);

    connect(m_timelineSearch, &QLineEdit::textChanged, this, &DiagnosticsCenterPanel::onTimelineFilterChanged);
    connect(m_timelineFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiagnosticsCenterPanel::onTimelineFilterChanged);
    connect(m_timelineTable, &QTableWidget::customContextMenuRequested,
            this, &DiagnosticsCenterPanel::onTimelineContextMenu);
    connect(exportJsonBtn, &QPushButton::clicked, this, &DiagnosticsCenterPanel::onExportTimelineJson);
    connect(exportTxtBtn,  &QPushButton::clicked, this, &DiagnosticsCenterPanel::onExportTimelineTxt);

    return page;
}

void DiagnosticsCenterPanel::applySnapshot(const DiagnosticsSnapshot &s)
{
    const QString registration = QStringLiteral("%1 (%2)").arg(s.registrationState, s.registrationStatusText);
    const QString audioSummary = QStringLiteral("%1 — %2").arg(connectedText(s.audioConnected), s.audioCodec.summaryString());
    const QString videoSummary = QStringLiteral("%1 — %2").arg(connectedText(s.videoConnected), s.videoCodec.summaryString());
    const QString resolutionText = s.videoResolution.isValid()
        ? QStringLiteral("%1x%2").arg(s.videoResolution.width()).arg(s.videoResolution.height())
        : diagnosticsNotAvailable();
    const QString audioChannelsText =
        s.audioCodec.channels == 1 ? tr("Mono")
        : s.audioCodec.channels == 2 ? tr("Stereo")
        : s.audioCodec.channels > 0 ? QString::number(s.audioCodec.channels)
                                     : diagnosticsNotAvailable();
    const QString audioPtimeText = s.audioCodec.ptime > 0
        ? QStringLiteral("%1 ms").arg(s.audioCodec.ptime) : diagnosticsNotAvailable();

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
    setValue(QStringLiteral("rtp.audio.codec"),  s.audioCodec.summaryString());
    setValue(QStringLiteral("rtp.audio.ptime"),  audioPtimeText);
    setValue(QStringLiteral("rtp.video.codec"),      s.videoCodec.summaryString());
    setValue(QStringLiteral("rtp.video.resolution"), resolutionText);
    setValue(QStringLiteral("rtp.video.fps"),        s.videoFps > 0 ? QString::number(s.videoFps) : diagnosticsNotAvailable());
    setValue(QStringLiteral("rtp.video.bitrate"),    s.videoBitrateKbps > 0 ? tr("%1 kbps").arg(s.videoBitrateKbps) : diagnosticsNotAvailable());

    // Audio
    setValue(QStringLiteral("audio.codec"),       s.audioCodec.isValid() ? s.audioCodec.name : diagnosticsNotAvailable());
    setValue(QStringLiteral("audio.payloadType"), s.audioCodec.payloadType >= 0 ? QString::number(s.audioCodec.payloadType) : diagnosticsNotAvailable());
    setValue(QStringLiteral("audio.clockRate"),   s.audioCodec.clockRate > 0 ? QStringLiteral("%1 Hz").arg(s.audioCodec.clockRate) : diagnosticsNotAvailable());
    setValue(QStringLiteral("audio.channels"),    audioChannelsText);
    setValue(QStringLiteral("audio.ptime"),       audioPtimeText);
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
    setValue(QStringLiteral("video.codec"),       s.videoCodec.isValid() ? s.videoCodec.name : diagnosticsNotAvailable());
    setValue(QStringLiteral("video.payloadType"), s.videoCodec.payloadType >= 0 ? QString::number(s.videoCodec.payloadType) : diagnosticsNotAvailable());
    setValue(QStringLiteral("video.resolution"),  s.videoCodec.width > 0 && s.videoCodec.height > 0
                 ? QStringLiteral("%1x%2").arg(s.videoCodec.width).arg(s.videoCodec.height) : diagnosticsNotAvailable());
    setValue(QStringLiteral("video.fps"),         s.videoCodec.fps > 0 ? QString::number(s.videoCodec.fps) : diagnosticsNotAvailable());
    setValue(QStringLiteral("video.bitrate"),     s.videoCodec.bitrate > 0 ? tr("%1 kbps").arg(s.videoCodec.bitrate / 1000) : diagnosticsNotAvailable());

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
    const QString suggested = QDir(DiagnosticsBundleExporter::defaultDirectory())
                                   .filePath(DiagnosticsBundleExporter::defaultFileName());
    const QString destination = QFileDialog::getSaveFileName(
        this, tr("Generate Diagnostics Bundle"), suggested, tr("ZIP Archive (*.zip)"));
    if (destination.isEmpty())
        return;

    const DiagnosticsBundleExporter::Result result =
        DiagnosticsBundleExporter::generateBundle(DiagnosticsCollector::instance().snapshot(), destination);

    if (!result.success) {
        m_bundleStatus->setText(tr("<span style='color:#c0392b'>Failed</span>"));
        m_bundleOpenFolderBtn->setEnabled(false);
        QMessageBox::warning(this, tr("Diagnostics Bundle"),
                              tr("Failed to generate diagnostics bundle.\n%1").arg(result.error));
        return;
    }

    m_lastBundlePath = result.path;
    m_bundleOpenFolderBtn->setEnabled(true);
    m_bundleStatus->setText(tr("<span style='color:#27ae60'>Success</span>: %1").arg(result.path));

    if (result.isZip) {
        QMessageBox::information(this, tr("Diagnostics Bundle"),
                                  tr("Diagnostics bundle written to:\n%1").arg(result.path));
    } else {
        QMessageBox::information(this, tr("Diagnostics Bundle"),
                                  tr("Real ZIP export is unavailable in this build, so the bundle was "
                                     "written as a folder instead:\n%1\n\n%2")
                                      .arg(result.path, result.error));
    }
}

void DiagnosticsCenterPanel::onOpenBundleFolder()
{
    if (m_lastBundlePath.isEmpty())
        return;
    const QFileInfo info(m_lastBundlePath);
    const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

bool DiagnosticsCenterPanel::timelineEntryMatchesFilters(const DiagnosticsTimelineEntry &e) const
{
    const CategoryFilter filter = categoryFilterForIndex(m_timelineFilter->currentIndex());
    switch (filter) {
    case CategoryFilter::All:                                                              break;
    case CategoryFilter::Registration: if (e.category != TimelineCategory::Registration) return false; break;
    case CategoryFilter::Call:         if (e.category != TimelineCategory::Call)         return false; break;
    case CategoryFilter::Sip:          if (e.category != TimelineCategory::Sip)          return false; break;
    case CategoryFilter::Media:        if (e.category != TimelineCategory::Media)        return false; break;
    case CategoryFilter::Rtp:          if (e.category != TimelineCategory::Rtp)          return false; break;
    case CategoryFilter::Camera:       if (e.category != TimelineCategory::Camera)       return false; break;
    case CategoryFilter::Audio:        if (e.category != TimelineCategory::Audio)        return false; break;
    case CategoryFilter::Video:        if (e.category != TimelineCategory::Video)        return false; break;
    case CategoryFilter::Rtt:          if (e.category != TimelineCategory::Rtt)          return false; break;
    case CategoryFilter::Warnings:     if (e.severity != TimelineSeverity::Warning)      return false; break;
    case CategoryFilter::Errors:       if (e.severity != TimelineSeverity::Error)        return false; break;
    }

    const QString search = m_timelineSearch->text().trimmed();
    if (!search.isEmpty()) {
        const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
            .arg(e.title, e.details, e.remoteUri,
                 e.sipCode > 0 ? QString::number(e.sipCode) : QString(),
                 timelineCategoryName(e.category));
        if (!haystack.contains(search, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

void DiagnosticsCenterPanel::appendTimelineRow(const DiagnosticsTimelineEntry &e)
{
    const int row = m_timelineTable->rowCount();
    m_timelineTable->insertRow(row);

    auto *tTime  = new QTableWidgetItem(e.timestamp.toString(QStringLiteral("hh:mm:ss")));
    auto *tCat   = new QTableWidgetItem(QStringLiteral("%1 %2").arg(e.iconHint, timelineCategoryName(e.category)));
    auto *tSev   = new QTableWidgetItem(timelineSeverityName(e.severity));
    const QString titleDetails = e.details.isEmpty() ? e.title : QStringLiteral("%1 — %2").arg(e.title, e.details);
    auto *tTitle = new QTableWidgetItem(titleDetails);

    const QColor color(e.colorHint);
    tSev->setForeground(color);
    tTitle->setForeground(color);

    // Full entry kept on the row for the context menu (Copy line / details / JSON).
    tTime->setData(Qt::UserRole, QVariant::fromValue(e));

    m_timelineTable->setItem(row, 0, tTime);
    m_timelineTable->setItem(row, 1, tCat);
    m_timelineTable->setItem(row, 2, tSev);
    m_timelineTable->setItem(row, 3, tTitle);
}

void DiagnosticsCenterPanel::refreshTimelineTable()
{
    const int previousScroll = m_timelineTable->verticalScrollBar()->value();
    const int previousMax    = m_timelineTable->verticalScrollBar()->maximum();

    m_timelineTable->setRowCount(0);
    for (const auto &e : DiagnosticsTimelineService::instance().model()->entries()) {
        if (timelineEntryMatchesFilters(e))
            appendTimelineRow(e);
    }

    if (m_timelineAutoScroll->isChecked() && previousScroll >= previousMax - 2)
        m_timelineTable->scrollToBottom();
    else
        m_timelineTable->verticalScrollBar()->setValue(previousScroll);
}

void DiagnosticsCenterPanel::refreshRecentActivity(const DiagnosticsTimelineEntry &e)
{
    if (!m_recentActivity)
        return;

    constexpr int kMaxRecent = 5;
    auto *item = new QListWidgetItem(
        QStringLiteral("[%1] %2 — %3").arg(e.timestamp.toString(QStringLiteral("hh:mm:ss")), timelineCategoryName(e.category), e.title));
    item->setForeground(QColor(e.colorHint));
    m_recentActivity->insertItem(0, item);

    while (m_recentActivity->count() > kMaxRecent)
        delete m_recentActivity->takeItem(m_recentActivity->count() - 1);
}

void DiagnosticsCenterPanel::onTimelineEntryAppended(const DiagnosticsTimelineEntry &entry)
{
    refreshRecentActivity(entry);

    if (!timelineEntryMatchesFilters(entry))
        return;

    appendTimelineRow(entry);
    if (m_timelineAutoScroll->isChecked())
        m_timelineTable->scrollToBottom();
}

void DiagnosticsCenterPanel::onTimelineFilterChanged()
{
    refreshTimelineTable();
}

void DiagnosticsCenterPanel::onTimelineContextMenu(const QPoint &pos)
{
    const QTableWidgetItem *timeItem = m_timelineTable->item(m_timelineTable->currentRow(), 0);
    if (!timeItem)
        return;
    const DiagnosticsTimelineEntry e = timeItem->data(Qt::UserRole).value<DiagnosticsTimelineEntry>();

    QMenu menu(this);
    QAction *copyLine    = menu.addAction(tr("Copy line"));
    QAction *copyDetails = menu.addAction(tr("Copy details"));
    QAction *copyJson    = menu.addAction(tr("Copy JSON"));

    QAction *chosen = menu.exec(m_timelineTable->viewport()->mapToGlobal(pos));
    if (chosen == copyLine) {
        QApplication::clipboard()->setText(
            QStringLiteral("[%1] [%2] [%3] %4").arg(e.timestamp.toString(Qt::ISODateWithMs),
                timelineCategoryName(e.category), timelineSeverityName(e.severity), e.title));
    } else if (chosen == copyDetails) {
        QApplication::clipboard()->setText(e.details);
    } else if (chosen == copyJson) {
        QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(e.toJson()).toJson(QJsonDocument::Indented)));
    }
}

void DiagnosticsCenterPanel::onExportTimelineJson()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Timeline JSON"),
                                                        QStringLiteral("timeline.json"), tr("JSON files (*.json)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!DiagnosticsTimelineExporter::exportJsonToFile(DiagnosticsTimelineService::instance().model()->entries(), path, &error))
        QMessageBox::warning(this, tr("Export Timeline"), tr("Failed to export timeline.\n%1").arg(error));
}

void DiagnosticsCenterPanel::onExportTimelineTxt()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Timeline TXT"),
                                                        QStringLiteral("timeline.txt"), tr("Text files (*.txt)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!DiagnosticsTimelineExporter::exportTxtToFile(DiagnosticsTimelineService::instance().model()->entries(), path, &error))
        QMessageBox::warning(this, tr("Export Timeline"), tr("Failed to export timeline.\n%1").arg(error));
}
