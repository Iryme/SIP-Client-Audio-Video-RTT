#pragma once
#include <QDateTime>
#include <QObject>
#include <memory>

#include "msrp/MsrpFrameParser.h"
#include "msrp/MsrpRelayAllocation.h"
#include "msrp/MsrpRelayConfig.h"
#include "msrp/MsrpRelayDiagnosticsEvent.h"
#include "msrp/MsrpTransport.h"

class QTimer;

// RFC 4976 MSRP relay client (Task W107): a separate control-connection
// component from MsrpSession — it owns its own MsrpTransport, connects to
// the configured relay, performs the AUTH challenge/response handshake,
// and produces an MsrpRelayAllocation (Use-Path) that the caller can then
// hand to MsrpSipMediaInjector for SDP offer/answer construction and to
// MsrpSession as the session's outbound path. MsrpSession itself is never
// modified to know about relay protocol details — see docs/msrp-relay-authentication.md.
class MsrpRelayClient : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Idle,
        Connecting,
        Connected,
        SendingInitialAuth,
        WaitingChallenge,
        SendingAuthenticatedAuth,
        WaitingAllocation,
        Allocated,
        Refreshing,
        Reauthenticating,
        Closing,
        Closed,
        Failed
    };
    Q_ENUM(State)

    explicit MsrpRelayClient(QObject *parent = nullptr);
    ~MsrpRelayClient() override;

    // Correlation ids the caller assigns for mapping (see MsrpRelayDiagnosticsEvent);
    // never used for allocation matching logic itself — see docs/msrp-relay-allocation.md.
    void setCorrelation(const QString &sipCallIdRedacted, int mediaIndex);

    void configure(const MsrpRelayConfig &config, const QString &password);

    // Connects, authenticates, and requests an allocation. Emits
    // allocationReady() or allocationFailed() exactly once for this call
    // (subsequent refresh cycles use allocationRefreshed()/allocationLost()).
    void start();
    void close();

    State state() const { return m_state; }
    MsrpRelayAllocation allocation() const { return m_allocation; }
    bool isAllocated() const;

signals:
    void stateChanged(MsrpRelayClient::State state);
    void allocationReady(const MsrpRelayAllocation &allocation);
    void allocationRefreshed(const MsrpRelayAllocation &allocation);
    void allocationFailed(const QString &reason);
    void allocationLost(const QString &reason);

private slots:
    void onTransportConnected();
    void onTransportBytes(const QByteArray &data);
    void onTransportError(const QString &message);
    void onTransportDisconnected();
    void onAuthTimeout();
    void onRefreshTimerFired();

private:
    void transitionTo(State s);
    void openTransport();
    QString digestUri() const;
    QString relayToPathHeader() const;
    void sendInitialAuth();
    void sendAuthenticatedAuth(const QString &password);
    void handleAuthFrame(const MsrpFrame &frame);
    bool parseUsePath(const MsrpFrame &frame, MsrpRelayAllocation *out, QString *error) const;
    void scheduleRefresh();
    void failAllocation(const QString &category, const QString &reason);
    void attemptRetryOrFail(const QString &category, const QString &reason);
    void logEvent(MsrpRelayDiagnosticsEvent::Kind kind, const QString &error = QString(),
                 int responseCode = 0, const QString &responseComment = QString());

    MsrpRelayConfig m_config;
    QString m_password; // held only in memory for the duration of the AUTH handshake, never logged/serialized
    QString m_sipCallIdRedacted;
    int m_mediaIndex{-1};

    State m_state{State::Idle};
    std::unique_ptr<MsrpTransport> m_transport;
    MsrpFrameParser m_parser;
    MsrpRelayAllocation m_allocation;

    // Digest state for the currently-outstanding challenge.
    QString m_realm;
    QString m_nonce;
    QString m_algorithm;
    QString m_qop;
    QString m_opaque;
    QString m_cnonce;
    int m_nonceCount{0};

    QString m_relayConnectionId;
    QString m_localTempSessionId; // From-Path session-id used only for the unauthenticated AUTH
    int m_retryCount{0};
    // Set just before a refresh/reauth AUTH cycle starts and consulted only
    // when that cycle's final response arrives — intervening state
    // transitions (SendingInitialAuth -> WaitingChallenge -> ...) would
    // otherwise make it impossible to tell an initial allocation from a
    // refreshed one purely from m_state at completion time.
    bool m_refreshInProgress{false};

    QTimer *m_authTimer{nullptr};
    QTimer *m_refreshTimer{nullptr};
};
