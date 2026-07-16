#include "MediaDeviceManager.h"
#include "media/QtMediaDeviceBackend.h"
#include "core/AppSettings.h"
#include "core/Logger.h"
#include <QDateTime>
#include <QThread>
#include <QTimer>

// Some platforms register internal/virtual audio endpoints as ordinary
// input/output devices (e.g. Windows Remote Desktop's redirected "Remote
// Audio" endpoint). These are not physical hardware — offering them as a
// selectable Speaker/Microphone can silently route call audio away from the
// real device, so they are excluded from the physical device lists by
// default. AppSettings::allowRedirectedAudioDevices() is an explicit opt-in
// for machines with no physical audio hardware at all (e.g. a VM reached
// only via RDP), where a redirected endpoint is the only usable device.
static bool isInternalAudioEndpoint(const QString &displayName)
{
    static const QStringList kBlocked = { QStringLiteral("remote audio") };
    const QString lower = displayName.trimmed().toLower();
    for (const QString &blocked : kBlocked) {
        if (lower.contains(blocked))
            return true;
    }
    return false;
}

static QList<MediaDevice> filterPhysicalAudioDevices(const QList<MediaDevice> &devices)
{
    if (AppSettings::allowRedirectedAudioDevices())
        return devices;
    QList<MediaDevice> out;
    out.reserve(devices.size());
    for (const MediaDevice &d : devices) {
        if (!isInternalAudioEndpoint(d.displayName))
            out.append(d);
    }
    return out;
}

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
    if (m_loaded)    return;
    if (m_refreshing) return; // async refresh in progress — caller gets empty/stale cache
    const_cast<MediaDeviceManager *>(this)->loadAll();
}

void MediaDeviceManager::loadAll()
{
    m_microphones = filterPhysicalAudioDevices(m_backend->microphones());
    m_speakers    = filterPhysicalAudioDevices(m_backend->speakers());
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
    if (m_refreshing) {
        Logger::instance().info(LogCategory::Media,
            "MediaDeviceManager: refresh already in progress, skipping");
        return;
    }
    if (!m_backend) {
        Logger::instance().warn(LogCategory::Media,
            "MediaDeviceManager: no backend installed");
        return;
    }

    m_refreshing = true;
    m_loaded     = false;
    emit refreshStarted();
    Logger::instance().info(LogCategory::Media,
        "MediaDeviceManager: audio/video device enumeration started");

    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();

    // Run enumeration on a worker thread so the UI thread stays responsive.
    // QMediaDevices static functions are safe to call from non-UI threads in Qt6
    // (internally mutex-protected). Results are posted back via QueuedConnection.
    auto *thread = QThread::create([this, startMs]() {
        const qint64 t0 = QDateTime::currentMSecsSinceEpoch();
        auto mics = filterPhysicalAudioDevices(m_backend->microphones());
        const qint64 t1 = QDateTime::currentMSecsSinceEpoch();
        auto spk  = filterPhysicalAudioDevices(m_backend->speakers());
        const qint64 t2 = QDateTime::currentMSecsSinceEpoch();
        auto cams = m_backend->cameras();
        const qint64 t3 = QDateTime::currentMSecsSinceEpoch();

        QMetaObject::invokeMethod(this, [this, mics, spk, cams, t0, t1, t2, t3, startMs]() {
            if (!m_refreshing) {
                Logger::instance().warn(LogCategory::Media,
                    "MediaDeviceManager: enumeration completed after timeout, results discarded");
                return;
            }
            m_microphones = mics;
            m_speakers    = spk;
            m_cameras     = cams;
            m_loaded      = true;
            m_refreshing  = false;
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Audio input enumeration finished: %1 device(s) in %2ms")
                    .arg(mics.size()).arg(t1 - t0));
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Audio output enumeration finished: %1 device(s) in %2ms")
                    .arg(spk.size()).arg(t2 - t1));
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Video device enumeration finished: %1 device(s) in %2ms")
                    .arg(cams.size()).arg(t3 - t2));
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("MediaDeviceManager: total enumeration time %1ms")
                    .arg(t3 - startMs));
            emit devicesChanged();
            emit refreshFinished();
        }, Qt::QueuedConnection);
    });

    thread->setObjectName("DeviceEnumThread");
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();

    // Defensive timeout: if the worker doesn't finish within 5 s, unblock the UI.
    QTimer::singleShot(5000, this, [this]() {
        if (!m_refreshing) return;
        m_refreshing = false;
        Logger::instance().warn(LogCategory::Media,
            "MediaDeviceManager: device enumeration timeout after 5000ms");
        emit refreshFinished();
    });
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
