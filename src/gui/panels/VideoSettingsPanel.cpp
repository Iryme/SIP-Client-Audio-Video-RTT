#include "VideoSettingsPanel.h"
#include "VideoPanel.h"

#include "core/Logger.h"
#include "gui/CameraController.h"
#include "media/MediaDeviceManager.h"
#include "media/VideoQualityManager.h"
#include "media/VideoStatistics.h"
#include "sip/CodecManager.h"
#include "sip/SipManager.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QShowEvent>
#include <QHideEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QVBoxLayout>

const QList<int> VideoSettingsPanel::kBitrateSteps{
    256, 512, 768, 1024, 2048, 3072, 5120, 10240
};

static QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet(
        "font-size: 9px; font-weight: 700; color: #3a6090; letter-spacing: 2px; "
        "padding-top: 8px;");
    return lbl;
}

VideoSettingsPanel::VideoSettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoSettingsPanel: constructing"));

    buildUi();

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoSettingsPanel: buildUi done, wiring signals"));

    connect(&VideoQualityManager::instance(), &VideoQualityManager::settingsChanged,
            this, &VideoSettingsPanel::onSettingsChanged);
    connect(&MediaDeviceManager::instance(), &MediaDeviceManager::devicesChanged,
            this, [this]() { populateCameras(); });

    populateCameras();
    loadFrom(VideoQualityManager::instance().current());

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoSettingsPanel: constructed OK"));
}

void VideoSettingsPanel::buildUi()
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(6);

    // ── Left: controls ─────────────────────────────────────────────────────
    auto *leftWidget = new QWidget(split);
    leftWidget->setMinimumWidth(300);
    leftWidget->setMaximumWidth(380);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(16, 12, 12, 12);
    leftLayout->setSpacing(4);

    // Camera
    leftLayout->addWidget(sectionLabel(tr("CAMERA"), leftWidget));
    m_cameraCombo = new QComboBox(leftWidget);
    leftLayout->addWidget(m_cameraCombo);

    // Resolution
    leftLayout->addWidget(sectionLabel(tr("RESOLUTION"), leftWidget));
    m_resolutionCombo = new QComboBox(leftWidget);
    leftLayout->addWidget(m_resolutionCombo);

    // FPS
    leftLayout->addWidget(sectionLabel(tr("FRAME RATE"), leftWidget));
    m_fpsCombo = new QComboBox(leftWidget);
    leftLayout->addWidget(m_fpsCombo);

    // Bitrate
    leftLayout->addWidget(sectionLabel(tr("BITRATE"), leftWidget));
    m_bitrateSlider = new QSlider(Qt::Horizontal, leftWidget);
    m_bitrateSlider->setRange(0, kBitrateSteps.size() - 1);
    m_bitrateSlider->setSingleStep(1);
    m_bitrateLabel = new QLabel(leftWidget);
    m_bitrateLabel->setMinimumWidth(80);
    auto *bitrateRow = new QHBoxLayout();
    bitrateRow->setSpacing(8);
    bitrateRow->addWidget(m_bitrateSlider, 1);
    bitrateRow->addWidget(m_bitrateLabel);
    leftLayout->addLayout(bitrateRow);

    // Codec priority
    leftLayout->addWidget(sectionLabel(tr("CODEC PRIORITY"), leftWidget));
    m_codecList = new QListWidget(leftWidget);
    m_codecList->setMaximumHeight(120);
    m_codecList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_codecList->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_codecUp   = new QPushButton(tr("↑"), leftWidget);
    m_codecDown = new QPushButton(tr("↓"), leftWidget);
    m_codecUp->setFixedWidth(28);
    m_codecDown->setFixedWidth(28);
    auto *codecBtns = new QVBoxLayout();
    codecBtns->addWidget(m_codecUp);
    codecBtns->addWidget(m_codecDown);
    codecBtns->addStretch(1);
    auto *codecRow = new QHBoxLayout();
    codecRow->setSpacing(4);
    codecRow->addWidget(m_codecList, 1);
    codecRow->addLayout(codecBtns);
    leftLayout->addLayout(codecRow);

    // Overlay
    m_overlayCheck = new QCheckBox(tr("Show debug overlay"), leftWidget);
    leftLayout->addWidget(m_overlayCheck);

    m_cameraToggle = new QPushButton(tr("Camera Off"), leftWidget);
    m_cameraToggle->setCheckable(true);
    m_cameraToggle->setChecked(false);
    m_cameraToggle->setFixedHeight(28);
    leftLayout->addWidget(m_cameraToggle);

    // Video TX warning — shown when PJSIP has no capture backend in this build.
    m_videoTxWarning = new QLabel(
        tr("⚠ Local preview uses Qt camera. "
           "PJSIP video transmit is unavailable in this build "
           "(PJMEDIA_VIDEO_DEV_HAS_DSHOW=0). "
           "Remote cannot see video from this device."),
        leftWidget);
    m_videoTxWarning->setWordWrap(true);
    m_videoTxWarning->setStyleSheet("color: #e0b850; font-size: 10px; padding: 4px 0;");
    m_videoTxWarning->setVisible(!SipManager::instance().hasPjsipVideoCapture());
    leftLayout->addWidget(m_videoTxWarning);

    // Buttons
    m_applyBtn = new QPushButton(tr("Apply"), leftWidget);
    m_applyBtn->setDefault(true);
    m_resetBtn = new QPushButton(tr("Reset"), leftWidget);
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    btnRow->addWidget(m_resetBtn);
    btnRow->addWidget(m_applyBtn);
    leftLayout->addLayout(btnRow);

    // Stats
    m_statsLabel = new QLabel(tr("—"), leftWidget);
    m_statsLabel->setStyleSheet("font-size: 10px; color: #6a7a9a;");
    leftLayout->addWidget(m_statsLabel);

    leftLayout->addStretch(1);
    split->addWidget(leftWidget);

    // ── Right: preview ─────────────────────────────────────────────────────
    m_preview = new VideoPanel(split, false);
    m_preview->setMinimumSize(320, 240);
    split->addWidget(m_preview);

    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);

    root->addWidget(split);

    // Connections
    connect(m_cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VideoSettingsPanel::onCameraChanged);
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        const QSize res = m_resolutionCombo->currentData().value<QSize>();
        const QString camId = m_cameraCombo->currentData().toString();
        populateFps(camId, res);
    });
    connect(m_bitrateSlider, &QSlider::valueChanged,
            this, &VideoSettingsPanel::onBitrateChanged);
    connect(m_codecUp,   &QPushButton::clicked, this, &VideoSettingsPanel::onCodecUp);
    connect(m_codecDown, &QPushButton::clicked, this, &VideoSettingsPanel::onCodecDown);
    connect(m_cameraToggle, &QPushButton::toggled,
            this, &VideoSettingsPanel::onCameraToggled);
    // Sync m_cameraToggle when CameraController changes from outside (Clients page, VideoPanel).
    connect(&CameraController::instance(), &CameraController::enabledChanged,
            this, [this](bool enabled) {
        if (m_cameraToggle) {
            QSignalBlocker b(m_cameraToggle);
            m_cameraToggle->setChecked(enabled);
            m_cameraToggle->setText(enabled ? tr("Camera On") : tr("Camera Off"));
        }
    });
    connect(m_applyBtn,  &QPushButton::clicked, this, &VideoSettingsPanel::onApply);
    connect(m_resetBtn,  &QPushButton::clicked, this, &VideoSettingsPanel::onReset);
}

