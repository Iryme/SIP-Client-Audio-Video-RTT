#include "emergency/EmergencyCallProfile.h"

bool EmergencyCallProfile::isValid() const
{
    return validationError().isEmpty();
}

QString EmergencyCallProfile::validationError() const
{
    if (serviceUrn.isEmpty())
        return QStringLiteral("serviceUrn must not be empty");
    if (!serviceUrn.startsWith(QStringLiteral("urn:service:")))
        return QStringLiteral("serviceUrn must start with 'urn:service:'");
    if (routingTarget.isEmpty())
        return QStringLiteral("routingTarget must not be empty");
    return {};
}

EmergencyCallProfile EmergencyCallProfile::makeSos(const QString &routingTarget,
                                                    const QString &callerDisplayName)
{
    EmergencyCallProfile p;
    p.serviceUrn        = QStringLiteral("urn:service:sos");
    p.routingTarget     = routingTarget;
    p.callerDisplayName = callerDisplayName;
    return p;
}
