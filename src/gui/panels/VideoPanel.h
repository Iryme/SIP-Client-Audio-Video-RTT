#pragma once
#include <QTimer>
#include <QWidget>

class QCamera;
class QMediaCaptureSession;
class QVideoFrame;
class QVideoSink;
class QLabel;
class QComboBox;
class QPushButton;

// VideoPanel renders local and remote video streams during an active call.
//
// Layout:
//   - Full-area background: remote video placeholder / future QVideoWidget
//   - Bottom-right PiP: local preview (160x90)
//   - Top-right overlay: signal / media state indicator
//   - Bottom-left overlay: remote participant label
//   - Top-left overlay: camera selector combo + video-mute button (call only)
//   - Swap button (bottom-center): swap local/remote view roles
//
// Before a call the panel shows a placeholder paintEvent with a crosshair.
// During a call in stub mode the crosshair is replaced with a "Video Active"
// tinted background and the overlay controls become visible.
class VideoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit VideoPanel(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onVideoMediaConnected();
    void onVideoMediaDisconnected();
    void onLocalVideoStarted();
    void onLocalVideoStopped();
    void onRemoteVideoStarted();
    void onRemoteVideoStopped();
    void onVideoMutedChanged(bool muted);
    void onCallStateChanged();
    void onIdlePreviewFrame(const QVideoFrame &frame);

private:
    void repositionOverlays();
    void populateCameraCombo();
    void applyVideoState();
    void startIdlePreview();
    void stopIdlePreview();
    void refreshIdlePreview();
    // Re-fit the embedded PJSIP video HWND to fill its parent widget area.
    // Called on resize when video is active.
    void resizeEmbeddedVideoWindows();

    // Main-area overlays
    QLabel      *m_remoteLabel{nullptr};
    QLabel      *m_signalIndicator{nullptr};

    // PiP (local preview)
    QLabel      *m_localPreview{nullptr};
    bool         m_swapped{false}; // true → local full, remote in PiP

    // Control overlay (shown only during a call)
    QWidget     *m_controlOverlay{nullptr};
    QComboBox   *m_cameraSelector{nullptr};
    QPushButton *m_btnVideoMute{nullptr};
    QPushButton *m_btnSwap{nullptr};

    // Current video state
    bool   m_videoActive{false};
    bool   m_localVideoAvail{false};
    bool   m_remoteVideoAvail{false};
    bool   m_videoMuted{false};
    bool   m_idlePreviewRunning{false};
    int    m_idlePreviewCapDev{-3};
    bool   m_noVideoDeviceAvailable{false};

    // Qt camera stack for idle local preview (used when no call is active).
    QCamera              *m_previewCamera{nullptr};
    QMediaCaptureSession *m_previewSession{nullptr};
    QVideoSink           *m_previewSink{nullptr};

    // Retries video window attachment every 2 s while a call is active.
    // Needed because PJSIP creates the remote render HWND lazily and
    // the first onCallMediaState callback often has videoIncomingWindowId=-1.
    QTimer m_videoRetryTimer;
};