// ---------------------------------------------------------------------------
// Population helpers
// ---------------------------------------------------------------------------

void VideoSettingsPanel::populateCameras()
{
    const QSignalBlocker blocker(m_cameraCombo);
    const QString curId = m_cameraCombo->currentData().toString();
    m_cameraCombo->clear();

    for (const MediaDevice &d : MediaDeviceManager::instance().listCameras())
        m_cameraCombo->addItem(d.displayName, d.id);

    // Restore selection
    for (int i = 0; i < m_cameraCombo->count(); ++i) {
        if (m_cameraCombo->itemData(i).toString() == curId) {
            m_cameraCombo->setCurrentIndex(i);
            return;
        }
    }
}

void VideoSettingsPanel::populateResolutions(const QString &cameraId)
{
    const QSignalBlocker blocker(m_resolutionCombo);
    const QSize curRes = m_resolutionCombo->currentData().value<QSize>();
    m_resolutionCombo->clear();

    for (const QSize &res : VideoQualityManager::resolutionsFor(cameraId)) {
        m_resolutionCombo->addItem(
            QStringLiteral("%1 x %2").arg(res.width()).arg(res.height()),
            QVariant::fromValue(res));
    }

    for (int i = 0; i < m_resolutionCombo->count(); ++i) {
        if (m_resolutionCombo->itemData(i).value<QSize>() == curRes) {
            m_resolutionCombo->setCurrentIndex(i);
            return;
        }
    }
    // Default to 1280x720 if available
    for (int i = 0; i < m_resolutionCombo->count(); ++i) {
        if (m_resolutionCombo->itemData(i).value<QSize>() == QSize(1280, 720)) {
            m_resolutionCombo->setCurrentIndex(i);
            return;
        }
    }
}

