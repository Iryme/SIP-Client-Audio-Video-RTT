#include "VideoPanel.h"

#include <QCamera>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>
#include <QtGlobal>

#include "core/Logger.h"
#include "core/AppSettings.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoMediaManager.h"
#include "media/VideoQualityManager.h"
#include "media/VideoStatistics.h"
#include "media/VideoPipelineMonitor.h"
#include "sip/SipManager.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <pjsua-lib/pjsua.h>
#include <pj/errno.h>
#endif

#if defined(HAVE_PJSIP) && defined(_WIN32)
#include "media/PjsipGdiRenderer.h"
#endif

#if defined(HAVE_PJSIP) && defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
static QString pjsipStatusText(pj_status_t st)
{
    char buf[PJ_ERR_MSG_SIZE] = {};
    pj_strerror(st, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#if defined(HAVE_PJSIP) && defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO && defined(_WIN32)
static pjmedia_vid_dev_index preferredPreviewRenderDevice()
{
    const pjmedia_vid_dev_index gdiRenderDev = PjsipGdiRenderer::deviceIndex();
    if (gdiRenderDev != PJMEDIA_VID_INVALID_DEV)
        return gdiRenderDev;
    return PJMEDIA_VID_DEFAULT_RENDER_DEV;
}
#endif

VideoPanel::VideoPanel(QWidget *parent, bool autoStartIdlePreview)
    : QWidget(parent)
{
    setObjectName("VideoPanel");
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_autoStartIdlePreview = autoStartIdlePreview;
    // Native window handle required for PJSIP video embedding via SetParent.
    setAttribute(Qt::WA_NativeWindow);

    // --- Remote video label (bottom-left) ------------------------------------
    m_remoteLabel = new QLabel(tr("No active call"), this);
    m_remoteLabel->setObjectName("RemoteLabel");
    m_remoteLabel->setStyleSheet(
        "background: rgba(0,0,0,140); color: #ffffff; padding: 4px 8px; border-radius: 4px;");
    m_remoteLabel->adjustSize();

    // --- Signal / media-state indicator (top-right) --------------------------
    m_signalIndicator = new QLabel(tr("● NO VIDEO"), this);
    m_signalIndicator->setObjectName("SignalIndicator");
    m_signalIndicator->setStyleSheet(
        "background: rgba(0,0,0,140); color: #888888; padding: 4px 8px; border-radius: 4px;");
    m_signalIndicator->adjustSize();

    // --- Local preview PiP (bottom-right) ------------------------------------
    m_localPreview = new QLabel(this);
    m_localPreview->setObjectName("LocalPreview");
    m_localPreview->setStyleSheet(
        "background: #1a2030; border: 1px solid #3a4060; border-radius: 4px;");
    m_localPreview->setAlignment(Qt::AlignCenter);
    m_localPreview->setText(tr("Camera\nOff"));
    m_localPreview->setWordWrap(true);
    m_localPreview->setFixedSize(160, 90);
    // Native handle required so PJSIP can embed the local preview via SetParent.
    m_localPreview->setAttribute(Qt::WA_NativeWindow);

    // --- Control overlay (hidden until a call is active) ---------------------
    m_controlOverlay = new QWidget(this);
    m_controlOverlay->setObjectName("VideoControlOverlay");
    m_controlOverlay->setAttribute(Qt::WA_TranslucentBackground);

    auto *overlayLayout = new QHBoxLayout(m_controlOverlay);
    overlayLayout->setContentsMargins(6, 6, 6, 6);
    overlayLayout->setSpacing(6);

    m_cameraSelector = new QComboBox(m_controlOverlay);
    m_cameraSelector->setObjectName("CameraSelector");
    m_cameraSelector->setFixedHeight(24);
    m_cameraSelector->setMinimumWidth(120);
    m_cameraSelector->setStyleSheet(
        "QComboBox { background: rgba(0,0,0,160); color: #ffffff; border: 1px solid #555; "
        "border-radius: 3px; padding: 1px 4px; }");

    m_btnCameraToggle = new QPushButton(tr("Camera On"), m_controlOverlay);
    m_btnCameraToggle->setObjectName("CameraToggleBtn");
    m_btnCameraToggle->setCheckable(true);
    m_btnCameraToggle->setChecked(true);
    m_btnCameraToggle->setFixedHeight(24);
    m_btnCameraToggle->setStyleSheet(
        "QPushButton { background: rgba(0,0,0,160); color: #ffffff; border: 1px solid #555; "
        "border-radius: 3px; padding: 1px 8px; }"
        "QPushButton:checked { background: rgba(40,120,70,180); }");

    m_btnVideoMute = new QPushButton(tr("Mute Video"), m_controlOverlay);
    m_btnVideoMute->setObjectName("VideoMuteBtn");
    m_btnVideoMute->setCheckable(true);
    m_btnVideoMute->setFixedHeight(24);
    m_btnVideoMute->setStyleSheet(
        "QPushButton { background: rgba(0,0,0,160); color: #ffffff; border: 1px solid #555; "
        "border-radius: 3px; padding: 1px 8px; }"
        "QPushButton:checked { background: rgba(180,50,50,180); }");

    overlayLayout->addWidget(m_cameraSelector);
    overlayLayout->addWidget(m_btnCameraToggle);
    overlayLayout->addWidget(m_btnVideoMute);
    overlayLayout->addStretch();

    m_controlOverlay->setVisible(false);
    m_controlOverlay->adjustSize();

    // --- Swap button (bottom-centre) -----------------------------------------
    m_btnSwap = new QPushButton(tr("⇄"), this);
    m_btnSwap->setObjectName("SwapBtn");
    m_btnSwap->setFixedSize(32, 32);
    m_btnSwap->setStyleSheet(
        "QPushButton { background: rgba(0,0,0,160); color: #ffffff; border: 1px solid #555; "
        "border-radius: 4px; font-size: 16px; }"
        "QPushButton:hover { background: rgba(60,80,120,200); }");
    m_btnSwap->setVisible(false);

    // --- Wiring --------------------------------------------------------------
    // Camera selector → VideoMediaManager
    connect(m_cameraSelector, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QString id = m_cameraSelector->itemData(idx).toString();
        if (!id.isEmpty())
            VideoMediaManager::instance().setCamera(id);
        refreshIdlePreview();
    });
    connect(m_btnCameraToggle, &QPushButton::toggled, this, [this](bool on) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Camera %1").arg(on ? QStringLiteral("ON") : QStringLiteral("OFF")));
        if (on) {
            Logger::instance().info(LogCategory::Media, QStringLiteral("Camera acquired"));
            startIdlePreview();
        } else {
            stopIdlePreview();
            Logger::instance().info(LogCategory::Media, QStringLiteral("Camera released"));
        }
    });

    // Video mute button → VideoMediaManager
    connect(m_btnVideoMute, &QPushButton::toggled,
            [](bool checked){ VideoMediaManager::instance().setVideoMuted(checked); });

    // Swap button → toggle swapped flag and reposition
    connect(m_btnSwap, &QPushButton::clicked, this, [this]() {
        m_swapped = !m_swapped;
        update(); // repaint background hint
        repositionOverlays();
    });

    // In media-preview mode (autoStartIdlePreview=false) permanently hide all
    // remote-video UI elements — this panel is for local camera testing only.
    if (!m_autoStartIdlePreview) {
        m_remoteLabel->setVisible(false);
        m_signalIndicator->setVisible(false);
        m_controlOverlay->setVisible(false);
        m_btnSwap->setVisible(false);
    }

    // Resize debounce: coalesce rapid WM_SIZE events (window drag) into a single
    // resizeEmbeddedVideoWindows() call 100 ms after the last resize.
    m_resizeDebounceTimer.setSingleShot(true);
    m_resizeDebounceTimer.setInterval(100);
    connect(&m_resizeDebounceTimer, &QTimer::timeout, this, [this]() {
        if (m_videoActive) {
            QElapsedTimer t;
            t.start();
            resizeEmbeddedVideoWindows();
            Logger::instance().debug(LogCategory::Media,
                QStringLiteral("VideoPanel: embedded video resized in %1 ms").arg(t.elapsed()));
        }
    });

    // Remote-video wiring is only needed for the call panel (autoStartIdlePreview=true).
    // The media-settings preview panel (autoStartIdlePreview=false) shows only local
    // camera preview and must not intercept PJSIP video handles from the call panel.
    if (m_autoStartIdlePreview) {
        // Retry timer: re-attach PJSIP video windows every 3 s while video is active.
        // 3 s is long enough to avoid micro-stutters while still recovering a lazily-
        // created remote HWND (PJSIP may deliver videoIncomingWindowId=-1 on the first
        // onCallMediaState callback).
        m_videoRetryTimer.setInterval(3000);
        connect(&m_videoRetryTimer, &QTimer::timeout, this, [this]() {
            if (!m_videoActive) return;
            Logger::instance().info(LogCategory::Media,
                "VideoPanel: retry attach video windows");
            QElapsedTimer t;
            t.start();
            VideoMediaManager::instance().attachVideoToWidgets(winId(), m_localPreview->winId());
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("VideoPanel: retry attach done in %1 ms").arg(t.elapsed()));
        });

        connect(&VideoMediaManager::instance(), &VideoMediaManager::videoMediaConnected,
                this, &VideoPanel::onVideoMediaConnected);
        connect(&VideoMediaManager::instance(), &VideoMediaManager::videoMediaDisconnected,
                this, &VideoPanel::onVideoMediaDisconnected);
        connect(&VideoMediaManager::instance(), &VideoMediaManager::localVideoStarted,
                this, &VideoPanel::onLocalVideoStarted);
        connect(&VideoMediaManager::instance(), &VideoMediaManager::localVideoStopped,
                this, &VideoPanel::onLocalVideoStopped);
        connect(&VideoMediaManager::instance(), &VideoMediaManager::remoteVideoStarted,
                this, &VideoPanel::onRemoteVideoStarted);
        connect(&VideoMediaManager::instance(), &VideoMediaManager::remoteVideoStopped,
                this, &VideoPanel::onRemoteVideoStopped);
        connect(&VideoMediaManager::instance(), &VideoMediaManager::videoMutedChanged,
                this, &VideoPanel::onVideoMutedChanged);
    }

    connect(&VideoMediaManager::instance(), &VideoMediaManager::cameraChanged,
            this, [this](const QString &deviceId) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoPanel: cameraChanged received for '%1'").arg(deviceId));
        if (m_autoStartIdlePreview)
            refreshIdlePreview();
    });

    connect(&VideoQualityManager::instance(), &VideoQualityManager::settingsChanged,
            this, [this](const VideoSettings &) {
        if (m_idlePreviewRunning)
            refreshIdlePreview();
    });

    connect(&MediaDeviceManager::instance(), &MediaDeviceManager::devicesChanged,
            this, [this]() {
        populateCameraCombo();
        // Only auto-refresh the idle preview when this panel is configured to
        // do so (e.g. the call panel). The media-settings preview panel uses
        // autoStartIdlePreview=false and must be started explicitly by the user.
        if (m_autoStartIdlePreview)
            refreshIdlePreview();
    });
    if (m_autoStartIdlePreview) {
        connect(&SipManager::instance(), &SipManager::initialized,
                this, [this]() { startIdlePreview(); });
    }
    connect(&SipManager::instance(), &SipManager::shutdownComplete,
            this, [this]() { stopIdlePreview(); });

    // SipManager call state → update remote label text
    connect(&SipManager::instance(), &SipManager::callConnected,
            this, [this](const QString &remoteUri) {
        m_remoteLabel->setText(remoteUri);
        m_remoteLabel->adjustSize();
        repositionOverlays();
    });
    connect(&SipManager::instance(), &SipManager::callDisconnected,
            this, [this](const QString &, const QString &, int) {
        m_remoteLabel->setText(tr("No active call"));
        m_remoteLabel->adjustSize();
        repositionOverlays();
    });

    applyVideoState();
    if (m_autoStartIdlePreview) {
        QTimer::singleShot(0, this, [this]() {
            startIdlePreview();
        });
    }
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void VideoPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionOverlays();
    // Debounce: coalesce rapid resize events (window drag) into one Win32 call.
    if (m_videoActive)
        m_resizeDebounceTimer.start();
}

