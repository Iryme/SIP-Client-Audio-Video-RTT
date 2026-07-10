#pragma once
#include <QSettings>
#include <QString>

// Thin wrapper for consistent settings keys across the application.
// Use QSettings directly for simple cases; extend this class for
// typed accessors as features are implemented.
class AppSettings
{
public:
    static QSettings &settings()
    {
        static QSettings s_settings(
            QSettings::IniFormat,
            QSettings::UserScope,
            "SIPClient", "SIPClient");
        return s_settings;
    }

    // Layout
    static void saveWindowGeometry(const QByteArray &geom)  { settings().setValue("ui/geometry", geom); }
    static QByteArray loadWindowGeometry()                   { return settings().value("ui/geometry").toByteArray(); }
    static void saveWindowState(const QByteArray &state)     { settings().setValue("ui/state", state); }
    static QByteArray loadWindowState()                      { return settings().value("ui/state").toByteArray(); }
    static void saveSplitterState(const QString &key, const QByteArray &s) { settings().setValue("ui/splitter/" + key, s); }
    static QByteArray loadSplitterState(const QString &key)  { return settings().value("ui/splitter/" + key).toByteArray(); }

    // Logging
    static void setLogLevelEnabled(const QString &level, bool on) { settings().setValue("log/level/" + level, on); }
    static bool isLogLevelEnabled(const QString &level, bool def) { return settings().value("log/level/" + level, def).toBool(); }

    // Media device selection
    static void    saveSelectedMicrophone(const QString &id) { settings().setValue("media/device/microphone", id); }
    static QString loadSelectedMicrophone()                  { return settings().value("media/device/microphone").toString(); }
    static void    saveSelectedSpeaker   (const QString &id) { settings().setValue("media/device/speaker", id); }
    static QString loadSelectedSpeaker   ()                  { return settings().value("media/device/speaker").toString(); }
    static void    saveSelectedCamera    (const QString &id) { settings().setValue("media/device/camera", id); }
    static QString loadSelectedCamera    ()                  { return settings().value("media/device/camera").toString(); }

    // Media volume — 0-100, 100 = default/unity gain. Applied to the active
    // call's capture/playback device media (PJSIP AudDevManager) and used as
    // the default for future calls.
    static void saveMicrophoneVolume(int percent) { settings().setValue("media/volume/microphone", percent); }
    static int  loadMicrophoneVolume()             { return settings().value("media/volume/microphone", 100).toInt(); }
    static void saveSpeakerVolume   (int percent) { settings().setValue("media/volume/speaker", percent); }
    static int  loadSpeakerVolume   ()             { return settings().value("media/volume/speaker", 100).toInt(); }

    // Call type selector — persisted as int matching CallType enum
    // (0 = AudioOnly, the default when unset).
    static int  loadLastCallType()        { return settings().value(QStringLiteral("call/lastType"), 0).toInt(); }
    static void saveLastCallType(int type) { settings().setValue(QStringLiteral("call/lastType"), type); }

    // Theme (persisted as int matching AppTheme enum; 0 = Auto)
    static int  savedThemeIndex()        { return settings().value(QStringLiteral("ui/theme"), 0).toInt(); }
    static void saveThemeIndex(int idx)  { settings().setValue(QStringLiteral("ui/theme"), idx); settings().sync(); }

    // Emergency test mode — disabled by default; must be explicitly enabled in the INI file.
    // When false the 112 emergency button is hidden and no emergency call can be initiated.
    // To enable: set emergency/testMode=true in SIPClient.ini (user scope).
    static bool emergencyTestModeEnabled()
    {
        return settings().value("emergency/testMode", false).toBool();
    }
    static void setEmergencyTestModeEnabled(bool on)
    {
        settings().setValue("emergency/testMode", on);
    }

    // Messaging Diagnostics — max MessagingEvent rows retained in memory
    // (oldest evicted first once the limit is exceeded). Diagnostic-only
    // setting; has no effect on SIP/MSRP transport behavior.
    static int  loadMaxMessagingEventsRetained()        { return settings().value(QStringLiteral("messaging/maxEventsRetained"), 1000).toInt(); }
    static void saveMaxMessagingEventsRetained(int max) { settings().setValue(QStringLiteral("messaging/maxEventsRetained"), max); }

    // SIP MESSAGE Foundation (Task W092) — conservative defaults: sending is
    // off until explicitly enabled from the UI, CPIM wrapping is off, and
    // IMDN is not requested by default. None of these affect MSRP, which
    // remains permanently disabled regardless of these settings.
    static bool enableSipMessage()            { return settings().value(QStringLiteral("messaging/enableSipMessage"), false).toBool(); }
    static void setEnableSipMessage(bool on)  { settings().setValue(QStringLiteral("messaging/enableSipMessage"), on); }
    static bool enableCpim()                  { return settings().value(QStringLiteral("messaging/enableCpim"), false).toBool(); }
    static void setEnableCpim(bool on)        { settings().setValue(QStringLiteral("messaging/enableCpim"), on); }
    static bool requestImdnByDefault()        { return settings().value(QStringLiteral("messaging/requestImdnByDefault"), false).toBool(); }
    static void setRequestImdnByDefault(bool on) { settings().setValue(QStringLiteral("messaging/requestImdnByDefault"), on); }

    // IMDN Foundation (Task W096). Auto Send Delivered defaults ON (a
    // "delivered" report is a transport-level acknowledgement with no
    // privacy implication — it does not disclose whether/when the user
    // actually read the message). Auto Send Displayed defaults OFF (it
    // discloses that the user has read the message, so it requires explicit
    // opt-in; when off the user marks messages as read manually). Neither
    // setting affects MSRP, which remains permanently disabled.
    static bool autoSendDeliveredImdn()          { return settings().value(QStringLiteral("messaging/autoSendDeliveredImdn"), true).toBool(); }
    static void setAutoSendDeliveredImdn(bool on) { settings().setValue(QStringLiteral("messaging/autoSendDeliveredImdn"), on); }
    static bool autoSendDisplayedImdn()          { return settings().value(QStringLiteral("messaging/autoSendDisplayedImdn"), false).toBool(); }
    static void setAutoSendDisplayedImdn(bool on) { settings().setValue(QStringLiteral("messaging/autoSendDisplayedImdn"), on); }

    // Lab PSAP target URI — used only in emergency test mode, never a real PSAP.
    static QString emergencyTarget()
    {
        return settings().value("emergency/target",
                                QStringLiteral("sip:psap@10.2.0.180")).toString();
    }
    static void setEmergencyTarget(const QString &uri)
    {
        settings().setValue("emergency/target", uri);
    }
};
