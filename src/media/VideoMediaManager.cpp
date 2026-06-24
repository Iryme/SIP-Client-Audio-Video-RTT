#include "VideoMediaManager.h"

#include "core/Logger.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "sip/SipCall.h"

// ---------------------------------------------------------------------------
// VideoMediaManager
// ---------------------------------------------------------------------------

VideoMediaManager &VideoMediaManager::instance()
{
    static VideoMediaManager s;
    return s;
}

VideoMediaManager::VideoMediaManager() : QObject(nullptr) {}

// ---------------------------------------------------------------------------
// Call attachment
// ---------------------------------------------------------------------------

void VideoMediaManager::attachCall(SipCall *call)
{
    detachCall(); // idempotent — disconnects any existing wiring
    if (!call)
        return;

    m_call = call;

    connect(call, &SipCall::videoMediaConnected,
            this, &VideoMediaManager::onCallVideoMediaConnected);
    connect(call, &SipCall::videoMediaDisconnected,
            this, &VideoMediaManager::onCallVideoMediaDisconnected);
    connect(call, &SipCall::localVideoStarted,
            this, &VideoMediaManager::onCallLocalVideoStarted);
    connect(call, &SipCall::localVideoStopped,
            this, &VideoMediaManager::onCallLocalVideoStopped);
    connect(call, &SipCall::remoteVideoStarted,
            this, &VideoMediaManager::onCallRemoteVideoStarted);
    connect(call, &SipCall::remoteVideoStopped,
            this, &VideoMediaManager::onCallRemoteVideoStopped);
    connect(call, &SipCall::videoMuteChanged,
            this, &VideoMediaManager::onCallVideoMuteChanged);
    connect(call, &SipCall::callDisconnected,
            this, &VideoMediaManager::onCallDisconnected);
    connect(call, &SipCall::callFailed,
            this, &VideoMediaManager::onCallFailed);

    // Sync current video-mute state to the newly attached call.
    if (m_videoMuted)
        call->setVideoMuted(true);
}

void VideoMediaManager::detachCall()
{
    // Disconnect signals even if QPointer is still valid.
    if (m_call)
        disconnect(m_call, nullptr, this, nullptr);
    m_call.clear();

    // Always reset video state — the SipCall object may already have been
    // destroyed (QPointer → null) before detachCall() was called.
    if (m_videoActive) {
        m_videoActive          = false;
        m_localVideoAvailable  = false;
        m_remoteVideoAvailable = false;
        emit localVideoStopped();
        emit remoteVideoStopped();
        emit videoMediaDisconnected();
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Video media disconnected (call detached)"));
    }
}

// ---------------------------------------------------------------------------
// Video mute
// ---------------------------------------------------------------------------

void VideoMediaManager::setVideoMuted(bool muted)
{
    if (m_videoMuted == muted)
        return;
    m_videoMuted = muted;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Video mute %1")
            .arg(muted ? QStringLiteral("enabled") : QStringLiteral("disabled")));
    if (m_call)
        m_call->setVideoMuted(muted);
    else
        emit videoMutedChanged(muted);
    // videoMuteChanged is forwarded from SipCall::videoMuteChanged via onCallVideoMuteChanged.
    // Emit directly only when there is no active call.
}

bool VideoMediaManager::isVideoMuted()           const { return m_videoMuted; }
bool VideoMediaManager::isVideoActive()          const { return m_videoActive; }
bool VideoMediaManager::isLocalVideoAvailable()  const { return m_localVideoAvailable; }
bool VideoMediaManager::isRemoteVideoAvailable() const { return m_remoteVideoAvailable; }

// ---------------------------------------------------------------------------
// Camera selection
// ---------------------------------------------------------------------------

void VideoMediaManager::attachVideoToWidgets(WId remoteWidget, WId localPreviewWidget)
{
    if (!m_call) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("attachVideoToWidgets: no active call"));
        return;
    }
    m_call->attachVideoWindows(remoteWidget, localPreviewWidget);
}

void VideoMediaManager::setCamera(const QString &deviceId)
{
    const MediaDevice dev = MediaDeviceManager::instance().findDevice(
        MediaDeviceType::Camera, deviceId);
    if (dev.isNull()) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("setCamera: device id '%1' not found").arg(deviceId));
        return;
    }

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Camera selected: %1").arg(dev.displayName));

    // Persist via a local selection model instance (same QSettings keys as MediaPanel).
    MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    sel.selectCamera(deviceId);

    if (m_videoActive) {
        // PJSIP video device re-initialisation not yet implemented.
        // The selection is persisted and will take effect on the next call.
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Camera changed during active call: %1 "
                           "(PJSIP hot-swap scaffolded; takes effect on next call)")
                .arg(dev.displayName));
    }
}

// ---------------------------------------------------------------------------
// Private slots — forward from SipCall signals
// ---------------------------------------------------------------------------

void VideoMediaManager::onCallVideoMediaConnected()
{
    if (m_videoActive)
        return;
    m_videoActive = true;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Video media connected"));
    emit videoMediaConnected();
}

void VideoMediaManager::onCallVideoMediaDisconnected()
{
    if (!m_videoActive)
        return;
    m_videoActive          = false;
    m_localVideoAvailable  = false;
    m_remoteVideoAvailable = false;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Video media disconnected"));
    emit videoMediaDisconnected();
}

void VideoMediaManager::onCallLocalVideoStarted()
{
    m_localVideoAvailable = true;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Local video started"));
    emit localVideoStarted();
}

void VideoMediaManager::onCallLocalVideoStopped()
{
    m_localVideoAvailable = false;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Local video stopped"));
    emit localVideoStopped();
}

void VideoMediaManager::onCallRemoteVideoStarted()
{
    m_remoteVideoAvailable = true;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Remote video started"));
    emit remoteVideoStarted();
}

void VideoMediaManager::onCallRemoteVideoStopped()
{
    m_remoteVideoAvailable = false;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Remote video stopped"));
    emit remoteVideoStopped();
}

void VideoMediaManager::onCallVideoMuteChanged(bool muted)
{
    m_videoMuted = muted;
    emit videoMutedChanged(muted);
}

void VideoMediaManager::onCallDisconnected(const QString &remoteUri,
                                           const QString &reason,
                                           int statusCode)
{
    Q_UNUSED(remoteUri)
    Q_UNUSED(reason)
    Q_UNUSED(statusCode)
    // Cleanup is driven by onCallVideoMediaDisconnected + detachCall().
}

void VideoMediaManager::onCallFailed(const QString &remoteUri,
                                     const QString &reason,
                                     int statusCode)
{
    Q_UNUSED(remoteUri)
    Q_UNUSED(reason)
    Q_UNUSED(statusCode)
}
