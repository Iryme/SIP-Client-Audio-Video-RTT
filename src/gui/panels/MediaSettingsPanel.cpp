#include "MediaSettingsPanel.h"

#include <cmath>

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "media/AudioMediaManager.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "sip/PjsipAudioMapper.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr double kPi = 3.14159265358979323846;

// 440 Hz sine, 0.6 s, faded in/out to avoid clicks. Real Qt Multimedia
// playback (QAudioSink) on the currently selected speaker device — not a
// simulated/fake meter value.
QByteArray generateTestTone(const QAudioFormat &format)
{
    constexpr double durationSecs = 0.6;
    constexpr double freqHz       = 440.0;
    constexpr double fadeSecs     = 0.02;

    const int sampleRate = format.sampleRate();
    const int frameCount = static_cast<int>(sampleRate * durationSecs);

    QByteArray data;
    data.resize(frameCount * static_cast<int>(sizeof(qint16)));
    auto *samples = reinterpret_cast<qint16 *>(data.data());

    for (int i = 0; i < frameCount; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        double amp = 0.4;
        if (t < fadeSecs)
            amp *= t / fadeSecs;
        else if (t > durationSecs - fadeSecs)
            amp *= (durationSecs - t) / fadeSecs;
        samples[i] = static_cast<qint16>(amp * 32767.0 * std::sin(2.0 * kPi * freqHz * t));
    }
    return data;
}

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet(
        "font-size: 9px; font-weight: 700; color: #3a6090; letter-spacing: 2px; "
        "padding-top: 8px;");
    return lbl;
}

} // namespace

MediaSettingsPanel::MediaSettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();

    m_micTestTimer = new QTimer(this);
    m_micTestTimer->setSingleShot(true);
    connect(m_micTestTimer, &QTimer::timeout, this, &MediaSettingsPanel::stopMicTest);

    connect(&MediaDeviceManager::instance(), &MediaDeviceManager::devicesChanged,
            this, &MediaSettingsPanel::onDevicesChanged);
    connect(&MediaDeviceManager::instance(), &MediaDeviceManager::refreshFinished,
            this, [this]() {
        m_refreshBtn->setEnabled(true);
        m_refreshBtn->setText(tr("Refresh Devices"));
    });

    connect(&AudioMediaManager::instance(), &AudioMediaManager::inputLevelChanged,
            this, &MediaSettingsPanel::onInputLevelChanged);
    connect(&AudioMediaManager::instance(), &AudioMediaManager::outputLevelChanged,
            this, &MediaSettingsPanel::onOutputLevelChanged);
    connect(&AudioMediaManager::instance(), &AudioMediaManager::microphoneVolumeChanged,
            this, &MediaSettingsPanel::onMicrophoneVolumeChanged);
    connect(&AudioMediaManager::instance(), &AudioMediaManager::speakerVolumeChanged,
            this, &MediaSettingsPanel::onSpeakerVolumeChanged);
    // Keep this panel's combos/status labels in sync when device selection
    // changes elsewhere (e.g. CallPanel's own device combos).
    connect(&AudioMediaManager::instance(), &AudioMediaManager::audioDeviceSelectionChanged,
            this, &MediaSettingsPanel::onDeviceSelectionChanged);
    // The "actual active device" status line can only be queried from PJSIP
    // once a call's audio media is up, so refresh it on connect/disconnect.
    connect(&AudioMediaManager::instance(), &AudioMediaManager::mediaConnected,
            this, &MediaSettingsPanel::onMediaConnectionChanged);
    connect(&AudioMediaManager::instance(), &AudioMediaManager::mediaDisconnected,
            this, &MediaSettingsPanel::onMediaConnectionChanged);

    populateDevices();

    const int micVol = AudioMediaManager::instance().microphoneVolume();
    const int spkVol = AudioMediaManager::instance().speakerVolume();
    m_micVolumeSlider->setValue(micVol);
    m_spkVolumeSlider->setValue(spkVol);
    m_micVolumeLabel->setText(QStringLiteral("%1%").arg(micVol));
    m_spkVolumeLabel->setText(QStringLiteral("%1%").arg(spkVol));
}

