#include "MediaDeviceSelectionModel.h"
#include "core/AppSettings.h"
#include "core/Logger.h"

MediaDeviceSelectionModel::MediaDeviceSelectionModel(MediaDeviceManager *manager,
                                                     QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    connect(m_manager, &MediaDeviceManager::devicesChanged,
            this,      &MediaDeviceSelectionModel::refresh);
}

// ---------------------------------------------------------------------------
// Settings keys
// ---------------------------------------------------------------------------
QString MediaDeviceSelectionModel::settingsKey(MediaDeviceType type)
{
    switch (type) {
        case MediaDeviceType::Microphone: return QStringLiteral("media/device/microphone");
        case MediaDeviceType::Speaker:    return QStringLiteral("media/device/speaker");
        case MediaDeviceType::Camera:     return QStringLiteral("media/device/camera");
    }
    return {};
}

QString MediaDeviceSelectionModel::loadPersistedId(MediaDeviceType type)
{
    return AppSettings::settings().value(settingsKey(type)).toString();
}

void MediaDeviceSelectionModel::savePersistedId(MediaDeviceType type, const QString &id)
{
    AppSettings::settings().setValue(settingsKey(type), id);
}

// ---------------------------------------------------------------------------
// Resolution: persisted id → available device → default fallback
// ---------------------------------------------------------------------------
MediaDevice MediaDeviceSelectionModel::resolve(MediaDeviceType type,
                                               const QString &persistedId) const
{
    if (!persistedId.isEmpty()) {
        const MediaDevice found = m_manager->findDevice(type, persistedId);
        if (!found.isNull())
            return found;

        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("Selected %1 device '%2' is no longer available; falling back to default")
                .arg(type == MediaDeviceType::Microphone ? QStringLiteral("microphone") :
                     type == MediaDeviceType::Speaker    ? QStringLiteral("speaker")    :
                                                           QStringLiteral("camera"))
                .arg(persistedId));
    }

    switch (type) {
        case MediaDeviceType::Microphone: return m_manager->defaultMicrophone();
        case MediaDeviceType::Speaker:    return m_manager->defaultSpeaker();
        case MediaDeviceType::Camera:     return m_manager->defaultCamera();
    }
    return MediaDevice::null();
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
MediaDevice MediaDeviceSelectionModel::selectedMicrophone() const
{
    return resolve(MediaDeviceType::Microphone, loadPersistedId(MediaDeviceType::Microphone));
}

MediaDevice MediaDeviceSelectionModel::selectedSpeaker() const
{
    return resolve(MediaDeviceType::Speaker, loadPersistedId(MediaDeviceType::Speaker));
}

MediaDevice MediaDeviceSelectionModel::selectedCamera() const
{
    return resolve(MediaDeviceType::Camera, loadPersistedId(MediaDeviceType::Camera));
}

// ---------------------------------------------------------------------------
// Selection (persist + notify)
// ---------------------------------------------------------------------------
void MediaDeviceSelectionModel::selectMicrophone(const QString &id)
{
    savePersistedId(MediaDeviceType::Microphone, id);
    const MediaDevice dev = selectedMicrophone();
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Microphone selected: %1 (%2)").arg(dev.displayName, dev.id));
    emit microphoneSelectionChanged(dev);
}

void MediaDeviceSelectionModel::selectSpeaker(const QString &id)
{
    savePersistedId(MediaDeviceType::Speaker, id);
    const MediaDevice dev = selectedSpeaker();
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Speaker selected: %1 (%2)").arg(dev.displayName, dev.id));
    emit speakerSelectionChanged(dev);
}

void MediaDeviceSelectionModel::selectCamera(const QString &id)
{
    savePersistedId(MediaDeviceType::Camera, id);
    const MediaDevice dev = selectedCamera();
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Camera selected: %1 (%2)").arg(dev.displayName, dev.id));
    emit cameraSelectionChanged(dev);
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------
void MediaDeviceSelectionModel::refresh()
{
    emit microphoneSelectionChanged(selectedMicrophone());
    emit speakerSelectionChanged   (selectedSpeaker());
    emit cameraSelectionChanged    (selectedCamera());
}
