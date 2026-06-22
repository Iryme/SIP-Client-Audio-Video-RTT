#pragma once
#include <QString>

enum class MediaDeviceType {
    Microphone,
    Speaker,
    Camera
};

struct MediaDevice {
    QString         id;
    QString         displayName;
    MediaDeviceType type{MediaDeviceType::Microphone};
    bool            isDefault{false};
    bool            isAvailable{true};

    bool isNull() const { return id.isEmpty(); }

    static MediaDevice null() { return {}; }
};
