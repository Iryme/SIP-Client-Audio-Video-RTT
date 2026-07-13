#include "MsrpRelayClient.h"

#include <QTimer>
#include <QUuid>

#include "msrp/MsrpDigestAuth.h"
#include "msrp/MsrpFrameSerializer.h"
#include "msrp/MsrpRelayDiagnosticsStore.h"
#include "msrp/MsrpTcpTransport.h"
#include "msrp/MsrpTlsTransport.h"

namespace {
QString redactHostPort(const QString &host, int port)
{
    return host + QLatin1Char(':') + QString::number(port);
}
}

MsrpRelayClient::MsrpRelayClient(QObject *parent)
    : QObject(parent)
    , m_parser(65536)
{
    m_authTimer = new QTimer(this);
    m_authTimer->setSingleShot(true);
    connect(m_authTimer, &QTimer::timeout, this, &MsrpRelayClient::onAuthTimeout);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &MsrpRelayClient::onRefreshTimerFired);
}

MsrpRelayClient::~MsrpRelayClient()
{
    // Mirrors ~MsrpSession()'s teardown ordering rationale: m_socket->abort()
    // (inside MsrpTransport::abortNow()) can synchronously emit
    // disconnected(), which — if left connected — would re-enter
    // onTransportDisconnected() -> attemptRetryOrFail() -> openTransport()
    // and reassign m_transport while it is still mid-destruction here.
    // Disconnecting first guarantees no transport signal can reach `this`
    // during teardown.
    m_refreshTimer->stop();
    m_authTimer->stop();
    if (m_transport) {
        m_transport->disconnect();
        m_transport->abortNow();
        m_transport.reset();
    }
}

void MsrpRelayClient::setCorrelation(const QString &sipCallIdRedacted, int mediaIndex)
{
    m_sipCallIdRedacted = sipCallIdRedacted;
    m_mediaIndex = mediaIndex;
}

void MsrpRelayClient::configure(const MsrpRelayConfig &config, const QString &password)
{
    m_config = config;
    m_password = password;
}

bool MsrpRelayClient::isAllocated() const
{
    return (m_state == State::Allocated || m_state == State::Refreshing) && m_allocation.valid;
}

void MsrpRelayClient::transitionTo(State s)
{
    m_state = s;
    emit stateChanged(s);
}

void MsrpRelayClient::logEvent(MsrpRelayDiagnosticsEvent::Kind kind, const QString &error,
                               int responseCode, const QString &responseComment)
{
    MsrpRelayDiagnosticsEvent ev;
    ev.timestamp = QDateTime::currentDateTimeUtc();
    ev.kind = kind;
    ev.relayConnectionId = m_relayConnectionId;
    ev.allocationId = m_allocation.allocationId;
    ev.sipCallIdRedacted = m_sipCallIdRedacted;
    ev.mediaIndex = m_mediaIndex;
    ev.responseCode = responseCode;
    ev.responseComment = responseComment;
    ev.algorithm = m_algorithm;
    ev.qopUsed = !m_qop.isEmpty();
    if (m_allocation.valid) {
        const MsrpUri uri = m_allocation.allocatedUri();
        ev.allocatedPathRedacted = redactHostPort(uri.host, uri.port);
        ev.expiresAt = m_allocation.expiresAt;
    }
    ev.error = error;
    MsrpRelayDiagnosticsStore::instance().record(ev);
}

void MsrpRelayClient::start()
{
    if (!m_config.isUsable()) {
        failAllocation(QStringLiteral("config"), QStringLiteral("relay not configured/usable"));
        return;
    }
    m_retryCount = 0;
    m_refreshInProgress = false;
    m_relayConnectionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    openTransport();
}

void MsrpRelayClient::openTransport()
{
    m_parser.reset();
    m_transport.reset();
    if (m_config.useTls) {
        auto *tls = new MsrpTlsTransport();
        tls->setVerifyPeer(m_config.tlsVerifyPeer);
        tls->setCaCertificatePath(m_config.tlsCaCertificatePath);
        m_transport.reset(tls);
    } else {
        m_transport.reset(new MsrpTcpTransport());
    }

    connect(m_transport.get(), &MsrpTransport::connected, this, &MsrpRelayClient::onTransportConnected);
    connect(m_transport.get(), &MsrpTransport::bytesReceived, this, &MsrpRelayClient::onTransportBytes);
    connect(m_transport.get(), &MsrpTransport::errorOccurred, this, &MsrpRelayClient::onTransportError);
    connect(m_transport.get(), &MsrpTransport::disconnected, this, &MsrpRelayClient::onTransportDisconnected);

    transitionTo(State::Connecting);
    m_transport->connectActive(m_config.relayHost, m_config.relayPort, m_config.connectTimeoutMs);
}

