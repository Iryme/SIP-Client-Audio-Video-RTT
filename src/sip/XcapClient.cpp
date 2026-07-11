#include "XcapClient.h"

#include <QAuthenticator>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QSslSocket>

#include "security/CredentialStore.h"
#include "sip/XcapAuthHeaderBuilder.h"
#include "sip/XcapRequestBuilder.h"
#include "sip/XcapUrlRedactor.h"
#include "sip/XcapXmlValidator.h"

namespace {
// XCAP passwords are stored under the same secure-backend mechanism as SIP
// profile credentials (CredentialStore), keyed by this fixed pseudo-profile
// id rather than a per-SIP-profile id — XCAP server config is independent
// of which SIP profile is currently active.
const QString kXcapCredentialProfileId = QStringLiteral("xcap");
} // namespace

XcapClient &XcapClient::instance()
{
    static XcapClient s_instance;
    return s_instance;
}

XcapClient::XcapClient(QObject *parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);

    // Digest credentials are only supplied when the reply carries the
    // xcapDigestUser property (set in execute() only when authMode ==
    // Digest and a stored password was found) — authMode None/Basic replies
    // are left unauthenticated here and simply receive the server's 401.
    connect(m_nam, &QNetworkAccessManager::authenticationRequired, this,
            [](QNetworkReply *reply, QAuthenticator *authenticator) {
        const QString user = reply->property("xcapDigestUser").toString();
        if (user.isEmpty())
            return;
        authenticator->setUser(user);
        authenticator->setPassword(reply->property("xcapDigestPass").toString());
    });
}

void XcapClient::get(const XcapServerConfig &config, const XcapDocument &document)
{
    execute(XcapHttpMethod::Get, config, document, QByteArray());
}

void XcapClient::del(const XcapServerConfig &config, const XcapDocument &document)
{
    execute(XcapHttpMethod::Delete, config, document, QByteArray());
}

void XcapClient::head(const XcapServerConfig &config, const XcapDocument &document)
{
    execute(XcapHttpMethod::Head, config, document, QByteArray());
}

void XcapClient::put(const XcapServerConfig &config, const XcapDocument &document, const QString &xmlContent)
{
    if (config.validateXmlBeforePut) {
        const XcapXmlValidator::Result validation = XcapXmlValidator::validate(xmlContent, false);
        if (!validation.ok) {
            XcapResult result;
            result.method = XcapHttpMethod::Put;
            result.rootUri = config.rootUri;
            result.auid = document.auid;
            result.xui = document.xui.isEmpty() ? config.xui : document.xui;
            result.documentSelector = document.documentName;
            result.nodeSelector = document.nodeSelector;
            result.urlRedacted = XcapUrlRedactor::redact(document.buildUri(config.rootUri, config.xui));
            result.timestamp = QDateTime::currentDateTimeUtc();
            result.parseStatus = XcapParseStatus::Error;
            result.warnings = validation.warnings;
            result.errorString = validation.errorMessage.isEmpty()
                ? QStringLiteral("XML validation failed") : validation.errorMessage;
            emit operationCompleted(result);
            return;
        }
    }
    execute(XcapHttpMethod::Put, config, document, xmlContent.toUtf8());
}

void XcapClient::execute(XcapHttpMethod method, const XcapServerConfig &config,
                          const XcapDocument &document, const QByteArray &body)
{
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();

    const XcapRequestBuilder::BuiltRequest built = XcapRequestBuilder::build(method, config, document);

    QNetworkRequest request(built.url);
    request.setTransferTimeout(built.timeoutMs);

    if (!built.verifyTls) {
        QSslConfiguration sslConfig = request.sslConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        request.setSslConfiguration(sslConfig);
    }

    bool foundPassword = false;
    QString password;
    if (config.authMode != XcapAuthMode::None && !config.username.isEmpty()) {
        password = CredentialStore::instance().loadPassword(
            kXcapCredentialProfileId, config.username, &foundPassword);
    }

    if (config.authMode == XcapAuthMode::Basic && foundPassword) {
        request.setRawHeader("Authorization",
            XcapAuthHeaderBuilder::basicAuthorizationHeader(config.username, password));
    }

    QNetworkReply *reply = nullptr;
    switch (method) {
    case XcapHttpMethod::Get:
        reply = m_nam->get(request);
        break;
    case XcapHttpMethod::Head:
        reply = m_nam->head(request);
        break;
    case XcapHttpMethod::Delete:
        reply = m_nam->deleteResource(request);
        break;
    case XcapHttpMethod::Put:
        request.setHeader(QNetworkRequest::ContentTypeHeader, built.contentType);
        reply = m_nam->put(request, body);
        break;
    }

    if (config.authMode == XcapAuthMode::Digest && foundPassword) {
        reply->setProperty("xcapDigestUser", config.username);
        reply->setProperty("xcapDigestPass", password);
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, method, config, document, startMs]() {
        onReplyFinished(reply, method, config, document, startMs);
    });
}

void XcapClient::onReplyFinished(QNetworkReply *reply, XcapHttpMethod method,
                                  const XcapServerConfig &config, const XcapDocument &document,
                                  qint64 startMs)
{
    reply->deleteLater();

    XcapResult result;
    result.method = method;
    result.rootUri = config.rootUri;
    result.auid = document.auid;
    result.xui = document.xui.isEmpty() ? config.xui : document.xui;
    result.documentSelector = document.documentName;
    result.nodeSelector = document.nodeSelector;
    result.urlRedacted = XcapUrlRedactor::redact(document.buildUri(config.rootUri));
    result.timestamp = QDateTime::currentDateTimeUtc();
    result.durationMs = QDateTime::currentMSecsSinceEpoch() - startMs;

    result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.httpReason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    result.contentType = QString::fromLatin1(reply->rawHeader("Content-Type"));
    result.etag = QString::fromLatin1(reply->rawHeader("ETag"));
    result.lastModified = QString::fromLatin1(reply->rawHeader("Last-Modified"));
    const QByteArray contentLengthHeader = reply->rawHeader("Content-Length");
    if (!contentLengthHeader.isEmpty())
        result.contentLength = contentLengthHeader.toLongLong();

    if (reply->error() != QNetworkReply::NoError) {
        result.networkError = true;
        result.errorString = reply->errorString();
    }

    if (method == XcapHttpMethod::Get) {
        const QByteArray body = reply->readAll();
        result.contentLength = body.size();
        const QString text = QString::fromUtf8(body);
        result.bodyPreview = text.left(XcapClient::kMaxBodyPreviewBytes);
        if (text.size() > XcapClient::kMaxBodyPreviewBytes)
            result.bodyPreview += QStringLiteral("…");

        if (result.contentType.contains(QLatin1String("xml"), Qt::CaseInsensitive)) {
            const XcapXmlValidator::Result validation = XcapXmlValidator::validate(text, true);
            result.parseStatus = validation.ok
                ? (validation.empty ? XcapParseStatus::Partial : XcapParseStatus::Ok)
                : XcapParseStatus::Error;
            result.warnings = validation.warnings;
            if (!validation.ok && !validation.errorMessage.isEmpty())
                result.warnings.append(validation.errorMessage);
        } else {
            result.parseStatus = XcapParseStatus::NotApplicable;
        }
    } else {
        result.parseStatus = XcapParseStatus::NotApplicable;
    }

    emit operationCompleted(result);
}
