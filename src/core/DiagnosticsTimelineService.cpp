#include "DiagnosticsTimelineService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

#include "gui/CameraController.h"
#include "media/AudioMediaManager.h"
#include "media/RtpStats.h"
#include "media/VideoMediaManager.h"
#include "sip/SipManager.h"

namespace {
// Packet loss / jitter thresholds above which RTP quality is considered
// degraded for timeline purposes. Chosen as "clearly audible" thresholds,
// not tied to any specific codec.
constexpr double kLossWarnPercent = 5.0;
constexpr double kJitterWarnMs = 50.0;
}

DiagnosticsTimelineService &DiagnosticsTimelineService::instance()
{
    static DiagnosticsTimelineService s_instance;
    return s_instance;
}

DiagnosticsTimelineModel *DiagnosticsTimelineService::model()
{
    return &m_model;
}

QString DiagnosticsTimelineService::persistedFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("timeline.json"));
}

DiagnosticsTimelineService::DiagnosticsTimelineService()
{
    loadPersisted();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(3000);
    connect(&m_saveTimer, &QTimer::timeout, this, &DiagnosticsTimelineService::savePersisted);

    auto &sip = SipManager::instance();
    connect(&sip, &SipManager::registrationStateChanged, this, &DiagnosticsTimelineService::onRegistrationStateChanged);
    connect(&sip, &SipManager::callStateChanged,         this, &DiagnosticsTimelineService::onCallStateChanged);
    connect(&sip, &SipManager::incomingCall,             this, &DiagnosticsTimelineService::onIncomingCall);
    connect(&sip, &SipManager::callConnected,            this, &DiagnosticsTimelineService::onCallConnected);
    connect(&sip, &SipManager::callDisconnected,         this, &DiagnosticsTimelineService::onCallDisconnected);
    connect(&sip, &SipManager::callFailed,               this, &DiagnosticsTimelineService::onCallFailed);
    connect(&sip, &SipManager::profileSwitchStarted,     this, &DiagnosticsTimelineService::onProfileSwitchStarted);
    connect(&sip, &SipManager::profileSwitchCompleted,   this, &DiagnosticsTimelineService::onProfileSwitchCompleted);
    connect(&sip, &SipManager::profileSwitchFailed,      this, &DiagnosticsTimelineService::onProfileSwitchFailed);
    // Call-level media up/down — "Media" category. AudioMediaManager /
    // VideoMediaManager below cover mute + local/remote video state instead,
    // so the same underlying SipCall event is never turned into two entries.
    connect(&sip, &SipManager::audioMediaConnected,      this, &DiagnosticsTimelineService::onAudioMediaConnected);
    connect(&sip, &SipManager::audioMediaDisconnected,   this, &DiagnosticsTimelineService::onAudioMediaDisconnected);
    connect(&sip, &SipManager::videoMediaConnected,      this, &DiagnosticsTimelineService::onVideoMediaConnected);
    connect(&sip, &SipManager::videoMediaDisconnected,   this, &DiagnosticsTimelineService::onVideoMediaDisconnected);
    connect(&sip, &SipManager::rtpStatsChanged,          this, &DiagnosticsTimelineService::onRtpStatsChanged);
    connect(&sip, &SipManager::initialized,              this, &DiagnosticsTimelineService::onSipInitialized);
    connect(&sip, &SipManager::initializationFailed,     this, &DiagnosticsTimelineService::onSipInitializationFailed);
    connect(sip.rttSession(), &RttSession::rttStateChanged, this, &DiagnosticsTimelineService::onRttStateChanged);

    auto &audio = AudioMediaManager::instance();
    connect(&audio, &AudioMediaManager::mutedChanged,               this, &DiagnosticsTimelineService::onAudioMuted);
    connect(&audio, &AudioMediaManager::audioDeviceSelectionChanged, this, &DiagnosticsTimelineService::onAudioDeviceSelectionChanged);

    auto &video = VideoMediaManager::instance();
    connect(&video, &VideoMediaManager::videoMutedChanged,   this, &DiagnosticsTimelineService::onVideoMuted);
    connect(&video, &VideoMediaManager::localVideoStarted,   this, &DiagnosticsTimelineService::onLocalVideoStarted);
    connect(&video, &VideoMediaManager::localVideoStopped,   this, &DiagnosticsTimelineService::onLocalVideoStopped);
    connect(&video, &VideoMediaManager::remoteVideoStarted,  this, &DiagnosticsTimelineService::onRemoteVideoStarted);
    connect(&video, &VideoMediaManager::remoteVideoStopped,  this, &DiagnosticsTimelineService::onRemoteVideoStopped);
    connect(&video, &VideoMediaManager::cameraChanged,       this, &DiagnosticsTimelineService::onCameraChanged);

    connect(&CameraController::instance(), &CameraController::enabledChanged,
            this, &DiagnosticsTimelineService::onCameraEnabledChanged);

    connect(&Logger::instance(), &Logger::entryAdded, this, &DiagnosticsTimelineService::onLogEntryAdded);
}

