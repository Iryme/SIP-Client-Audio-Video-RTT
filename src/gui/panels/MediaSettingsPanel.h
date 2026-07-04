#pragma once
#include <QAudio>
#include <QWidget>

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSlider;
class QTimer;
class QAudioSink;
class QIODevice;
class QBuffer;

// Settings tab for microphone / speaker device selection and volume.
//
// Device selection and volume are delegated to AudioMediaManager, which
// persists them (AppSettings) and applies them to the active call's PJSIP
// audio media immediately when one exists. CallPanel uses the same
// AudioMediaManager API, so changes made here are reflected there and vice
// versa (both listen to AudioMediaManager's *Changed signals).
class MediaSettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MediaSettingsPanel(QWidget *parent = nullptr);
    ~MediaSettingsPanel() override;

private slots:
    void onMicrophoneComboChanged(int index);
    void onSpeakerComboChanged(int index);
    void onMicVolumeSliderChanged(int value);
    void onSpeakerVolumeSliderChanged(int value);
    void onMicrophoneVolumeChanged(int percent);
    void onSpeakerVolumeChanged(int percent);
    void onInputLevelChanged(int level);
    void onOutputLevelChanged(int level);
    void onDevicesChanged();
    void onDeviceSelectionChanged();
    void onMediaConnectionChanged();
    void onRefreshClicked();
    void onResetToDefaultClicked();
    void onTestSpeakerClicked();
    void onTestToneStateChanged(QAudio::State state);
    void onTestMicrophoneClicked();

private:
    void buildUi();
    void populateDevices();
    void updateDeviceStatusLabels();
    void stopMicTest();

    QComboBox   *m_micCombo{nullptr};
    QComboBox   *m_spkCombo{nullptr};
    QSlider     *m_micVolumeSlider{nullptr};
    QSlider     *m_spkVolumeSlider{nullptr};
    QLabel      *m_micVolumeLabel{nullptr};
    QLabel      *m_spkVolumeLabel{nullptr};
    QProgressBar *m_micMeter{nullptr};
    QProgressBar *m_spkMeter{nullptr};
    QPushButton *m_refreshBtn{nullptr};
    QPushButton *m_resetBtn{nullptr};
    QPushButton *m_testSpeakerBtn{nullptr};
    QPushButton *m_testMicBtn{nullptr};
    QLabel      *m_micWarningLabel{nullptr};
    QLabel      *m_spkWarningLabel{nullptr};
    QLabel      *m_micStatusLabel{nullptr};
    QLabel      *m_spkStatusLabel{nullptr};
    QLabel      *m_micTestStatusLabel{nullptr};

    // Test-tone playback (real QAudioSink output on the selected speaker).
    QAudioSink  *m_testSink{nullptr};
    QBuffer     *m_testBuffer{nullptr};
    QByteArray   m_testToneData;

    // Microphone test mode — does not fabricate any level; it just
    // highlights the existing live input meter for a bounded window so the
    // user knows to speak. Real values still come from AudioMediaManager.
    QTimer *m_micTestTimer{nullptr};
    bool    m_micTestActive{false};
};
