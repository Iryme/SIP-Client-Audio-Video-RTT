#pragma once
#include "media/IMediaDeviceBackend.h"

// Production backend: enumerates devices via Qt Multimedia (QMediaDevices).
class QtMediaDeviceBackend : public IMediaDeviceBackend
{
public:
    QList<MediaDevice> microphones() const override;
    QList<MediaDevice> speakers()    const override;
    QList<MediaDevice> cameras()     const override;
};
