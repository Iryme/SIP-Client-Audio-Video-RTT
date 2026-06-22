#pragma once
#include <QWidget>
#include "media/MediaDevice.h"

class QComboBox;
class QLabel;
class QPushButton;
class MediaDeviceManager;
class MediaDeviceSelectionModel;

class MediaPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MediaPanel(QWidget *parent = nullptr);

private slots:
    void onMicrophoneChanged(int index);
    void onSpeakerChanged(int index);
    void onCameraChanged(int index);
    void onRefreshClicked();

    void onMicrophoneSelectionChanged(const MediaDevice &device);
    void onSpeakerSelectionChanged   (const MediaDevice &device);
    void onCameraSelectionChanged    (const MediaDevice &device);

private:
    void buildUi();
    void populateCombo(QComboBox *combo, const QList<MediaDevice> &devices,
                       const QString &selectedId);
    void refreshAll();

    QComboBox   *m_micCombo{nullptr};
    QComboBox   *m_speakerCombo{nullptr};
    QComboBox   *m_cameraCombo{nullptr};
    QPushButton *m_refreshBtn{nullptr};

    // Placeholder audio level meter labels (no real capture yet)
    QLabel *m_micLevel{nullptr};
    QLabel *m_speakerLevel{nullptr};

    MediaDeviceManager       *m_manager{nullptr};
    MediaDeviceSelectionModel *m_selectionModel{nullptr};
};
