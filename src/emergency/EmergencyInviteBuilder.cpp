#include "emergency/EmergencyInviteBuilder.h"

EmergencyInviteBuilder::EmergencyInviteBuilder(const EmergencyCallProfile &profile)
    : m_profile(profile)
{}

EmergencyInviteBuilder &EmergencyInviteBuilder::setMediaPolicy(const EmergencyMediaPolicy &policy)
{
    m_mediaPolicy = policy;
    return *this;
}

EmergencyInviteBuilder &EmergencyInviteBuilder::setLocationAvailable(bool available)
{
    m_locationAvailable = available;
    return *this;
}

EmergencyInviteBuilder &EmergencyInviteBuilder::setLocationRequired(bool required)
{
    m_locationRequired = required;
    return *this;
}

EmergencyInvite EmergencyInviteBuilder::build() const
{
    EmergencyInvite inv;
    inv.requestUri       = m_profile.routingTarget;
    inv.routeTarget      = m_profile.routingTarget;
    inv.serviceUrn       = m_profile.serviceUrn;
    inv.mediaPolicy      = m_mediaPolicy;
    inv.hasLocation      = m_locationAvailable;
    inv.locationRequired = m_locationRequired;

    // Deterministic header order — same input always produces the same list.
    inv.headers.append({QStringLiteral("Accept"),
                        QStringLiteral("application/sdp")});
    inv.headers.append({QStringLiteral("Supported"),
                        QStringLiteral("geolocation")});

    // Geolocation headers follow RFC 6442 — include only when location is available.
    // The value is a placeholder until the PIDF-LO builder is implemented (Task 38).
    if (m_locationAvailable) {
        inv.headers.append({QStringLiteral("Geolocation"),
                            QStringLiteral("<placeholder-cid@ng112>")});
        inv.headers.append({QStringLiteral("Geolocation-Routing"),
                            QStringLiteral("yes")});
    }

    return inv;
}

EmergencyInviteValidationResult EmergencyInviteBuilder::validate(const EmergencyInvite &invite)
{
    EmergencyInviteValidationResult result;

    // --- hard errors ---

    if (invite.serviceUrn.isEmpty())
        result.errors.append(QStringLiteral("serviceUrn must not be empty"));
    else if (!invite.serviceUrn.startsWith(QStringLiteral("urn:service:")))
        result.errors.append(QStringLiteral("serviceUrn must start with 'urn:service:'"));

    if (invite.requestUri.isEmpty())
        result.errors.append(QStringLiteral("requestUri (routing target) must not be empty"));

    if (!invite.mediaPolicy.requireAudio)
        result.errors.append(QStringLiteral("emergency call must require audio"));

    if (!invite.mediaPolicy.requireRtt)
        result.errors.append(QStringLiteral("emergency call must require RTT (RFC 4103)"));

    if (invite.locationRequired && !invite.hasLocation)
        result.errors.append(QStringLiteral("location is required but unavailable"));

    // --- warnings ---

    if (!invite.mediaPolicy.allowVideo)
        result.warnings.append(QStringLiteral("video is disabled for this emergency call"));

    if (!invite.hasLocation && !invite.locationRequired)
        result.warnings.append(QStringLiteral("location is unavailable and not required"));

    return result;
}