QString MsrpRelayClient::digestUri() const
{
    // The relay's own MSRP URI is the AUTH digest-uri (RFC 4976 §5.2):
    // scheme depends on transport, session-id is not yet known before
    // allocation, so the control connection's To-Path target is expressed
    // as host:port with an empty session-id placeholder — matches the
    // relay's own advertised control endpoint, not a media session.
    const QString scheme = m_config.useTls ? QStringLiteral("msrps") : QStringLiteral("msrp");
    return QStringLiteral("%1://%2:%3").arg(scheme, m_config.relayHost).arg(m_config.relayPort);
}

QString MsrpRelayClient::relayToPathHeader() const
{
    return digestUri() + QStringLiteral("/;tcp");
}

void MsrpRelayClient::onTransportConnected()
{
    transitionTo(State::Connected);
    logEvent(MsrpRelayDiagnosticsEvent::Kind::Connect);
    sendInitialAuth();
}

void MsrpRelayClient::sendInitialAuth()
{
    m_localTempSessionId = QStringLiteral("relayauth-") + QUuid::createUuid().toString(QUuid::Id128);

    MsrpFrame frame;
    frame.isRequest = true;
    frame.method = QStringLiteral("AUTH");
    frame.transactionId = QUuid::createUuid().toString(QUuid::Id128).left(16);
    frame.toPath = relayToPathHeader();
    frame.fromPath = QStringLiteral("msrp://0.0.0.0:0/%1;tcp").arg(m_localTempSessionId);

    bool ok = false;
    const QByteArray bytes = MsrpFrameSerializer::serialize(frame, &ok);
    if (!ok) {
        failAllocation(QStringLiteral("protocol"), QStringLiteral("failed to serialize initial AUTH"));
        return;
    }

    transitionTo(State::SendingInitialAuth);
    m_transport->sendBytes(bytes);
    logEvent(MsrpRelayDiagnosticsEvent::Kind::InitialAuthSent);
    transitionTo(State::WaitingChallenge);
    m_authTimer->start(m_config.authTimeoutMs);
}

void MsrpRelayClient::onTransportBytes(const QByteArray &data)
{
    const auto results = m_parser.feed(data);
    for (const auto &res : results) {
        if (res.status != MsrpFrameParser::Status::Complete)
            continue;
        if (!res.frame.isRequest && res.frame.method.compare(QStringLiteral("AUTH"), Qt::CaseInsensitive) == 0)
            handleAuthFrame(res.frame);
        else if (!res.frame.isRequest)
            handleAuthFrame(res.frame); // any response while awaiting AUTH is treated as the AUTH response
    }
}

