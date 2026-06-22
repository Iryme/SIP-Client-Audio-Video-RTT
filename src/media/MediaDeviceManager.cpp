#include "MediaDeviceManager.h"
#include "media/QtMediaDeviceBackend.h"
#include "core/Logger.h"

MediaDeviceManager &MediaDeviceManager::instance()
{
    static MediaDeviceManager s_instance;
    return s_instance;
}

MediaDeviceManager::MediaDeviceManager()
    : QObject(nullptr)
    , m_backend(std::make_unique<QtMediaDeviceBackend>())
{
}

void MediaDeviceManager::setBackend(std::unique_ptr<IMediaDeviceBackend> backend)
{
    m_backend = std::move(backend);
    m_loaded  = false;
    m_microphones.clear();
    m_speakers.clear();
    m_cameras.clear();
}

void MediaDeviceManager::ensureLoaded() const
{
    if (!m_loaded)
        const_cast<MediaDeviceManager *>(this)->loadAll();
}

void MediaDeviceManager::loadAll()
{
    m_microphones = m_backend->microphones();
    m_speakers    = m_backend->speakers();
    m_cameras     = m_backend->cameras();
    m_loaded      = true;

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Media devices enumerated: %1 microphone(s), %2 speaker(s), %3 camera(s)")
            .arg(m_microphones.size())
            .arg(m_speakers.size())
            .arg(m_cameras.size()));
}

void MediaDeviceManager::refreshDevices()
{
    m_loaded = false;
    loadAll();
    emit devicesChanged();
}

QList<MediaDevice> MediaDeviceManager::listMicrophones() const
{
    ensureLoaded();
    return m_microphones;
}

QList<MediaDevice> MediaDeviceManager::listSpeakers() const
{
    ensureLoaded();
    return m_speakers;
}

QList<MediaDevice> MediaDeviceManager::listCameras() const
{
    ensureLoaded();
    return m_cameras;
}

static MediaDevice firstDefault(const QList<MediaDevice> &list)
{
    for (const MediaDevice &d : list) {
        if (d.isDefault)
            return d;
    }
    return list.isEmpty() ? MediaDevice::null() : list.first();
}

MediaDevice MediaDeviceManager::defaultMicrophone() const
{
    ensureLoaded();
    return firstDefault(m_microphones);
}

MediaDevice MediaDeviceManager::defaultSpeaker() const
{
    ensureLoaded();
    return firstDefault(m_speakers);
}

MediaDevice MediaDeviceManager::defaultCamera() const
{
    ensureLoaded();
    return firstDefault(m_cameras);
}

MediaDevice MediaDeviceManager::findDevice(MediaDeviceType type, const QString &id) const
{
    ensureLoaded();
    const QList<MediaDevice> *list = nullptr;
    switch (type) {
        case MediaDeviceType::Microphone: list = &m_microphones; break;
        case MediaDeviceType::Speaker:    list = &m_speakers;    break;
        case MediaDeviceType::Camera:     list = &m_cameras;     break;
    }
    if (list) {
        for (const MediaDevice &d : *list) {
            if (d.id == id)
                return d;
        }
    }
    return MediaDevice::null();
}
