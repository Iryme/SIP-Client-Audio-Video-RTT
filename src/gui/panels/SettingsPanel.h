#pragma once

#include <QWidget>

class QCheckBox;
class QLineEdit;
class QPushButton;

class SettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);

private slots:
    void onLoadDefaults();
    void onSave();

private:
    void load();
    void applyDebugToggle(bool enabled);

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
