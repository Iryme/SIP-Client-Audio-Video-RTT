#pragma once
#include <QWidget>
#include "media/VideoSettings.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QShowEvent;
class QHideEvent;
class QSlider;
class VideoPanel;

class VideoSettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit VideoSettingsPanel(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onCameraChanged(int idx);
    void onBitrateChanged(int value);
    void onCodecUp();
    void onCodecDown();
    void onApply();
    void onReset();
    void onStatsUpdated(float fps, int drops);
    void onSettingsChanged(const VideoSettings &s);

private:
    void buildUi();
    void populateCameras();
    void populateResolutions(const QString &cameraId);
    void populateFps(const QString &cameraId, const QSize &resolution);
    void populateCodecs(const QStringList &order);
    void loadFrom(const VideoSettings &s);
    VideoSettings collectSettings() const;
    static QString formatBitrate(int kbps);

    static const QList<int> kBitrateSteps;

    QComboBox   *m_cameraCombo{nullptr};
    QComboBox   *m_resolutionCombo{nullptr};
    QComboBox   *m_fpsCombo{nullptr};
    QSlider     *m_bitrateSlider{nullptr};
    QLabel      *m_bitrateLabel{nullptr};
    QListWidget *m_codecList{nullptr};
    QPushButton *m_codecUp{nullptr};
    QPushButton *m_codecDown{nullptr};
    QCheckBox   *m_overlayCheck{nullptr};
    QPushButton *m_applyBtn{nullptr};
    QPushButton *m_resetBtn{nullptr};
    QLabel      *m_statsLabel{nullptr};
    VideoPanel  *m_preview{nullptr};

    bool m_statsConnected{false};
};