MediaSettingsPanel::~MediaSettingsPanel()
{
    if (m_testSink) {
        m_testSink->stop();
        delete m_testSink;
    }
    delete m_testBuffer;
}

void MediaSettingsPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(4);

    // ── Header: title + Reset to Default ──────────────────────────────────
    {
        auto *headerRow = new QHBoxLayout();
        auto *title = new QLabel(tr("Microphone and speaker device, volume, and levels."), this);
        title->setStyleSheet("color: #b7c4d6;");
        m_resetBtn = new QPushButton(tr("Reset to Default"), this);
        m_resetBtn->setToolTip(tr("Reset microphone and speaker to the system default device "
                                   "and 100% volume. Applied immediately."));
        headerRow->addWidget(title, 1);
        headerRow->addWidget(m_resetBtn);
        root->addLayout(headerRow);
    }

    // ── Microphone ──────────────────────────────────────────────────────────
    root->addWidget(sectionLabel(tr("MICROPHONE"), this));
    {
        auto *row = new QHBoxLayout();
        row->setSpacing(8);
        m_micCombo = new QComboBox(this);
        m_refreshBtn = new QPushButton(tr("Refresh Devices"), this);
        m_refreshBtn->setToolTip(tr("Re-scan the system for microphones and speakers. "
                                     "Keeps your current selection if the device is still present."));
        row->addWidget(m_micCombo, 1);
        row->addWidget(m_refreshBtn);
        root->addLayout(row);
    }
    m_micStatusLabel = new QLabel(this);
    m_micStatusLabel->setStyleSheet("color: #8aa0b8; font-size: 10px;");
    m_micStatusLabel->setWordWrap(true);
    root->addWidget(m_micStatusLabel);
    m_micWarningLabel = new QLabel(this);
    m_micWarningLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
    m_micWarningLabel->setWordWrap(true);
    m_micWarningLabel->setVisible(false);
    root->addWidget(m_micWarningLabel);

    {
        auto *form = new QFormLayout();
        form->setLabelAlignment(Qt::AlignLeft);

        m_micMeter = new QProgressBar(this);
        m_micMeter->setRange(0, 100);
        m_micMeter->setTextVisible(false);
        m_micMeter->setFixedHeight(10);
        m_micMeter->setStyleSheet(
            "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; }"
            "QProgressBar::chunk { background: #50c878; border-radius: 2px; }");
        m_micMeter->setToolTip(tr("Live microphone input level. Only moves while a call is active."));

        m_testMicBtn = new QPushButton(tr("Test Microphone"), this);
        m_testMicBtn->setToolTip(tr("Highlights the input level meter above for 10 seconds so you can "
                                     "confirm the microphone is picking up sound. Does not send anything "
                                     "over SIP; the meter only moves for real during an active call."));
        auto *meterRow = new QHBoxLayout();
        meterRow->addWidget(m_micMeter, 1);
        meterRow->addWidget(m_testMicBtn);
        form->addRow(tr("Level:"), meterRow);

        m_micTestStatusLabel = new QLabel(this);
        m_micTestStatusLabel->setStyleSheet("color: #50c878; font-size: 10px; font-weight: 600;");
        m_micTestStatusLabel->setVisible(false);
        form->addRow(QString(), m_micTestStatusLabel);

        auto *volRow = new QHBoxLayout();
        m_micVolumeSlider = new QSlider(Qt::Horizontal, this);
        m_micVolumeSlider->setRange(0, 100);
        m_micVolumeSlider->setValue(100);
        m_micVolumeSlider->setToolTip(tr("Microphone gain (0-100%, 100% = default). Applied immediately "
                                          "to the active call; otherwise used as the default for the next call."));
        m_micVolumeLabel = new QLabel(QStringLiteral("100%"), this);
        m_micVolumeLabel->setFixedWidth(40);
        volRow->addWidget(m_micVolumeSlider, 1);
        volRow->addWidget(m_micVolumeLabel);
        form->addRow(tr("Volume:"), volRow);

        root->addLayout(form);
    }

    // ── Speaker ─────────────────────────────────────────────────────────────
    root->addWidget(sectionLabel(tr("SPEAKER"), this));
    {
        auto *row = new QHBoxLayout();
        row->setSpacing(8);
        m_spkCombo = new QComboBox(this);
        m_testSpeakerBtn = new QPushButton(tr("Test Speaker"), this);
        row->addWidget(m_spkCombo, 1);
        row->addWidget(m_testSpeakerBtn);
        root->addLayout(row);
    }
    m_spkStatusLabel = new QLabel(this);
    m_spkStatusLabel->setStyleSheet("color: #8aa0b8; font-size: 10px;");
    m_spkStatusLabel->setWordWrap(true);
    root->addWidget(m_spkStatusLabel);
    m_spkWarningLabel = new QLabel(this);
    m_spkWarningLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
    m_spkWarningLabel->setWordWrap(true);
    m_spkWarningLabel->setVisible(false);
    root->addWidget(m_spkWarningLabel);

    {
        auto *form = new QFormLayout();
        form->setLabelAlignment(Qt::AlignLeft);

        m_spkMeter = new QProgressBar(this);
        m_spkMeter->setRange(0, 100);
        m_spkMeter->setTextVisible(false);
        m_spkMeter->setFixedHeight(10);
        m_spkMeter->setStyleSheet(
            "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; }"
            "QProgressBar::chunk { background: #5090e0; border-radius: 2px; }");
        m_spkMeter->setToolTip(tr("Live speaker output level. Only moves while a call is active."));
        form->addRow(tr("Level:"), m_spkMeter);

        auto *volRow = new QHBoxLayout();
        m_spkVolumeSlider = new QSlider(Qt::Horizontal, this);
        m_spkVolumeSlider->setRange(0, 100);
        m_spkVolumeSlider->setValue(100);
        m_spkVolumeSlider->setToolTip(tr("Speaker gain (0-100%, 100% = default). Applied immediately "
                                          "to the active call; otherwise used as the default for the next call."));
        m_spkVolumeLabel = new QLabel(QStringLiteral("100%"), this);
        m_spkVolumeLabel->setFixedWidth(40);
        volRow->addWidget(m_spkVolumeSlider, 1);
        volRow->addWidget(m_spkVolumeLabel);
        form->addRow(tr("Volume:"), volRow);

        root->addLayout(form);
    }

    auto *note = new QLabel(
        tr("Volume is applied to the active call immediately when PJSIP audio "
           "is connected, and used as the default for the next call. Level "
           "meters only move while a call is in progress."),
        this);
    note->setWordWrap(true);
    note->setStyleSheet("color: #6a7a9a; font-size: 10px; padding-top: 8px;");
    root->addWidget(note);

    root->addStretch(1);

    connect(m_micCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaSettingsPanel::onMicrophoneComboChanged);
    connect(m_spkCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaSettingsPanel::onSpeakerComboChanged);
    connect(m_micVolumeSlider, &QSlider::valueChanged,
            this, &MediaSettingsPanel::onMicVolumeSliderChanged);
    connect(m_spkVolumeSlider, &QSlider::valueChanged,
            this, &MediaSettingsPanel::onSpeakerVolumeSliderChanged);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &MediaSettingsPanel::onRefreshClicked);
    connect(m_resetBtn, &QPushButton::clicked,
            this, &MediaSettingsPanel::onResetToDefaultClicked);
    connect(m_testSpeakerBtn, &QPushButton::clicked,
            this, &MediaSettingsPanel::onTestSpeakerClicked);
    connect(m_testMicBtn, &QPushButton::clicked,
            this, &MediaSettingsPanel::onTestMicrophoneClicked);
}

