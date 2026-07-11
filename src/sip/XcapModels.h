#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

// XCAP Foundation (Task W099) data model — RFC 4825 (XCAP) client-side
// building blocks. This stage treats every AUID (pres-rules, resource-lists,
// rls-services, xcap-caps, ...) as a generic XML document: no per-AUID
// schema/semantics are implemented here (that is deferred to a future task).
//
// UI code must never talk to QNetworkReply/QNetworkAccessManager directly —
// only to these plain-data structs, mirroring the PresenceInfo /
// "UI never binds to pjsua2 objects" rule from Task W098.

enum class XcapHttpMethod { Get, Put, Delete, Head };

inline QString xcapHttpMethodToString(XcapHttpMethod m)
{
    switch (m) {
    case XcapHttpMethod::Get:    return QStringLiteral("GET");
    case XcapHttpMethod::Put:    return QStringLiteral("PUT");
    case XcapHttpMethod::Delete: return QStringLiteral("DELETE");
    case XcapHttpMethod::Head:   return QStringLiteral("HEAD");
    }
    return QStringLiteral("GET");
}

enum class XcapAuthMode { None, Basic, Digest };

inline QString xcapAuthModeToString(XcapAuthMode a)
{
    switch (a) {
    case XcapAuthMode::None:   return QStringLiteral("none");
    case XcapAuthMode::Basic:  return QStringLiteral("basic");
    case XcapAuthMode::Digest: return QStringLiteral("digest");
    }
    return QStringLiteral("none");
}

inline XcapAuthMode xcapAuthModeFromString(const QString &raw)
{
    const QString v = raw.trimmed().toLower();
    if (v == QLatin1String("basic"))  return XcapAuthMode::Basic;
    if (v == QLatin1String("digest")) return XcapAuthMode::Digest;
    return XcapAuthMode::None;
}

enum class XcapParseStatus { NotApplicable, Ok, Partial, Error };

inline QString xcapParseStatusToString(XcapParseStatus s)
{
    switch (s) {
    case XcapParseStatus::NotApplicable: return QStringLiteral("n/a");
    case XcapParseStatus::Ok:            return QStringLiteral("ok");
    case XcapParseStatus::Partial:       return QStringLiteral("partial");
    case XcapParseStatus::Error:         return QStringLiteral("error");
    }
    return QStringLiteral("n/a");
}

// Server-level configuration — never hardcoded; always sourced from
// profile/config/UI (AppSettings-backed in the UI layer).
struct XcapServerConfig
{
    QString rootUri;              // e.g. https://<xcap-host>/xcap-root
    QString xui;                  // XCAP User Identifier (path segment under .../users/<xui>/...)
    QString username;             // auth username; password is never stored here (CredentialStore)
    XcapAuthMode authMode{XcapAuthMode::None};
    bool validateXmlBeforePut{true};
    int  timeoutSeconds{15};
    bool verifyTls{true};
};

// Identifies one XCAP document (RFC 4825 document selector), optionally
// narrowed to a node inside it via a node selector.
struct XcapDocument
{
    QString auid;             // e.g. "resource-lists", "pres-rules", "rls-services", "xcap-caps"
    QString xui;              // overrides XcapServerConfig::xui when non-empty; empty => global document
    QString documentName{QStringLiteral("index")};
    QString nodeSelector;     // optional RFC 4825 node selector, appended after "~~"

    // Builds the full document URI: {root}/{auid}/{users/<xui>|global}/{documentName}[~~nodeSelector]
    // defaultXui is used when this document's own xui is empty (e.g. the
    // server config's XUI); still empty after that ⇒ a "global" document.
    QString buildUri(const QString &rootUri, const QString &defaultXui = QString()) const
    {
        QString root = rootUri;
        while (root.endsWith(QLatin1Char('/')))
            root.chop(1);

        const QString effectiveXui = xui.trimmed().isEmpty() ? defaultXui.trimmed() : xui.trimmed();

        QString path = root + QLatin1Char('/') + auid.trimmed();
        if (effectiveXui.isEmpty())
            path += QStringLiteral("/global/");
        else
            path += QStringLiteral("/users/") + effectiveXui + QLatin1Char('/');
        path += documentName.trimmed();

        if (!nodeSelector.trimmed().isEmpty())
            path += QStringLiteral("~~") + nodeSelector.trimmed();

        return path;
    }
};

// One outbound XCAP operation request (before execution).
struct XcapOperation
{
    XcapHttpMethod method{XcapHttpMethod::Get};
    XcapDocument   document;
    QString        rootUri;
    QString        content;   // XML body, PUT only
    QDateTime      timestamp;
};

// Outcome of an executed XCAP operation — the only XCAP-related type the UI
// ever binds to directly.
struct XcapResult
{
    XcapHttpMethod method{XcapHttpMethod::Get};
    QString rootUri;
    QString auid;
    QString xui;
    QString documentSelector;
    QString nodeSelector;
    QString urlRedacted;

    int     httpStatus{0};
    QString httpReason;
    QString contentType;
    qint64  contentLength{-1};
    QString etag;
    QString lastModified;

    QDateTime timestamp;
    qint64    durationMs{0};

    XcapParseStatus parseStatus{XcapParseStatus::NotApplicable};
    QStringList     warnings;

    // Safe, size-capped preview of the response/request body — never the
    // full raw bytes (see XcapClient::kMaxBodyPreviewBytes).
    QString bodyPreview;

    bool    networkError{false};
    QString errorString;

    bool ok() const { return !networkError && httpStatus >= 200 && httpStatus < 300; }
};

Q_DECLARE_METATYPE(XcapResult)
