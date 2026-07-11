#pragma once
#include <QObject>

#include "sip/XcapModels.h"

class QNetworkAccessManager;
class QNetworkReply;
class QAuthenticator;

// Asynchronous XCAP HTTP client (Task W099) built on Qt's existing
// QNetworkAccessManager — never blocks the UI thread; every operation
// completes via the operationCompleted signal. Only ever emits XcapResult
// (never QNetworkReply) so the UI never touches Qt Network types directly.
//
// Authentication: Basic credentials are sent proactively (a plain base64
// Authorization header) since XCAP GET/PUT/DELETE are typically not
// preceded by a discovery round-trip in this client. Digest is left to
// QNetworkAccessManager's built-in challenge/response handling via
// authenticationRequired (Qt performs the actual digest computation) —
// credentials are only supplied when authMode is Digest and the server
// actually challenges the request. authMode None never attaches
// credentials, even if a username/password happens to be configured.
//
// Never stores or logs the password in cleartext; it is fetched from
// CredentialStore just-in-time for each request and not retained.
class XcapClient : public QObject
{
    Q_OBJECT
public:
    static XcapClient &instance();

    void get(const XcapServerConfig &config, const XcapDocument &document);
    void put(const XcapServerConfig &config, const XcapDocument &document, const QString &xmlContent);
    void del(const XcapServerConfig &config, const XcapDocument &document);
    void head(const XcapServerConfig &config, const XcapDocument &document);

    // Size cap for GET response body previews retained in XcapResult /
    // diagnostics — never the raw unbounded body.
    static constexpr int kMaxBodyPreviewBytes = 4096;

signals:
    void operationCompleted(const XcapResult &result);

private:
    explicit XcapClient(QObject *parent = nullptr);

    void execute(XcapHttpMethod method, const XcapServerConfig &config,
                 const XcapDocument &document, const QByteArray &body);
    void onReplyFinished(QNetworkReply *reply, XcapHttpMethod method,
                         const XcapServerConfig &config, const XcapDocument &document,
                         qint64 startMs);
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator,
                                  XcapAuthMode authMode, const QString &username,
                                  const QString &password);

    QNetworkAccessManager *m_nam{nullptr};
};