void MediaSettingsPanel::populateDevices()
{
    const QString curMic = m_micCombo->currentData().toString();
    const QString curSpk = m_spkCombo->currentData().toString();

    MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    const QString defaultMicId = sel.selectedMicrophone().id;
    const QString defaultSpkId = sel.selectedSpeaker().id;

    const QList<MediaDevice> mics = MediaDeviceManager::instance().listMicrophones();
    const QList<MediaDevice> spks = MediaDeviceManager::instance().listSpeakers();

    // A non-empty persisted id that no longer resolves to an enumerated
    // device means the user's previous pick disappeared (unplugged, driver
    // removed, ...) and MediaDeviceSelectionModel already fell back to the
    // default — surface that explicitly instead of silently switching.
    const QString persistedMicId = AppSettings::loadSelectedMicrophone();
    const bool micDeviceMissing = !persistedMicId.isEmpty()
        && MediaDeviceManager::instance().findDevice(MediaDeviceType::Microphone, persistedMicId).isNull();
    const QString persistedSpkId = AppSettings::loadSelectedSpeaker();
    const bool spkDeviceMissing = !persistedSpkId.isEmpty()
        && MediaDeviceManager::instance().findDevice(MediaDeviceType::Speaker, persistedSpkId).isNull();

    {
        const QSignalBlocker b(m_micCombo);
        m_micCombo->clear();
        m_micCombo->addItem(tr("Default (system)"), QString{});
        for (const MediaDevice &d : mics)
            m_micCombo->addItem(d.displayName, d.id);

        m_micCombo->setEnabled(!mics.isEmpty());
        if (mics.isEmpty()) {
            m_micCombo->setToolTip(tr("No microphone detected — connect a device and click Refresh Devices"));
            m_micWarningLabel->setText(tr("⚠ No microphone detected. Connect a device and click Refresh Devices."));
            m_micWarningLabel->setVisible(true);
        } else {
            m_micCombo->setToolTip(tr("Microphone device — applied to the active call immediately when "
                                       "possible, otherwise on the next call"));
            if (micDeviceMissing) {
                m_micWarningLabel->setText(tr("⚠ Previously selected microphone is no longer connected. "
                                               "Reverted to Default (system)."));
                m_micWarningLabel->setVisible(true);
            } else {
                m_micWarningLabel->setVisible(false);
            }

            bool found = false;
            for (int i = 0; i < m_micCombo->count(); ++i) {
                if (m_micCombo->itemData(i).toString() == curMic) { m_micCombo->setCurrentIndex(i); found = true; break; }
            }
            if (!found) {
                for (int i = 0; i < m_micCombo->count(); ++i) {
                    if (m_micCombo->itemData(i).toString() == defaultMicId) { m_micCombo->setCurrentIndex(i); break; }
                }
            }
        }
    }

    {
        const QSignalBlocker b(m_spkCombo);
        m_spkCombo->clear();
        m_spkCombo->addItem(tr("Default (system)"), QString{});
        for (const MediaDevice &d : spks)
            m_spkCombo->addItem(d.displayName, d.id);

        m_spkCombo->setEnabled(!spks.isEmpty());
        m_testSpeakerBtn->setEnabled(!spks.isEmpty());
        if (spks.isEmpty()) {
            m_spkCombo->setToolTip(tr("No speaker detected — connect a device and click Refresh Devices"));
            m_testSpeakerBtn->setToolTip(tr("No speaker detected"));
            m_spkWarningLabel->setText(tr("⚠ No speaker detected. Connect a device and click Refresh Devices."));
            m_spkWarningLabel->setVisible(true);
        } else {
            m_spkCombo->setToolTip(tr("Speaker device — applied to the active call immediately when "
                                       "possible, otherwise on the next call"));
            m_testSpeakerBtn->setToolTip(tr("Play a short test tone through the selected speaker"));
            if (spkDeviceMissing) {
                m_spkWarningLabel->setText(tr("⚠ Previously selected speaker is no longer connected. "
                                               "Reverted to Default (system)."));
                m_spkWarningLabel->setVisible(true);
            } else {
                m_spkWarningLabel->setVisible(false);
            }

            bool found = false;
            for (int i = 0; i < m_spkCombo->count(); ++i) {
                if (m_spkCombo->itemData(i).toString() == curSpk) { m_spkCombo->setCurrentIndex(i); found = true; break; }
            }
            if (!found) {
                for (int i = 0; i < m_spkCombo->count(); ++i) {
                    if (m_spkCombo->itemData(i).toString() == defaultSpkId) { m_spkCombo->setCurrentIndex(i); break; }
                }
            }
        }
    }

    updateDeviceStatusLabels();
}

