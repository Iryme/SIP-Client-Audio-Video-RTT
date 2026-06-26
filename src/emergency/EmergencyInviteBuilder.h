#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "emergency/EmergencyCallProfile.h"

// One SIP header name/value pair.
struct EmergencySipHeader
{
    QString name;
    QString value;
};

// Media requirements for an NG112 INVITE.
// Default: audio + RTT required, video not allowed.
struct EmergencyMediaPolicy
{
    bool requireAudio = true;
    bool requireRtt   = true;
    bool allowVideo   = false;
};

// Declarative, PJSIP-free representation of an NG112 SIP INVITE.
//
// Produced by EmergencyInviteBuilder::build(). Contains all the information
// a future SipCall/SipManager integration layer needs to construct a real
// PJSIP INVITE without coupling the builder to the PJSIP stack.
//
// contentType and body are placeholders — populated in Task 38 (PIDF-LO /
// multipart/mixed). Keeping them here now avoids a breaking struct change later.
struct EmergencyInvite
{
    QString                   requestUri;        // PSAP SIP URI (Request-URI)
    QString                   routeTarget;       // Route: header value (initially == requestUri)
    QString                   serviceUrn;        // RFC 5031 URN, e.g. urn:service:sos
    QList<EmergencySipHeader> headers;           // Ordered extra SIP headers
    QString                   contentType;       // placeholder — empty until Task 38
    QString                   body;              // placeholder — empty until Task 38
    EmergencyMediaPolicy      mediaPolicy;
    bool                      hasLocation      = false;
    bool                      locationRequired = false;
};

// Validation result returned by EmergencyInviteBuilder::validate().
// errors must be empty for isValid() to return true; warnings are informational.
struct EmergencyInviteValidationResult
{
    bool        isValid()  const { return errors.isEmpty(); }
    QStringList errors;
    QStringList warnings;
};

// Builds an EmergencyInvite from an EmergencyCallProfile plus optional
// configuration. No PJSIP headers are included — the output is a pure data
// struct testable without a PJSIP build.
//
// Usage:
//   EmergencyInvite inv = EmergencyInviteBuilder(profile)
//       .setLocationAvailable(true)
//       .setLocationRequired(false)
//       .build();
//   auto result = EmergencyInviteBuilder::validate(inv);
class EmergencyInviteBuilder
{
public:
    explicit EmergencyInviteBuilder(const EmergencyCallProfile &profile);

    EmergencyInviteBuilder &setMediaPolicy(const EmergencyMediaPolicy &policy);
    EmergencyInviteBuilder &setLocationAvailable(bool available);
    EmergencyInviteBuilder &setLocationRequired(bool required);

    // Build the declarative INVITE struct. Always succeeds; call validate() to
    // check whether the result is safe to use.
    EmergencyInvite build() const;

    // Validate an EmergencyInvite. Returns errors for hard failures and
    // warnings for conditions that are allowed but noteworthy.
    static EmergencyInviteValidationResult validate(const EmergencyInvite &invite);

private:
    EmergencyCallProfile m_profile;
    EmergencyMediaPolicy m_mediaPolicy;
    bool                 m_locationAvailable = false;
    bool                 m_locationRequired  = false;
};
