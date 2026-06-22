#pragma once
#include <QByteArray>
#include <QSettings>
#include <QString>

#include "core/Logger.h"

// ApplicationSettings — typed, persistent application settings via QSettings INI.
//
// Organization: "SIPClient"   Application: "SIPClient"
// Stored in user-scope INI; never contains passwords or secrets.
// Credentials use the OS keychain (Windows Credential Manager / libsecret) — see ADR-009.
//
// Usage:
//   ApplicationSettings::instance().diagLevelEnabled(LogLevel::Debug);
//   ApplicationSettings::instance().setDiagLevelEnabled(LogLevel::Debug, true);
//
// Test isolation: construct directly with a custom org/app name so tests
// never read or write the real user settings.

class ApplicationSettings
{
public:
    // Production singleton — org="SIPClient", app="SIPClient"
    static ApplicationSettings &instance();

    // Testable constructor — use a unique org/app per test to isolate state.
    explicit ApplicationSettings(const QString &org, const QString &app,
                                  QSettings::Format format = QSettings::IniFormat);

    // -----------------------------------------------------------------------
    // Diagnostics level toggles
    // -----------------------------------------------------------------------
    bool diagLevelEnabled(LogLevel level) const;
    void setDiagLevelEnabled(LogLevel level, bool enabled);

    // -----------------------------------------------------------------------
    // Diagnostics UI state
    // -----------------------------------------------------------------------
    QString diagCategoryFilter() const;
    void    setDiagCategoryFilter(const QString &filter);

    // -----------------------------------------------------------------------
    // Window geometry and layout
    // -----------------------------------------------------------------------
    QByteArray windowGeometry() const;
    void       setWindowGeometry(const QByteArray &geom);

    QByteArray windowState() const;
    void       setWindowState(const QByteArray &state);

    QByteArray splitterState(const QString &key) const;
    void       setSplitterState(const QString &key, const QByteArray &state);

    // -----------------------------------------------------------------------
    // Theme
    // -----------------------------------------------------------------------
    QString theme() const;        // default: "dark"
    void    setTheme(const QString &theme);

    // -----------------------------------------------------------------------
    // Placeholders — filled in by later tasks
    // -----------------------------------------------------------------------
    QString selectedSipProfileId() const;
    void    setSelectedSipProfileId(const QString &id);

    QString selectedMicrophoneId() const;
    void    setSelectedMicrophoneId(const QString &id);

    QString selectedSpeakerId() const;
    void    setSelectedSpeakerId(const QString &id);

    QString selectedCameraId() const;
    void    setSelectedCameraId(const QString &id);

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------
    void resetToDefaults();
    void sync();

private:
    // Default for each log level — RAW is always false to prevent accidental activation.
    static bool levelDefault(LogLevel level);
    static QString levelKey(LogLevel level);

    QSettings m_settings;
};
