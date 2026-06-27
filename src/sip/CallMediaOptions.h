#pragma once
#include <QString>

enum class CallType {
    AudioOnly,
    AudioVideo,
    AudioRtt,
    AudioVideoRtt,
    RttOnly,
};

inline QString callTypeName(CallType t)
{
    switch (t) {
    case CallType::AudioOnly:     return QStringLiteral("Audio only");
    case CallType::AudioVideo:    return QStringLiteral("Audio + Video");
    case CallType::AudioRtt:      return QStringLiteral("Audio + RTT");
    case CallType::AudioVideoRtt: return QStringLiteral("Audio + Video + RTT");
    case CallType::RttOnly:       return QStringLiteral("RTT only");
    }
    return QStringLiteral("Unknown");
}

struct CallMediaOptions {
    CallType type{CallType::AudioOnly};
    bool enableAudio{true};
    bool enableVideo{false};
    bool enableRtt{false};
    bool enableLmpe{false};

    static CallMediaOptions fromType(CallType t)
    {
        CallMediaOptions o;
        o.type = t;
        switch (t) {
        case CallType::AudioOnly:
            o.enableAudio = true;
            break;
        case CallType::AudioVideo:
            o.enableAudio = true; o.enableVideo = true;
            break;
        case CallType::AudioRtt:
            o.enableAudio = true; o.enableRtt = true;
            break;
        case CallType::AudioVideoRtt:
            o.enableAudio = true; o.enableVideo = true; o.enableRtt = true;
            break;
        case CallType::RttOnly:
            o.enableRtt = true;
            break;
        }
        return o;
    }
};
