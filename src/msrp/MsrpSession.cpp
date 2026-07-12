#include "MsrpSession.h"

#include <QRandomGenerator>

#include "msrp/MsrpDiagnosticsStore.h"
#include "msrp/MsrpFrameSerializer.h"
#include "msrp/MsrpMessageChunker.h"
#include "msrp/MsrpSessionStore.h"
#include "msrp/MsrpTcpTransport.h"
#include "msrp/MsrpTlsTransport.h"

namespace {
QString redactPath(const QString &path)
{
    // MSRP paths carry a host/port/session-id chain; keep only URI count +
    // scheme, drop host/session-id specifics from diagnostics/UI text.
    const auto uris = MsrpPath::parsePath(path);
    QStringList redacted;
    for (const auto &u : uris)
        redacted << QStringLiteral("%1://…").arg(u.scheme);
    return redacted.join(QLatin1Char(' '));
}

QString randomMessageId()
{
    static const QString kAlphabet = QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789");
    QString id;
    for (int i = 0; i < 12; ++i)
        id.append(kAlphabet.at(QRandomGenerator::global()->bounded(kAlphabet.size())));
    return id;
}
} // namespace

MsrpSession::MsrpSession(const QString &sessionKey, QObject *parent) : QObject(parent)
{
    m_sessionKey = sessionKey;
    m_info.sessionKey = sessionKey;
    m_info.state = MsrpSessionState::Detected;
    m_info.createdAt = QDateTime::currentDateTimeUtc();
}

MsrpSession::~MsrpSession()
{
    // Disconnect and tear down the transport explicitly, before any data
    // member's implicit destructor runs. MsrpTransport::abortNow() (called
    // from ~MsrpTcpTransport/~MsrpTlsTransport) can synchronously emit
    // disconnected(), which would otherwise re-enter onTransportDisconnected()
    // -> publishInfo() -> m_transactions.pendingCount() after m_transactions
    // (a later-declared, earlier-destroyed member) has already been
    // destroyed — a use-after-free. Disconnecting first guarantees no
    // transport signal can reach `this` during teardown, regardless of
    // member declaration order.
    if (m_transport) {
        m_transport->disconnect();
        m_transport->abortNow();
        m_transport.reset();
    }
}

void MsrpSession::setLocalUri(const MsrpUri &uri) { m_localUri = uri; m_info.localSessionId = uri.sessionId; }
void MsrpSession::setRemotePath(const QList<MsrpUri> &path)
{
    m_remotePath = path;
    if (!path.isEmpty())
        m_info.remoteSessionId = path.first().sessionId;
}
void MsrpSession::setSipCallId(const QString &callId) { m_info.sipCallId = callId; }
void MsrpSession::setChunkSizeBytes(int n) { m_chunkSizeBytes = n; }
void MsrpSession::setRequestReports(bool on) { m_requestReports = on; }
void MsrpSession::setMaxFrameBytes(int n) { m_parser = MsrpFrameParser(n); }
void MsrpSession::setMaxMessageBytes(qint64 n) { m_assembler = MsrpChunkAssembler(n); }
void MsrpSession::setTlsVerifyPeer(bool verify) { m_tlsVerifyPeer = verify; }
void MsrpSession::setTlsCaCertificatePath(const QString &path) { m_tlsCaCertificatePath = path; }

void MsrpSession::createTransport(MsrpTransportProtocol proto)
{
    // No QObject parent here: m_transport (a unique_ptr) is the sole
    // owner. Parenting to `this` as well would give the transport two
    // owners — QObject's parent-child deletion and the unique_ptr's own
    // destructor would each try to delete it, corrupting the heap.
    if (proto == MsrpTransportProtocol::Tls) {
        auto *tls = new MsrpTlsTransport();
        tls->setVerifyPeer(m_tlsVerifyPeer);
        tls->setCaCertificatePath(m_tlsCaCertificatePath);
        m_transport.reset(tls);
        m_info.localTransport = MsrpTransportProtocol::Tls;
    } else {
        m_transport.reset(new MsrpTcpTransport());
        m_info.localTransport = MsrpTransportProtocol::Tcp;
    }
    connect(m_transport.get(), &MsrpTransport::connected, this, &MsrpSession::onTransportConnected);
    connect(m_transport.get(), &MsrpTransport::bytesReceived, this, &MsrpSession::onTransportBytes);
    connect(m_transport.get(), &MsrpTransport::errorOccurred, this, &MsrpSession::onTransportError);
    connect(m_transport.get(), &MsrpTransport::disconnected, this, &MsrpSession::onTransportDisconnected);
}