void VideoPanel::repositionOverlays()
{
    const int margin = 8;
    const int w = width();
    const int h = height();

    // Remote label: bottom-left
    if (m_remoteLabel)
        m_remoteLabel->move(margin, h - m_remoteLabel->height() - margin);

    // Signal indicator: top-right
    if (m_signalIndicator)
        m_signalIndicator->move(w - m_signalIndicator->width() - margin, margin);

    // Control overlay: top-left
    if (m_controlOverlay) {
        m_controlOverlay->adjustSize();
        m_controlOverlay->move(margin, margin);
    }

    // Local preview / PiP
    if (m_localPreview) {
        if (!m_autoStartIdlePreview) {
            // Settings -> Video mode: local preview only, fill the panel.
            m_localPreview->setFixedSize(qMax(1, w - margin * 2), qMax(1, h - margin * 2));
            m_localPreview->move(margin, margin);
        } else if (!m_swapped) {
            // Normal: local PiP bottom-right
            m_localPreview->setFixedSize(160, 90);
            m_localPreview->move(w - m_localPreview->width() - margin,
                                  h - m_localPreview->height() - margin);
        } else {
            // Swapped: local fills main area via full-size frame indicator;
            // remote goes in PiP. We just resize the PiP label to give the
            // illusion (real video rendering via QVideoWidget is a future task).
            m_localPreview->setFixedSize(160, 90);
            m_localPreview->move(margin, h - m_localPreview->height() - margin);
        }
    }

    // Swap button: bottom-centre
    if (m_btnSwap)
        m_btnSwap->move((w - m_btnSwap->width()) / 2,
                         h - m_btnSwap->height() - margin);
}

