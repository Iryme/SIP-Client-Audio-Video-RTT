#pragma once

#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

enum class SipMediaType { Audio, Video, Text };

struct CodecEntry {
    QString      codecId;
    SipMediaType mediaType;
    int          priority;  // 0=disabled, 1-255=enabled (higher=preferred)
    bool         available; // true when PJSIP reported this codec at runtime
};

class CodecManager
{
public:
    static CodecManager &instance();

    // Called once after PJSIP libStart() — detects available codecs,
    // applies default priorities, logs the full codec matrix.
    // Safe to call without PJSIP; populates stub entries.
    void initialize();

    QList<CodecEntry> audioCodecs() const;
    QList<CodecEntry> videoCodecs() const;
    QList<CodecEntry> rttCodecs()   const; // model only; PJSIP has no T.140 codec

    // True if any video codec is available and has priority > 0.
    bool hasUsableVideoCodec() const;

    // True if any audio codec is available and has priority > 0.
    bool hasUsableAudioCodec() const;

    // Log all discovered codecs in a structured table.
    void logCodecMatrix() const;

    // Apply video codec priority order from user preferences.
    // Codecs in order get decreasing priority (240, 230, ...); others get priority 1.
    // No-op when PJSIP or PJMEDIA_HAS_VIDEO is unavailable.
    void applyVideoCodecOrder(const QStringList &order);

    // Apply bitrate to the first PJSIP video codec matching preferredCodec prefix.
    // Sets avg_bps = bitrateKbps * 1000, max_bps = bitrateKbps * 2000.
    void applyVideoCodecBitrate(const QString &preferredCodec, int bitrateKbps);

    // Apply resolution and fps to the encoder format of the first matching codec.
    void applyVideoCodecFormat(const QStringList &order, const QSize &resolution, int fps);

private:
    CodecManager() = default;

    QList<CodecEntry> m_audioCodecs;
    QList<CodecEntry> m_videoCodecs;
};
