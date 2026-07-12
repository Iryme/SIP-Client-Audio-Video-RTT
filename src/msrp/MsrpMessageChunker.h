#pragma once
#include <QByteArray>
#include <QList>
#include <QString>

#include "msrp/MsrpFrame.h"

// Splits an outbound message body into one or more SEND-frame chunks
// (Task W100, section K) — pure, transport-free, fully unit-testable.
// chunkSizeBytes <= 0 or body.size() <= chunkSizeBytes produces a single
// chunk with no Byte-Range header (matches the common unchunked case).
namespace MsrpMessageChunker {

QList<MsrpFrame> buildSendFrames(const QString &transactionIdPrefix,
                                 const QString &toPath, const QString &fromPath,
                                 const QString &messageId, const QString &contentType,
                                 const QByteArray &body, int chunkSizeBytes,
                                 bool requestSuccessReport, bool requestFailureReport);

} // namespace MsrpMessageChunker
