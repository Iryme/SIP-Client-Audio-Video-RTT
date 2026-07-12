#include "MsrpFrameParser.h"

namespace {

bool isValidTransactionIdChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '.' || c == '-' || c == '+' || c == '%' || c == '=';
}

QMap<QString, QString> parseKnownAndUnknown(const QByteArray &headerBytes, MsrpFrame &frame)
{
    QMap<QString, QString> unknown;
    const QList<QByteArray> lines = headerBytes.split('\n');
    for (QByteArray rawLine : lines) {
        if (rawLine.endsWith('\r'))
            rawLine.chop(1);
        if (rawLine.isEmpty())
            continue;
        const int colonIdx = rawLine.indexOf(':');
        if (colonIdx < 0)
            continue; // malformed header line: tolerated, not fatal
        const QString name = QString::fromUtf8(rawLine.left(colonIdx)).trimmed();
        const QString value = QString::fromUtf8(rawLine.mid(colonIdx + 1)).trimmed();

        if (name.compare(QStringLiteral("To-Path"), Qt::CaseInsensitive) == 0) {
            frame.toPath = value;
        } else if (name.compare(QStringLiteral("From-Path"), Qt::CaseInsensitive) == 0) {
            frame.fromPath = value;
        } else if (name.compare(QStringLiteral("Message-ID"), Qt::CaseInsensitive) == 0) {
            frame.messageId = value;
        } else if (name.compare(QStringLiteral("Content-Type"), Qt::CaseInsensitive) == 0) {
            frame.contentType = value;
        } else if (name.compare(QStringLiteral("Content-Disposition"), Qt::CaseInsensitive) == 0) {
            frame.contentDisposition = value;
        } else if (name.compare(QStringLiteral("Success-Report"), Qt::CaseInsensitive) == 0) {
            frame.successReport = value;
        } else if (name.compare(QStringLiteral("Failure-Report"), Qt::CaseInsensitive) == 0) {
            frame.failureReport = value;
        } else if (name.compare(QStringLiteral("Status"), Qt::CaseInsensitive) == 0) {
            frame.status = value;
        } else if (name.compare(QStringLiteral("Byte-Range"), Qt::CaseInsensitive) == 0) {
            bool ok = false;
            frame.byteRange = MsrpByteRange::fromHeaderValue(value, &ok);
            frame.hasByteRange = ok;
            if (!ok)
                unknown.insert(name, value); // preserved for diagnostics even though invalid
        } else {
            // Duplicate known headers or genuinely unknown headers both
            // land here, last-one-wins in the map — preserved for
            // diagnostics, never re-interpreted as protocol-affecting.
            unknown.insert(name, value);
        }
    }
    return unknown;
}

} // namespace

MsrpFrameParser::MsrpFrameParser(int maxFrameBytes) : m_maxFrameBytes(maxFrameBytes) {}

void MsrpFrameParser::reset()
{
    m_buffer.clear();
}

QList<MsrpFrameParser::Result> MsrpFrameParser::feed(const QByteArray &data)
{
    m_buffer.append(data);

    QList<Result> results;
    while (true) {
        Result r = tryParseOne();
        if (r.status == Status::NeedMoreData)
            break;
        results.append(r);
        if (r.status == Status::Invalid || r.status == Status::LimitExceeded) {
            // Recovery: search for the next plausible start-line ("MSRP ")
            // after the current buffer start and discard everything before
            // it, so one malformed frame does not permanently wedge the
            // connection. If none is found, drop the whole buffer.
            const int nextStart = m_buffer.indexOf("MSRP ", 1);
            if (nextStart > 0)
                m_buffer.remove(0, nextStart);
            else
                m_buffer.clear();
            if (m_buffer.isEmpty())
                break;
        }
    }
    return results;
}