void VideoPanel::resizeEmbeddedVideoWindows()
{
#ifdef Q_OS_WIN
    HWND parentHwnd = reinterpret_cast<HWND>(static_cast<quintptr>(winId()));
    if (!parentHwnd)
        return;
    RECT rc{};
    GetClientRect(parentHwnd, &rc);

    // m_localPreview has WA_NativeWindow so it owns an HWND that is a direct
    // child of this VideoPanel. Skip it — Qt positions it via repositionOverlays.
    // Any other child HWND is an embedded PJSIP video window and should fill
    // the entire panel area.
    const HWND localPreviewHwnd = m_localPreview
        ? reinterpret_cast<HWND>(static_cast<quintptr>(m_localPreview->winId()))
        : nullptr;

    HWND child = GetWindow(parentHwnd, GW_CHILD);
    while (child) {
        if (child != localPreviewHwnd) {
            MoveWindow(child, 0, 0, rc.right, rc.bottom, TRUE);
            Logger::instance().debug(LogCategory::Media,
                QStringLiteral("Remote video HWND resized to %1x%2")
                    .arg(rc.right).arg(rc.bottom));
        }
        child = GetNextWindow(child, GW_HWNDNEXT);
    }

    // Resize the PJSIP preview HWND embedded inside m_localPreview.
    if (localPreviewHwnd) {
        RECT prc{};
        GetClientRect(localPreviewHwnd, &prc);
        HWND pchild = GetWindow(localPreviewHwnd, GW_CHILD);
        if (pchild)
            MoveWindow(pchild, 0, 0, prc.right, prc.bottom, TRUE);
    }
#endif
}

void VideoPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);

    if (m_videoActive) {
        // Frames are delivered from the PJSIP media thread via QMetaObject::invokeMethod
        // and stored in the "_pjFrame" property.  Draw the latest one; fall back to
        // black if no frame has arrived yet.
        const QImage frame = property("_pjFrame").value<QImage>();
        if (!frame.isNull())
            p.drawImage(rect(), frame);
        else
            p.fillRect(rect(), QColor(0x0a, 0x0a, 0x0a));
        if (VideoQualityManager::instance().current().overlayEnabled)
            drawDebugOverlay(p);
        return;
    }

    // Idle — dark background with placeholder crosshair
    p.fillRect(rect(), QColor(0x0d, 0x11, 0x1a));
    if (!m_autoStartIdlePreview) {
        p.setPen(QColor(0x2f, 0x3f, 0x58));
        p.drawText(rect(), Qt::AlignCenter, tr("Camera preview"));
        return;
    }
    p.setPen(QColor(0x2a, 0x35, 0x50));
    p.drawLine(width() / 2, 0, width() / 2, height());
    p.drawLine(0, height() / 2, width(), height() / 2);
    p.setPen(QColor(0x3a, 0x45, 0x70));
    p.setFont(QFont("Sans", 12));
    const QString idleText = m_autoStartIdlePreview
        ? tr("No video — waiting for call")
        : tr("Camera preview");
    p.drawText(rect(), Qt::AlignCenter, idleText);
    if (m_idlePreviewRunning && VideoQualityManager::instance().current().overlayEnabled)
        drawDebugOverlay(p);
}

