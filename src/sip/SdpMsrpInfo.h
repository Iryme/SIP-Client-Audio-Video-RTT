#pragma once
#include <QString>

// SDP MSRP media-line diagnostics, per RFC 4975 / RFC 4976. Read-only —
// detected/logged/displayed only; no MSRP session is ever started from
// this data.
struct SdpMsrpInfo
{
    bool    present{false};          // an "m=message" line was found
    QString mediaLine;               // raw "m=message ..." line
    QString transportProtocol;       // "TCP/MSRP" or "TCP/TLS/MSRP"
    QString path;                    // a=path value (space-separated MSRP URI list)
    QString acceptTypes;             // a=accept-types value
    QString setup;                   // a=setup value (active/passive/actpass)
    QString connection;              // a=connection value (new/existing)
    QString sessionId;               // extracted from the last a=path URI, if possible
};
