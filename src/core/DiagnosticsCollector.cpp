#include "DiagnosticsCollector.h"

#include <QCoreApplication>
#include <QSysInfo>

#include "AppVersion.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"
#include "media/AudioMediaManager.h"
#include "media/VideoMediaManager.h"
#include "media/MediaDeviceManager.h"
#include "media/VideoQualityManager.h"
#include "sip/PjsipAudioMapper.h"
#include "gui/CameraController.h"
#include "rtt/RttSession.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#endif

DiagnosticsCollector &DiagnosticsCollector::instance()
{
    static DiagnosticsCollector s_instance;
    return s_instance;
}

DiagnosticsCollector::DiagnosticsCollector()
{
    // 1 Hz refresh is enough for a diagnostics dashboard and keeps this well
    // away from "query PJSIP on every repaint" — the UI never drives this timer.
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &DiagnosticsCollector::rebuild);
    m_timer.start();

    // Rebuild immediately (instead of waiting for the first tick) on the
    // state-change signals that matter most for a "just refreshed my view" feel.
    auto &sip = SipManager::instance();
    connect(&sip, &SipManager::registrationStateChanged, this, &DiagnosticsCollector::rebuild);
    connect(&sip, &SipManager::callStateChanged,          this, &DiagnosticsCollector::rebuild);
    connect(&sip, &SipManager::rtpStatsChanged,           this, &DiagnosticsCollector::rebuild);
    connect(&sip, &SipManager::callMuteChanged,           this, &DiagnosticsCollector::rebuild);
    connect(&sip, &SipManager::callVideoMuteChanged,      this, &DiagnosticsCollector::rebuild);
    connect(&sip, &SipManager::audioMediaConnected,       this, &DiagnosticsCollector::rebuild);
    connect(&sip, &SipManager::audioMediaDisconnected,    this, &DiagnosticsCollector::rebuild);
    connect(&sip, &SipManager::videoMediaConnected,       this, &DiagnosticsCollector::rebuild);
    connect(&sip, &SipManager::videoMediaDisconnected,    this, &DiagnosticsCollector::rebuild);

    rebuild();
}

DiagnosticsSnapshot DiagnosticsCollector::snapshot() const
{
    return m_snapshot;
}

void DiagnosticsCollector::refreshNow()
{
    rebuild();
}

