#include "MsrpMessageChunker.h"

#include <QRandomGenerator>

namespace MsrpMessageChunker {

namespace {
QString randomTransactionId(const QString &prefix, int index)
{
    static const QString kAlphabet = QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789");
    QString suffix;
    for (int i = 0; i < 8; ++i)
        suffix.append(kAlphabet.at(QRandomGenerator::global()->bounded(kAlphabet.size())));
    return QStringLiteral("%1%2-%3").arg(prefix).arg(index).arg(suffix);
}
} // namespace

QList<MsrpFrame> buildSendFrames(const QString &transactionIdPrefix,
                                 const QString &toPath, const QString &fromPath,
                                 const QString &messageId, const QString &contentType,
                                 const QByteArray &body, int chunkSizeBytes,
                                 bool requestSuccessReport, bool requestFailureReport,
                                 const QString &contentDisposition)
{
    QList<MsrpFrame> frames;

    const bool unchunked = chunkSizeBytes <= 0 || body.size() <= chunkSizeBytes;
    const QString successVal = requestSuccessReport ? QStringLiteral("yes") : QStringLiteral("no");
    const QString failureVal = requestFailureReport ? QStringLiteral("yes") : QStringLiteral("no");

    if (unchunked) {
        MsrpFrame f;
        f.isRequest = true;
        f.method = QStringLiteral("SEND");
        f.transactionId = randomTransactionId(transactionIdPrefix, 0);
        f.toPath = toPath;
        f.fromPath = fromPath;
        f.messageId = messageId;
        f.contentType = contentType;
        f.contentDisposition = contentDisposition;
        f.successReport = successVal;
        f.failureReport = failureVal;
        f.body = body;
        f.continuation = MsrpContinuation::Complete;
        frames.append(f);
        return frames;
    }

    const qint64 total = body.size();
    qint64 offset = 0;
    int index = 0;
    while (offset < total) {
        const qint64 remaining = total - offset;
        const qint64 thisChunkSize = qMin<qint64>(chunkSizeBytes, remaining);
        const bool isLast = (offset + thisChunkSize) >= total;

        MsrpFrame f;
        f.isRequest = true;
        f.method = QStringLiteral("SEND");
        f.transactionId = randomTransactionId(transactionIdPrefix, index);
        f.toPath = toPath;
        f.fromPath = fromPath;
        f.messageId = messageId;
        f.contentType = contentType;
        f.contentDisposition = contentDisposition;
        // Success-Report/Failure-Report are only meaningful on the final
        // chunk per RFC 4975 (intermediate chunks report per-chunk
        // acceptance via the ordinary 200 response only).
        f.successReport = isLast ? successVal : QStringLiteral("no");
        f.failureReport = isLast ? failureVal : QStringLiteral("no");
        f.hasByteRange = true;
        f.byteRange.start = offset + 1;
        f.byteRange.end = offset + thisChunkSize;
        f.byteRange.total = total;
        f.body = body.mid(static_cast<int>(offset), static_cast<int>(thisChunkSize));
        f.continuation = isLast ? MsrpContinuation::Complete : MsrpContinuation::More;

        frames.append(f);
        offset += thisChunkSize;
        ++index;
    }

    return frames;
}

} // namespace MsrpMessageChunker
