#include "VideoPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "core/Logger.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoMediaManager.h"
#include "sip/SipManager.h"

VideoPanel::VideoPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("VideoPanel");
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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

    m_btnVideoMute = new QPushButton(tr("Mute Video"), m_controlOverlay);
    m_btnVideoMute->setObjectName("VideoMuteBtn");
    m_btnVideoMute->setCheckable(true);
    m_btnVideoMute->setFixedHeight(24);
    m_btnVideoMute->setStyleSheet(
        "QPushButton { background: rgba(0,0,0,160); color: #ffffff; border: 1px solid #555; "
        "border-radius: 3px; padding: 1px 8px; }"
        "QPushButton:checked { background: rgba(180,50,50,180); }");

    overlayLayout->addWidget(m_cameraSelector);
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

    // Retry timer: re-attach PJSIP video windows every 2 s while video is active.
    // PJSIP often returns videoIncomingWindowId=-1 on the first media callback;
    // the window ID becomes valid only after one or more re-negotiations.
    m_videoRetryTimer.setInterval(2000);
    connect(&m_videoRetryTimer, &QTimer::timeout, this, [this]() {
        if (m_videoActive)
            VideoMediaManager::instance().attachVideoToWidgets(winId(), m_localPreview->winId());
    });

    // VideoMediaManager → this
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
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void VideoPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionOverlays();
    if (m_videoActive)
        resizeEmbeddedVideoWindows();
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
        if (!m_swapped) {
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

    if (m_videoActive) {
        // The GDI renderer writes frames directly into this HWND via
        // StretchDIBits on PJSIP's media thread.  Paint only a dark
        // background on the first expose; subsequent GDI frames overwrite it.
        QPainter p(this);
        p.fillRect(rect(), QColor(0x0a, 0x0a, 0x0a));
        return;
    }

    QPainter p(this);
    // Idle — dark background with placeholder crosshair
    p.fillRect(rect(), QColor(0x0d, 0x11, 0x1a));
    p.setPen(QColor(0x2a, 0x35, 0x50));
    p.drawLine(width() / 2, 0, width() / 2, height());
    p.drawLine(0, height() / 2, width(), height() / 2);
    p.setPen(QColor(0x3a, 0x45, 0x70));
    p.setFont(QFont("Sans", 12));
    p.drawText(rect(), Qt::AlignCenter, tr("No video — waiting for call"));
}

// ---------------------------------------------------------------------------
// applyVideoState — sync all overlay visibility / text
// ---------------------------------------------------------------------------

void VideoPanel::applyVideoState()
{
    const bool inCall = m_videoActive || SipManager::instance().callState() != CallState::Idle;

    m_controlOverlay->setVisible(inCall);
    m_btnSwap->setVisible(inCall);

    if (m_videoActive) {
        m_signalIndicator->setText(tr("● VIDEO"));
        m_signalIndicator->setStyleSheet(
            "background: rgba(0,0,0,140); color: #50e050; padding: 4px 8px; border-radius: 4px;");
        // Clear label text: the GDI renderer writes camera frames directly into
        // the local-preview widget HWND; text would overdraw on Qt repaints.
        m_localPreview->setText(QString{});
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

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void VideoPanel::onVideoMediaConnected()
{
    m_videoActive = true;
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

    VideoMediaManager::instance().attachVideoToWidgets(remoteHwnd, localHwnd);
    m_videoRetryTimer.start();
}

void VideoPanel::onVideoMediaDisconnected()
{
    m_videoRetryTimer.stop();
    m_videoActive      = false;
    m_localVideoAvail  = false;
    m_remoteVideoAvail = false;
    m_swapped          = false;
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
    // Re-attach on every media re-negotiation. PJSIP creates the incoming
    // render HWND lazily (may be PJSUA_INVALID_ID on the first callback and
    // become valid only after a subsequent UPDATE/re-INVITE). attachVideoToWidgets
    // is idempotent: it skips remote if winId==-1, and skips preview if already
    // embedded (SetParent on an already-parented child is a no-op).
    if (m_videoActive)
        VideoMediaManager::instance().attachVideoToWidgets(winId(), m_localPreview->winId());
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
