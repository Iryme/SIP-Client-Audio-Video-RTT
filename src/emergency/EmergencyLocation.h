#pragma once

#include <QString>
#include <QStringList>

enum class LocationSource {
    Unknown,
    Manual,
    Static,
    WindowsLocation
};

QString locationSourceName(LocationSource source);

// Pure data struct representing a geodetic location for NG112 emergency calls.
// No PJSIP dependency — testable without a SIP build.
//
// latitude/longitude follow WGS-84 (EPSG:4326).
// uncertaintyMeters < 0 means "not set"; set via setUncertainty().
// timestamp must be UTC ISO-8601 (e.g. "2026-06-27T12:00:00Z").
struct EmergencyLocation
{
    double   latitude          = 0.0;   // degrees, -90..90
    double   longitude         = 0.0;   // degrees, -180..180
    double   altitude          = 0.0;   // meters above WGS-84 ellipsoid
    bool     hasAltitude       = false;
    double   uncertaintyMeters = -1.0;  // < 0 means not set
    QString  timestamp;                 // UTC ISO-8601
    QString  civicAddress;              // optional free-form text placeholder
    LocationSource source      = LocationSource::Unknown;

    bool        isValid()          const;
    QStringList validationErrors() const;

    // Convenience factory for a simple static point.
    static EmergencyLocation makeStatic(double latitude,
                                        double longitude,
                                        const QString &timestamp);
};
