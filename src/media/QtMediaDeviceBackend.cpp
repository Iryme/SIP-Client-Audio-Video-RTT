#include "QtMediaDeviceBackend.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QCameraDevice>

static MediaDevice fromAudioDevice(const QAudioDevice &dev, MediaDeviceType type, bool isDefault)
{
    MediaDevice md;
    md.id          = dev.id();
    md.displayName = dev.description();
    md.type        = type;
    md.isDefault   = isDefault;
    md.isAvailable = true;
    return md;
}

static MediaDevice fromCameraDevice(const QCameraDevice &dev, bool isDefault)
{
    MediaDevice md;
    md.id          = dev.id();
    md.displayName = dev.description();
    md.type        = MediaDeviceType::Camera;
    md.isDefault   = isDefault;
    md.isAvailable = true;
    return md;
}

QList<MediaDevice> QtMediaDeviceBackend::microphones() const
{
    QList<MediaDevice> result;
    const QAudioDevice defaultDev = QMediaDevices::defaultAudioInput();
    const auto inputs = QMediaDevices::audioInputs();
    for (const QAudioDevice &dev : inputs) {
        const bool isDefault = (dev.id() == defaultDev.id());
        result.append(fromAudioDevice(dev, MediaDeviceType::Microphone, isDefault));
    }
    return result;
}

QList<MediaDevice> QtMediaDeviceBackend::speakers() const
{
    QList<MediaDevice> result;
    const QAudioDevice defaultDev = QMediaDevices::defaultAudioOutput();
    const auto outputs = QMediaDevices::audioOutputs();
    for (const QAudioDevice &dev : outputs) {
        const bool isDefault = (dev.id() == defaultDev.id());
        result.append(fromAudioDevice(dev, MediaDeviceType::Speaker, isDefault));
    }
    return result;
}

QList<MediaDevice> QtMediaDeviceBackend::cameras() const
{
    QList<MediaDevice> result;
    const QCameraDevice defaultDev = QMediaDevices::defaultVideoInput();
    const auto inputs = QMediaDevices::videoInputs();
    for (const QCameraDevice &dev : inputs) {
        const bool isDefault = (dev.id() == defaultDev.id());
        result.append(fromCameraDevice(dev, isDefault));
    }
    return result;
}
