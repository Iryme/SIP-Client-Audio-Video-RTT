#pragma once
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>
#include "gui/CameraController.h"

class QCamera;
class QMediaCaptureSession;
class QVideoFrame;
class QVideoSink;
class QLabel;

// VideoPanel renders local and remote video streams during an active call.
//
// Layout:
//   - Full-area background: remote video placeholder / future QVideoWidget
//   - Bottom-right PiP: local preview (160x90)
//   - Top-right overlay: signal / media state indicator (informational only)
//   - Bottom-left overlay: remote participant label (informational only)
//
// All interactive controls (camera toggle, video mute, swap) have been moved
// to CallPanel so the video window contains no overlay buttons.
//
// Before a call the panel shows a placeholder paintEvent with a crosshair.
class VideoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit VideoPanel(QWidget *parent = nullptr, bool autoStartIdlePreview = true);

public slots:
    void startIdlePreview();
    void stopIdlePreview();
    void refreshIdlePreview();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onVideoMediaConnected();
    void onVideoMediaDisconnected();
    void onLocalVideoStarted();
    void onLocalVideoStopped();
    void onRemoteVideoStarted();
    void onRemoteVideoStopped();
    void onIdlePreviewFrame(const QVideoFrame &frame);

private:
    void repositionOverlays();
    void applyVideoState();
    void drawDebugOverlay(QPainter &p);
    // Re-fit the embedded PJSIP video HWND to fill its parent widget area.
    // Called on resize when video is active.
    void resizeEmbeddedVideoWindows();

    // Informational overlays (no interactive controls)
    QLabel      *m_remoteLabel{nullptr};
    QLabel      *m_signalIndicator{nullptr};

    // PiP (local preview)
    QLabel      *m_localPreview{nullptr};

    // Current video state
    bool   m_videoActive{false};
    bool   m_localVideoAvail{false};
    bool   m_remoteVideoAvail{false};
    bool   m_idlePreviewRunning{false};
    int    m_idlePreviewCapDev{-3};
    bool   m_noVideoDeviceAvailable{false};
    bool   m_autoStartIdlePreview{true};
    bool   m_previewFrameSeen{false};

    // Qt camera stack for idle local preview (used when no call is active).
    QCamera              *m_previewCamera{nullptr};
    QMediaCaptureSession *m_previewSession{nullptr};
    QVideoSink           *m_previewSink{nullptr};

    // Retries video window attachment while a call is active (lazy PJSIP HWND).
    QTimer m_videoRetryTimer;

    // Debounces resizeEmbeddedVideoWindows: coalesces rapid WM_SIZE events.
    QTimer m_resizeDebounceTimer;

    // Watchdog: while video is active, blank the remote view (black frame)
    // when no decoded frame has arrived recently (e.g. peer camera off).
    QTimer m_remoteStaleTimer;

    // Frame throttle: drop idle-preview frames arriving faster than ~30 fps.
    QElapsedTimer m_frameThrottle;

    // True once PJSIP video windows have been successfully attached to this
    // panel; reset on disconnect. Prevents redundant attach calls.
    bool m_remoteAttached{false};

    // Failed attach attempts since the current call's video connected; the
    // retry timer gives up after kMaxVideoAttachRetries to avoid looping.
    static constexpr int kMaxVideoAttachRetries = 10;
    int  m_videoRetryCount{0};
};
