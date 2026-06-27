#pragma once
#include <QObject>
#include <QString>

// Available application themes.
enum class AppTheme {
    Auto = 0,
    Dark,
    Light,
    Fluent,
    Aqua,
    FusionModern,
    MaterialDark,
    MaterialLight,
};

inline QString appThemeName(AppTheme t)
{
    switch (t) {
    case AppTheme::Auto:          return QStringLiteral("Auto");
    case AppTheme::Dark:          return QStringLiteral("Dark");
    case AppTheme::Light:         return QStringLiteral("Light");
    case AppTheme::Fluent:        return QStringLiteral("Fluent");
    case AppTheme::Aqua:          return QStringLiteral("Aqua");
    case AppTheme::FusionModern:  return QStringLiteral("Fusion Modern");
    case AppTheme::MaterialDark:  return QStringLiteral("Material Dark");
    case AppTheme::MaterialLight: return QStringLiteral("Material Light");
    }
    return {};
}

// Singleton that owns the application-wide QSS.
// Calling apply() replaces qApp->styleSheet() without rebuilding any widget.
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    static ThemeManager &instance();

    AppTheme currentTheme() const { return m_current; }

    // Apply theme and save to QSettings immediately.
    void apply(AppTheme theme);

    // Read saved theme from QSettings and apply. Called once at startup.
    void applyFromSettings();

signals:
    void themeChanged(AppTheme theme);

private:
    ThemeManager();

    static QString buildQss(AppTheme theme);

    AppTheme m_current{AppTheme::Auto};
};
