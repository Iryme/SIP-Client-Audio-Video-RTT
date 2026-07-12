#include "MsrpFrameSerializer.h"

namespace {

bool headerValueSafe(const QString &value)
{
    for (const QChar &c : value) {
        if (c == QLatin1Char('\r') || c == QLatin1Char('\n') || c == QLatin1Char('\0'))
            return false;
    }
    return true;
}

void appendHeader(QByteArray &out, const QString &name, const QString &value)
{
    if (value.isEmpty())
        return;
    out += name.toUtf8() + ": " + value.toUtf8() + "\r\n";
}

} // namespace

namespace MsrpFrameSerializer {

QByteArray serialize(const MsrpFrame &frame, bool *ok)
{
    if (ok) *ok = false;

    if (frame.transactionId.isEmpty() || !headerValueSafe(frame.transactionId))
        return QByteArray();

    if (!headerValueSafe(frame.toPath) || !headerValueSafe(frame.fromPath)
        || !headerValueSafe(frame.messageId) || !headerValueSafe(frame.contentType)
        || !headerValueSafe(frame.contentDisposition)
        || !headerValueSafe(frame.successReport) || !headerValueSafe(frame.failureReport)
        || !headerValueSafe(frame.status) || !headerValueSafe(frame.method))
        return QByteArray();

    for (auto it = frame.unknownHeaders.constBegin(); it != frame.unknownHeaders.constEnd(); ++it) {
        if (!headerValueSafe(it.key()) || !headerValueSafe(it.value()))
            return QByteArray();
    }

    QByteArray out;
    if (frame.isRequest) {
        out += "MSRP " + frame.transactionId.toUtf8() + " " + frame.method.toUtf8() + "\r\n";
    } else {
        out += "MSRP " + frame.transactionId.toUtf8() + " "
             + QByteArray::number(frame.responseCode);
        if (!frame.responseComment.isEmpty())
            out += " " + frame.responseComment.toUtf8();
        out += "\r\n";
    }

    appendHeader(out, QStringLiteral("To-Path"), frame.toPath);
    appendHeader(out, QStringLiteral("From-Path"), frame.fromPath);
    appendHeader(out, QStringLiteral("Message-ID"), frame.messageId);
    if (frame.hasByteRange)
        appendHeader(out, QStringLiteral("Byte-Range"), frame.byteRange.toString());
    appendHeader(out, QStringLiteral("Success-Report"), frame.successReport);
    appendHeader(out, QStringLiteral("Failure-Report"), frame.failureReport);
    appendHeader(out, QStringLiteral("Status"), frame.status);
    for (auto it = frame.unknownHeaders.constBegin(); it != frame.unknownHeaders.constEnd(); ++it)
        appendHeader(out, it.key(), it.value());

    // Content-Type + blank line + body only when there actually is a body
    // (or an explicit Content-Type) — matches RFC 4975's example framing.
    if (!frame.contentType.isEmpty() || !frame.body.isEmpty()) {
        appendHeader(out, QStringLiteral("Content-Disposition"), frame.contentDisposition);
        appendHeader(out, QStringLiteral("Content-Type"), frame.contentType);
        out += "\r\n";
        out += frame.body; // binary-safe, never converted to/from QString
    }

    out += "\r\n-------" + frame.transactionId.toUtf8();
    out += msrpContinuationToChar(frame.continuation).toLatin1();
    out += "\r\n";

    if (ok) *ok = true;
    return out;
}

} // namespace MsrpFrameSerializer