void DiagnosticsTimelineService::record(TimelineCategory category, TimelineSeverity severity,
                                        const QString &title, const QString &details,
                                        const QString &remoteUri, int sipCode,
                                        const QString &profileId)
{
    DiagnosticsTimelineEntry e;
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.timestamp = QDateTime::currentDateTime();
    e.category = category;
    e.severity = severity;
    e.title = title;
    e.details = details;
    e.remoteUri = remoteUri;
    e.sipCode = sipCode;
    e.profileId = profileId;
    e.colorHint = timelineSeverityColor(severity);
    e.iconHint = timelineCategoryName(category).left(4).toUpper();

    m_model.append(e);

    if (!m_saveTimer.isActive())
        m_saveTimer.start();
}

void DiagnosticsTimelineService::onRegistrationStateChanged(RegistrationState state, const QString &statusText, int statusCode)
{
    TimelineSeverity severity = TimelineSeverity::Info;
    if (state == RegistrationState::Registered)             severity = TimelineSeverity::Success;
    else if (state == RegistrationState::RegistrationFailed) severity = TimelineSeverity::Error;

    record(TimelineCategory::Registration, severity,
           tr("Registration: %1").arg(registrationStateName(state)), statusText, {}, statusCode);
}

void DiagnosticsTimelineService::onCallStateChanged(CallState state, const QString &statusText, int statusCode)
{
    TimelineSeverity severity = TimelineSeverity::Info;
    if (state == CallState::Active)      severity = TimelineSeverity::Success;
    else if (state == CallState::Failed) severity = TimelineSeverity::Error;

    record(TimelineCategory::Call, severity,
           tr("Call: %1").arg(callStateName(state)), statusText, {}, statusCode);
}

void DiagnosticsTimelineService::onIncomingCall(const QString &remoteUri)
{
    record(TimelineCategory::Call, TimelineSeverity::Info, tr("Incoming call"), {}, remoteUri);
}

void DiagnosticsTimelineService::onCallConnected(const QString &remoteUri)
{
    record(TimelineCategory::Call, TimelineSeverity::Success, tr("Call connected"), {}, remoteUri);
}

void DiagnosticsTimelineService::onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode)
{
    record(TimelineCategory::Call, TimelineSeverity::Info, tr("Call disconnected"), reason, remoteUri, statusCode);
}

void DiagnosticsTimelineService::onCallFailed(const QString &remoteUri, const QString &reason, int statusCode)
{
    record(TimelineCategory::Call, TimelineSeverity::Error, tr("Call failed"), reason, remoteUri, statusCode);
}

void DiagnosticsTimelineService::onProfileSwitchStarted(const QString &profileId)
{
    record(TimelineCategory::Sip, TimelineSeverity::Info, tr("Profile switch started"), {}, {}, 0, profileId);
}

