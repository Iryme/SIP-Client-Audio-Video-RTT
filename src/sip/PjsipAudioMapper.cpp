#include "PjsipAudioMapper.h"
#include "core/Logger.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <pjmedia/audiodev.h>
#endif

// ---------------------------------------------------------------------------
// Internal helper — find the best PJSIP device index by display-name match.
// Matching strategy (highest score wins):
//   2 — case-insensitive exact match
//   1 — one name is a substring of the other (case-insensitive)
//   0 — no match; returns -1 (PJSIP default)
// ---------------------------------------------------------------------------

#ifdef HAVE_PJSIP
static int findDeviceIndex(const QString &displayName, bool isCapture)
{
    if (displayName.isEmpty())
        return isCapture ? PJMEDIA_AUD_DEFAULT_CAPTURE_DEV
                         : PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

    try {
        pj::AudDevManager &adm = pj::Endpoint::instance().audDevManager();
        const int count = adm.getDevCount();
        const QString needle = displayName.toLower();

        int bestIdx   = -1;
        int bestScore = 0;

        for (int i = 0; i < count; ++i) {
            try {
                pj::AudioDevInfo info = adm.getDevInfo(i);

                if (isCapture  && info.inputCount  == 0) continue;
                if (!isCapture && info.outputCount == 0) continue;

                const QString name = QString::fromStdString(info.name).toLower();

                int score = 0;
                if (name == needle)
                    score = 2;
                else if (name.contains(needle) || needle.contains(name))
                    score = 1;

                if (score > bestScore) {
                    bestScore = score;
                    bestIdx   = i;
                }
            } catch (...) {}
        }

        return bestIdx; // -1 when no match
    } catch (...) {
        return -1;
    }
}
#endif // HAVE_PJSIP

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void PjsipAudioMapper::logAllDevices()
{
#ifdef HAVE_PJSIP
    try {
        pj::AudDevManager &adm = pj::Endpoint::instance().audDevManager();
        const int count = adm.getDevCount();
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("PJSIP audio devices (%1 total):").arg(count));
        for (int i = 0; i < count; ++i) {
            try {
                pj::AudioDevInfo info = adm.getDevInfo(i);
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("  [%1] \"%2\"  in=%3 out=%4")
                        .arg(i)
                        .arg(QString::fromStdString(info.name))
                        .arg(info.inputCount)
                        .arg(info.outputCount));
            } catch (...) {}
        }
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("PJSIP current audio selection: capture=%1 playback=%2")
                .arg(adm.getCaptureDev())
                .arg(adm.getPlaybackDev()));
    } catch (...) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("PjsipAudioMapper: failed to enumerate audio devices"));
    }
#endif
}

int PjsipAudioMapper::findCaptureIndex(const QString &displayName)
{
#ifdef HAVE_PJSIP
    return findDeviceIndex(displayName, /*isCapture=*/true);
#else
    Q_UNUSED(displayName)
    return -1;
#endif
}

int PjsipAudioMapper::findPlaybackIndex(const QString &displayName)
{
#ifdef HAVE_PJSIP
    return findDeviceIndex(displayName, /*isCapture=*/false);
#else
    Q_UNUSED(displayName)
    return -1;
#endif
}

bool PjsipAudioMapper::applyDevices(int captureIdx, int playbackIdx)
{
#ifdef HAVE_PJSIP
    try {
        pj::AudDevManager &adm = pj::Endpoint::instance().audDevManager();
        adm.setCaptureDev(captureIdx);
        adm.setPlaybackDev(playbackIdx);
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("PJSIP audio devices applied: capture=%1 playback=%2")
                .arg(captureIdx).arg(playbackIdx));
        return true;
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("PjsipAudioMapper: applyDevices failed: %1")
                .arg(QString::fromStdString(e.reason)));
        return false;
    }
#else
    Q_UNUSED(captureIdx)
    Q_UNUSED(playbackIdx)
    return false;
#endif
}

bool PjsipAudioMapper::applyDevicesByName(const QString &captureName,
                                          const QString &playbackName)
{
    const int captureIdx  = findCaptureIndex(captureName);
    const int playbackIdx = findPlaybackIndex(playbackName);

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("PjsipAudioMapper: \"%1\" → capture[%2],  \"%3\" → playback[%4]")
            .arg(captureName.isEmpty() ? QStringLiteral("(default)") : captureName)
            .arg(captureIdx)
            .arg(playbackName.isEmpty() ? QStringLiteral("(default)") : playbackName)
            .arg(playbackIdx));

    return applyDevices(captureIdx, playbackIdx);
}
