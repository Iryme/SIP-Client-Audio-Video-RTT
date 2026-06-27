#include "MediaPanel.h"

#include "core/Logger.h"
#include "media/AudioMediaManager.h"
#include "media/MediaDevice.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoMediaManager.h"
#include "gui/panels/VideoPanel.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
static QWidget *makeZone(QWidget *parent, const QString &title, const QString &subtitle = QString{})
{
    auto *zone = new QFrame(parent);
    zone->setObjectName("MediaZone");
    zone->setFrameShape(QFrame::StyledPanel);
    auto *layout = new QVBoxLayout(zone);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(title, zone);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: 600;");
    layout->addWidget(titleLabel);

    if (!subtitle.isEmpty()) {
        auto *subtitleLabel = new QLabel(subtitle, zone);
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setStyleSheet("color: #9aa8b8; font-size: 11px;");
        layout->addWidget(subtitleLabel);
    }

    return zone;
}
}

MediaPanel::MediaPanel(QWidget *parent)
    : QWidget(parent)
{
    m_manager = &MediaDeviceManager::instance();
    m_selectionModel = new MediaDeviceSelectionModel(m_manager, this);

    buildUi();
    refreshAll();

    connect(m_selectionModel, &MediaDeviceSelectionModel::microphoneSelectionChanged,
            this, &MediaPanel::onMicrophoneSelectionChanged);
    connect(m_selectionModel, &MediaDeviceSelectionModel::speakerSelectionChanged,
            this, &MediaPanel::onSpeakerSelectionChanged);
    connect(m_selectionModel, &MediaDeviceSelectionModel::cameraSelectionChanged,
            this, &MediaPanel::onCameraSelectionChanged);
    connect(m_selectionModel, &MediaDeviceSelectionModel::cameraSelectionChanged,
            this, [](const MediaDevice &dev) {
                if (!dev.isNull())
                    VideoMediaManager::instance().setCamera(dev.id);
            });

    connect(m_selectionModel, &MediaDeviceSelectionModel::microphoneSelectionChanged,
            this, [](const MediaDevice &dev) {
                if (!dev.isNull())
                    AudioMediaManager::instance().setMicrophone(dev.id);
            });
    connect(m_selectionModel, &MediaDeviceSelectionModel::speakerSelectionChanged,
            this, [](const MediaDevice &dev) {
                if (!dev.isNull())
                    AudioMediaManager::instance().setSpeaker(dev.id);
            });

    connect(m_manager, &MediaDeviceManager::devicesChanged,
            this, &MediaPanel::onDevicesChanged);
    connect(&VideoMediaManager::instance(), &VideoMediaManager::cameraChanged,
            this, [this](const QString &) { updateStatuses(); });
    connect(&AudioMediaManager::instance(), &AudioMediaManager::inputLevelChanged,
            this, [this](int level) {
                if (m_micLevel) {
                    m_micLevel->setText(tr("Mic level: %1%").arg(level));
                    m_micLevel->setVisible(true);
                }
            });
    connect(&AudioMediaManager::instance(), &AudioMediaManager::outputLevelChanged,
            this, [this](int level) {
                if (m_speakerLevel) {
                    m_speakerLevel->setText(tr("Output level: %1%").arg(level));
                    m_speakerLevel->setVisible(true);
                }
            });
}

void MediaPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *title = new QLabel(tr("Media / Devices"), this);
    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    root->addWidget(title);

    auto *desc = new QLabel(
        tr("Three-zone device workspace for audio input, audio output, and video preview."),
        this);
    desc->setWordWrap(true);
    desc->setStyleSheet("color: #b7c4d6;");
    root->addWidget(desc);

    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);

    auto *inputZone = makeZone(split, tr("Audio Input"),
                               tr("Microphones available on this system."));
    auto *inputLayout = qobject_cast<QVBoxLayout *>(inputZone->layout());
    m_micCombo = new QComboBox(inputZone);
    m_micCombo->setObjectName("MicrophoneCombo");
    inputLayout->addWidget(new QLabel(tr("Microphone"), inputZone));
    inputLayout->addWidget(m_micCombo);
    m_inputStatus = new QLabel(inputZone);
    m_inputStatus->setWordWrap(true);
    m_inputStatus->setStyleSheet("color: #9aa8b8;");
    inputLayout->addWidget(m_inputStatus);
    m_micLevel = new QLabel(tr("Mic level: 0%"), inputZone);
    m_micLevel->setStyleSheet("color: #6fcf97;");
    inputLayout->addWidget(m_micLevel);
    m_refreshBtn = new QPushButton(tr("Refresh devices"), inputZone);
    inputLayout->addWidget(m_refreshBtn);
    inputLayout->addStretch();
    split->addWidget(inputZone);

    auto *outputZone = makeZone(split, tr("Audio Output"),
                                tr("Speakers and headsets."));
    auto *outputLayout = qobject_cast<QVBoxLayout *>(outputZone->layout());
    m_speakerCombo = new QComboBox(outputZone);
    m_speakerCombo->setObjectName("SpeakerCombo");
    outputLayout->addWidget(new QLabel(tr("Speaker / Headset"), outputZone));
    outputLayout->addWidget(m_speakerCombo);
    m_outputStatus = new QLabel(outputZone);
    m_outputStatus->setWordWrap(true);
    m_outputStatus->setStyleSheet("color: #9aa8b8;");
    outputLayout->addWidget(m_outputStatus);
    m_speakerLevel = new QLabel(tr("Output level: 0%"), outputZone);
    m_speakerLevel->setStyleSheet("color: #6fcf97;");
    outputLayout->addWidget(m_speakerLevel);
    m_testOutputBtn = new QPushButton(tr("Test output"), outputZone);
    m_testOutputBtn->setEnabled(false);
    m_testOutputBtn->setToolTip(tr("No output test infrastructure available yet"));
    outputLayout->addWidget(m_testOutputBtn);
    outputLayout->addStretch();
    split->addWidget(outputZone);

    auto *videoZone = makeZone(split, tr("Video Device"),
                               tr("Camera selection with explicit preview start."));
    auto *videoLayout = qobject_cast<QVBoxLayout *>(videoZone->layout());
    m_cameraCombo = new QComboBox(videoZone);
    m_cameraCombo->setObjectName("CameraCombo");
    videoLayout->addWidget(new QLabel(tr("Camera"), videoZone));
    videoLayout->addWidget(m_cameraCombo);
    m_videoStatus = new QLabel(videoZone);
    m_videoStatus->setWordWrap(true);
    m_videoStatus->setStyleSheet("color: #9aa8b8;");
    videoLayout->addWidget(m_videoStatus);
    m_startPreviewBtn = new QPushButton(tr("Start preview"), videoZone);
    videoLayout->addWidget(m_startPreviewBtn);
    m_videoFallback = new QLabel(tr("No video device available"), videoZone);
    m_videoFallback->setAlignment(Qt::AlignCenter);
    m_videoFallback->setWordWrap(true);
    m_videoFallback->setMinimumHeight(160);
    m_videoFallback->setStyleSheet(
        "color: #aab4c4; background: #0f141d; border: 1px dashed #34455d; padding: 12px;");
    m_previewPanel = new VideoPanel(videoZone, false);
    m_previewPanel->setMinimumHeight(240);
    videoLayout->addWidget(m_videoFallback);
    videoLayout->addWidget(m_previewPanel, 1);
    videoLayout->addStretch();
    split->addWidget(videoZone);

    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 1);
    split->setSizes(QList<int>{360, 360, 460});

    root->addWidget(split, 1);

    connect(m_micCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaPanel::onMicrophoneChanged);
    connect(m_speakerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaPanel::onSpeakerChanged);
    connect(m_cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaPanel::onCameraChanged);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &MediaPanel::onRefreshClicked);
    connect(m_startPreviewBtn, &QPushButton::clicked,
            this, &MediaPanel::onStartPreviewClicked);
}