void MediaSettingsPanel::updateDeviceStatusLabels()
{
    MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    const MediaDevice mic = sel.selectedMicrophone();
    const MediaDevice spk = sel.selectedSpeaker();

    const QString micName = mic.isNull() ? tr("none")
        : (mic.isDefault ? tr("Default (system) — %1").arg(mic.displayName) : mic.displayName);
    const QString spkName = spk.isNull() ? tr("none")
        : (spk.isDefault ? tr("Default (system) — %1").arg(spk.displayName) : spk.displayName);

    QString micText = tr("Selected: %1").arg(micName);
    const QString micActive = PjsipAudioMapper::activeCaptureDeviceName();
    if (!micActive.isEmpty())
        micText += tr("   |   Active now: %1").arg(micActive);
    m_micStatusLabel->setText(micText);

    QString spkText = tr("Selected: %1").arg(spkName);
    const QString spkActive = PjsipAudioMapper::activePlaybackDeviceName();
    if (!spkActive.isEmpty())
        spkText += tr("   |   Active now: %1").arg(spkActive);
    m_spkStatusLabel->setText(spkText);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MediaSettingsPanel::onMicrophoneComboChanged(int index)
{
    if (index < 0)
        return;
    AudioMediaManager::instance().setMicrophone(m_micCombo->itemData(index).toString());
}

void MediaSettingsPanel::onSpeakerComboChanged(int index)
{
    if (index < 0)
        return;
    AudioMediaManager::instance().setSpeaker(m_spkCombo->itemData(index).toString());
}

void MediaSettingsPanel::onMicVolumeSliderChanged(int value)
{
    m_micVolumeLabel->setText(QStringLiteral("%1%").arg(value));
    AudioMediaManager::instance().setMicrophoneVolume(value);
}

void MediaSettingsPanel::onSpeakerVolumeSliderChanged(int value)
{
    m_spkVolumeLabel->setText(QStringLiteral("%1%").arg(value));
    AudioMediaManager::instance().setSpeakerVolume(value);
}

void MediaSettingsPanel::onMicrophoneVolumeChanged(int percent)
{
    if (m_micVolumeSlider->value() == percent)
        return;
    const QSignalBlocker b(m_micVolumeSlider);
    m_micVolumeSlider->setValue(percent);
    m_micVolumeLabel->setText(QStringLiteral("%1%").arg(percent));
}

void MediaSettingsPanel::onSpeakerVolumeChanged(int percent)
{
    if (m_spkVolumeSlider->value() == percent)
        return;
    const QSignalBlocker b(m_spkVolumeSlider);
    m_spkVolumeSlider->setValue(percent);
    m_spkVolumeLabel->setText(QStringLiteral("%1%").arg(percent));
}

void MediaSettingsPanel::onInputLevelChanged(int level)
{
    m_micMeter->setValue(level);
}

void MediaSettingsPanel::onOutputLevelChanged(int level)
{
    m_spkMeter->setValue(level);
}

void MediaSettingsPanel::onDevicesChanged()
{
    populateDevices();
}

void MediaSettingsPanel::onDeviceSelectionChanged()
{
    populateDevices();
}

void MediaSettingsPanel::onMediaConnectionChanged()
{
    updateDeviceStatusLabels();
}

void MediaSettingsPanel::onRefreshClicked()
{
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(tr("Refreshing…"));
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("MediaSettingsPanel: manual device refresh requested"));
    MediaDeviceManager::instance().refreshDevices();
}