// ---------------------------------------------------------------------------
// applyVideoState — sync all overlay visibility / text
// ---------------------------------------------------------------------------

void VideoPanel::applyVideoState()
{
    // Remote-video UI elements stay permanently hidden in media-preview mode.
    if (m_autoStartIdlePreview) {
        const bool inCall = m_videoActive || SipManager::instance().callState() != CallState::Idle;
        m_controlOverlay->setVisible(inCall);
        m_btnSwap->setVisible(inCall);
    }

    if (m_videoActive || m_idlePreviewRunning) {
        m_signalIndicator->setText(tr("● VIDEO"));
        m_signalIndicator->setStyleSheet(
            "background: rgba(0,0,0,140); color: #50e050; padding: 4px 8px; border-radius: 4px;");
        // For the PJSIP/GDI path (videoActive), clear label text so it doesn't
        // overdraw GDI frames. For the Qt-camera idle preview path, leave the
        // pixmap alone — setText would clear it since they're mutually exclusive.
        if (m_videoActive && !m_idlePreviewRunning)
            m_localPreview->setText(QString{});
    } else if (m_noVideoDeviceAvailable) {
        m_signalIndicator->setText(tr("● NO VIDEO"));
        m_signalIndicator->setStyleSheet(
            "background: rgba(0,0,0,140); color: #d0a040; padding: 4px 8px; border-radius: 4px;");
        m_localPreview->setText(tr("No video\ndevice available"));
    } else {
        m_signalIndicator->setText(tr("● NO VIDEO"));
        m_signalIndicator->setStyleSheet(
            "background: rgba(0,0,0,140); color: #888888; padding: 4px 8px; border-radius: 4px;");
        m_localPreview->setText(tr("Camera\nOff"));
    }

    m_signalIndicator->adjustSize();
    repositionOverlays();
    update();
}

void VideoPanel::populateCameraCombo()
{
    const QString cur = m_cameraSelector->currentData().toString();
    MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    const QString defaultId = sel.selectedCamera().id;

    QSignalBlocker blocker(m_cameraSelector);
    m_cameraSelector->clear();
    for (const MediaDevice &d : MediaDeviceManager::instance().listCameras())
        m_cameraSelector->addItem(d.displayName, d.id);

    for (int i = 0; i < m_cameraSelector->count(); ++i) {
        if (m_cameraSelector->itemData(i).toString() == cur) {
            m_cameraSelector->setCurrentIndex(i);
            return;
        }
    }
    for (int i = 0; i < m_cameraSelector->count(); ++i) {
        if (m_cameraSelector->itemData(i).toString() == defaultId) {
            m_cameraSelector->setCurrentIndex(i);
            return;
        }
    }
}

void VideoPanel::startIdlePreview()
{
    Logger::instance().info(LogCategory::Media, "VideoPanel: preview start requested");
    if (m_idlePreviewRunning || m_videoActive) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoPanel: preview start skipped (running=%1 videoActive=%2)")
                .arg(m_idlePreviewRunning).arg(m_videoActive));
        return;
    }
    if (!m_localPreview)
        return;

    QElapsedTimer startTimer;
    startTimer.start();

    // Find the Qt camera device — prefer VideoQualityManager ID, then name match.
    const VideoSettings vs = VideoQualityManager::instance().current();
    const QList<QCameraDevice> inputs = QMediaDevices::videoInputs();

    QCameraDevice qtDev;
    // 1. ID-based match from VideoQualityManager
    if (!vs.cameraId.isEmpty()) {
        const QByteArray wantId = vs.cameraId.toLatin1();
        for (const QCameraDevice &d : inputs) {
            if (d.id() == wantId) { qtDev = d; break; }
        }
    }
    // 2. Name-based match from MediaDeviceSelectionModel
    if (qtDev.isNull()) {
        MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
        const MediaDevice cam = sel.selectedCamera();
        for (const QCameraDevice &d : inputs) {
            if (!cam.isNull()
                && (d.description().contains(cam.displayName, Qt::CaseInsensitive)
                    || cam.displayName.contains(d.description(), Qt::CaseInsensitive))) {
                qtDev = d;
                break;
            }
        }
    }
    // 3. Fallback: first available
    if (qtDev.isNull() && !inputs.isEmpty())
        qtDev = inputs.first();

    if (qtDev.isNull()) {
        Logger::instance().warn(LogCategory::Media,
            "VideoPanel: preview start failed — no camera device found");
        m_noVideoDeviceAvailable = true;
        applyVideoState();
        return;
    }

    m_previewCamera  = new QCamera(qtDev);
    m_previewSession = new QMediaCaptureSession();
    m_previewSink    = new QVideoSink();

    // Apply configured resolution + fps via camera format
    const QCameraFormat fmt = VideoQualityManager::bestFormat(qtDev, vs.resolution, vs.fps);
    if (!fmt.isNull()) {
        m_previewCamera->setCameraFormat(fmt);
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoPanel: format applied %1x%2 @ %3 fps")
                .arg(fmt.resolution().width())
                .arg(fmt.resolution().height())
                .arg(static_cast<int>(fmt.maxFrameRate())));
    }

    m_previewSession->setCamera(m_previewCamera);
    m_previewSession->setVideoSink(m_previewSink);

    connect(m_previewSink, &QVideoSink::videoFrameChanged,
            this, &VideoPanel::onIdlePreviewFrame, Qt::QueuedConnection);

    m_previewCamera->start();
    m_idlePreviewRunning     = true;
    m_noVideoDeviceAvailable = false;

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoPanel: preview started — camera='%1' in %2 ms")
            .arg(qtDev.description()).arg(startTimer.elapsed()));
    Logger::instance().info(LogCategory::Media, QStringLiteral("Camera acquired"));
    applyVideoState();
}

