#pragma once

#include <QString>

#include "emergency/EmergencyLocation.h"
#include "emergency/EmergencyLocationProvider.h"

// Location provider backed by a static/manually-specified EmergencyLocation.
// No Windows Location API, no network — pure in-process data.
//
// If the location is valid, status() returns Available and requestLocation()
// emits locationAvailable(pidfLo). If the location is invalid, status()
// returns Unavailable and unavailableReason() explains why.
//
// Does NOT take ownership of any external resource.
class StaticLocationProvider : public EmergencyLocationProvider
{
    Q_OBJECT
public:
    explicit StaticLocationProvider(const EmergencyLocation &location,
                                    QObject *parent = nullptr);

    void setLocation(const EmergencyLocation &location);
    EmergencyLocation location()          const;
    QString           unavailableReason() const;

    LocationStatus status()          const override;
    QString        pidfLo()          const override;
    void           requestLocation()       override;

private:
    void rebuildCache();

    EmergencyLocation m_location;
    QString           m_cachedPidfLo;
    QString           m_unavailableReason;
};
