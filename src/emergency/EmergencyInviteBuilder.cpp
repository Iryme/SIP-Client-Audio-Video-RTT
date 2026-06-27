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

EmergencyInviteBuilder &EmergencyInviteBuilder::setContentId(const QString &cid)
{
    m_contentId = cid;
    return *this;
}

EmergencyInvite EmergencyInviteBuilder::build() const
{
    EmergencyInvite inv;
    inv.requestUri       = m_profile.routingTarget;
    inv.routeTarget      = m_profile.routingTarget;
    inv.serviceUrn       = m_profile.serviceUrn;
    inv.mediaPolicy      = m_mediaPolicy;
    inv.locationRequired = m_locationRequired;

    // If the profile carries a PIDF-LO body, populate body/contentType/contentId
    // and treat hasLocation=true. Otherwise fall back to the explicit flag set by
    // the caller (backward-compatible with Task 36 usage).
    const bool hasPidfLo = !m_profile.pidfLo.isEmpty();
    inv.hasLocation      = hasPidfLo || m_locationAvailable;

    if (hasPidfLo) {
        inv.body        = m_profile.pidfLo;
        inv.contentType = QStringLiteral("application/pidf+xml");
        inv.contentId   = m_contentId.isEmpty()
                          ? QStringLiteral("pidflo-1@ng112.local")
                          : m_contentId;
    }

    // Deterministic header order — same input always produces the same list.
    inv.headers.append({QStringLiteral("Accept"),
                        QStringLiteral("application/sdp")});
    inv.headers.append({QStringLiteral("Supported"),
                        QStringLiteral("geolocation")});

    // Geolocation header (RFC 6442) — only when location is available.
    if (inv.hasLocation) {
        const QString geoValue = inv.contentId.isEmpty()
                                 ? QStringLiteral("<placeholder-cid@ng112>")
                                 : QStringLiteral("<cid:%1>").arg(inv.contentId);
        inv.headers.append({QStringLiteral("Geolocation"), geoValue});
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
