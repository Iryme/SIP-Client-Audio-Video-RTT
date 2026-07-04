#include "DiagnosticsSnapshot.h"

QJsonObject DiagnosticsSnapshot::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("capturedAtUtc"), capturedAtUtc.toString(Qt::ISODateWithMs));

    o.insert(QStringLiteral("registrationState"), registrationState);
    o.insert(QStringLiteral("registrationStatusText"), registrationStatusText);
    o.insert(QStringLiteral("registrationStatusCode"), registrationStatusCode);
    o.insert(QStringLiteral("callState"), callState);
    o.insert(QStringLiteral("callStatusText"), callStatusText);
    o.insert(QStringLiteral("callStatusCode"), callStatusCode);
    o.insert(QStringLiteral("currentProfileName"), currentProfileName);
    o.insert(QStringLiteral("registrar"), registrar);
    o.insert(QStringLiteral("outboundProxy"), outboundProxy);
    o.insert(QStringLiteral("transport"), transport);
    o.insert(QStringLiteral("remoteUri"), remoteUri);
    o.insert(QStringLiteral("callId"), callId);
    o.insert(QStringLiteral("dialogState"), dialogState);
    o.insert(QStringLiteral("lastSipResponse"), lastSipResponse);
    o.insert(QStringLiteral("lastSipError"), lastSipError);

    o.insert(QStringLiteral("audioConnected"), audioConnected);
    o.insert(QStringLiteral("audioCodec"), audioCodec);
    o.insert(QStringLiteral("audioPtime"), audioPtime);
    o.insert(QStringLiteral("audioPacketsRxAvailable"), audioPacketsRxAvailable);
    o.insert(QStringLiteral("audioPacketsRx"), static_cast<qint64>(audioPacketsRx));
    o.insert(QStringLiteral("audioPacketsTx"), audioPacketsTx);
    o.insert(QStringLiteral("audioLossAvailable"), audioLossAvailable);
    o.insert(QStringLiteral("audioLossPercent"), audioLossPercent);
    o.insert(QStringLiteral("audioJitterAvailable"), audioJitterAvailable);
    o.insert(QStringLiteral("audioJitterMs"), audioJitterMs);
    o.insert(QStringLiteral("audioRttAvailable"), audioRttAvailable);
    o.insert(QStringLiteral("audioRttMs"), audioRttMs);

    o.insert(QStringLiteral("videoConnected"), videoConnected);
    o.insert(QStringLiteral("videoCodec"), videoCodec);
    o.insert(QStringLiteral("videoResolutionWidth"), videoResolution.width());
    o.insert(QStringLiteral("videoResolutionHeight"), videoResolution.height());
    o.insert(QStringLiteral("videoFps"), videoFps);
    o.insert(QStringLiteral("videoBitrateKbps"), videoBitrateKbps);

    o.insert(QStringLiteral("rttState"), rttState);

    o.insert(QStringLiteral("microphoneName"), microphoneName);
    o.insert(QStringLiteral("speakerName"), speakerName);
    o.insert(QStringLiteral("microphoneVolume"), microphoneVolume);
    o.insert(QStringLiteral("speakerVolume"), speakerVolume);
    o.insert(QStringLiteral("microphoneLevel"), microphoneLevel);
    o.insert(QStringLiteral("speakerLevel"), speakerLevel);
    o.insert(QStringLiteral("audioMuted"), audioMuted);

    o.insert(QStringLiteral("cameraName"), cameraName);
    o.insert(QStringLiteral("cameraEnabled"), cameraEnabled);
    o.insert(QStringLiteral("videoMuted"), videoMuted);
    o.insert(QStringLiteral("localVideoAvailable"), localVideoAvailable);
    o.insert(QStringLiteral("remoteVideoAvailable"), remoteVideoAvailable);

    o.insert(QStringLiteral("localIp"), localIp);
    o.insert(QStringLiteral("remoteIp"), remoteIp);
    o.insert(QStringLiteral("localPort"), localPort);
    o.insert(QStringLiteral("remotePort"), remotePort);
    o.insert(QStringLiteral("ice"), ice);
    o.insert(QStringLiteral("stun"), stun);
    o.insert(QStringLiteral("turn"), turn);

    o.insert(QStringLiteral("qtVersion"), qtVersion);
    o.insert(QStringLiteral("pjsipVersion"), pjsipVersion);
    o.insert(QStringLiteral("appVersion"), appVersion);
    o.insert(QStringLiteral("gitCommit"), gitCommit);
    o.insert(QStringLiteral("platform"), platform);
    o.insert(QStringLiteral("architecture"), architecture);
    o.insert(QStringLiteral("buildType"), buildType);
    o.insert(QStringLiteral("compiler"), compiler);

    return o;
}

