#pragma once
#include <QByteArray>
#include <QString>

// Pure Basic-auth header construction, factored out of XcapClient so it is
// unit-testable without a live QNetworkAccessManager/server (Task W099,
// requirement 12: "Basic auth" test). Digest auth has no equivalent pure
// helper: it is handled entirely by Qt's built-in QNetworkAccessManager
// challenge/response machinery (see XcapClient::onAuthenticationRequired),
// which cannot be exercised without a real digest-challenging HTTP server —
// see docs/xcap.md for the documented manual test covering it.
namespace XcapAuthHeaderBuilder {

inline QByteArray basicAuthorizationHeader(const QString &username, const QString &password)
{
    const QByteArray token = (username + QLatin1Char(':') + password).toUtf8().toBase64();
    return QByteArray("Basic ") + token;
}

} // namespace XcapAuthHeaderBuilder