void MsrpSession::connectAsActive(int connectTimeoutMs)
{
    if (m_remotePath.isEmpty() || !m_remotePath.first().ok) {
        m_info.lastError = QStringLiteral("no valid remote path to connect to");
        updateState(MsrpSessionState::Failed);
        return;
    }
    m_info.role = MsrpRole::ActiveConnector;
    createTransport(m_remotePath.first().transportProtocol());
    updateState(MsrpSessionState::Connecting);
    m_transport->connectActive(m_remotePath.first().host, m_remotePath.first().port, connectTimeoutMs);
}

void MsrpSession::listenAsPassive(const QString &bindAddress, int acceptTimeoutMs)
{
    m_info.role = MsrpRole::PassiveListener;
    createTransport(m_localUri.transportProtocol());
    updateState(MsrpSessionState::Connecting);
    m_transport->listenPassive(bindAddress, m_localUri.port, acceptTimeoutMs);
}

void MsrpSession::onTransportConnected()
{
    m_info.connectedAt = QDateTime::currentDateTimeUtc();
    updateState(MsrpSessionState::Established);
}

void MsrpSession::onTransportBytes(const QByteArray &data)
{
    const auto results = m_parser.feed(data);
    for (const auto &r : results) {
        if (r.status == MsrpFrameParser::Status::Complete) {
            handleFrame(r.frame);
        } else if (r.status == MsrpFrameParser::Status::Invalid || r.status == MsrpFrameParser::Status::LimitExceeded) {
            m_info.warnings << r.errorMessage;
            MsrpDiagnosticsEvent ev;
            ev.timestamp = QDateTime::currentDateTimeUtc();
            ev.direction = MsrpDiagnosticsEvent::Direction::Inbound;
            ev.kind = MsrpDiagnosticsEvent::Kind::Error;
            ev.sessionKey = m_sessionKey;
            ev.error = r.errorMessage;
            ev.parseStatus = MsrpParseStatus::Error;
            MsrpDiagnosticsStore::instance().record(ev);
        }
    }
}

void MsrpSession::handleFrame(const MsrpFrame &frame)
{
    m_info.framesReceived++;
    m_info.bytesReceived += frame.body.size();
    logDiagnostic(true, frame);

    if (frame.isRequest && frame.method == QStringLiteral("SEND")) {
        const auto assembled = m_assembler.feedChunk(frame);

        MsrpFrame response;
        response.isRequest = false;
        response.transactionId = frame.transactionId;
        response.toPath = frame.fromPath;
        response.fromPath = frame.toPath;
        response.continuation = MsrpContinuation::Complete;

        if (assembled.status == MsrpChunkAssembler::FeedStatus::Error) {
            response.responseCode = 400;
            response.responseComment = QStringLiteral("Bad Request");
            sendFrame(response);
            return;
        }

        response.responseCode = 200;
        response.responseComment = QStringLiteral("OK");
        sendFrame(response);

        if (assembled.status == MsrpChunkAssembler::FeedStatus::Complete) {
            m_info.messagesCompleted++;
            emit payloadReceived(m_sessionKey, assembled.message.messageId,
                                assembled.message.contentType, assembled.message.body);

            if (frame.successReport.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0) {
                MsrpFrame report;
                report.isRequest = true;
                report.method = QStringLiteral("REPORT");
                report.transactionId = randomMessageId();
                report.toPath = frame.fromPath;
                report.fromPath = frame.toPath;
                report.messageId = assembled.message.messageId;
                report.status = QStringLiteral("000 200 OK");
                report.hasByteRange = true;
                report.byteRange = MsrpByteRange{1, static_cast<qint64>(assembled.message.body.size()),
                                                static_cast<qint64>(assembled.message.body.size())};
                report.continuation = MsrpContinuation::Complete;
                sendFrame(report);
            }
        }
        publishInfo();
        return;
    }

    if (frame.isRequest && frame.method == QStringLiteral("REPORT")) {
        for (const auto &t : m_transactions.all()) {
            if (t.messageId == frame.messageId && t.outbound) {
                const bool success = frame.status.contains(QStringLiteral("200"));
                m_transactions.applyReport(t.transactionId, frame.status, success);
                break;
            }
        }
        MsrpFrame response;
        response.isRequest = false;
        response.transactionId = frame.transactionId;
        response.responseCode = 200;
        response.responseComment = QStringLiteral("OK");
        response.continuation = MsrpContinuation::Complete;
        sendFrame(response);
        publishInfo();
        return;
    }

    if (!frame.isRequest) {
        const bool success = frame.isSuccessResponse();
        m_transactions.updateStatus(frame.transactionId,
            success ? MsrpTransactionStatus::Accepted : MsrpTransactionStatus::Failed,
            frame.responseCode, frame.responseComment);
        publishInfo();
    }
}

