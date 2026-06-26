#pragma once

#include <QObject>
#include <QString>

enum class LocationStatus {
    Unavailable,   // Geolocation API not accessible
    NotImplemented // Provider not yet implemented (Task 34 default)
};

QString locationStatusName(LocationStatus status);

// Abstract interface for geolocation providers.
// In Task 34: only NullLocationProvider exists (NotImplemented).
// Future tasks will add GPS, network, or manual-entry providers.
class EmergencyLocationProvider : public QObject
{
    Q_OBJECT
public:
    explicit EmergencyLocationProvider(QObject *parent = nullptr);
    ~EmergencyLocationProvider() override;

    virtual LocationStatus status() const = 0;

    // Returns a PIDF-LO XML string when status == Available, empty otherwise.
    virtual QString pidfLo() const = 0;

    // Trigger an asynchronous location fetch. Results arrive via locationAvailable().
    virtual void requestLocation() = 0;

signals:
    void locationStatusChanged(LocationStatus status);
    void locationAvailable(const QString &pidfLo);
};

// Default implementation — always NotImplemented.
// Used by EmergencyCallController until a real provider is wired in.
class NullLocationProvider : public EmergencyLocationProvider
{
    Q_OBJECT
public:
    explicit NullLocationProvider(QObject *parent = nullptr);

    LocationStatus status() const override;
    QString        pidfLo() const override;
    void           requestLocation() override;
};
