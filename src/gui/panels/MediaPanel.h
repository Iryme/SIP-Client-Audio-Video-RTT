#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include "media/MediaDevice.h"

class QShowEvent;

class QLabel;
class QComboBox;
class QPushButton;
class VideoPanel;
class MediaDeviceManager;
class MediaDeviceSelectionModel;

class MediaPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MediaPanel(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onMicrophoneChanged(int index);
    void onSpeakerChanged(int index);
    void onCameraChanged(int index);
    void onRefreshClicked();
    void onStartPreviewClicked();

    void onMicrophoneSelectionChanged(const MediaDevice &device);
    void onSpeakerSelectionChanged(const MediaDevice &device);
    void onCameraSelectionChanged(const MediaDevice &device);
    void onDevicesChanged();
    void onRefreshStarted();
    void onRefreshFinished();

private:
    void buildUi();
    void populateCombo(QComboBox *combo, const QList<MediaDevice> &devices,
                       const QString &selectedId, bool showDefault = false);
    void refreshAll();
    void updateStatuses();

    QLabel      *m_inputStatus{nullptr};
    QLabel      *m_outputStatus{nullptr};
    QLabel      *m_videoStatus{nullptr};
    QLabel      *m_micLevel{nullptr};
    QLabel      *m_speakerLevel{nullptr};
    QLabel      *m_videoFallback{nullptr};

    QComboBox   *m_micCombo{nullptr};
    QComboBox   *m_speakerCombo{nullptr};
    QComboBox   *m_cameraCombo{nullptr};

    QPushButton *m_refreshBtn{nullptr};
    QPushButton *m_testOutputBtn{nullptr};
    QPushButton *m_startPreviewBtn{nullptr};

    VideoPanel  *m_previewPanel{nullptr};

    MediaDeviceManager       *m_manager{nullptr};
    MediaDeviceSelectionModel *m_selectionModel{nullptr};
};
