#include "AudioMediaManager.h"

#include "core/Logger.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "sip/SipCall.h"

// ---------------------------------------------------------------------------
// AudioMediaManager
// ---------------------------------------------------------------------------

AudioMediaManager &AudioMediaManager::instance()
{
    static AudioMediaManager s;
    return s;
}

AudioMediaManager::AudioMediaManager() : QObject(nullptr) {}

// ---------------------------------------------------------------------------
// Call attachment
// ---------------------------------------------------------------------------

void AudioMediaManager::attachCall(SipCall *call)
{
    detachCall(); // idempotent — disconnects any existing wiring
    if (!call)
        return;

    m_call = call;

    connect(call, &SipCall::callConnected,
            this, &AudioMediaManager::onCallConnected);
    connect(call, &SipCall::callDisconnected,
            this, &AudioMediaManager::onCallDisconnected);
    connect(call, &SipCall::callFailed,
            this, &AudioMediaManager::onCallFailed);
    connect(call, &SipCall::audioMediaConnected,
            this, &AudioMediaManager::onCallAudioMediaConnected);
    connect(call, &SipCall::audioMediaDisconnected,
            this, &AudioMediaManager::onCallAudioMediaDisconnected);
    connect(call, &SipCall::muteChanged,
            this, &AudioMediaManager::onCallMuteChanged);
    connect(call, &SipCall::inputLevelChanged,
            this, &AudioMediaManager::onCallInputLevel);
    connect(call, &SipCall::outputLevelChanged,
            this, &AudioMediaManager::onCallOutputLevel);

    // Sync mute state to the newly attached call.
    if (m_muted)
        call->setMuted(true);
}

void AudioMediaManager::detachCall()
{
    // Disconnect signals even if QPointer is still valid.
    if (m_call)
        disconnect(m_call, nullptr, this, nullptr);
    m_call.clear();

    // Always reset media state — the SipCall object may already have been
    // destroyed (QPointer → null) before detachCall() was called.
    if (m_mediaActive) {
        m_mediaActive = false;
        m_inputLevel  = 0;
        m_outputLevel = 0;
        emit inputLevelChanged(0);
        emit outputLevelChanged(0);
        emit mediaDisconnected();
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Audio media disconnected (call detached)"));
    }
}

// ---------------------------------------------------------------------------
// Mute
// ---------------------------------------------------------------------------

void AudioMediaManager::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    m_muted = muted;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Mute %1").arg(muted ? QStringLiteral("enabled") : QStringLiteral("disabled")));
    if (m_call)
        m_call->setMuted(muted);
    else
        emit mutedChanged(muted);
    // muteChanged is forwarded from SipCall::muteChanged via onCallMuteChanged.
    // Emit directly only when there is no active call.
}

bool AudioMediaManager::isMuted()      const { return m_muted; }
bool AudioMediaManager::isMediaActive() const { return m_mediaActive; }
int  AudioMediaManager::inputLevel()   const { return m_inputLevel; }
int  AudioMediaManager::outputLevel()  const { return m_outputLevel; }

// ---------------------------------------------------------------------------
// Device selection
// ---------------------------------------------------------------------------

void AudioMediaManager::setMicrophone(const QString &deviceId)
{
    const MediaDevice dev = MediaDeviceManager::instance().findDevice(
        MediaDeviceType::Microphone, deviceId);
    if (dev.isNull()) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("setMicrophone: device id '%1' not found").arg(deviceId));
        return;
    }

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Microphone selected: %1").arg(dev.displayName));

    // Persist via a local selection model instance (same QSettings keys as MediaPanel).
    MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    sel.selectMicrophone(deviceId);

    if (m_mediaActive)
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Microphone changed during active call: %1 "
                           "(takes effect on next call)")
                .arg(dev.displayName));

    emit audioDeviceSelectionChanged();
}

void AudioMediaManager::setSpeaker(const QString &deviceId)
{
    const MediaDevice dev = MediaDeviceManager::instance().findDevice(
        MediaDeviceType::Speaker, deviceId);
    if (dev.isNull()) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("setSpeaker: device id '%1' not found").arg(deviceId));
        return;
    }

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Speaker selected: %1").arg(dev.displayName));

    MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    sel.selectSpeaker(deviceId);

    if (m_mediaActive)
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Speaker changed during active call: %1 "
                           "(takes effect on next call)")
                .arg(dev.displayName));

    emit audioDeviceSelectionChanged();
}

// ---------------------------------------------------------------------------
// Private slots — forward from SipCall signals
// ---------------------------------------------------------------------------

void AudioMediaManager::onCallAudioMediaConnected()
{
    if (m_mediaActive)
        return;
    m_mediaActive = true;
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Audio media connected"));
    emit mediaConnected();
}

void AudioMediaManager::onCallAudioMediaDisconnected()
{
    if (!m_mediaActive)
        return;
    m_mediaActive = false;
    m_inputLevel  = 0;
    m_outputLevel = 0;
    emit inputLevelChanged(0);
    emit outputLevelChanged(0);
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Audio media disconnected"));
    emit mediaDisconnected();
}

void AudioMediaManager::onCallMuteChanged(bool muted)
{
    m_muted = muted;
    emit mutedChanged(muted);
}

void AudioMediaManager::onCallInputLevel(int level)
{
    if (m_inputLevel == level)
        return;
    m_inputLevel = level;
    emit inputLevelChanged(level);
}

void AudioMediaManager::onCallOutputLevel(int level)
{
    if (m_outputLevel == level)
        return;
    m_outputLevel = level;
    emit outputLevelChanged(level);
}

void AudioMediaManager::onCallConnected(const QString &remoteUri)
{
    Q_UNUSED(remoteUri)
}

void AudioMediaManager::onCallDisconnected(const QString &remoteUri,
                                           const QString &reason,
                                           int statusCode)
{
    Q_UNUSED(remoteUri)
    Q_UNUSED(reason)
    Q_UNUSED(statusCode)
    // Cleanup is driven by onCallAudioMediaDisconnected + detachCall().
}

void AudioMediaManager::onCallFailed(const QString &remoteUri,
                                     const QString &reason,
                                     int statusCode)
{
    Q_UNUSED(remoteUri)
    Q_UNUSED(reason)
    Q_UNUSED(statusCode)
}
