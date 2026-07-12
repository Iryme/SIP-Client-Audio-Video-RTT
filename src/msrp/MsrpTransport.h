#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

// Abstract MSRP TCP-family transport (Task W100, sections F/G). Concrete
// implementations (MsrpTcpTransport / MsrpTlsTransport) wrap QTcpSocket/
// QSslSocket. Runs entirely on the Qt event loop — every signal here
// already fires on the Qt thread, so callers never need
// QMetaObject::invokeMethod marshaling (unlike pjsua2 worker-thread
// callbacks elsewhere in this codebase).
class MsrpTransport : public QObject
{
    Q_OBJECT
public:
    enum class Mode { ActiveConnector, PassiveListener };

    explicit MsrpTransport(QObject *parent = nullptr) : QObject(parent) {}
    ~MsrpTransport() override = default;

    // Active mode: connect out to host:port. Passive mode: bind/listen on
    // bindAddress:port (0 = OS-assigned ephemeral port) and wait for one
    // inbound connection. Neither call blocks the calling thread.
    virtual void connectActive(const QString &host, int port, int timeoutMs) = 0;
    virtual void listenPassive(const QString &bindAddress, int port, int acceptTimeoutMs) = 0;

    virtual void sendBytes(const QByteArray &data) = 0;
    virtual void closeGracefully() = 0;
    virtual void abortNow() = 0;

    virtual bool isConnected() const = 0;
    virtual int localPort() const = 0;
    virtual QString localAddressRedacted() const = 0;
    virtual QString peerAddressRedacted() const = 0;

signals:
    void connected();
    void bytesReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
    void disconnected();
    // TLS only; MsrpTcpTransport never emits this.
    void tlsHandshakeCompleted();
    void tlsErrors(const QStringList &errors);
};
