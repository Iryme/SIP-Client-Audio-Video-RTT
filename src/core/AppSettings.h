#pragma once
// Thin backward-compatibility shim. New code should use ApplicationSettings directly.
#include "settings/ApplicationSettings.h"

class AppSettings
{
public:
    static void saveWindowGeometry(const QByteArray &geom)
        { ApplicationSettings::instance().setWindowGeometry(geom); }
    static QByteArray loadWindowGeometry()
        { return ApplicationSettings::instance().windowGeometry(); }

    static void saveWindowState(const QByteArray &state)
        { ApplicationSettings::instance().setWindowState(state); }
    static QByteArray loadWindowState()
        { return ApplicationSettings::instance().windowState(); }

    static void saveSplitterState(const QString &key, const QByteArray &s)
        { ApplicationSettings::instance().setSplitterState(key, s); }
    static QByteArray loadSplitterState(const QString &key)
        { return ApplicationSettings::instance().splitterState(key); }

    static void setLogLevelEnabled(const QString &level, bool on)
        { ApplicationSettings::instance().setDiagLevelEnabled(levelFromString(level), on); }
    static bool isLogLevelEnabled(const QString &level, bool def)
    {
        Q_UNUSED(def) // ApplicationSettings returns the canonical default internally
        return ApplicationSettings::instance().diagLevelEnabled(levelFromString(level));
    }

private:
    static LogLevel levelFromString(const QString &s)
    {
        if (s.compare("WARN",  Qt::CaseInsensitive) == 0) return LogLevel::Warn;
        if (s.compare("ERROR", Qt::CaseInsensitive) == 0) return LogLevel::Error;
        if (s.compare("DEBUG", Qt::CaseInsensitive) == 0) return LogLevel::Debug;
        if (s.compare("RAW",   Qt::CaseInsensitive) == 0) return LogLevel::Raw;
        return LogLevel::Info;
    }
};