void MediaSettingsPanel::onResetToDefaultClicked()
{
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("MediaSettingsPanel: reset to default requested"));

    AudioMediaManager &mgr = AudioMediaManager::instance();
    mgr.setMicrophone(QString());
    mgr.setSpeaker(QString());
    mgr.setMicrophoneVolume(100);
    mgr.setSpeakerVolume(100);

    // The *Changed signals above already update the volume sliders; refresh
    // the device combos/status labels immediately rather than waiting for
    // the audioDeviceSelectionChanged round-trip.
    populateDevices();
}

void MediaSettingsPanel::onTestSpeakerClicked()
{
    if (m_testSink) {
        m_testSink->stop();
        m_testSink->deleteLater();
        m_testSink = nullptr;
    }
    if (m_testBuffer) {
        m_testBuffer->close();
        m_testBuffer->deleteLater();
        m_testBuffer = nullptr;
    }

    const QString spkId = m_spkCombo->currentData().toString();
    QAudioDevice outDevice = QMediaDevices::defaultAudioOutput();
    if (!spkId.isEmpty()) {
        for (const QAudioDevice &d : QMediaDevices::audioOutputs()) {
            if (QString(d.id()) == spkId) { outDevice = d; break; }
        }
    }

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!outDevice.isFormatSupported(format))
        format = outDevice.preferredFormat();

    m_testToneData = generateTestTone(format);
    m_testBuffer = new QBuffer(&m_testToneData, this);
    m_testBuffer->open(QIODevice::ReadOnly);

    m_testSink = new QAudioSink(outDevice, format, this);
    m_testSink->setVolume(AudioMediaManager::instance().speakerVolume() / 100.0);
    connect(m_testSink, &QAudioSink::stateChanged,
            this, &MediaSettingsPanel::onTestToneStateChanged);

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("MediaSettingsPanel: playing test tone on \"%1\"")
            .arg(outDevice.description()));

    m_testSink->start(m_testBuffer);
}

