#pragma once

#include <QWidget>
#include "gui/theme/ThemeManager.h"

class SidebarPanel;
class VideoSettingsPanel;
class QTabWidget;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);
    void reload();
    void focusVideoTab();

private slots:
    void onLoadDefaults();
    void onSave();
    void onThemeChanged(int comboIndex);

private:
    void load();
    void applyDebugToggle(bool enabled);
    void loadTextAppearance();
    void saveTextAppearance();

    SidebarPanel      *m_accountsPanel{nullptr};
    VideoSettingsPanel *m_videoSettings{nullptr};
    QTabWidget        *m_tabs{nullptr};

    // Connection tab
    QLineEdit  *m_serverIp{nullptr};
    QLineEdit  *m_sipDomain{nullptr};
    QLineEdit  *m_sipPort{nullptr};
    QLineEdit  *m_wsUrl{nullptr};
    QCheckBox  *m_debugSIP{nullptr};
    QCheckBox  *m_rawSIP{nullptr};
    QCheckBox  *m_persistMedia{nullptr};
    QPushButton *m_save{nullptr};
    QPushButton *m_reset{nullptr};

    // Appearance tab
    QComboBox  *m_themeCombo{nullptr};

    // Text / Accessibility tab
    QComboBox *m_rttFontFamily{nullptr};
    QSpinBox  *m_rttFontSize{nullptr};
    QCheckBox *m_rttBold{nullptr};
    QCheckBox *m_rttHighContrast{nullptr};
    QComboBox *m_lmpeFontFamily{nullptr};
    QSpinBox      *m_lmpeFontSize{nullptr};
    QCheckBox     *m_lmpeBold{nullptr};
    QCheckBox     *m_lmpeHighContrast{nullptr};
};
