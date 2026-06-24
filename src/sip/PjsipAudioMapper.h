#pragma once
#include <QString>

// Maps Qt Multimedia device display names to PJSIP AudDevManager integer indices.
// All public methods are safe to call regardless of whether PJSIP is compiled in;
// they are no-ops that return -1 / false when HAVE_PJSIP is not defined.
class PjsipAudioMapper
{
public:
    // Log all PJSIP audio devices with index, name, and I/O capability.
    // Call once after PJSIP libStart().
    static void logAllDevices();

    // Find the best-matching PJSIP capture device index for the given Qt display
    // name. Returns -1 (PJSIP default) if displayName is empty or no match found.
    static int findCaptureIndex(const QString &displayName);

    // Find the best-matching PJSIP playback device index for the given Qt display
    // name. Returns -1 (PJSIP default) if displayName is empty or no match found.
    static int findPlaybackIndex(const QString &displayName);

    // Apply capture and playback indices to the PJSIP AudDevManager.
    // An index of -1 sets the PJSIP default device for that direction.
    // Returns true on success; false if PJSIP is unavailable or an error occurs.
    static bool applyDevices(int captureIdx, int playbackIdx);

    // Convenience: look up indices by display name and apply in one call.
    // Empty name → -1 (PJSIP default) for that direction.
    static bool applyDevicesByName(const QString &captureName,
                                   const QString &playbackName);
};