void DiagnosticsTimelineService::onProfileSwitchCompleted(const QString &profileId)
{
    record(TimelineCategory::Sip, TimelineSeverity::Success, tr("Profile switch completed"), {}, {}, 0, profileId);
}

void DiagnosticsTimelineService::onProfileSwitchFailed(const QString &profileId, const QString &reason)
{
    record(TimelineCategory::Sip, TimelineSeverity::Error, tr("Profile switch failed"), reason, {}, 0, profileId);
}

void DiagnosticsTimelineService::onAudioMediaConnected()
{
    record(TimelineCategory::Media, TimelineSeverity::Success, tr("Audio media connected"));
}

void DiagnosticsTimelineService::onAudioMediaDisconnected()
{
    record(TimelineCategory::Media, TimelineSeverity::Info, tr("Audio media disconnected"));
}

void DiagnosticsTimelineService::onVideoMediaConnected()
{
    record(TimelineCategory::Media, TimelineSeverity::Success, tr("Video media connected"));
}

void DiagnosticsTimelineService::onVideoMediaDisconnected()
{
    record(TimelineCategory::Media, TimelineSeverity::Info, tr("Video media disconnected"));
}

void DiagnosticsTimelineService::onAudioMuted(bool muted)
{
    record(TimelineCategory::Audio, TimelineSeverity::Info,
           muted ? tr("Microphone muted") : tr("Microphone unmuted"));
}

void DiagnosticsTimelineService::onAudioDeviceSelectionChanged()
{
    record(TimelineCategory::Audio, TimelineSeverity::Info, tr("Audio device selection changed"));
}

void DiagnosticsTimelineService::onVideoMuted(bool muted)
{
    record(TimelineCategory::Video, TimelineSeverity::Info,
           muted ? tr("Video muted") : tr("Video unmuted"));
}

void DiagnosticsTimelineService::onLocalVideoStarted()
{
    record(TimelineCategory::Video, TimelineSeverity::Info, tr("Local video started"));
}

void DiagnosticsTimelineService::onLocalVideoStopped()
{
    record(TimelineCategory::Video, TimelineSeverity::Info, tr("Local video stopped"));
}

void DiagnosticsTimelineService::onRemoteVideoStarted()
{
    record(TimelineCategory::Video, TimelineSeverity::Info, tr("Remote video started"));
}

void DiagnosticsTimelineService::onRemoteVideoStopped()
{
    record(TimelineCategory::Video, TimelineSeverity::Info, tr("Remote video stopped"));
}

void DiagnosticsTimelineService::onCameraChanged(const QString &deviceId)
{
    record(TimelineCategory::Video, TimelineSeverity::Info, tr("Camera device changed"), deviceId);
}

void DiagnosticsTimelineService::onCameraEnabledChanged(bool enabled)
{
    record(TimelineCategory::Camera, TimelineSeverity::Info,
           enabled ? tr("Camera enabled") : tr("Camera disabled"));
}

void DiagnosticsTimelineService::onRttStateChanged(RttState state)
{
    TimelineSeverity severity = TimelineSeverity::Info;
    if (state == RttState::Active)      severity = TimelineSeverity::Success;
    else if (state == RttState::Failed) severity = TimelineSeverity::Error;

    record(TimelineCategory::Rtt, severity, tr("RTT: %1").arg(rttStateName(state)));
}

void DiagnosticsTimelineService::onRtpStatsChanged(const RtpStatsSnapshot &stats)
{
    const bool bad = (stats.packetLossAvailable && stats.packetLossPercent > kLossWarnPercent) ||
                      (stats.jitterAvailable && stats.jitterMs > kJitterWarnMs);

    // Only append on a state transition (OK -> degraded or back), never on
    // every stats tick — rtpStatsChanged fires ~1 Hz during a call and would
    // otherwise flood the timeline with duplicate information already
    // visible in the RTP tab.
    if (bad && !m_rtpWarned) {
        m_rtpWarned = true;
        record(TimelineCategory::Rtp, TimelineSeverity::Warning, tr("RTP quality degraded"),
               tr("loss=%1%% jitter=%2ms").arg(stats.packetLossPercent, 0, 'f', 1).arg(stats.jitterMs, 0, 'f', 1));
    } else if (!bad && m_rtpWarned) {
        m_rtpWarned = false;
        record(TimelineCategory::Rtp, TimelineSeverity::Success, tr("RTP quality recovered"));
    }
}