void DiagnosticsCollector::rebuild()
{
    DiagnosticsSnapshot s;
    s.capturedAtUtc = QDateTime::currentDateTimeUtc();

    auto &sip = SipManager::instance();
    auto &profiles = SipProfileManager::instance();
    auto &audio = AudioMediaManager::instance();
    auto &video = VideoMediaManager::instance();
    auto &devices = MediaDeviceManager::instance();
    auto &camera = CameraController::instance();
    const VideoSettings videoSettings = VideoQualityManager::instance().current();

    // ---- Registration / call ------------------------------------------
    s.registrationState = registrationStateName(sip.registrationState());
    s.registrationStatusText = sip.registrationStatusText();
    s.registrationStatusCode = sip.registrationStatusCode();
    s.callState = callStateName(sip.callState());
    s.callStatusText = sip.callStatusText();
    s.remoteUri = sip.activeCallRemoteUri();
    s.lastSipError = sip.lastError();

    // callStatusCode is not separately exposed by SipManager; reuse the
    // registration status code when no call is active, otherwise leave 0
    // (SipManager only forwards a code via callFailed/callDisconnected signals,
    // not through a persistent getter).
    s.callStatusCode = 0;

    if (sip.callState() != CallState::Idle) {
        s.lastSipResponse = s.callStatusCode > 0
            ? QStringLiteral("%1 %2").arg(s.callStatusCode).arg(s.callStatusText)
            : s.callStatusText;
    } else if (s.registrationStatusCode > 0) {
        s.lastSipResponse = QStringLiteral("%1 %2").arg(s.registrationStatusCode).arg(s.registrationStatusText);
    } else {
        s.lastSipResponse = s.registrationStatusText;
    }

    if (profiles.hasActiveProfile()) {
        const SipProfile p = profiles.activeProfile();
        s.currentProfileName = p.displayName;
        s.registrar = p.registrar;
        s.outboundProxy = p.outboundProxy;
        s.transport = SipProfileManager::transportToString(p.transport);
    }

    // Call-ID / dialog state of the active call (empty → N/A when idle).
    const QString sipCallId = sip.activeCallSipId();
    s.callId = sipCallId.isEmpty() ? diagnosticsNotAvailable() : sipCallId;
    const QString dialogState = sip.activeCallDialogState();
    s.dialogState = dialogState.isEmpty() ? diagnosticsNotAvailable() : dialogState;

    // ---- RTP / media stats ---------------------------------------------
    const RtpStatsSnapshot rtp = sip.currentRtpStats();
    s.audioJitterAvailable = rtp.jitterAvailable;
    s.audioJitterMs = rtp.jitterMs;
    s.audioLossAvailable = rtp.packetLossAvailable;
    s.audioLossPercent = rtp.packetLossPercent;
    s.audioRttAvailable = rtp.rttAvailable;
    s.audioRttMs = rtp.rttMs;
    s.audioPacketsRxAvailable = rtp.available;
    s.audioPacketsRx = rtp.packetReceivedPackets;
    if (rtp.packetsTxAvailable)
        s.audioPacketsTx = QString::number(rtp.packetsTx);

    s.audioCodec = sip.activeAudioCodecInfo();

    s.audioConnected = audio.isMediaActive();
    s.audioMuted = audio.isMuted();
    s.microphoneVolume = audio.microphoneVolume();
    s.speakerVolume = audio.speakerVolume();
    s.microphoneLevel = audio.inputLevel();
    s.speakerLevel = audio.outputLevel();

    {
        // Resolve each device from the persisted selection; a stale persisted
        // id (device ids change across sessions, e.g. RDP-redirected devices)
        // falls back to the default device. When Qt device enumeration comes
        // up empty (seen with RDP-redirected audio), ask PJSIP which devices
        // it actually opened for the call instead of reporting N/A.
        const auto resolve = [&devices](MediaDeviceType type, const QString &id) {
            MediaDevice dev;
            if (!id.isEmpty())
                dev = devices.findDevice(type, id);
            if (dev.isNull()) {
                switch (type) {
                case MediaDeviceType::Microphone: dev = devices.defaultMicrophone(); break;
                case MediaDeviceType::Speaker:    dev = devices.defaultSpeaker();    break;
                case MediaDeviceType::Camera:     dev = devices.defaultCamera();     break;
                }
            }
            return dev;
        };

        const MediaDevice mic = resolve(MediaDeviceType::Microphone,
                                        AppSettings::loadSelectedMicrophone());
        QString micName = mic.isNull() ? QString{} : mic.displayName;
        if (micName.isEmpty())
            micName = PjsipAudioMapper::activeCaptureDeviceName();
        s.microphoneName = micName.isEmpty() ? diagnosticsNotAvailable() : micName;

        const MediaDevice spk = resolve(MediaDeviceType::Speaker,
                                        AppSettings::loadSelectedSpeaker());
        QString spkName = spk.isNull() ? QString{} : spk.displayName;
        if (spkName.isEmpty())
            spkName = PjsipAudioMapper::activePlaybackDeviceName();
        s.speakerName = spkName.isEmpty() ? diagnosticsNotAvailable() : spkName;

        const MediaDevice cam = resolve(MediaDeviceType::Camera,
                                        AppSettings::loadSelectedCamera());
        s.cameraName = cam.isNull() ? diagnosticsNotAvailable() : cam.displayName;
    }

    s.videoConnected = video.isVideoActive();
    s.videoMuted = video.isVideoMuted();
    s.localVideoAvailable = video.isLocalVideoAvailable();
    s.remoteVideoAvailable = video.isRemoteVideoAvailable();
    s.cameraEnabled = camera.isEnabled();
    s.videoResolution = videoSettings.resolution;
    s.videoFps = videoSettings.fps;
    s.videoBitrateKbps = videoSettings.bitrateKbps;
    // Prefer the codec actually negotiated for the active call. Fall back to
    // a best-effort description from the configured codec preference order
    // and video quality settings, marked negotiated=false so consumers can
    // tell it is only the local configuration.
    s.videoCodec = sip.activeVideoCodecInfo();
    if (!s.videoCodec.isValid() && !videoSettings.codecOrder.isEmpty()) {
        VideoCodecInfo configured;
        configured.name = videoSettings.codecOrder.first();
        configured.width = videoSettings.resolution.width();
        configured.height = videoSettings.resolution.height();
        configured.fps = videoSettings.fps;
        configured.bitrate = videoSettings.bitrateKbps * 1000;
        configured.negotiated = false;
        s.videoCodec = configured;
    }

    s.rttState = rttStateName(sip.rttSession()->state());

    // ---- Network ----------------------------------------------------
    // Local SIP transport and remote RTP endpoint. ICE/STUN/TURN are not
    // configured in this codebase (plain UDP/TCP/TLS transports) — those
    // stay N/A.
    {
        const auto splitHostPort = [](const QString &addr,
                                      QString &host, QString &port) {
            const int idx = addr.lastIndexOf(QLatin1Char(':'));
            if (idx > 0) {
                host = addr.left(idx);
                port = addr.mid(idx + 1);
            } else if (!addr.isEmpty()) {
                host = addr;
            }
        };

        QString host, port;
        splitHostPort(sip.localTransportAddress(), host, port);
        if (!host.isEmpty()) {
            s.localIp = host;
            s.localPort = port.isEmpty() ? diagnosticsNotAvailable() : port;
        }

        host.clear();
        port.clear();
        splitHostPort(sip.activeCallRemoteMediaAddress(), host, port);
        if (!host.isEmpty()) {
            s.remoteIp = host;
            s.remotePort = port.isEmpty() ? diagnosticsNotAvailable() : port;
        }
    }

    // ---- System -------------------------------------------------------
    s.qtVersion = QString::fromLatin1(qVersion());
#ifdef HAVE_PJSIP
    s.pjsipVersion = QString::fromLatin1(PJ_VERSION);
#endif
    s.appVersion = QStringLiteral(APP_VERSION_STRING);
    s.gitCommit = QStringLiteral(APP_GIT_COMMIT_HASH);
    s.platform = QSysInfo::prettyProductName();
    s.architecture = QSysInfo::currentCpuArchitecture();
    s.buildType = QStringLiteral(APP_BUILD_TYPE_STRING).isEmpty()
        ? QStringLiteral("Unknown") : QStringLiteral(APP_BUILD_TYPE_STRING);
#if defined(_MSC_VER)
    s.compiler = QStringLiteral("MSVC %1").arg(_MSC_VER);
#elif defined(__clang__)
    s.compiler = QStringLiteral("Clang %1").arg(__clang_version__);
#elif defined(__GNUC__)
    s.compiler = QStringLiteral("GCC %1.%2").arg(__GNUC__).arg(__GNUC_MINOR__);
#else
    s.compiler = diagnosticsNotAvailable();
#endif

    m_snapshot = s;
    emit snapshotUpdated(m_snapshot);
}
