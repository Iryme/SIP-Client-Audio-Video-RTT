#pragma once
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>

// Structured codec descriptions for the Diagnostics Center and Diagnostics
// Bundle. These replace the old "PCMA/8000 pt=8" strings so every field can
// be displayed and exported individually.
//
// Header-only on purpose: the structs are consumed by the app target and by
// several independent test binaries (which each compile their own subset of
// src/), so keeping everything inline avoids adding a .cpp to every target.
//
// Field semantics: 0 / -1 / empty mean "unknown" — summaryString() and the
// UI render those as N/A instead of inventing a value.

struct AudioCodecInfo
{
    QString name;             // encoding name, e.g. "PCMA", "opus"
    int     payloadType{-1};  // RTP payload type; -1 = unknown
    int     clockRate{0};     // Hz; 0 = unknown
    int     channels{0};      // 1 = mono, 2 = stereo; 0 = unknown
    int     ptime{0};         // packet time in ms; 0 = unknown
    int     bitrate{0};       // average bitrate in bps; 0 = unknown
    bool    negotiated{false}; // true when taken from the active call's SDP

    bool isValid() const
    {
        return !name.isEmpty() && clockRate > 0 && payloadType >= 0;
    }

    // "PCMA • 8000 Hz • PT=8 • Mono • 20 ms" — unknown parts are omitted;
    // an invalid codec renders as "N/A".
    QString summaryString() const
    {
        if (!isValid())
            return QStringLiteral("N/A");
        QStringList parts;
        parts << name
              << QStringLiteral("%1 Hz").arg(clockRate)
              << QStringLiteral("PT=%1").arg(payloadType);
        if (channels == 1)
            parts << QStringLiteral("Mono");
        else if (channels == 2)
            parts << QStringLiteral("Stereo");
        else if (channels > 2)
            parts << QStringLiteral("%1 ch").arg(channels);
        if (ptime > 0)
            parts << QStringLiteral("%1 ms").arg(ptime);
        if (bitrate > 0)
            parts << QStringLiteral("%1 kbps").arg(bitrate / 1000);
        return parts.join(QStringLiteral(" • "));
    }

    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert(QStringLiteral("name"), name);
        o.insert(QStringLiteral("payloadType"), payloadType);
        o.insert(QStringLiteral("clockRate"), clockRate);
        o.insert(QStringLiteral("channels"), channels);
        o.insert(QStringLiteral("ptime"), ptime);
        o.insert(QStringLiteral("bitrate"), bitrate);
        o.insert(QStringLiteral("negotiated"), negotiated);
        return o;
    }

    static AudioCodecInfo fromJson(const QJsonObject &o)
    {
        AudioCodecInfo c;
        c.name = o.value(QStringLiteral("name")).toString();
        c.payloadType = o.value(QStringLiteral("payloadType")).toInt(-1);
        c.clockRate = o.value(QStringLiteral("clockRate")).toInt();
        c.channels = o.value(QStringLiteral("channels")).toInt();
        c.ptime = o.value(QStringLiteral("ptime")).toInt();
        c.bitrate = o.value(QStringLiteral("bitrate")).toInt();
        c.negotiated = o.value(QStringLiteral("negotiated")).toBool();
        return c;
    }
};

struct VideoCodecInfo
{
    QString name;             // encoding name, e.g. "H264", "VP8"
    int     payloadType{-1};  // RTP payload type; -1 = unknown
    int     width{0};         // 0 = unknown
    int     height{0};        // 0 = unknown
    int     fps{0};           // 0 = unknown
    int     bitrate{0};       // average bitrate in bps; 0 = unknown
    bool    negotiated{false}; // true when taken from the active call's SDP

    bool isValid() const { return !name.isEmpty(); }

    // "H264 • 1280x720 • 30 fps • PT=97 • 512 kbps" — unknown parts are
    // omitted; an invalid codec renders as "N/A".
    QString summaryString() const
    {
        if (!isValid())
            return QStringLiteral("N/A");
        QStringList parts;
        parts << name;
        if (width > 0 && height > 0)
            parts << QStringLiteral("%1x%2").arg(width).arg(height);
        if (fps > 0)
            parts << QStringLiteral("%1 fps").arg(fps);
        if (payloadType >= 0)
            parts << QStringLiteral("PT=%1").arg(payloadType);
        if (bitrate > 0)
            parts << QStringLiteral("%1 kbps").arg(bitrate / 1000);
        return parts.join(QStringLiteral(" • "));
    }

    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert(QStringLiteral("name"), name);
        o.insert(QStringLiteral("payloadType"), payloadType);
        o.insert(QStringLiteral("width"), width);
        o.insert(QStringLiteral("height"), height);
        o.insert(QStringLiteral("fps"), fps);
        o.insert(QStringLiteral("bitrate"), bitrate);
        o.insert(QStringLiteral("negotiated"), negotiated);
        return o;
    }

    static VideoCodecInfo fromJson(const QJsonObject &o)
    {
        VideoCodecInfo c;
        c.name = o.value(QStringLiteral("name")).toString();
        c.payloadType = o.value(QStringLiteral("payloadType")).toInt(-1);
        c.width = o.value(QStringLiteral("width")).toInt();
        c.height = o.value(QStringLiteral("height")).toInt();
        c.fps = o.value(QStringLiteral("fps")).toInt();
        c.bitrate = o.value(QStringLiteral("bitrate")).toInt();
        c.negotiated = o.value(QStringLiteral("negotiated")).toBool();
        return c;
    }
};

Q_DECLARE_METATYPE(AudioCodecInfo)
Q_DECLARE_METATYPE(VideoCodecInfo)
