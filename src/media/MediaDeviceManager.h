#pragma once
#include <QObject>
#include <QList>
#include <memory>
#include "media/MediaDevice.h"
#include "media/IMediaDeviceBackend.h"

// Singleton service for media device enumeration.
// Wraps an IMediaDeviceBackend so the backend can be swapped in tests.
// GUI and other callers use this class exclusively.
class MediaDeviceManager : public QObject
{
    Q_OBJECT
public:
    static MediaDeviceManager &instance();

    // Device lists (returns empty list on error or no devices)
    QList<MediaDevice> listMicrophones() const;
    QList<MediaDevice> listSpeakers()    const;
    QList<MediaDevice> listCameras()     const;

    // Default devices (returns MediaDevice::null() if no devices)
    MediaDevice defaultMicrophone() const;
    MediaDevice defaultSpeaker()    const;
    MediaDevice defaultCamera()     const;

    // Re-enumerate all devices asynchronously; emits devicesChanged() on completion.
    // Non-blocking: returns immediately and runs enumeration on a worker thread.
    void refreshDevices();

    // Find a device by id within a type's list (null if not found)
    MediaDevice findDevice(MediaDeviceType type, const QString &id) const;

    // Backend injection — use in tests before any other call
    void setBackend(std::unique_ptr<IMediaDeviceBackend> backend);

    bool isLoaded()     const { return m_loaded;     }
    bool isRefreshing() const { return m_refreshing; }

signals:
    void devicesChanged();
    void refreshStarted();
    void refreshFinished();

private:
    MediaDeviceManager();

    std::unique_ptr<IMediaDeviceBackend> m_backend;

    mutable QList<MediaDevice> m_microphones;
    mutable QList<MediaDevice> m_speakers;
    mutable QList<MediaDevice> m_cameras;
    bool m_loaded{false};
    bool m_refreshing{false};

    void ensureLoaded() const;
    void loadAll();
};
