#pragma once

#include <QString>

#include "emergency/EmergencyLocation.h"

// Result of a PidfLoBuilder::build() call.
struct PidfLoResult
{
    bool    success     = false;
    QString xml;                                           // PIDF-LO XML document
    QString contentType = QStringLiteral("application/pidf+xml"); // MIME content type
    QString contentId;                                     // e.g. "pidflo-1@ng112.local"
    QString error;                                         // non-empty on failure
};

// Builds a minimal PIDF-LO XML document (RFC 4119) from an EmergencyLocation.
// No PJSIP dependency — uses plain QString construction with XML escaping.
//
// Geodetic encoding:
//   - Without uncertainty: gml:Point (RFC 4119 §3)
//   - With uncertainty:    gs:Circle with gs:radius (RFC 5491 §5)
//
// Usage:
//   PidfLoResult r = PidfLoBuilder(loc)
//       .setEntity("pres:user@example.com")
//       .setContentId("pidflo-abc@ng112.local")
//       .build();
//   if (r.success) { ... r.xml ... }
class PidfLoBuilder
{
public:
    explicit PidfLoBuilder(const EmergencyLocation &location);

    PidfLoBuilder &setEntity(const QString &entity);
    PidfLoBuilder &setTupleId(const QString &id);
    PidfLoBuilder &setContentId(const QString &cid);

    PidfLoResult build() const;

    // MIME content type returned for all PIDF-LO documents.
    static QString contentType();

    // Escape XML text content (& < >) — public for testability.
    static QString escapeXml(const QString &text);

    // Escape XML attribute values (& < > ").
    static QString escapeXmlAttr(const QString &text);

private:
    EmergencyLocation m_location;
    QString           m_entity;
    QString           m_tupleId;
    QString           m_contentId;
};
