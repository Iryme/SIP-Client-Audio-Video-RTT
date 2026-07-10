#pragma once
#include <QString>

// Diagnostic-only info parsed from an RCS FT HTTP file-transfer descriptor
// (Content-Type: application/vnd.gsma.rcs-ft-http+xml, GSMA RCC.07 / OMA
// CPM). This client never downloads the referenced URL, never accesses the
// network on its behalf, and never auto-displays/opens the referenced file
// — this struct only carries the metadata the descriptor itself declares.
struct RcsFtHttpInfo
{
    bool    present{false};
    QString fileInfoType;   // "file" or "thumbnail" attribute of the primary <file-info>
    QString fileName;
    qint64  fileSize{-1};   // -1 = not present in the descriptor
    QString contentType;
    QString dataUrl;        // full, un-redacted download URL — see UrlRedactor for the safe form
    QString expiresAt;      // <data until="..."> attribute, verbatim
    bool    thumbnailPresent{false};
    QString disposition;    // e.g. "render" / "attachment", when present
    QString playingLength;  // audio/video duration hint, when present
};