void VideoSettingsPanel::populateFps(const QString &cameraId, const QSize &resolution)
{
    const QSignalBlocker blocker(m_fpsCombo);
    const int curFps = m_fpsCombo->currentData().toInt();
    m_fpsCombo->clear();

    for (int fps : VideoQualityManager::fpsValuesFor(cameraId, resolution))
        m_fpsCombo->addItem(QStringLiteral("%1 fps").arg(fps), fps);

    for (int i = 0; i < m_fpsCombo->count(); ++i) {
        if (m_fpsCombo->itemData(i).toInt() == curFps) {
            m_fpsCombo->setCurrentIndex(i);
            return;
        }
    }
    // Default: prefer 30, then last
    for (int i = 0; i < m_fpsCombo->count(); ++i) {
        if (m_fpsCombo->itemData(i).toInt() == 30) {
            m_fpsCombo->setCurrentIndex(i);
            return;
        }
    }
    if (m_fpsCombo->count() > 0)
        m_fpsCombo->setCurrentIndex(m_fpsCombo->count() - 1);
}

void VideoSettingsPanel::populateCodecs(const QStringList &order)
{
    // knownCodecs() is a fixed reference list (H264/VP8/VP9/AV1/H265) used so
    // a user's saved preference order survives switching to a PJSIP build
    // that supports more or fewer codecs than the one that saved it. Not
    // every name in it is necessarily compiled into *this* PJSIP build (e.g.
    // this project currently links libvpx only — VP8 — with no H264/OpenH264
    // codec available), so reordering an unavailable entry to the top has no
    // effect on real negotiation (CodecManager::applyVideoCodecOrder() simply
    // can't find it in videoCodecEnum2() and falls through). Mark those
    // entries so the user isn't left guessing why their preferred codec never
    // takes effect.
    const QList<CodecEntry> real = CodecManager::instance().videoCodecs();
    auto isAvailable = [&real](const QString &name) {
        for (const auto &c : real) {
            if (c.codecId.startsWith(name, Qt::CaseInsensitive))
                return true;
        }
        return false;
    };

    m_codecList->clear();
    auto addCodecItem = [&](const QString &name) {
        const bool available = isAvailable(name);
        auto *item = new QListWidgetItem(available
            ? name
            : tr("%1 (not available in this build)").arg(name));
        // The saved codecOrder must always be the plain codec name, never the
        // annotated display text -- collectInto() reads this back.
        item->setData(Qt::UserRole, name);
        if (!available) {
            item->setForeground(QBrush(QColor(150, 150, 150)));
            item->setToolTip(tr("This PJSIP build does not have a %1 codec compiled in — "
                                 "reordering it has no effect on the actual call.").arg(name));
        }
        m_codecList->addItem(item);
    };

    // Ordered codecs first
    for (const QString &c : order)
        addCodecItem(c);
    // Then known codecs not already in the list
    for (const QString &c : VideoQualityManager::knownCodecs()) {
        if (!order.contains(c, Qt::CaseInsensitive))
            addCodecItem(c);
    }
    if (m_codecList->count() > 0)
        m_codecList->setCurrentRow(0);
}

// ---------------------------------------------------------------------------
// Load / collect
// ---------------------------------------------------------------------------

