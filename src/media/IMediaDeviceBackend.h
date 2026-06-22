#pragma once
#include <QList>
#include "media/MediaDevice.h"

// Pure interface for media device enumeration.
// QtMediaDeviceBackend wraps QMediaDevices (production).
// StubMediaDeviceBackend is injected in unit tests.
class IMediaDeviceBackend
{
public:
    virtual ~IMediaDeviceBackend() = default;

    virtual QList<MediaDevice> microphones() const = 0;
    virtual QList<MediaDevice> speakers()    const = 0;
    virtual QList<MediaDevice> cameras()     const = 0;
};
