#pragma once
#include <QObject>
#include <memory>

#include "msrp/MsrpChunkAssembler.h"
#include "msrp/MsrpFrameParser.h"
#include "msrp/MsrpPath.h"
#include "msrp/MsrpSessionInfo.h"
#include "msrp/MsrpTransactionStore.h"
#include "msrp/MsrpTransport.h"

// Orchestrates one MSRP session (Task W100): owns the transport, feeds
// inbound bytes through MsrpFrameParser, reassembles chunked messages via
// MsrpChunkAssembler, tracks transactions via MsrpTransactionStore, and
// publishes session/diagnostics updates to the global
// MsrpSessionStore/MsrpDiagnosticsStore singletons. Runs entirely on the
// Qt event loop (see MsrpTransport) — no worker-thread marshaling needed.
class MsrpSession : public QObject
{
    Q_OBJECT
public:
    explicit MsrpSession(const QString &sessionKey, QObject *parent = nullptr);
    ~MsrpSession() override;
    // Non-copyable/movable: owns a live transport + parser/assembler state.
    MsrpSession(const MsrpSession &) = delete;
    MsrpSession &operator=(const MsrpSession &) = delete;

    void setLocalUri(const MsrpUri &uri);
    void setRemotePath(const QList<MsrpUri> &path);
    void setSipCallId(const QString &callId);
    void setChunkSizeBytes(int n);
    void setRequestReports(bool on);
    void setMaxFrameBytes(int n);
    void setMaxMessageBytes(qint64 n);
    void setTlsVerifyPeer(bool verify);
    void setTlsCaCertificatePath(const QString &path);

    void connectAsActive(int connectTimeoutMs);
    void listenAsPassive(const QString &bindAddress, int acceptTimeoutMs);
    void closeSession();
    void abortSession(const QString &reason);

    // Returns the generated Message-ID.
    QString sendMessage(const QString &contentType, const QByteArray &body);

    MsrpSessionInfo info() const { return m_info; }

    // Actual bound/connected local TCP port — needed by callers (and the
    // local test harness) once listenAsPassive() has chosen an OS-assigned
    // ephemeral port, before the SDP answer embedding it can be built.
    int transportLocalPort() const { return m_transport ? m_transport->localPort() : 0; }

signals:
    void payloadReceived(const QString &sessionKey, const QString &messageId,
                         const QString &contentType, const QByteArray &body);

private slots:
    void onTransportConnected();
    void onTransportBytes(const QByteArray &data);
    void onTransportError(const QString &message);
    void onTransportDisconnected();

private:
    void createTransport(MsrpTransportProtocol proto);
    void handleFrame(const MsrpFrame &frame);
    void sendFrame(MsrpFrame frame, bool track = false, MsrpMethod method = MsrpMethod::Unknown,
                  const QString &messageId = QString());
    void updateState(MsrpSessionState state);
    void publishInfo();
    void logDiagnostic(bool inbound, const MsrpFrame &frame, const QStringList &warnings = {});

    QString m_sessionKey;
    MsrpSessionInfo m_info;
    MsrpUri m_localUri;
    QList<MsrpUri> m_remotePath;

    std::unique_ptr<MsrpTransport> m_transport;
    MsrpFrameParser m_parser;
    MsrpChunkAssembler m_assembler;
    MsrpTransactionStore m_transactions;

    int m_chunkSizeBytes{2048};
    bool m_requestReports{true};
    bool m_tlsVerifyPeer{true};
    QString m_tlsCaCertificatePath;
};
