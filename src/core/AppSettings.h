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
};