void MsrpRelayClient::handleAuthFrame(const MsrpFrame &frame)
{
    m_authTimer->stop();

    if (frame.responseCode == 401 || frame.responseCode == 407) {
        // A 401 received while WaitingAllocation means the relay rejected
        // the *authenticated* retry we already sent (wrong credentials, or
        // a stale nonce it still won't accept a second time) — never retry
        // this indefinitely; that would hang forever against a relay that
        // just keeps re-challenging. Only the very first, unauthenticated
        // AUTH's 401 is treated as the normal initial challenge.
        if (m_state == State::WaitingAllocation) {
            attemptRetryOrFail(QStringLiteral("auth"),
                               QStringLiteral("relay rejected authenticated AUTH (%1)").arg(frame.responseComment));
            return;
        }

        const QString challengeHeader = frame.unknownHeaders.value(QStringLiteral("WWW-Authenticate"));
        const auto challenge = MsrpDigestAuth::parseChallenge(challengeHeader);
        if (!challenge.ok) {
            attemptRetryOrFail(QStringLiteral("protocol"), QStringLiteral("malformed/missing WWW-Authenticate challenge"));
            return;
        }
        m_realm = challenge.realm;
        m_nonce = challenge.nonce;
        m_algorithm = challenge.algorithm;
        m_qop = challenge.qop;
        m_opaque = challenge.opaque;
        m_nonceCount = 0;
        m_cnonce = MsrpDigestAuth::generateCnonce();

        logEvent(MsrpRelayDiagnosticsEvent::Kind::ChallengeReceived, QString(),
                frame.responseCode, frame.responseComment);

        if (m_state == State::WaitingChallenge)
            transitionTo(State::SendingAuthenticatedAuth);
        else
            transitionTo(State::Reauthenticating);
        sendAuthenticatedAuth(m_password);
        return;
    }

    if (frame.responseCode >= 200 && frame.responseCode < 300) {
        MsrpRelayAllocation allocation;
        QString error;
        if (!parseUsePath(frame, &allocation, &error)) {
            attemptRetryOrFail(QStringLiteral("protocol"), error);
            return;
        }
        allocation.relayConnectionId = m_relayConnectionId;
        allocation.allocationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        allocation.allocatedAt = QDateTime::currentDateTimeUtc();

        const bool wasRefresh = m_refreshInProgress;
        m_refreshInProgress = false;
        m_allocation = allocation;
        m_retryCount = 0;
        transitionTo(State::Allocated);
        logEvent(MsrpRelayDiagnosticsEvent::Kind::AllocationSuccess, QString(),
                frame.responseCode, frame.responseComment);
        scheduleRefresh();
        if (wasRefresh)
            emit allocationRefreshed(m_allocation);
        else
            emit allocationReady(m_allocation);
        return;
    }

    // Any other final response is a hard authentication/allocation failure —
    // never invented/assumed, only what the relay actually returned.
    attemptRetryOrFail(QStringLiteral("auth"),
                       QStringLiteral("relay returned %1 %2").arg(frame.responseCode).arg(frame.responseComment));
}

void MsrpRelayClient::sendAuthenticatedAuth(const QString &password)
{
    MsrpDigestAuth::Challenge challenge;
    challenge.ok = true;
    challenge.realm = m_realm;
    challenge.nonce = m_nonce;
    challenge.algorithm = m_algorithm;
    challenge.qop = m_qop;
    challenge.opaque = m_opaque;

    ++m_nonceCount;
    const QString nc = QStringLiteral("%1").arg(m_nonceCount, 8, 16, QLatin1Char('0'));
    const QString uri = digestUri();
    const QString authorization = MsrpDigestAuth::buildAuthorizationHeader(
        challenge, m_config.username, password, uri, m_cnonce, nc);

    MsrpFrame frame;
    frame.isRequest = true;
    frame.method = QStringLiteral("AUTH");
    frame.transactionId = QUuid::createUuid().toString(QUuid::Id128).left(16);
    frame.toPath = relayToPathHeader();
    frame.fromPath = QStringLiteral("msrp://0.0.0.0:0/%1;tcp").arg(m_localTempSessionId);
    frame.unknownHeaders.insert(QStringLiteral("Authorization"), authorization);

    bool ok = false;
    const QByteArray bytes = MsrpFrameSerializer::serialize(frame, &ok);
    if (!ok) {
        failAllocation(QStringLiteral("protocol"), QStringLiteral("failed to serialize authenticated AUTH"));
        return;
    }

    m_transport->sendBytes(bytes);
    logEvent(MsrpRelayDiagnosticsEvent::Kind::AuthenticatedAuthSent);
    transitionTo(State::WaitingAllocation);
    m_authTimer->start(m_config.authTimeoutMs);
}

bool MsrpRelayClient::parseUsePath(const MsrpFrame &frame, MsrpRelayAllocation *out, QString *error) const
{
    const QString usePathHeader = frame.unknownHeaders.value(QStringLiteral("Use-Path"));
    if (usePathHeader.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("200 OK missing Use-Path header");
        return false;
    }
    const QList<MsrpUri> path = MsrpPath::parsePath(usePathHeader);
    if (path.isEmpty() || !path.first().ok) {
        if (error) *error = QStringLiteral("Use-Path did not parse to a valid MSRP URI");
        return false;
    }

    int expirySeconds = 600; // RFC 4976 default when the relay omits Expires
    const QString expiresHeader = frame.unknownHeaders.value(QStringLiteral("Expires"));
    if (!expiresHeader.isEmpty()) {
        bool ok = false;
        const int parsed = expiresHeader.toInt(&ok);
        if (ok && parsed > 0)
            expirySeconds = parsed;
    }

    out->valid = true;
    out->usePath = path;
    out->expiresAt = QDateTime::currentDateTimeUtc().addSecs(expirySeconds);
    return true;
}

