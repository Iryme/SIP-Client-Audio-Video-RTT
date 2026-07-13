#include "MsrpTlsTransport.h"

#include <QFile>
#include <QHostAddress>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTcpServer>
#include <QTimer>

MsrpTlsTransport::MsrpTlsTransport(QObject *parent) : MsrpTransport(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
}

MsrpTlsTransport::~MsrpTlsTransport()
{
    abortNow();
}

void MsrpTlsTransport::setupSocket(QSslSocket *socket)
{
    socket->setReadBufferSize(kMaxSocketReadBufferBytes);
    if (!m_verifyPeer) {
        socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    } else {
        socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
        if (!m_caCertificatePath.isEmpty()) {
            QFile caFile(m_caCertificatePath);
            if (caFile.open(QIODevice::ReadOnly)) {
                const QList<QSslCertificate> certs = QSslCertificate::fromDevice(&caFile, QSsl::Pem);
                if (!certs.isEmpty()) {
                    QSslConfiguration cfg = socket->sslConfiguration();
                    cfg.setCaCertificates(certs);
                    socket->setSslConfiguration(cfg);
                }
            }
        }
    }
    connect(socket, &QSslSocket::encrypted, this, &MsrpTlsTransport::onEncrypted);
    connect(socket, &QSslSocket::readyRead, this, &MsrpTlsTransport::onReadyRead);
    connect(socket, &QSslSocket::sslErrors, this, &MsrpTlsTransport::onSslErrors);
    connect(socket, &QSslSocket::errorOccurred, this, &MsrpTlsTransport::onSocketError);
    connect(socket, &QSslSocket::disconnected, this, &MsrpTlsTransport::onDisconnected);
}

void MsrpTlsTransport::connectActive(const QString &host, int port, int timeoutMs)
{
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_expectedHost = host;
    m_socket = new QSslSocket(this);
    setupSocket(m_socket);

    if (timeoutMs > 0) {
        disconnect(m_timeoutTimer, nullptr, this, nullptr);
        connect(m_timeoutTimer, &QTimer::timeout, this, &MsrpTlsTransport::onConnectTimeout);
        m_timeoutTimer->start(timeoutMs);
    }

    // SNI/hostname verification is handled by connectToHostEncrypted's
    // hostName argument, matched against the peer certificate automatically
    // by QSslSocket when peerVerifyMode is VerifyPeer.
    m_socket->connectToHostEncrypted(host, static_cast<quint16>(port), host);
}

void MsrpTlsTransport::listenPassive(const QString &bindAddress, int port, int acceptTimeoutMs)
{
    if (m_server) {
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &MsrpTlsTransport::onNewConnection);

    m_bindAddress = bindAddress;
    const QHostAddress addr = bindAddress.isEmpty() ? QHostAddress::AnyIPv4 : QHostAddress(bindAddress);
    if (!m_server->listen(addr, static_cast<quint16>(port))) {
        emit errorOccurred(QStringLiteral("listen failed: %1").arg(m_server->errorString()));
        return;
    }

    m_boundPort = m_server->serverPort();
    m_acceptTimeoutMs = acceptTimeoutMs;
    if (acceptTimeoutMs > 0) {
        m_acceptElapsed.start();
        disconnect(m_timeoutTimer, nullptr, this, nullptr);
        connect(m_timeoutTimer, &QTimer::timeout, this, &MsrpTlsTransport::onAcceptTimeout);
        m_timeoutTimer->start(acceptTimeoutMs);
    }
}

void MsrpTlsTransport::relisten()
{
    if (!m_server)
        return;

    int remainingMs = 0;
    if (m_acceptTimeoutMs > 0) {
        remainingMs = m_acceptTimeoutMs - static_cast<int>(m_acceptElapsed.elapsed());
        if (remainingMs <= 0) {
            emit errorOccurred(QStringLiteral("accept timeout: no peer connected"));
            return;
        }
    }

    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    if (m_server->isListening())
        return;

    const QHostAddress addr = m_bindAddress.isEmpty() ? QHostAddress::AnyIPv4 : QHostAddress(m_bindAddress);
    if (!m_server->listen(addr, static_cast<quint16>(m_boundPort))) {
        emit errorOccurred(QStringLiteral("relisten failed: %1").arg(m_server->errorString()));
        return;
    }

    if (m_acceptTimeoutMs > 0) {
        disconnect(m_timeoutTimer, nullptr, this, nullptr);
        connect(m_timeoutTimer, &QTimer::timeout, this, &MsrpTlsTransport::onAcceptTimeout);
        m_timeoutTimer->start(remainingMs);
    }
}

