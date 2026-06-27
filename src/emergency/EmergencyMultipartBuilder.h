#pragma once
#include <QList>
#include <QPair>
#include <QString>

// One MIME part destined for a SIP multipart/mixed body.
// Produced by EmergencyMultipartBuilder; consumed by SipCall::makeCallWithOptions().
struct MultipartPart {
    QString contentType;                    // e.g. "application/pidf+xml"
    QString contentId;                      // raw ID without angle brackets
    QList<QPair<QString,QString>> headers;  // e.g. Content-ID: <id>
    QString body;

    // Returns "<contentId>" per RFC 2183 (angle brackets required in Content-ID header).
    QString contentIdHeaderValue() const {
        return contentId.isEmpty()
            ? QString()
            : QStringLiteral("<%1>").arg(contentId);
    }
};

// Declarative builder for SIP INVITE multipart/mixed body parts.
// No PJSIP dependency — all output is pure Qt data.
// The SDP part is NOT included here; PJSIP adds it automatically when
// txOption.multipartContentType is set.
class EmergencyMultipartBuilder {
public:
    // Appends a PIDF-LO part. No-op when xml is empty.
    EmergencyMultipartBuilder &addPidfLo(const QString &xml, const QString &contentId);

    QList<MultipartPart> build() const;
    bool isEmpty() const;

private:
    QList<MultipartPart> m_parts;
};
