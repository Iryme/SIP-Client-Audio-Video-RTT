#include "MsrpTcpTransport.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

MsrpTcpTransport::MsrpTcpTransport(QObject *parent) : MsrpTransport(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
}

MsrpTcpTransport::~MsrpTcpTransport()
{
    abortNow();
}

void MsrpTcpTransport::connectActive(const QString &host, int port, int timeoutMs)
{
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_socket = new QTcpSocket(this);
    m_socket->setReadBufferSize(kMaxSocketReadBufferBytes);
    connect(m_socket, &QTcpSocket::connected, this, &MsrpTcpTransport::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MsrpTcpTransport::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &MsrpTcpTransport::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &MsrpTcpTransport::onSocketError);

    if (timeoutMs > 0) {
        disconnect(m_timeoutTimer, nullptr, this, nullptr);
        connect(m_timeoutTimer, &QTimer::timeout, this, &MsrpTcpTransport::onConnectTimeout);
        m_timeoutTimer->start(timeoutMs);
    }

    m_socket->connectToHost(host, static_cast<quint16>(port));
}

void MsrpTcpTransport::listenPassive(const QString &bindAddress, int port, int acceptTimeoutMs)
{
    if (m_server) {
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &MsrpTcpTransport::onNewConnection);

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
        connect(m_timeoutTimer, &QTimer::timeout, this, &MsrpTcpTransport::onAcceptTimeout);
        m_timeoutTimer->start(acceptTimeoutMs);
    }
}

void MsrpTcpTransport::relisten()
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
        // Drop the rejected connection silently: no disconnected()/
        // errorOccurred() for this — the caller (MsrpSession) already knows
        // why it was dropped and is choosing to keep listening rather than
        // treat the session as closed/failed.
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
        connect(m_timeoutTimer, &QTimer::timeout, this, &MsrpTcpTransport::onAcceptTimeout);
        m_timeoutTimer->start(remainingMs);
    }
}

void MsrpTcpTransport::adoptSocket(QTcpSocket *socket)
{
    if (m_socket) {
        m_socket->deleteLater();
    }
    m_socket = socket;
    m_socket->setParent(this);
    m_socket->setReadBufferSize(kMaxSocketReadBufferBytes);
    connect(m_socket, &QTcpSocket::readyRead, this, &MsrpTcpTransport::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &MsrpTcpTransport::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &MsrpTcpTransport::onSocketError);
}

void MsrpTcpTransport::onConnected()
{
    m_timeoutTimer->stop();
    emit connected();
}

void MsrpTcpTransport::onReadyRead()
{
    if (!m_socket)
        return;
    emit bytesReceived(m_socket->readAll());
}

void MsrpTcpTransport::onSocketError()
{
    if (!m_socket)
        return;
    emit errorOccurred(m_socket->errorString());
}

void MsrpTcpTransport::onDisconnected()
{
    emit disconnected();
}

void MsrpTcpTransport::onNewConnection()
{
    if (!m_server)
        return;
    m_timeoutTimer->stop();
    QTcpSocket *incoming = m_server->nextPendingConnection();
    if (!incoming)
        return;
    // A single MSRP connection per session — stop listening once the first
    // peer connects (subsequent connection attempts are refused by closing
    // the listener, matching a passive-role session's expectations).
    m_server->close();
    adoptSocket(incoming);
    emit connected();
}

void MsrpTcpTransport::onAcceptTimeout()
{
    if (m_server) {
        m_server->close();
    }
    emit errorOccurred(QStringLiteral("accept timeout: no peer connected"));
}

void MsrpTcpTransport::onConnectTimeout()
{
    if (m_socket && m_socket->state() != QAbstractSocket::ConnectedState) {
        m_socket->abort();
        emit errorOccurred(QStringLiteral("connect timeout"));
    }
}

void MsrpTcpTransport::sendBytes(const QByteArray &data)
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->write(data);
}

void MsrpTcpTransport::closeGracefully()
{
    if (m_socket)
        m_socket->disconnectFromHost();
    if (m_server)
        m_server->close();
}

void MsrpTcpTransport::abortNow()
{
    m_timeoutTimer->stop();
    if (m_socket)
        m_socket->abort();
    if (m_server)
        m_server->close();
}

bool MsrpTcpTransport::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

int MsrpTcpTransport::localPort() const
{
    if (m_server && m_server->isListening())
        return m_server->serverPort();
    if (m_socket)
        return m_socket->localPort();
    return 0;
}

QString MsrpTcpTransport::localAddressRedacted() const
{
    return QStringLiteral("local:%1").arg(localPort());
}

QString MsrpTcpTransport::peerAddressRedacted() const
{
    if (!m_socket)
        return QString();
    return QStringLiteral("peer:%1").arg(m_socket->peerPort());
}
