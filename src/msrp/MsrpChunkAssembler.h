#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include "msrp/MsrpFrame.h"

// Reassembles a chunked MSRP message (RFC 4975 section 7.3.1) from
// individual SEND-frame chunks, correlated by Message-ID. Memory use per
// message is bounded by maxMessageBytes — a peer can never make this
// allocate unbounded memory (Task W100, section K).
//
// Chunks are expected to arrive in order on a single well-behaved
// connection (the common case); out-of-order/overlapping chunks are still
// buffered (once the total size is known from a Byte-Range "start-end/total"
// header) but are flagged with a warning rather than silently accepted as
// fully RFC-perfect reassembly — see docs/msrp-protocol.md for the
// documented limitation.
class MsrpChunkAssembler
{
public:
    struct AssembledMessage
    {
        QString messageId;
        QString contentType;
        // RFC 5547 file transfer (Task W104): captured from the first chunk,
        // same as contentType — empty for ordinary chat messages.
        QString contentDisposition;
        QByteArray body;
        bool complete{false};
        bool aborted{false};
        QStringList warnings;
    };

    enum class FeedStatus { InProgress, Complete, Aborted, Error };

    struct FeedResult
    {
        FeedStatus status{FeedStatus::Error};
        AssembledMessage message;
        QString errorMessage;
    };

    explicit MsrpChunkAssembler(qint64 maxMessageBytes = 2 * 1024 * 1024);

    // frame must be a SEND request; range/continuation/body/messageId/
    // contentType are read directly from it.
    FeedResult feedChunk(const MsrpFrame &frame);

    void abort(const QString &messageId);
    bool hasPending(const QString &messageId) const;
    QDateTime lastActivity(const QString &messageId) const;

    // Removes (without completing) any pending message whose lastActivity
    // is older than idleDeadline — caller (MsrpSession) drives this off its
    // own timer using msrpTransactionTimeoutMs.
    QStringList purgeStale(const QDateTime &idleDeadline);

private:
    struct Pending
    {
        QByteArray data;
        qint64 total{-1};
        QString contentType;
        QString contentDisposition;
        QDateTime lastActivity;
        QSet<qint64> receivedStarts;
        qint64 bytesReceived{0};
        bool gapOrOverlap{false};
    };

    QMap<QString, Pending> m_pending;
    qint64 m_maxMessageBytes;
};
