#include "MediaPanel.h"
#include "media/AudioMediaManager.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoMediaManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QFrame>

MediaPanel::MediaPanel(QWidget *parent)
    : QWidget(parent)
{
    m_manager        = &MediaDeviceManager::instance();
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
                VideoMediaManager::instance().setCamera(dev.id);
            });

    // Forward device selections to AudioMediaManager so SipManager can wire them
    // to the PJSIP AudDevManager.
    connect(m_selectionModel, &MediaDeviceSelectionModel::microphoneSelectionChanged,
            this, [](const MediaDevice &dev) {
                AudioMediaManager::instance().setMicrophone(dev.id);
            });
    connect(m_selectionModel, &MediaDeviceSelectionModel::speakerSelectionChanged,
            this, [](const MediaDevice &dev) {
                AudioMediaManager::instance().setSpeaker(dev.id);
            });
}

void MediaPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // ---- Audio group ----
    auto *audioGroup = new QGroupBox(tr("Audio Devices"), this);
    auto *audioGrid  = new QGridLayout(audioGroup);
    audioGrid->setColumnStretch(1, 1);

    audioGrid->addWidget(new QLabel(tr("Microphone:"), this), 0, 0);
    m_micCombo = new QComboBox(this);
    m_micCombo->setObjectName("MicrophoneCombo");
    audioGrid->addWidget(m_micCombo, 0, 1);

    m_micLevel = new QLabel(this);
    m_micLevel->setObjectName("MicLevelMeter");
    m_micLevel->setFixedHeight(8);
    m_micLevel->setStyleSheet("background: #333; border-radius: 3px;");
    m_micLevel->setToolTip(tr("Microphone level (inactive — no capture yet)"));
    audioGrid->addWidget(m_micLevel, 1, 1);

    audioGrid->addWidget(new QLabel(tr("Speaker:"), this), 2, 0);
    m_speakerCombo = new QComboBox(this);
    m_speakerCombo->setObjectName("SpeakerCombo");
    audioGrid->addWidget(m_speakerCombo, 2, 1);

    m_speakerLevel = new QLabel(this);
    m_speakerLevel->setObjectName("SpeakerLevelMeter");
    m_speakerLevel->setFixedHeight(8);
    m_speakerLevel->setStyleSheet("background: #333; border-radius: 3px;");
    m_speakerLevel->setToolTip(tr("Speaker level (inactive — no playback yet)"));
    audioGrid->addWidget(m_speakerLevel, 3, 1);

    root->addWidget(audioGroup);

    // ---- Video group ----
    auto *videoGroup = new QGroupBox(tr("Video Device"), this);
    auto *videoGrid  = new QGridLayout(videoGroup);
    videoGrid->setColumnStretch(1, 1);

    videoGrid->addWidget(new QLabel(tr("Camera:"), this), 0, 0);
    m_cameraCombo = new QComboBox(this);
    m_cameraCombo->setObjectName("CameraCombo");
    videoGrid->addWidget(m_cameraCombo, 0, 1);

    root->addWidget(videoGroup);

    // ---- Refresh button ----
    m_refreshBtn = new QPushButton(tr("Refresh Devices"), this);
    m_refreshBtn->setObjectName("RefreshDevicesBtn");
    m_refreshBtn->setFixedHeight(28);
    root->addWidget(m_refreshBtn, 0, Qt::AlignLeft);

    root->addStretch(1);

    // Wire combo signals after widgets are created
    connect(m_micCombo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaPanel::onMicrophoneChanged);
    connect(m_speakerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaPanel::onSpeakerChanged);
    connect(m_cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaPanel::onCameraChanged);
    connect(m_refreshBtn,  &QPushButton::clicked,
            this, &MediaPanel::onRefreshClicked);
}

void MediaPanel::populateCombo(QComboBox *combo,
                                const QList<MediaDevice> &devices,
                                const QString &selectedId,
                                bool showDefault)
{
    const bool blocked = combo->blockSignals(true);
    combo->clear();

    int selectIdx = 0;

    if (showDefault) {
        combo->addItem(tr("Default (system)"), QString{});
        if (selectedId.isEmpty())
            selectIdx = 0;
    }

    if (devices.isEmpty() && !showDefault) {
        combo->addItem(tr("(no device available)"), QString{});
        combo->setEnabled(false);
        combo->blockSignals(blocked);
        return;
    }

    combo->setEnabled(true);
    for (int i = 0; i < devices.size(); ++i) {
        const MediaDevice &d = devices[i];
        const QString label  = d.isDefault
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
    populateCombo(m_micCombo,     m_manager->listMicrophones(),
                  m_selectionModel->selectedMicrophone().id, /*showDefault=*/true);
    populateCombo(m_speakerCombo, m_manager->listSpeakers(),
                  m_selectionModel->selectedSpeaker().id,    /*showDefault=*/true);
    populateCombo(m_cameraCombo,  m_manager->listCameras(),
                  m_selectionModel->selectedCamera().id);
}

// ---------------------------------------------------------------------------
// User-initiated selection
// ---------------------------------------------------------------------------
void MediaPanel::onMicrophoneChanged(int index)
{
    const QString id = m_micCombo->itemData(index).toString();
    m_selectionModel->selectMicrophone(id);
}

void MediaPanel::onSpeakerChanged(int index)
{
    const QString id = m_speakerCombo->itemData(index).toString();
    m_selectionModel->selectSpeaker(id);
}

void MediaPanel::onCameraChanged(int index)
{
    const QString id = m_cameraCombo->itemData(index).toString();
    m_selectionModel->selectCamera(id);
}

void MediaPanel::onRefreshClicked()
{
    m_manager->refreshDevices();
    // selectionModel refresh() is triggered by devicesChanged signal;
    // populateCombo is driven by the selection signals that follow.
}

// ---------------------------------------------------------------------------
// Selection-model notifications → keep combos in sync
// ---------------------------------------------------------------------------
void MediaPanel::onMicrophoneSelectionChanged(const MediaDevice &device)
{
    populateCombo(m_micCombo, m_manager->listMicrophones(), device.id, /*showDefault=*/true);
}

void MediaPanel::onSpeakerSelectionChanged(const MediaDevice &device)
{
    populateCombo(m_speakerCombo, m_manager->listSpeakers(), device.id, /*showDefault=*/true);
}

void MediaPanel::onCameraSelectionChanged(const MediaDevice &device)
{
    populateCombo(m_cameraCombo, m_manager->listCameras(), device.id);
}
