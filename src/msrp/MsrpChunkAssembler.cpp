#include "MsrpChunkAssembler.h"

#include <cstring>

MsrpChunkAssembler::MsrpChunkAssembler(qint64 maxMessageBytes) : m_maxMessageBytes(maxMessageBytes) {}

MsrpChunkAssembler::FeedResult MsrpChunkAssembler::feedChunk(const MsrpFrame &frame)
{
    FeedResult result;

    if (frame.messageId.isEmpty()) {
        result.status = FeedStatus::Error;
        result.errorMessage = QStringLiteral("chunk has no Message-ID");
        return result;
    }

    if (frame.continuation == MsrpContinuation::Abort) {
        abort(frame.messageId);
        result.status = FeedStatus::Aborted;
        result.message.messageId = frame.messageId;
        result.message.aborted = true;
        return result;
    }

    // Single-chunk fast path: a complete Byte-Range (1-N/N) or no
    // Byte-Range at all with a '$' continuation.
    const bool singleChunk = !frame.hasByteRange
        || (frame.byteRange.start == 1 && !frame.byteRange.endIsStar()
            && !frame.byteRange.totalIsStar() && frame.byteRange.end == frame.byteRange.total);

    if (singleChunk && frame.continuation == MsrpContinuation::Complete && !m_pending.contains(frame.messageId)) {
        if (frame.body.size() > m_maxMessageBytes) {
            result.status = FeedStatus::Error;
            result.errorMessage = QStringLiteral("message exceeds max message size");
            return result;
        }
        result.status = FeedStatus::Complete;
        result.message.messageId = frame.messageId;
        result.message.contentType = frame.contentType;
        result.message.body = frame.body;
        result.message.complete = true;
        return result;
    }

    Pending &pending = m_pending[frame.messageId];
    pending.lastActivity = QDateTime::currentDateTimeUtc();
    if (pending.contentType.isEmpty())
        pending.contentType = frame.contentType;
    else if (!frame.contentType.isEmpty() && pending.contentType != frame.contentType)
        result.errorMessage = QStringLiteral("Content-Type mismatch between chunks");

    const qint64 start = frame.hasByteRange ? frame.byteRange.start : 1;
    if (frame.hasByteRange && !frame.byteRange.totalIsStar() && pending.total < 0)
        pending.total = frame.byteRange.total;

    if (pending.total >= 0 && pending.total > m_maxMessageBytes) {
        m_pending.remove(frame.messageId);
        result.status = FeedStatus::Error;
        result.errorMessage = QStringLiteral("declared total exceeds max message size");
        return result;
    }

    if (pending.receivedStarts.contains(start)) {
        result.status = FeedStatus::InProgress;
        result.message.warnings << QStringLiteral("duplicate chunk at start=%1 ignored").arg(start);
        return result;
    }
    pending.receivedStarts.insert(start);

    const qint64 endOffset = start - 1 + frame.body.size();
    if (pending.total >= 0 && endOffset > pending.total) {
        result.status = FeedStatus::Error;
        result.errorMessage = QStringLiteral("chunk overflows declared total");
        m_pending.remove(frame.messageId);
        return result;
    }
    if (endOffset > m_maxMessageBytes) {
        result.status = FeedStatus::Error;
        result.errorMessage = QStringLiteral("assembled message would exceed max message size");
        m_pending.remove(frame.messageId);
        return result;
    }

    const qint64 expectedNext = pending.data.size() + 1;
    if (start != expectedNext) {
        pending.gapOrOverlap = true;
        result.message.warnings << (start > expectedNext
            ? QStringLiteral("gap detected before start=%1").arg(start)
            : QStringLiteral("overlap detected at start=%1").arg(start));
    }

    if (pending.data.size() < endOffset)
        pending.data.resize(static_cast<int>(endOffset));
    memcpy(pending.data.data() + (start - 1), frame.body.constData(), frame.body.size());
    pending.bytesReceived += frame.body.size();

    const bool isFinal = frame.continuation == MsrpContinuation::Complete;
    if (isFinal && pending.total < 0)
        pending.total = endOffset;

    if (pending.total >= 0 && pending.data.size() >= pending.total && !pending.gapOrOverlap) {
        result.status = FeedStatus::Complete;
        result.message.messageId = frame.messageId;
        result.message.contentType = pending.contentType;
        result.message.body = pending.data;
        result.message.complete = true;
        m_pending.remove(frame.messageId);
        return result;
    }

    result.status = FeedStatus::InProgress;
    result.message.messageId = frame.messageId;
    return result;
}

void MsrpChunkAssembler::abort(const QString &messageId)
{
    m_pending.remove(messageId);
}

bool MsrpChunkAssembler::hasPending(const QString &messageId) const
{
    return m_pending.contains(messageId);
}

QDateTime MsrpChunkAssembler::lastActivity(const QString &messageId) const
{
    return m_pending.value(messageId).lastActivity;
}

QStringList MsrpChunkAssembler::purgeStale(const QDateTime &idleDeadline)
{
    QStringList purged;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (it.value().lastActivity < idleDeadline) {
            purged << it.key();
            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }
    return purged;
}
