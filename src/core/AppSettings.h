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