void MediaSettingsPanel::onTestToneStateChanged(QAudio::State state)
{
    if (state != QAudio::IdleState && state != QAudio::StoppedState)
        return;
    if (m_testSink) {
        m_testSink->stop();
        m_testSink->deleteLater();
        m_testSink = nullptr;
    }
    if (m_testBuffer) {
        m_testBuffer->close();
        m_testBuffer->deleteLater();
        m_testBuffer = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Microphone test mode
//
// There is no standalone local-capture pipeline outside of an active call —
// AudioMediaManager's input level only comes from the SipCall audio stream.
// So "Test Microphone" does not fabricate a level or start any SIP/PJSIP
// action; it just highlights the existing live meter and shows guidance
// text for a bounded window, matching whatever the meter actually reports
// (0 with no active call, real movement during one).
// ---------------------------------------------------------------------------

void MediaSettingsPanel::onTestMicrophoneClicked()
{
    if (m_micTestActive) {
        stopMicTest();
        return;
    }

    m_micTestActive = true;
    m_testMicBtn->setText(tr("Stop Test"));
    m_micMeter->setStyleSheet(
        "QProgressBar { border: 2px solid #50c878; border-radius: 3px; background: #222; }"
        "QProgressBar::chunk { background: #50c878; border-radius: 2px; }");

    const bool callActive = AudioMediaManager::instance().isMediaActive();
    m_micTestStatusLabel->setText(callActive
        ? tr("Speak now — input meter should move")
        : tr("Speak now — input meter should move (meter stays at 0 with no active call)"));
    m_micTestStatusLabel->setVisible(true);

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("MediaSettingsPanel: microphone test started (10s, mediaActive=%1)")
            .arg(callActive));

    m_micTestTimer->start(10000);
}

void MediaSettingsPanel::stopMicTest()
{
    if (!m_micTestActive)
        return;
    m_micTestActive = false;
    m_micTestTimer->stop();
    m_testMicBtn->setText(tr("Test Microphone"));
    m_micTestStatusLabel->setVisible(false);
    m_micMeter->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; }"
        "QProgressBar::chunk { background: #50c878; border-radius: 2px; }");

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("MediaSettingsPanel: microphone test stopped"));
}