MsrpFrameParser::Result MsrpFrameParser::tryParseOne()
{
    Result result;

    const int startLineEnd = m_buffer.indexOf("\r\n");
    if (startLineEnd < 0) {
        if (m_buffer.size() > m_maxFrameBytes) {
            result.status = Status::LimitExceeded;
            result.errorMessage = QStringLiteral("no CRLF found within max frame size");
        }
        return result;
    }

    const QByteArray startLine = m_buffer.left(startLineEnd);
    if (!startLine.startsWith("MSRP ")) {
        result.status = Status::Invalid;
        result.errorMessage = QStringLiteral("start-line does not begin with 'MSRP '");
        return result;
    }

    const QList<QByteArray> tokens = startLine.split(' ');
    // tokens: ["MSRP", "", ...] guard against empty splits from multiple spaces
    QList<QByteArray> filtered;
    for (const QByteArray &t : tokens)
        if (!t.isEmpty()) filtered.append(t);

    if (filtered.size() < 3) {
        result.status = Status::Invalid;
        result.errorMessage = QStringLiteral("malformed start-line");
        return result;
    }

    const QByteArray tidBytes = filtered.at(1);
    for (char c : tidBytes) {
        if (!isValidTransactionIdChar(c)) {
            result.status = Status::Invalid;
            result.errorMessage = QStringLiteral("invalid character in transaction-id");
            return result;
        }
    }
    if (tidBytes.isEmpty()) {
        result.status = Status::Invalid;
        result.errorMessage = QStringLiteral("empty transaction-id");
        return result;
    }
    const QString transactionId = QString::fromLatin1(tidBytes);

    const QByteArray thirdToken = filtered.at(2);
    bool isNumeric = !thirdToken.isEmpty();
    for (char c : thirdToken) {
        if (c < '0' || c > '9') { isNumeric = false; break; }
    }

    MsrpFrame frame;
    frame.transactionId = transactionId;
    if (isNumeric) {
        frame.isRequest = false;
        frame.responseCode = thirdToken.toInt();
        QByteArrayList commentParts;
        for (int i = 3; i < filtered.size(); ++i)
            commentParts.append(filtered.at(i));
        frame.responseComment = QString::fromUtf8(commentParts.join(' '));
    } else {
        frame.isRequest = true;
        frame.method = QString::fromLatin1(thirdToken).toUpper();
    }

    const QByteArray delimiter = "\r\n-------" + tidBytes;
    const int delimiterPos = m_buffer.indexOf(delimiter, startLineEnd + 2);
    if (delimiterPos < 0) {
        if (m_buffer.size() > m_maxFrameBytes) {
            result.status = Status::LimitExceeded;
            result.errorMessage = QStringLiteral("frame exceeds max size before delimiter found");
            return result;
        }
        return result; // NeedMoreData
    }

    const int flagPos = delimiterPos + delimiter.size();
    if (flagPos >= m_buffer.size())
        return result; // NeedMoreData — need the continuation flag byte
    const char flagChar = m_buffer.at(flagPos);
    if (flagChar != '+' && flagChar != '$' && flagChar != '#') {
        result.status = Status::Invalid;
        result.errorMessage = QStringLiteral("invalid continuation flag");
        return result;
    }
    const int afterFlag = flagPos + 1;
    if (afterFlag + 2 > m_buffer.size())
        return result; // NeedMoreData — need trailing CRLF
    if (m_buffer.at(afterFlag) != '\r' || m_buffer.at(afterFlag + 1) != '\n') {
        result.status = Status::Invalid;
        result.errorMessage = QStringLiteral("end-line missing trailing CRLF");
        return result;
    }

    if (delimiterPos - startLineEnd > m_maxFrameBytes) {
        result.status = Status::LimitExceeded;
        result.errorMessage = QStringLiteral("frame body/headers exceed max frame size");
        return result;
    }

    frame.continuation = msrpContinuationFromChar(QLatin1Char(flagChar));

    const QByteArray middle = m_buffer.mid(startLineEnd + 2, delimiterPos - (startLineEnd + 2));
    const int blankLineIdx = middle.indexOf("\r\n\r\n");
    QByteArray headerBytes;
    if (blankLineIdx >= 0) {
        headerBytes = middle.left(blankLineIdx);
        frame.body = middle.mid(blankLineIdx + 4);
    } else {
        headerBytes = middle;
    }

    frame.unknownHeaders = parseKnownAndUnknown(headerBytes, frame);

    const int totalConsumed = afterFlag + 2;
    m_buffer.remove(0, totalConsumed);

    result.status = Status::Complete;
    result.frame = frame;
    return result;
}