void MsrpTlsTransport::onNewConnection()
{
    if (!m_server)
        return;
    m_timeoutTimer->stop();

    QTcpSocket *incoming = m_server->nextPendingConnection();
    if (!incoming)
        return;
    m_server->close();

    // Server-side TLS: wrap the accepted plain descriptor into a QSslSocket
    // and start the handshake in server mode. Requires a local certificate/
    // key to be configured on the socket by the caller before this is
    // usable in production — see docs/msrp-security.md for the current
    // test-harness-only limitation on server-side identity provisioning.
    if (m_socket) m_socket->deleteLater();
    m_socket = new QSslSocket(this);
    setupSocket(m_socket);
    const qintptr descriptor = incoming->socketDescriptor();
    incoming->setParent(nullptr);
    incoming->deleteLater();
    m_socket->setSocketDescriptor(descriptor);
    m_socket->startServerEncryption();
}

void MsrpTlsTransport::onEncrypted()
{
    m_timeoutTimer->stop();
    emit tlsHandshakeCompleted();
    emit connected();
}

void MsrpTlsTransport::onReadyRead()
{
    if (!m_socket)
        return;
    emit bytesReceived(m_socket->readAll());
}

void MsrpTlsTransport::onSslErrors(const QList<QSslError> &errors)
{
    QStringList messages;
    for (const auto &e : errors)
        messages << e.errorString();
    emit tlsErrors(messages);

    if (!m_verifyPeer && m_socket) {
        // Explicit, config-gated bypass only — never silent.
        m_socket->ignoreSslErrors();
    } else {
        emit errorOccurred(QStringLiteral("TLS certificate validation failed: %1").arg(messages.join(QStringLiteral("; "))));
        if (m_socket)
            m_socket->abort();
    }
}

void MsrpTlsTransport::onSocketError()
{
    if (m_socket)
        emit errorOccurred(m_socket->errorString());
}

void MsrpTlsTransport::onDisconnected()
{
    emit disconnected();
}

void MsrpTlsTransport::onAcceptTimeout()
{
    if (m_server) m_server->close();
    emit errorOccurred(QStringLiteral("accept timeout: no peer connected"));
}

void MsrpTlsTransport::onConnectTimeout()
{
    if (m_socket && m_socket->state() != QAbstractSocket::ConnectedState) {
        m_socket->abort();
        emit errorOccurred(QStringLiteral("connect timeout"));
    }
}

void MsrpTlsTransport::sendBytes(const QByteArray &data)
{
    if (m_socket && m_socket->isEncrypted())
        m_socket->write(data);
}

void MsrpTlsTransport::closeGracefully()
{
    if (m_socket) m_socket->disconnectFromHost();
    if (m_server) m_server->close();
}

void MsrpTlsTransport::abortNow()
{
    m_timeoutTimer->stop();
    if (m_socket) m_socket->abort();
    if (m_server) m_server->close();
}

bool MsrpTlsTransport::isConnected() const
{
    return m_socket && m_socket->isEncrypted();
}

int MsrpTlsTransport::localPort() const
{
    if (m_server && m_server->isListening())
        return m_server->serverPort();
    if (m_socket)
        return m_socket->localPort();
    return 0;
}

QString MsrpTlsTransport::localAddressRedacted() const
{
    return QStringLiteral("local:%1").arg(localPort());
}

QString MsrpTlsTransport::peerAddressRedacted() const
{
    if (!m_socket)
        return QString();
    return QStringLiteral("peer:%1").arg(m_socket->peerPort());
}
