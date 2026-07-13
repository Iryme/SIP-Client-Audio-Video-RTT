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
    void setSipHeaderCallId(const QString &callId);
    void setMediaIndex(int index);
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

    // Task W104 (RFC 5547 MSRP file transfer). Reads filePath fully into
    // memory (same bounded-in-memory model as every other MSRP message —
    // see setMaxMessageBytes()) and sends it as one MSRP message with a
    // Content-Disposition: attachment header carrying a sanitized filename.
    // Rejects (ok=false) rather than truncating/streaming-partial when the
    // file cannot be opened or exceeds the configured max message size.
    struct FileSendResult
    {
        bool ok{false};
        QString error;
        QString messageId;
        qint64 fileSize{0};
        QString sha1Hex;
    };
    FileSendResult sendFile(const QString &filePath, const QString &contentType);

    MsrpSessionInfo info() const { return m_info; }

    // Actual bound/connected local TCP port — needed by callers (and the
    // local test harness) once listenAsPassive() has chosen an OS-assigned
    // ephemeral port, before the SDP answer embedding it can be built.
    int transportLocalPort() const { return m_transport ? m_transport->localPort() : 0; }

signals:
    void payloadReceived(const QString &sessionKey, const QString &messageId,
                         const QString &contentType, const QByteArray &body);

    // Task W101 Phase 6: fires once this outbound message's delivery is
    // known — either from the MSRP response to SEND (when no REPORT was
    // requested) or from a subsequent REPORT frame. Never fires twice for
    // the same messageId with a "final" outcome from the SEND response
    // path if a REPORT is still pending for it.
    void messageDeliveryStatusChanged(const QString &sessionKey, const QString &messageId,
                                      bool success, const QString &statusText);

    // Task W104: fires in addition to payloadReceived (never instead of it)
    // whenever an assembled inbound message's Content-Disposition indicates
    // a file transfer ("attachment"). Nothing here touches disk — the
    // caller decides whether/where to save via MsrpFileReceiver::saveToPath.
    void fileTransferReceived(const QString &sessionKey, const QString &messageId,
                              const QString &contentType, const QString &suggestedFileName,
                              const QByteArray &body);

private slots:
    void onTransportConnected();
    void onTransportBytes(const QByteArray &data);
    void onTransportError(const QString &message);
    void onTransportDisconnected();

private:
    void createTransport(MsrpTransportProtocol proto);
    void handleFrame(const MsrpFrame &frame);

    // Task W105 (RFC 4975 §7.1 hardening): an MSRP passive listener accepts
    // the first TCP connection unconditionally (see MsrpTcpTransport), so
    // without this check any local process able to reach the ephemeral port
    // before the real peer would be silently trusted and could inject
    // SEND/REPORT requests into the session. Returns true only when the
    // request's To-Path last URI's session-id equals this session's own
    // negotiated local session-id — the one piece of the URI an attacker
    // cannot guess without having already observed the SDP a=path exchanged
    // over the signaling channel.
    bool toPathTargetsThisSession(const QString &toPathHeader) const;
    void rejectUnauthorizedRequest(const MsrpFrame &frame, const QString &reason);
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
    qint64 m_maxMessageBytes{2 * 1024 * 1024};
    bool m_requestReports{true};
    bool m_tlsVerifyPeer{true};
    QString m_tlsCaCertificatePath;
};
