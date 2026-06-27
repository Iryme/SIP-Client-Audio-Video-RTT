#include "emergency/EmergencyLocationProvider.h"

QString locationStatusName(LocationStatus status)
{
    switch (status) {
    case LocationStatus::Unavailable:    return QStringLiteral("Unavailable");
    case LocationStatus::NotImplemented: return QStringLiteral("NotImplemented");
    case LocationStatus::Available:      return QStringLiteral("Available");
    }
    return QStringLiteral("Unknown");
}

// ---- EmergencyLocationProvider ------------------------------------------

EmergencyLocationProvider::EmergencyLocationProvider(QObject *parent)
    : QObject(parent)
{
}

EmergencyLocationProvider::~EmergencyLocationProvider() = default;

// ---- NullLocationProvider -----------------------------------------------

NullLocationProvider::NullLocationProvider(QObject *parent)
    : EmergencyLocationProvider(parent)
{
}

LocationStatus NullLocationProvider::status() const
{
    return LocationStatus::NotImplemented;
}

QString NullLocationProvider::pidfLo() const
{
    return {};
}

void NullLocationProvider::requestLocation()
{
    // Not implemented in Task 34 — emit status so callers can react.
    emit locationStatusChanged(LocationStatus::NotImplemented);
}
