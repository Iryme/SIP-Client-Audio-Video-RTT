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

// contentDisposition (Task W104, RFC 5547 file transfer) is repeated on
// every chunk, same as contentType — defaults to empty for the ordinary
// chat-message case, which leaves the Content-Disposition header omitted.
QList<MsrpFrame> buildSendFrames(const QString &transactionIdPrefix,
                                 const QString &toPath, const QString &fromPath,
                                 const QString &messageId, const QString &contentType,
                                 const QByteArray &body, int chunkSizeBytes,
                                 bool requestSuccessReport, bool requestFailureReport,
                                 const QString &contentDisposition = QString());

} // namespace MsrpMessageChunker