void MediaPanel::populateCombo(QComboBox *combo,
                               const QList<MediaDevice> &devices,
                               const QString &selectedId,
                               bool showDefault)
{
    const bool blocked = combo->blockSignals(true);
    combo->clear();

    if (showDefault)
        combo->addItem(tr("Default (system)"), QString{});

    if (devices.isEmpty()) {
        combo->addItem(tr("(no device available)"), QString{});
        combo->setEnabled(false);
        combo->blockSignals(blocked);
        return;
    }

    combo->setEnabled(true);
    int selectIdx = 0;
    for (const MediaDevice &d : devices) {
        const QString label = d.isDefault
            ? QStringLiteral("%1 (default)").arg(d.displayName)
            : d.displayName;
        combo->addItem(label, d.id);
        if (!selectedId.isEmpty() && d.id == selectedId)
            selectIdx = combo->count() - 1;
    }
    combo->setCurrentIndex(selectIdx);
    combo->blockSignals(blocked);
}

void MediaPanel::refreshAll()
{
    populateCombo(m_micCombo, m_manager->listMicrophones(),
                  m_selectionModel->selectedMicrophone().id, true);
    populateCombo(m_speakerCombo, m_manager->listSpeakers(),
                  m_selectionModel->selectedSpeaker().id, true);
    populateCombo(m_cameraCombo, m_manager->listCameras(),
                  m_selectionModel->selectedCamera().id, false);
    updateStatuses();
}

void MediaPanel::updateStatuses()
{
    const auto mics = m_manager->listMicrophones();
    const auto speakers = m_manager->listSpeakers();
    const auto cams = m_manager->listCameras();

    const MediaDevice mic = m_selectionModel->selectedMicrophone();
    const MediaDevice speaker = m_selectionModel->selectedSpeaker();
    const MediaDevice cam = m_selectionModel->selectedCamera();

    if (m_inputStatus) {
        m_inputStatus->setText(mics.isEmpty()
            ? tr("No audio input device available")
            : tr("Current: %1").arg(mic.isNull() ? tr("Default (system)") : mic.displayName));
    }
    if (m_outputStatus) {
        m_outputStatus->setText(speakers.isEmpty()
            ? tr("No audio output device available")
            : tr("Current: %1").arg(speaker.isNull() ? tr("Default (system)") : speaker.displayName));
    }
    if (m_videoStatus) {
        m_videoStatus->setText(cams.isEmpty()
            ? tr("No video device available")
            : tr("Current: %1").arg(cam.isNull() ? tr("No camera selected") : cam.displayName));
    }
    if (m_videoFallback)
        m_videoFallback->setVisible(cams.isEmpty());
    if (m_previewPanel)
        m_previewPanel->setVisible(!cams.isEmpty());
    if (m_startPreviewBtn)
        m_startPreviewBtn->setEnabled(!cams.isEmpty());
}

void MediaPanel::onMicrophoneChanged(int index)
{
    const QString id = m_micCombo->itemData(index).toString();
    if (id.isEmpty() && m_manager->listMicrophones().isEmpty())
        return;
    m_selectionModel->selectMicrophone(id);
}

void MediaPanel::onSpeakerChanged(int index)
{
    const QString id = m_speakerCombo->itemData(index).toString();
    if (id.isEmpty() && m_manager->listSpeakers().isEmpty())
        return;
    m_selectionModel->selectSpeaker(id);
}

void MediaPanel::onCameraChanged(int index)
{
    const QString id = m_cameraCombo->itemData(index).toString();
    if (id.isEmpty() && m_manager->listCameras().isEmpty()) {
        updateStatuses();
        return;
    }
    m_selectionModel->selectCamera(id);
    updateStatuses();
}

void MediaPanel::onRefreshClicked()
{
    m_manager->refreshDevices();
}

void MediaPanel::onStartPreviewClicked()
{
    if (m_previewPanel)
        m_previewPanel->startIdlePreview();
}

void MediaPanel::onMicrophoneSelectionChanged(const MediaDevice &device)
{
    populateCombo(m_micCombo, m_manager->listMicrophones(), device.id, true);
    updateStatuses();
}

void MediaPanel::onSpeakerSelectionChanged(const MediaDevice &device)
{
    populateCombo(m_speakerCombo, m_manager->listSpeakers(), device.id, true);
    updateStatuses();
}

void MediaPanel::onCameraSelectionChanged(const MediaDevice &device)
{
    populateCombo(m_cameraCombo, m_manager->listCameras(), device.id, false);
    updateStatuses();
}

void MediaPanel::onDevicesChanged()
{
    refreshAll();
}
