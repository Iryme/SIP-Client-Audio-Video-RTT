#include "CallInfoModel.h"

CallInfoModel::CallInfoModel(QObject *parent)
    : QObject(parent)
{
}

void CallInfoModel::setState(CallState state, const QString &statusText)
{
    m_state = state;
    m_statusText = statusText;
    emit callInfoChanged();
}

void CallInfoModel::setRemoteUri(const QString &uri)
{
    m_remoteUri = uri;
    emit callInfoChanged();
}

void CallInfoModel::setDisplayName(const QString &name)
{
    m_displayName = name;
    emit callInfoChanged();
}

void CallInfoModel::setPresenceText(const QString &text)
{
    m_presenceText = text;
    emit callInfoChanged();
}

void CallInfoModel::setDurationSeconds(int seconds)
{
    m_durationSeconds = seconds;
    emit callInfoChanged();
}

void CallInfoModel::setMuted(bool muted)
{
    m_muted = muted;
    emit callInfoChanged();
}

void CallInfoModel::setHeld(bool held)
{
    m_held = held;
    emit callInfoChanged();
}

void CallInfoModel::setVideoConnected(bool connected)
{
    m_videoConnected = connected;
    emit callInfoChanged();
}

void CallInfoModel::setVideoRequested(bool requested)
{
    m_videoRequested = requested;
    emit callInfoChanged();
}

void CallInfoModel::setRttConnected(bool connected)
{
    m_rttConnected = connected;
    emit callInfoChanged();
}

void CallInfoModel::setRttRequested(bool requested)
{
    m_rttRequested = requested;
    emit callInfoChanged();
}

void CallInfoModel::setSelectedMedia(const CallMediaOptions &options)
{
    m_selectedMedia = options;
    emit callInfoChanged();
}

void CallInfoModel::setNegotiatedAudio(const AudioCodecInfo &info)
{
    m_negotiatedAudio = info;
    emit callInfoChanged();
}

void CallInfoModel::setNegotiatedVideo(const VideoCodecInfo &info)
{
    m_negotiatedVideo = info;
    emit callInfoChanged();
}

void CallInfoModel::setRtpStats(const RtpStatsSnapshot &stats)
{
    m_rtpStats = stats;
    emit callInfoChanged();
}

void CallInfoModel::setVideoStats(float fps, int dropsPerSecond)
{
    m_videoFps = fps;
    m_videoDropsPerSecond = dropsPerSecond;
    emit callInfoChanged();
}

void CallInfoModel::setDeviceNames(const QString &mic, const QString &speaker, const QString &camera)
{
    m_micDeviceName = mic;
    m_speakerDeviceName = speaker;
    m_cameraDeviceName = camera;
    emit callInfoChanged();
}

void CallInfoModel::reset()
{
    m_state = CallState::Idle;
    m_statusText.clear();
    m_remoteUri.clear();
    m_displayName.clear();
    m_presenceText.clear();
    m_durationSeconds = 0;
    m_muted = false;
    m_held = false;
    m_videoConnected = false;
    m_videoRequested = false;
    m_rttConnected = false;
    m_rttRequested = false;
    m_selectedMedia = CallMediaOptions();
    m_negotiatedAudio = AudioCodecInfo();
    m_negotiatedVideo = VideoCodecInfo();
    m_rtpStats = RtpStatsSnapshot();
    m_videoFps = 0.0f;
    m_videoDropsPerSecond = 0;
    m_micDeviceName.clear();
    m_speakerDeviceName.clear();
    m_cameraDeviceName.clear();
    emit callInfoChanged();
}