void MsrpRelayClient::scheduleRefresh()
{
    if (!m_allocation.valid || !m_allocation.expiresAt.isValid())
        return;
    const qint64 secsUntilExpiry = QDateTime::currentDateTimeUtc().secsTo(m_allocation.expiresAt);
    const qint64 refreshInSecs = qMax<qint64>(1, secsUntilExpiry - m_config.refreshMarginSeconds);
    m_refreshTimer->start(static_cast<int>(refreshInSecs * 1000));
}

void MsrpRelayClient::onRefreshTimerFired()
{
    if (!m_transport || !m_transport->isConnected()) {
        // Connection dropped silently between allocations — reconnect from
        // scratch rather than trying to refresh over a dead socket.
        logEvent(MsrpRelayDiagnosticsEvent::Kind::Reconnect);
        transitionTo(State::Reauthenticating);
        openTransport();
        return;
    }
    transitionTo(State::Refreshing);
    m_refreshInProgress = true;
    logEvent(MsrpRelayDiagnosticsEvent::Kind::Refresh);
    // A fresh AUTH transaction (relay may issue a new nonce/stale=true);
    // sendInitialAuth() re-drives the full challenge/response cycle.
    sendInitialAuth();
}

void MsrpRelayClient::onAuthTimeout()
{
    attemptRetryOrFail(QStringLiteral("timeout"), QStringLiteral("relay AUTH response timed out"));
}

void MsrpRelayClient::onTransportError(const QString &message)
{
    logEvent(MsrpRelayDiagnosticsEvent::Kind::ConnectFailed, message);
    attemptRetryOrFail(QStringLiteral("network"), message);
}

void MsrpRelayClient::onTransportDisconnected()
{
    if (m_allocation.valid) {
        m_allocation.valid = false;
        emit allocationLost(QStringLiteral("relay control connection closed"));
    }
    // A relay that rejects an AUTH and then closes the connection fires
    // both handleAuthFrame() (via bytesReceived) and this slot (via
    // disconnected()) for the same underlying failure — handleAuthFrame()
    // already called attemptRetryOrFail() and moved the state machine to
    // Reauthenticating/Failed, so retrying again here would double-consume
    // the retry budget and schedule a duplicate reconnect. Only treat the
    // disconnect itself as a new failure when nothing has already reacted
    // to it (i.e. we still believed the connection was healthy).
    const bool alreadyHandled = m_state == State::Closing || m_state == State::Closed
        || m_state == State::Reauthenticating || m_state == State::Failed;
    if (!alreadyHandled)
        attemptRetryOrFail(QStringLiteral("network"), QStringLiteral("relay control connection closed"));
}

void MsrpRelayClient::attemptRetryOrFail(const QString &category, const QString &reason)
{
    m_refreshTimer->stop();
    if (m_retryCount >= m_config.maxRetries) {
        failAllocation(category, reason);
        return;
    }
    ++m_retryCount;
    logEvent(MsrpRelayDiagnosticsEvent::Kind::Reconnect, reason);
    transitionTo(State::Reauthenticating);
    // Deferred: attemptRetryOrFail() is frequently reached from inside one
    // of m_transport's own signal handlers (bytesReceived/errorOccurred/
    // disconnected) — openTransport() destroys that same transport via
    // m_transport.reset(), which must never happen while it is still
    // unwinding its own synchronous signal emission. QTimer::singleShot(0)
    // defers the reconnect to the next event-loop iteration instead.
    QTimer::singleShot(0, this, [this]() { openTransport(); });
}

void MsrpRelayClient::failAllocation(const QString &category, const QString &reason)
{
    m_refreshTimer->stop();
    m_authTimer->stop();
    m_refreshInProgress = false;
    const bool hadAllocation = m_allocation.valid;
    m_allocation.valid = false;
    transitionTo(State::Failed);
    MsrpRelayDiagnosticsEvent::Kind kind = MsrpRelayDiagnosticsEvent::Kind::AllocationFailure;
    logEvent(kind, reason);
    if (hadAllocation)
        emit allocationLost(reason);
    else
        emit allocationFailed(QStringLiteral("[%1] %2").arg(category, reason));
}

void MsrpRelayClient::close()
{
    m_refreshTimer->stop();
    m_authTimer->stop();
    transitionTo(State::Closing);
    if (m_transport)
        m_transport->closeGracefully();
    m_allocation.valid = false;
    logEvent(MsrpRelayDiagnosticsEvent::Kind::Closed);
    transitionTo(State::Closed);
}
