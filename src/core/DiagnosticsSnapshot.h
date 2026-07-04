#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QSize>
#include <QString>

// Placeholder used for any value that has no real backing source yet.
// Never invent a plausible-looking value instead of this — see task notes
// in DiagnosticsCollector.cpp for exactly which fields are real vs N/A.
inline QString diagnosticsNotAvailable() { return QStringLiteral("N/A"); }

// Immutable-by-convention data holder for everything the Diagnostics Center
// displays. Built exclusively by DiagnosticsCollector from already-existing
// singletons (SipManager, AudioMediaManager, VideoMediaManager, ...) — the
// UI (DiagnosticsCenterPanel) only ever reads a snapshot, never touches
// SipManager or PJSIP directly.
struct DiagnosticsSnapshot
{
    QDateTime capturedAtUtc;

    // ---- Overview / SIP -----------------------------------------------
    QString registrationState;       // Unregistered / Registering / Registered / Unregistering / RegistrationFailed
    QString registrationStatusText;
    int     registrationStatusCode{0};
    QString callState;               // Idle / Ringing / Active / Held / Disconnecting / ...
    QString callStatusText;
    int     callStatusCode{0};
    QString currentProfileName;
    QString registrar;
    QString outboundProxy;
    QString transport;                // UDP / TCP / TLS
    QString remoteUri;
    QString callId;                   // Not exposed by SipCall today -> N/A
    QString dialogState;              // Not tracked separately from callState today -> N/A
    QString lastSipResponse;          // "<code> <text>" from the most recent registration or call event
    QString lastSipError;

    // ---- Media / RTP ----------------------------------------------------
    bool    audioConnected{false};
    QString audioCodec{diagnosticsNotAvailable()};   // negotiated codec name is only logged today, not queryable
    QString audioPtime{diagnosticsNotAvailable()};    // not exposed by RtpStatsSnapshot
    bool    audioPacketsRxAvailable{false};
    unsigned audioPacketsRx{0};
    QString audioPacketsTx{diagnosticsNotAvailable()}; // no TX counter exists
    bool    audioLossAvailable{false};
    double  audioLossPercent{0.0};
    bool    audioJitterAvailable{false};
    double  audioJitterMs{0.0};
    bool    audioRttAvailable{false};
    double  audioRttMs{0.0};

    bool    videoConnected{false};
    QString videoCodec{diagnosticsNotAvailable()};    // best-effort: first entry of the configured codec order
    QSize   videoResolution;
    int     videoFps{0};
    int     videoBitrateKbps{0};

    QString rttState;                 // Disabled / Offered / Negotiated / Active / Failed

    // ---- Audio devices ---------------------------------------------------
    QString microphoneName;
    QString speakerName;
    int     microphoneVolume{0};
    int     speakerVolume{0};
    int     microphoneLevel{0};
    int     speakerLevel{0};
    bool    audioMuted{false};

    // ---- Video devices ---------------------------------------------------
    QString cameraName;
    bool    cameraEnabled{false};
    bool    videoMuted{false};
    bool    localVideoAvailable{false};
    bool    remoteVideoAvailable{false};

    // ---- Network -----------------------------------------------------
    QString localIp{diagnosticsNotAvailable()};
    QString remoteIp{diagnosticsNotAvailable()};
    QString localPort{diagnosticsNotAvailable()};
    QString remotePort{diagnosticsNotAvailable()};
    QString ice{diagnosticsNotAvailable()};
    QString stun{diagnosticsNotAvailable()};
    QString turn{diagnosticsNotAvailable()};

    // ---- System ------------------------------------------------------
    QString qtVersion;
    QString pjsipVersion{diagnosticsNotAvailable()};
    QString appVersion;
    QString gitCommit;
    QString platform;
    QString architecture;
    QString buildType;
    QString compiler;

    QJsonObject toJson() const;
    static DiagnosticsSnapshot fromJson(const QJsonObject &obj);
};

Q_DECLARE_METATYPE(DiagnosticsSnapshot)
