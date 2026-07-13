#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include <iostream>

#include "security/CredentialStore.h"
#include "sip/XcapClient.h"
#include "sip/XcapModels.h"

static QString envValue(const char *name)
{
    return QString::fromLocal8Bit(qgetenv(name)).trimmed();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString rootUri = envValue("XCAP_LIVE_ROOT");
    const QString username = envValue("XCAP_LIVE_USERNAME");
    const QString password = envValue("XCAP_LIVE_PASSWORD");
    const QString xui = envValue("XCAP_LIVE_XUI");
    const QString auid = envValue("XCAP_LIVE_AUID").isEmpty()
        ? QStringLiteral("xcap-caps") : envValue("XCAP_LIVE_AUID");
    const QString documentName = envValue("XCAP_LIVE_DOCUMENT").isEmpty()
        ? QStringLiteral("index") : envValue("XCAP_LIVE_DOCUMENT");

    if (rootUri.isEmpty() || username.isEmpty() || password.isEmpty()) {
        std::cerr << "Missing XCAP_LIVE_ROOT/XCAP_LIVE_USERNAME/XCAP_LIVE_PASSWORD environment.\n";
        return 2;
    }

    CredentialStore::instance().storePassword(QStringLiteral("xcap"), username, password);

    XcapServerConfig config;
    config.rootUri = rootUri;
    config.xui = xui;
    config.username = username;
    config.authMode = XcapAuthMode::Digest;

    XcapDocument document;
    document.auid = auid;
    document.documentName = documentName;

    std::cout << "XCAP live GET probe\n";
    std::cout << "Root: " << qPrintable(rootUri) << '\n';
    std::cout << "AUID: " << qPrintable(auid) << " document: " << qPrintable(documentName) << '\n';
    std::cout << "XUI: " << (xui.isEmpty() ? "(global)" : qPrintable(xui)) << '\n';

    bool done = false;
    XcapResult finalResult;
    QObject::connect(&XcapClient::instance(), &XcapClient::operationCompleted,
                     [&](const XcapResult &result) {
        done = true;
        finalResult = result;
    });

    XcapClient::instance().get(config, document);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (done) loop.quit(); });
    poll.start(50);
    timeout.start(20000);
    loop.exec();

    if (!done) {
        std::cerr << "No XCAP response within timeout.\n";
        return 3;
    }

    std::cout << "URL: " << qPrintable(finalResult.urlRedacted) << '\n';
    std::cout << "HTTP status: " << finalResult.httpStatus
              << " " << qPrintable(finalResult.httpReason) << '\n';
    std::cout << "Content-Type: " << qPrintable(finalResult.contentType) << '\n';
    std::cout << "Duration: " << finalResult.durationMs << "ms\n";
    if (finalResult.networkError) {
        std::cout << "Network error: " << qPrintable(finalResult.errorString) << '\n';
    }
    if (!finalResult.bodyPreview.isEmpty()) {
        std::cout << "Body preview:\n" << qPrintable(finalResult.bodyPreview) << '\n';
    }
    for (const auto &w : finalResult.warnings)
        std::cout << "Warning: " << qPrintable(w) << '\n';

    // Any well-formed HTTP response (including 401/403/404) proves the real
    // network path, TLS, and request-building are all working end to end —
    // only a transport-level failure (no response at all) is a hard failure
    // for this probe's purpose.
    return finalResult.networkError ? 4 : 0;
}