void DiagnosticsTimelineService::onSipInitialized()
{
    record(TimelineCategory::System, TimelineSeverity::Success, tr("SIP backend initialized"));
}

void DiagnosticsTimelineService::onSipInitializationFailed(const QString &reason)
{
    record(TimelineCategory::System, TimelineSeverity::Error, tr("SIP backend initialization failed"), reason);
}

void DiagnosticsTimelineService::onLogEntryAdded(const LogEntry &entry)
{
    // Debug/Raw and per-packet perf chatter are already fully visible in the
    // Logs viewer and SIP Ladder — never diagnostically useful as individual
    // timeline rows, and would drown out everything else.
    if (entry.category == LogCategory::Perf)
        return;
    if (entry.level == LogLevel::Debug || entry.level == LogLevel::Raw)
        return;

    const QString &msg = entry.message;
    if (msg.startsWith(QLatin1String("TX ")) || msg.startsWith(QLatin1String("RX ")) ||
        msg.contains(QLatin1String("keepalive"), Qt::CaseInsensitive))
        return;

    TimelineCategory category;
    switch (entry.category) {
    case LogCategory::Sip:
    case LogCategory::Sdp:
        category = TimelineCategory::Sip;
        break;
    case LogCategory::Media:
        category = TimelineCategory::Media;
        break;
    case LogCategory::Rtt:
        category = TimelineCategory::Rtt;
        break;
    case LogCategory::App:
        category = TimelineCategory::System;
        break;
    default:
        // Platform/Lmpe/Etsi and anything else without a dedicated
        // subsystem category: only surface as a Warning/Error timeline
        // entry, bucketed by severity rather than by (rarely-relevant)
        // source category. Info-level entries here are routine chatter.
        if (entry.level == LogLevel::Error)      category = TimelineCategory::Error;
        else if (entry.level == LogLevel::Warn)  category = TimelineCategory::Warning;
        else return;
        break;
    }

    const TimelineSeverity severity = entry.level == LogLevel::Error ? TimelineSeverity::Error
                                     : entry.level == LogLevel::Warn  ? TimelineSeverity::Warning
                                                                      : TimelineSeverity::Info;

    // Info-level entries in categories that already have a richer dedicated
    // signal source above (Sip/Media/Rtt/System) are only forwarded when
    // they don't look like routine chatter, to avoid duplicating what those
    // dedicated signals already recorded.
    if (severity == TimelineSeverity::Info) {
        if (entry.category == LogCategory::Sip &&
            msg.contains(QLatin1String("suppressed"), Qt::CaseInsensitive))
            return;
        if (entry.category == LogCategory::Media &&
            (msg.contains(QLatin1String("frame"), Qt::CaseInsensitive) ||
             msg.contains(QLatin1String("retry"), Qt::CaseInsensitive)))
            return;
    }

    record(category, severity, msg, entry.payload);
}

void DiagnosticsTimelineService::loadPersisted()
{
    QFile f(persistedFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;

    const QJsonArray arr = doc.object().value(QStringLiteral("entries")).toArray();
    QList<DiagnosticsTimelineEntry> entries;
    entries.reserve(arr.size());
    for (const QJsonValue &v : arr)
        entries.append(DiagnosticsTimelineEntry::fromJson(v.toObject()));

    m_model.reset(entries);
}

void DiagnosticsTimelineService::savePersisted()
{
    const QString path = persistedFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QJsonArray arr;
    for (const auto &e : m_model.entries())
        arr.append(e.toJson());

    QJsonObject root;
    root.insert(QStringLiteral("entries"), arr);
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}