void VideoPanel::stopIdlePreview()
{
    if (!m_idlePreviewRunning && !m_previewCamera)
        return;

    QElapsedTimer stopTimer;
    stopTimer.start();

    if (m_previewCamera) {
        m_previewCamera->stop();
        delete m_previewCamera;
        m_previewCamera = nullptr;
    }
    delete m_previewSession;
    m_previewSession = nullptr;
    delete m_previewSink;
    m_previewSink = nullptr;

    m_idlePreviewRunning = false;
    m_idlePreviewCapDev  = -3;

    m_localPreview->setPixmap(QPixmap());

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoPanel: preview stopped in %1 ms").arg(stopTimer.elapsed()));
    Logger::instance().info(LogCategory::Media, QStringLiteral("Camera released"));
    applyVideoState();
}

void VideoPanel::refreshIdlePreview()
{
    if (m_videoActive)
        return;
    stopIdlePreview();
    startIdlePreview();
}

void VideoPanel::drawDebugOverlay(QPainter &p)
{
    const VideoSettings vs   = VideoQualityManager::instance().current();
    const float fps          = VideoStatistics::instance().currentFps();
    const int   drops        = VideoStatistics::instance().totalDrops();
    const qint64 capMs       = VideoPipelineMonitor::instance().latencyMs(
                                   VideoPipelineMonitor::Stage::Capture);
    const qint64 rendMs      = VideoPipelineMonitor::instance().latencyMs(
                                   VideoPipelineMonitor::Stage::Render);

    const QString modeStr = m_videoActive
        ? QStringLiteral("Call (PJSIP)")
        : QStringLiteral("Preview (Qt)");

    const QStringList lines{
        QStringLiteral("Mode: %1").arg(modeStr),
        QStringLiteral("FPS: %1").arg(fps, 0, 'f', 1),
        QStringLiteral("Res: %1x%2").arg(vs.resolution.width()).arg(vs.resolution.height()),
        QStringLiteral("Codec: %1").arg(vs.codecOrder.isEmpty() ? QStringLiteral("?") : vs.codecOrder.first()),
        QStringLiteral("Bitrate: %1 kbps").arg(vs.bitrateKbps),
        QStringLiteral("Dropped: %1").arg(drops),
        QStringLiteral("Capture+Render: %1ms").arg(capMs + rendMs),
    };

    p.save();
    p.setFont(QFont(QStringLiteral("Monospace"), 9));

    const int lineH   = 16;
    const int padding = 6;
    const int boxW    = 200;
    const int boxH    = lines.size() * lineH + padding * 2;

    p.fillRect(padding, padding, boxW, boxH, QColor(0, 0, 0, 160));
    p.setPen(Qt::white);
    for (int i = 0; i < lines.size(); ++i)
        p.drawText(padding * 2, padding + lineH * i + lineH - 3, lines[i]);
    p.restore();
}

