#pragma once
#include "msrp/MsrpTransport.h"

class QTcpSocket;
class QTcpServer;
class QTimer;

// Plain-TCP MSRP transport (Task W100, section F). Non-blocking:
// connectActive()/listenPassive() return immediately; results arrive via
// the connected()/errorOccurred() signals. No waitForConnected()/
// waitForReadyRead() anywhere — those would block the Qt event loop.
class MsrpTcpTransport : public MsrpTransport
{
    Q_OBJECT
public:
    explicit MsrpTcpTransport(QObject *parent = nullptr);
    ~MsrpTcpTransport() override;

    void connectActive(const QString &host, int port, int timeoutMs) override;
    void listenPassive(const QString &bindAddress, int port, int acceptTimeoutMs) override;
    void sendBytes(const QByteArray &data) override;
    void closeGracefully() override;
    void abortNow() override;

    bool isConnected() const override;
    int localPort() const override;
    QString localAddressRedacted() const override;
    QString peerAddressRedacted() const override;

protected:
    // Exposed so MsrpTlsTransport can reuse the listener/accept plumbing
    // and wrap the accepted plain socket into a TLS socket.
    void adoptSocket(QTcpSocket *socket);

private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError();
    void onDisconnected();
    void onNewConnection();
    void onAcceptTimeout();
    void onConnectTimeout();

private:
    QTcpSocket *m_socket{nullptr};
    QTcpServer *m_server{nullptr};
    QTimer *m_timeoutTimer{nullptr};
    // Bounds unread-but-buffered bytes if the application is slow to
    // consume them — backpressure/oversized-buffer protection
    // (task section F: "buffer limitat").
    static constexpr qint64 kMaxSocketReadBufferBytes = 8 * 1024 * 1024;
};
