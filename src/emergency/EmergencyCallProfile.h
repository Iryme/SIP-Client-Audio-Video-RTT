#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

// Data-only struct holding the parameters for an emergency call.
// No PJSIP dependency — pure data + validation.
//
// Service URNs follow RFC 5031 (urn:service:sos, urn:service:sos.police, etc.).
// routingTarget is a SIP URI for the PSAP — not used for real routing until Task 36.
// pidfLo is a PIDF-LO XML body (RFC 4119) — populated by EmergencyLocationProvider.
struct EmergencyCallProfile
{
    // urn:service:sos, urn:service:sos.police, urn:service:sos.fire, etc.
    QString serviceUrn;

    // SIP URI of the target PSAP (e.g. sip:psap@ng112.example.com)
    QString routingTarget;

    // From display name for the emergency INVITE
    QString callerDisplayName;

    // PIDF-LO XML — empty until EmergencyLocationProvider supplies it
    QString pidfLo;

    // RFC 7852 Additional Data URIs — reserved for future use
    QStringList additionalDataUris;

    // Returns true if the profile is minimally valid for initiating a call.
    bool isValid() const;

    // Human-readable description of the first validation error, empty if valid.
    QString validationError() const;

    // Convenience factory: SOS call with the given routing target.
    static EmergencyCallProfile makeSos(const QString &routingTarget,
                                        const QString &callerDisplayName = {});
};

Q_DECLARE_METATYPE(EmergencyCallProfile)
