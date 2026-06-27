#pragma once

#include <QWidget>

class SidebarPanel;
class VideoSettingsPanel;
class QCheckBox;
class QLineEdit;
class QPushButton;

class SettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);
    void reload();

private slots:
    void onLoadDefaults();
    void onSave();

private:
    void load();
    void applyDebugToggle(bool enabled);

    SidebarPanel      *m_accountsPanel{nullptr};
    VideoSettingsPanel *m_videoSettings{nullptr};
    QLineEdit  *m_serverIp{nullptr};
    QLineEdit  *m_sipDomain{nullptr};
    QLineEdit  *m_sipPort{nullptr};
    QLineEdit  *m_wsUrl{nullptr};
    QCheckBox  *m_debugSIP{nullptr};
    QCheckBox  *m_rawSIP{nullptr};
    QCheckBox  *m_persistMedia{nullptr};
    QPushButton *m_save{nullptr};
    QPushButton *m_reset{nullptr};
};
