#pragma once
#include <QObject>

// Singleton that owns the global camera-enabled state.
// Both VideoPanel instances (Clients and Settings) and all Camera On/Off
// buttons subscribe to enabledChanged and call setEnabled() to stay in sync.
// When disabled, every panel must stop its QCamera — this ensures the
// hardware LED turns off regardless of which panel held the camera last.
class CameraController : public QObject
{
    Q_OBJECT
public:
    static CameraController &instance();

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled, const QString &source = {});

signals:
    void enabledChanged(bool enabled);

private:
    explicit CameraController(QObject *parent = nullptr);
    bool m_enabled{true};
};
