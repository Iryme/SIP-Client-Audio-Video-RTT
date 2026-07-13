#pragma once
#include <QElapsedTimer>
#include <QList>
#include <QSslError>

#include "msrp/MsrpTransport.h"

class QSslSocket;
class QTcpServer;
class QTimer;

// TCP/TLS MSRP transport (Task W100, section G) via QSslSocket. Peer
// certificate verification defaults ON; verifyPeer=false must be set
// explicitly (intended for local/test harness use only — see
// docs/msrp-security.md). Never silently accepts an invalid certificate
// when verification is enabled.
class MsrpTlsTransport : public MsrpTransport
{
    Q_OBJECT
public:
    explicit MsrpTlsTransport(QObject *parent = nullptr);
    ~MsrpTlsTransport() override;

    void setVerifyPeer(bool verify) { m_verifyPeer = verify; }
    void setCaCertificatePath(const QString &path) { m_caCertificatePath = path; }

    void connectActive(const QString &host, int port, int timeoutMs) override;
    void listenPassive(const QString &bindAddress, int port, int acceptTimeoutMs) override;
    void relisten() override;
    void sendBytes(const QByteArray &data) override;
    void closeGracefully() override;
    void abortNow() override;

    bool isConnected() const override;
    int localPort() const override;
    QString localAddressRedacted() const override;
    QString peerAddressRedacted() const override;

private slots:
    void onEncrypted();
    void onReadyRead();
    void onSslErrors(const QList<QSslError> &errors);
    void onSocketError();
    void onDisconnected();
    void onNewConnection();
    void onAcceptTimeout();
    void onConnectTimeout();

private:
    void setupSocket(QSslSocket *socket);

    QSslSocket *m_socket{nullptr};
    QTcpServer *m_server{nullptr};
    QTimer *m_timeoutTimer{nullptr};
    bool m_verifyPeer{true};
    QString m_caCertificatePath;
    QString m_expectedHost; // for SNI/hostname verification in active mode

    static constexpr qint64 kMaxSocketReadBufferBytes = 8 * 1024 * 1024;

    // Task W106: see MsrpTcpTransport's identical fields for rationale.
    QString m_bindAddress;
    int m_boundPort{0};
    int m_acceptTimeoutMs{0};
    QElapsedTimer m_acceptElapsed;
};
