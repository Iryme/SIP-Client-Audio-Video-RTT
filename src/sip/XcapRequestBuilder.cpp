#include "XcapRequestBuilder.h"

namespace XcapRequestBuilder {

BuiltRequest build(XcapHttpMethod method,
                    const XcapServerConfig &config,
                    const XcapDocument &document,
                    const QString &contentType)
{
    BuiltRequest req;
    req.method = xcapHttpMethodToString(method);
    req.url = QUrl(document.buildUri(config.rootUri, config.xui));
    req.timeoutMs = qMax(1, config.timeoutSeconds) * 1000;
    req.verifyTls = config.verifyTls;
    req.auid = document.auid;
    req.xui = document.xui.isEmpty() ? config.xui : document.xui;
    req.documentSelector = document.documentName;
    req.nodeSelector = document.nodeSelector;
    req.rootUri = config.rootUri;

    if (method == XcapHttpMethod::Put)
        req.contentType = contentType;

    return req;
}

} // namespace XcapRequestBuilder