QString MsrpSession::sendMessage(const QString &contentType, const QByteArray &body)
{
    const QString messageId = randomMessageId();
    const QString toPath = MsrpPath::buildPath(m_remotePath);
    const QString fromPath = m_localUri.toString();

    const auto frames = MsrpMessageChunker::buildSendFrames(
        m_sessionKey.left(6), toPath, fromPath, messageId, contentType, body,
        m_chunkSizeBytes, m_requestReports, m_requestReports);

    for (const auto &f : frames)
        sendFrame(f, /*track=*/true, MsrpMethod::Send, messageId);

    publishInfo();
    return messageId;
}

void MsrpSession::sendFrame(MsrpFrame frame, bool track, MsrpMethod method, const QString &messageId)
{
    if (frame.isRequest && frame.transactionId.isEmpty())
        frame.transactionId = randomMessageId();

    if (track)
        m_transactions.begin(frame.transactionId, m_sessionKey, method, messageId, /*outbound=*/true);

    bool ok = false;
    const QByteArray wire = MsrpFrameSerializer::serialize(frame, &ok);
    if (!ok) {
        m_info.lastError = QStringLiteral("failed to serialize outbound frame (rejected: possible header injection)");
        return;
    }

    if (m_transport)
        m_transport->sendBytes(wire);

    m_info.framesSent++;
    m_info.bytesSent += frame.body.size();
    logDiagnostic(false, frame);
}

void MsrpSession::onTransportError(const QString &message)
{
    m_info.lastError = message;
    m_info.warnings << message;
    updateState(MsrpSessionState::Failed);

    MsrpDiagnosticsEvent ev;
    ev.timestamp = QDateTime::currentDateTimeUtc();
    ev.kind = MsrpDiagnosticsEvent::Kind::Error;
    ev.sessionKey = m_sessionKey;
    ev.error = message;
    MsrpDiagnosticsStore::instance().record(ev);
}

void MsrpSession::onTransportDisconnected()
{
    updateState(MsrpSessionState::Closed);
    m_info.closedAt = QDateTime::currentDateTimeUtc();

    MsrpDiagnosticsEvent ev;
    ev.timestamp = QDateTime::currentDateTimeUtc();
    ev.kind = MsrpDiagnosticsEvent::Kind::Close;
    ev.sessionKey = m_sessionKey;
    MsrpDiagnosticsStore::instance().record(ev);
    publishInfo();
}

void MsrpSession::closeSession()
{
    if (m_transport)
        m_transport->closeGracefully();
    updateState(MsrpSessionState::Disconnecting);
}

void MsrpSession::abortSession(const QString &reason)
{
    m_info.lastError = reason;
    if (m_transport)
        m_transport->abortNow();
    updateState(MsrpSessionState::Failed);
}

void MsrpSession::updateState(MsrpSessionState state)
{
    m_info.state = state;
    publishInfo();
}

void MsrpSession::publishInfo()
{
    m_info.transactionsPending = m_transactions.pendingCount(m_sessionKey);
    MsrpSessionStore::instance().upsert(m_info);
}

void MsrpSession::logDiagnostic(bool inbound, const MsrpFrame &frame, const QStringList &warnings)
{
    MsrpDiagnosticsEvent ev;
    ev.timestamp = QDateTime::currentDateTimeUtc();
    ev.direction = inbound ? MsrpDiagnosticsEvent::Direction::Inbound : MsrpDiagnosticsEvent::Direction::Outbound;
    ev.kind = MsrpDiagnosticsEvent::Kind::Frame;
    ev.sessionKey = m_sessionKey;
    ev.transactionId = frame.transactionId;
    ev.messageId = frame.messageId;
    ev.method = frame.isRequest ? frame.method : QStringLiteral("response");
    ev.responseCode = frame.responseCode;
    ev.statusHeader = frame.status;
    ev.toPathRedacted = redactPath(frame.toPath);
    ev.fromPathRedacted = redactPath(frame.fromPath);
    ev.contentType = frame.contentType;
    ev.byteRangeText = frame.hasByteRange ? frame.byteRange.toString() : QString();
    ev.continuation = frame.continuation;
    ev.bodyLength = frame.body.size();
    ev.bodyPreview = QString::fromUtf8(frame.body.left(160));
    ev.transport = m_info.localTransport;
    ev.warnings = warnings;
    MsrpDiagnosticsStore::instance().record(ev);
}