DiagnosticsSnapshot DiagnosticsSnapshot::fromJson(const QJsonObject &o)
{
    DiagnosticsSnapshot s;

    s.capturedAtUtc = QDateTime::fromString(o.value(QStringLiteral("capturedAtUtc")).toString(), Qt::ISODateWithMs);

    s.registrationState = o.value(QStringLiteral("registrationState")).toString();
    s.registrationStatusText = o.value(QStringLiteral("registrationStatusText")).toString();
    s.registrationStatusCode = o.value(QStringLiteral("registrationStatusCode")).toInt();
    s.callState = o.value(QStringLiteral("callState")).toString();
    s.callStatusText = o.value(QStringLiteral("callStatusText")).toString();
    s.callStatusCode = o.value(QStringLiteral("callStatusCode")).toInt();
    s.currentProfileName = o.value(QStringLiteral("currentProfileName")).toString();
    s.registrar = o.value(QStringLiteral("registrar")).toString();
    s.outboundProxy = o.value(QStringLiteral("outboundProxy")).toString();
    s.transport = o.value(QStringLiteral("transport")).toString();
    s.remoteUri = o.value(QStringLiteral("remoteUri")).toString();
    s.callId = o.value(QStringLiteral("callId")).toString();
    s.dialogState = o.value(QStringLiteral("dialogState")).toString();
    s.lastSipResponse = o.value(QStringLiteral("lastSipResponse")).toString();
    s.lastSipError = o.value(QStringLiteral("lastSipError")).toString();

    s.audioConnected = o.value(QStringLiteral("audioConnected")).toBool();
    s.audioCodec = o.value(QStringLiteral("audioCodec")).toString();
    s.audioPtime = o.value(QStringLiteral("audioPtime")).toString();
    s.audioPacketsRxAvailable = o.value(QStringLiteral("audioPacketsRxAvailable")).toBool();
    s.audioPacketsRx = static_cast<unsigned>(o.value(QStringLiteral("audioPacketsRx")).toInteger());
    s.audioPacketsTx = o.value(QStringLiteral("audioPacketsTx")).toString();
    s.audioLossAvailable = o.value(QStringLiteral("audioLossAvailable")).toBool();
    s.audioLossPercent = o.value(QStringLiteral("audioLossPercent")).toDouble();
    s.audioJitterAvailable = o.value(QStringLiteral("audioJitterAvailable")).toBool();
    s.audioJitterMs = o.value(QStringLiteral("audioJitterMs")).toDouble();
    s.audioRttAvailable = o.value(QStringLiteral("audioRttAvailable")).toBool();
    s.audioRttMs = o.value(QStringLiteral("audioRttMs")).toDouble();

    s.videoConnected = o.value(QStringLiteral("videoConnected")).toBool();
    s.videoCodec = o.value(QStringLiteral("videoCodec")).toString();
    s.videoResolution = QSize(o.value(QStringLiteral("videoResolutionWidth")).toInt(),
                              o.value(QStringLiteral("videoResolutionHeight")).toInt());
    s.videoFps = o.value(QStringLiteral("videoFps")).toInt();
    s.videoBitrateKbps = o.value(QStringLiteral("videoBitrateKbps")).toInt();

    s.rttState = o.value(QStringLiteral("rttState")).toString();

    s.microphoneName = o.value(QStringLiteral("microphoneName")).toString();
    s.speakerName = o.value(QStringLiteral("speakerName")).toString();
    s.microphoneVolume = o.value(QStringLiteral("microphoneVolume")).toInt();
    s.speakerVolume = o.value(QStringLiteral("speakerVolume")).toInt();
    s.microphoneLevel = o.value(QStringLiteral("microphoneLevel")).toInt();
    s.speakerLevel = o.value(QStringLiteral("speakerLevel")).toInt();
    s.audioMuted = o.value(QStringLiteral("audioMuted")).toBool();

    s.cameraName = o.value(QStringLiteral("cameraName")).toString();
    s.cameraEnabled = o.value(QStringLiteral("cameraEnabled")).toBool();
    s.videoMuted = o.value(QStringLiteral("videoMuted")).toBool();
    s.localVideoAvailable = o.value(QStringLiteral("localVideoAvailable")).toBool();
    s.remoteVideoAvailable = o.value(QStringLiteral("remoteVideoAvailable")).toBool();

    s.localIp = o.value(QStringLiteral("localIp")).toString();
    s.remoteIp = o.value(QStringLiteral("remoteIp")).toString();
    s.localPort = o.value(QStringLiteral("localPort")).toString();
    s.remotePort = o.value(QStringLiteral("remotePort")).toString();
    s.ice = o.value(QStringLiteral("ice")).toString();
    s.stun = o.value(QStringLiteral("stun")).toString();
    s.turn = o.value(QStringLiteral("turn")).toString();

    s.qtVersion = o.value(QStringLiteral("qtVersion")).toString();
    s.pjsipVersion = o.value(QStringLiteral("pjsipVersion")).toString();
    s.appVersion = o.value(QStringLiteral("appVersion")).toString();
    s.gitCommit = o.value(QStringLiteral("gitCommit")).toString();
    s.platform = o.value(QStringLiteral("platform")).toString();
    s.architecture = o.value(QStringLiteral("architecture")).toString();
    s.buildType = o.value(QStringLiteral("buildType")).toString();
    s.compiler = o.value(QStringLiteral("compiler")).toString();

    return s;
}