void VideoPanel::onIdlePreviewFrame(const QVideoFrame &frame)
{
    if (!m_idlePreviewRunning || !m_localPreview || !frame.isValid())
        return;

    VideoPipelineMonitor::instance().stageBegin(VideoPipelineMonitor::Stage::Capture);

    // Throttle to configured fps max (default ~30 fps).
    const int wantFps = qMax(1, VideoQualityManager::instance().current().fps);
    const int throttleMs = 1000 / wantFps;
    if (m_frameThrottle.isValid() && m_frameThrottle.elapsed() < throttleMs) {
        VideoStatistics::instance().frameDrop();
        VideoPipelineMonitor::instance().stageEnd(VideoPipelineMonitor::Stage::Capture);
        return;
    }
    m_frameThrottle.start();

    const QImage img = frame.toImage()
                             .scaled(m_localPreview->size(),
                                     Qt::KeepAspectRatio,
                                     Qt::FastTransformation);
    if (!img.isNull())
        m_localPreview->setPixmap(QPixmap::fromImage(img));

    VideoPipelineMonitor::instance().stageEnd(VideoPipelineMonitor::Stage::Render);
    VideoStatistics::instance().frameProduced();
    update(); // repaint to show overlay if enabled
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void VideoPanel::onVideoMediaConnected()
{
    m_videoActive = true;
    m_noVideoDeviceAvailable = false;
    // Stop Qt Camera: PJSIP's DirectShow capture locks the camera device.
    // PJSIP renders local frames to m_localPreview via our GDI renderer.
    stopIdlePreview();
    populateCameraCombo();
    applyVideoState();

    // Force native window creation before passing handles to PJSIP.
    // winId() on a WA_NativeWindow widget creates the HWND if not yet realised.
    const WId remoteHwnd = winId();
    const WId localHwnd  = m_localPreview->winId();

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoPanel: attaching PJSIP video windows — "
                       "remoteHwnd=0x%1 localHwnd=0x%2")
            .arg(static_cast<quintptr>(remoteHwnd), 0, 16)
            .arg(static_cast<quintptr>(localHwnd), 0, 16));

    QElapsedTimer attachTimer;
    attachTimer.start();
    VideoMediaManager::instance().attachVideoToWidgets(remoteHwnd, localHwnd);
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("VideoPanel: attach local preview done in %1 ms")
            .arg(attachTimer.elapsed()));

    m_remoteAttached = true;
    m_videoRetryTimer.start();
}

void VideoPanel::onVideoMediaDisconnected()
{
    m_videoRetryTimer.stop();
    m_resizeDebounceTimer.stop();
    m_videoActive      = false;
    m_localVideoAvail  = false;
    m_remoteVideoAvail = false;
    m_remoteAttached   = false;
    m_swapped          = false;
    if (m_autoStartIdlePreview)
        startIdlePreview();
    applyVideoState();
}

void VideoPanel::onLocalVideoStarted()
{
    m_localVideoAvail = true;
    applyVideoState();
}

void VideoPanel::onLocalVideoStopped()
{
    m_localVideoAvail = false;
    applyVideoState();
}

void VideoPanel::onRemoteVideoStarted()
{
    m_remoteVideoAvail = true;
    applyVideoState();
    // Attach remote video HWND only if not yet attached. PJSIP creates the
    // incoming render HWND lazily; if the handle was invalid on first attach
    // the retry timer will pick it up. Skip if already attached to avoid
    // redundant SetParent calls that can flicker the video frame.
    if (m_videoActive && !m_remoteAttached) {
        QElapsedTimer t;
        t.start();
        VideoMediaManager::instance().attachVideoToWidgets(winId(), m_localPreview->winId());
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoPanel: attach remote video on start in %1 ms")
                .arg(t.elapsed()));
        m_remoteAttached = true;
    }
}

void VideoPanel::onRemoteVideoStopped()
{
    m_remoteVideoAvail = false;
    applyVideoState();
}

void VideoPanel::onVideoMutedChanged(bool muted)
{
    m_videoMuted = muted;
    QSignalBlocker blocker(m_btnVideoMute);
    m_btnVideoMute->setChecked(muted);
    m_btnVideoMute->setText(muted ? tr("Unmute Video") : tr("Mute Video"));
    update();
}

void VideoPanel::onCallStateChanged()
{
    applyVideoState();
}
