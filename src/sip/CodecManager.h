#pragma once

#include <QList>
#include <QString>

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

private:
    CodecManager() = default;

    QList<CodecEntry> m_audioCodecs;
    QList<CodecEntry> m_videoCodecs;
};