void VideoSettingsPanel::loadFrom(const VideoSettings &s)
{
    // Camera
    {
        const QSignalBlocker b(m_cameraCombo);
        for (int i = 0; i < m_cameraCombo->count(); ++i) {
            if (m_cameraCombo->itemData(i).toString() == s.cameraId) {
                m_cameraCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    populateResolutions(s.cameraId);
    {
        const QSignalBlocker b(m_resolutionCombo);
        for (int i = 0; i < m_resolutionCombo->count(); ++i) {
            if (m_resolutionCombo->itemData(i).value<QSize>() == s.resolution) {
                m_resolutionCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    const QSize res = m_resolutionCombo->currentData().value<QSize>();
    populateFps(s.cameraId, res);
    {
        const QSignalBlocker b(m_fpsCombo);
        for (int i = 0; i < m_fpsCombo->count(); ++i) {
            if (m_fpsCombo->itemData(i).toInt() == s.fps) {
                m_fpsCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // Bitrate
    {
        const QSignalBlocker b(m_bitrateSlider);
        int sliderIdx = kBitrateSteps.size() - 1;
        for (int i = 0; i < kBitrateSteps.size(); ++i) {
            if (kBitrateSteps[i] >= s.bitrateKbps) { sliderIdx = i; break; }
        }
        m_bitrateSlider->setValue(sliderIdx);
        m_bitrateLabel->setText(formatBitrate(kBitrateSteps[sliderIdx]));
    }

    populateCodecs(s.codecOrder);
    m_overlayCheck->setChecked(s.overlayEnabled);
}

VideoSettings VideoSettingsPanel::collectSettings() const
{
    VideoSettings s;
    s.cameraId       = m_cameraCombo->currentData().toString();
    s.resolution     = m_resolutionCombo->currentData().value<QSize>();
    s.fps            = m_fpsCombo->currentData().toInt();
    const int idx    = m_bitrateSlider->value();
    s.bitrateKbps    = (idx >= 0 && idx < kBitrateSteps.size())
                         ? kBitrateSteps[idx] : 1024;
    s.overlayEnabled = m_overlayCheck->isChecked();

    QStringList codecs;
    for (int i = 0; i < m_codecList->count(); ++i)
        codecs.append(m_codecList->item(i)->data(Qt::UserRole).toString());
    s.codecOrder = codecs;

    return s;
}

QString VideoSettingsPanel::formatBitrate(int kbps)
{
    if (kbps < 1000)
        return QStringLiteral("%1 kbps").arg(kbps);
    return QStringLiteral("%1 Mbps").arg(static_cast<double>(kbps) / 1024.0, 0, 'f', 1);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void VideoSettingsPanel::onCameraChanged(int idx)
{
    const QString camId = m_cameraCombo->itemData(idx).toString();
    populateResolutions(camId);
    const QSize res = m_resolutionCombo->currentData().value<QSize>();
    populateFps(camId, res);
    if (m_cameraToggle && m_cameraToggle->isChecked() && m_preview)
        m_preview->refreshIdlePreview();
}

void VideoSettingsPanel::onCameraToggled(bool on)
{
    if (m_cameraToggle)
        m_cameraToggle->setText(on ? tr("Camera On") : tr("Camera Off"));
    // Delegate to CameraController so both Settings and Clients stay in sync
    // and the hardware LED turns off when disabled from either location.
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Camera %1 requested from Settings")
            .arg(on ? QStringLiteral("On") : QStringLiteral("Off")));
    CameraController::instance().setEnabled(on, QStringLiteral("Settings"));
    if (on && m_preview && m_preview->isVisible())
        m_preview->startIdlePreview();
}

void VideoSettingsPanel::onBitrateChanged(int value)
{
    if (value >= 0 && value < kBitrateSteps.size())
        m_bitrateLabel->setText(formatBitrate(kBitrateSteps[value]));
}

void VideoSettingsPanel::onCodecUp()
{
    const int row = m_codecList->currentRow();
    if (row <= 0) return;
    auto *item = m_codecList->takeItem(row);
    m_codecList->insertItem(row - 1, item);
    m_codecList->setCurrentRow(row - 1);
}

void VideoSettingsPanel::onCodecDown()
{
    const int row = m_codecList->currentRow();
    if (row < 0 || row >= m_codecList->count() - 1) return;
    auto *item = m_codecList->takeItem(row);
    m_codecList->insertItem(row + 1, item);
    m_codecList->setCurrentRow(row + 1);
}

void VideoSettingsPanel::onApply()
{
    VideoQualityManager::instance().apply(collectSettings());

    const CallState cs = SipManager::instance().callState();
    const bool callActive = (cs != CallState::Idle && cs != CallState::Failed);

    if (callActive) {
        m_statsLabel->setText(tr("Settings saved — will apply on next call"));
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoSettings changed during active call — "
                           "will apply on next call start"));
    } else {
        if (m_preview && m_preview->isVisible() && m_cameraToggle && m_cameraToggle->isChecked())
            m_preview->refreshIdlePreview();
    }
}

void VideoSettingsPanel::onReset()
{
    loadFrom(VideoQualityManager::instance().current());
    if (m_cameraToggle)
        m_cameraToggle->setChecked(false);
}

void VideoSettingsPanel::onStatsUpdated(float fps, int drops)
{
    m_statsLabel->setText(
        tr("FPS: %1  |  Dropped: %2").arg(fps, 0, 'f', 1).arg(drops));
}

void VideoSettingsPanel::onSettingsChanged(const VideoSettings &s)
{
    loadFrom(s);
}

// ---------------------------------------------------------------------------
// Show / hide
// ---------------------------------------------------------------------------

void VideoSettingsPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoSettingsPanel: showEvent"));

    if (!m_statsConnected) {
        connect(&VideoStatistics::instance(), &VideoStatistics::statsUpdated,
                this, &VideoSettingsPanel::onStatsUpdated);
        m_statsConnected = true;
    }

    // Read global camera state and sync UI without triggering setEnabled.
    const bool camEnabled = CameraController::instance().isEnabled();
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoSettingsPanel: camera enabled=%1").arg(camEnabled));

    if (m_cameraToggle) {
        QSignalBlocker b(m_cameraToggle);
        m_cameraToggle->setChecked(camEnabled);
        m_cameraToggle->setText(camEnabled ? tr("Camera On") : tr("Camera Off"));
    }
    if (camEnabled && m_preview) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoSettingsPanel: starting idle preview"));
        m_preview->startIdlePreview();
    }

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoSettingsPanel: showEvent done"));
}

void VideoSettingsPanel::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (m_preview)
        m_preview->stopIdlePreview();
}
