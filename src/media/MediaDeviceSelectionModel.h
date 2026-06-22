#pragma once
#include <QObject>
#include "media/MediaDevice.h"
#include "media/MediaDeviceManager.h"

// Handles device selection persistence and fallback logic.
// Reads/writes AppSettings (media/device/*).
// Separated from MediaDeviceManager so selection logic is unit-testable
// without real QMediaDevices hardware.
class MediaDeviceSelectionModel : public QObject
{
    Q_OBJECT
public:
    explicit MediaDeviceSelectionModel(MediaDeviceManager *manager,
                                       QObject *parent = nullptr);

    // Resolved device: persisted id if still available, otherwise default.
    // Returns null device if no devices of that type exist.
    MediaDevice selectedMicrophone() const;
    MediaDevice selectedSpeaker()    const;
    MediaDevice selectedCamera()     const;

    // Persist a user selection. Falls through to selectDefault if id is empty.
    void selectMicrophone(const QString &id);
    void selectSpeaker   (const QString &id);
    void selectCamera    (const QString &id);

    // Re-resolve all selections (call after refreshDevices()).
    void refresh();

signals:
    void microphoneSelectionChanged(const MediaDevice &device);
    void speakerSelectionChanged   (const MediaDevice &device);
    void cameraSelectionChanged    (const MediaDevice &device);

private:
    MediaDevice resolve(MediaDeviceType type, const QString &persistedId) const;

    static QString loadPersistedId(MediaDeviceType type);
    static void    savePersistedId(MediaDeviceType type, const QString &id);
    static QString settingsKey(MediaDeviceType type);

    MediaDeviceManager *m_manager{nullptr};
};
