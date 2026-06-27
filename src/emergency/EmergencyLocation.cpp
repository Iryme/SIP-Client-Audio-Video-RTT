#include "emergency/EmergencyLocation.h"

QString locationSourceName(LocationSource source)
{
    switch (source) {
    case LocationSource::Unknown:         return QStringLiteral("Unknown");
    case LocationSource::Manual:          return QStringLiteral("Manual");
    case LocationSource::Static:          return QStringLiteral("Static");
    case LocationSource::WindowsLocation: return QStringLiteral("WindowsLocation");
    }
    return QStringLiteral("Unknown");
}

QStringList EmergencyLocation::validationErrors() const
{
    QStringList errors;

    if (latitude < -90.0 || latitude > 90.0)
        errors.append(QStringLiteral("latitude must be in range -90..90"));

    if (longitude < -180.0 || longitude > 180.0)
        errors.append(QStringLiteral("longitude must be in range -180..180"));

    if (uncertaintyMeters >= 0.0 && uncertaintyMeters < 0.0)
        errors.append(QStringLiteral("uncertaintyMeters must be >= 0 if set"));

    // uncertaintyMeters negative is "not set" — allowed.
    // The only forbidden value is a negative non-sentinel: we store sentinel as < 0.
    // Nothing to check there beyond the guard above.

    if (timestamp.trimmed().isEmpty())
        errors.append(QStringLiteral("timestamp must not be empty"));

    return errors;
}

bool EmergencyLocation::isValid() const
{
    return validationErrors().isEmpty();
}

EmergencyLocation EmergencyLocation::makeStatic(double lat,
                                                  double lon,
                                                  const QString &ts)
{
    EmergencyLocation loc;
    loc.latitude   = lat;
    loc.longitude  = lon;
    loc.timestamp  = ts;
    loc.source     = LocationSource::Static;
    return loc;
}
