#include "emergency/StaticLocationProvider.h"
#include "emergency/PidfLoBuilder.h"

StaticLocationProvider::StaticLocationProvider(const EmergencyLocation &location,
                                               QObject *parent)
    : EmergencyLocationProvider(parent)
    , m_location(location)
{
    rebuildCache();
}

void StaticLocationProvider::setLocation(const EmergencyLocation &location)
{
    m_location = location;
    rebuildCache();
    emit locationStatusChanged(status());
}

EmergencyLocation StaticLocationProvider::location() const
{
    return m_location;
}

QString StaticLocationProvider::unavailableReason() const
{
    return m_unavailableReason;
}

LocationStatus StaticLocationProvider::status() const
{
    return m_cachedPidfLo.isEmpty() ? LocationStatus::Unavailable
                                    : LocationStatus::Available;
}

QString StaticLocationProvider::pidfLo() const
{
    return m_cachedPidfLo;
}

void StaticLocationProvider::requestLocation()
{
    if (status() == LocationStatus::Available) {
        emit locationStatusChanged(LocationStatus::Available);
        emit locationAvailable(m_cachedPidfLo);
    } else {
        emit locationStatusChanged(LocationStatus::Unavailable);
    }
}

void StaticLocationProvider::rebuildCache()
{
    m_cachedPidfLo.clear();
    m_unavailableReason.clear();

    PidfLoResult r = PidfLoBuilder(m_location).build();
    if (r.success) {
        m_cachedPidfLo = r.xml;
    } else {
        m_unavailableReason = r.error;
    }
}
