#pragma once
#include <QString>
#include <QUrl>

#include "sip/XcapModels.h"

// Pure request-building logic (Task W099, requirement 12: "GET/PUT/DELETE
// request model" tests) — separated from XcapClient's actual network
// execution so it is fully unit-testable without a live QNetworkAccessManager/
// server, mirroring the "separate the callback -> model mapping" testing
// approach used for pjsua2 callbacks in Task W098.
namespace XcapRequestBuilder {

struct BuiltRequest
{
    QUrl    url;
    QString method;          // "GET" / "PUT" / "DELETE" / "HEAD"
    QString contentType;     // set for PUT only ("application/xml" default)
    int     timeoutMs{15000};
    bool    verifyTls{true};
    QString auid;
    QString xui;
    QString documentSelector;
    QString nodeSelector;
    QString rootUri;
};

// contentType is only meaningful for PUT; pass an empty string for
// GET/DELETE/HEAD (no request body).
BuiltRequest build(XcapHttpMethod method,
                    const XcapServerConfig &config,
                    const XcapDocument &document,
                    const QString &contentType = QStringLiteral("application/xml"));

} // namespace XcapRequestBuilder
