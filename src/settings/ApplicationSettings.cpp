#include "settings/ApplicationSettings.h"

// -----------------------------------------------------------------------
// Singleton
// -----------------------------------------------------------------------
ApplicationSettings &ApplicationSettings::instance()
{
    static ApplicationSettings s_instance("SIPClient", "SIPClient");
    return s_instance;
}

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
ApplicationSettings::ApplicationSettings(const QString &org, const QString &app,
                                          QSettings::Format format)
    : m_settings(format, QSettings::UserScope, org, app)
{
}

// -----------------------------------------------------------------------
// Diagnostics level toggles
// -----------------------------------------------------------------------
bool ApplicationSettings::diagLevelEnabled(LogLevel level) const
{
    const QString key = "diagnostics/level/" + levelKey(level);
    const bool def    = levelDefault(level);
    const QVariant v  = m_settings.value(key);

    if (!v.isValid())
        return def;

    // QSettings INI stores everything as strings. Qt's toBool() returns true for any
    // non-empty string that isn't "0"/"false"/"no"/"off", so "garbage_value" → true.
    // We validate strictly: only accept the canonical written forms "true"/"false".
    // Any other value (corrupt, hand-edited) falls back to the safe default.
    const QString s = v.toString().trimmed().toLower();
    if (s == "true"  || s == "1") return true;
    if (s == "false" || s == "0") return false;
    return def; // corrupt / unrecognised → safe default
}

void ApplicationSettings::setDiagLevelEnabled(LogLevel level, bool enabled)
{
    m_settings.setValue("diagnostics/level/" + levelKey(level), enabled);
}

// -----------------------------------------------------------------------
// Diagnostics UI state
// -----------------------------------------------------------------------
QString ApplicationSettings::diagCategoryFilter() const
{
    return m_settings.value("diagnostics/category_filter", QString()).toString();
}

void ApplicationSettings::setDiagCategoryFilter(const QString &filter)
{
    m_settings.setValue("diagnostics/category_filter", filter);
}

// -----------------------------------------------------------------------
// Window geometry and layout
// -----------------------------------------------------------------------
QByteArray ApplicationSettings::windowGeometry() const
{
    return m_settings.value("ui/geometry").toByteArray();
}

void ApplicationSettings::setWindowGeometry(const QByteArray &geom)
{
    m_settings.setValue("ui/geometry", geom);
}

QByteArray ApplicationSettings::windowState() const
{
    return m_settings.value("ui/state").toByteArray();
}

void ApplicationSettings::setWindowState(const QByteArray &state)
{
    m_settings.setValue("ui/state", state);
}

QByteArray ApplicationSettings::splitterState(const QString &key) const
{
    return m_settings.value("ui/splitter/" + key).toByteArray();
}

void ApplicationSettings::setSplitterState(const QString &key, const QByteArray &state)
{
    m_settings.setValue("ui/splitter/" + key, state);
}

// -----------------------------------------------------------------------
// Theme
// -----------------------------------------------------------------------
QString ApplicationSettings::theme() const
{
    return m_settings.value("ui/theme", "dark").toString();
}

void ApplicationSettings::setTheme(const QString &theme)
{
    m_settings.setValue("ui/theme", theme);
}

// -----------------------------------------------------------------------
// Placeholders
// -----------------------------------------------------------------------
QString ApplicationSettings::selectedSipProfileId() const
{
    return m_settings.value("sip/profile_id", QString()).toString();
}

void ApplicationSettings::setSelectedSipProfileId(const QString &id)
{
    m_settings.setValue("sip/profile_id", id);
}

QString ApplicationSettings::selectedMicrophoneId() const
{
    return m_settings.value("media/microphone_id", QString()).toString();
}

void ApplicationSettings::setSelectedMicrophoneId(const QString &id)
{
    m_settings.setValue("media/microphone_id", id);
}

QString ApplicationSettings::selectedSpeakerId() const
{
    return m_settings.value("media/speaker_id", QString()).toString();
}

void ApplicationSettings::setSelectedSpeakerId(const QString &id)
{
    m_settings.setValue("media/speaker_id", id);
}

QString ApplicationSettings::selectedCameraId() const
{
    return m_settings.value("media/camera_id", QString()).toString();
}

void ApplicationSettings::setSelectedCameraId(const QString &id)
{
    m_settings.setValue("media/camera_id", id);
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------
void ApplicationSettings::resetToDefaults()
{
    m_settings.remove("diagnostics");
    m_settings.remove("ui");
    m_settings.remove("sip");
    m_settings.remove("media");
    m_settings.sync();
}

void ApplicationSettings::sync()
{
    m_settings.sync();
}

// -----------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------
bool ApplicationSettings::levelDefault(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:  return true;
    case LogLevel::Warn:  return true;
    case LogLevel::Error: return true;
    case LogLevel::Debug: return false;
    case LogLevel::Raw:   return false; // RAW must never be enabled by accident
    }
    return false;
}

QString ApplicationSettings::levelKey(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:  return "info";
    case LogLevel::Warn:  return "warn";
    case LogLevel::Error: return "error";
    case LogLevel::Debug: return "debug";
    case LogLevel::Raw:   return "raw";
    }
    return "info";
}
