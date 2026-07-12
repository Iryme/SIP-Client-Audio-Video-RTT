#pragma once
#include <QByteArray>
#include <QMap>
#include <QMetaType>
#include <QString>

#include "msrp/MsrpTypes.h"

// RFC 4975 Byte-Range header: "start-end/total", with end/total allowed to
// be "*" (unknown at send time). -1 encodes "*"/absent.
struct MsrpByteRange
{
    qint64 start{1};
    qint64 end{-1};
    qint64 total{-1};

    bool endIsStar() const { return end == -1; }
    bool totalIsStar() const { return total == -1; }

    QString toString() const
    {
        const QString endStr = endIsStar() ? QStringLiteral("*") : QString::number(end);
        const QString totalStr = totalIsStar() ? QStringLiteral("*") : QString::number(total);
        return QStringLiteral("%1-%2/%3").arg(start).arg(endStr, totalStr);
    }

    static MsrpByteRange fromHeaderValue(const QString &value, bool *ok = nullptr)
    {
        MsrpByteRange range;
        if (ok) *ok = false;
        const int dashIdx = value.indexOf(QLatin1Char('-'));
        const int slashIdx = value.indexOf(QLatin1Char('/'));
        if (dashIdx <= 0 || slashIdx <= dashIdx)
            return range;

        bool okStart = false;
        range.start = value.left(dashIdx).toLongLong(&okStart);
        if (!okStart || range.start < 1)
            return range;

        const QString endPart = value.mid(dashIdx + 1, slashIdx - dashIdx - 1);
        if (endPart == QLatin1String("*")) {
            range.end = -1;
        } else {
            bool okEnd = false;
            range.end = endPart.toLongLong(&okEnd);
            if (!okEnd || range.end < range.start)
                return range;
        }

        const QString totalPart = value.mid(slashIdx + 1);
        if (totalPart == QLatin1String("*")) {
            range.total = -1;
        } else {
            bool okTotal = false;
            range.total = totalPart.toLongLong(&okTotal);
            if (!okTotal || range.total < 0 || (!range.endIsStar() && range.total < range.end))
                return range;
        }

        if (ok) *ok = true;
        return range;
    }
};

// One parsed/serialized MSRP frame (request or response). Never derived
// from or converted to a QString for the body — always raw QByteArray.
struct MsrpFrame
{
    bool isRequest{true};
    QString transactionId;

    // Request fields
    QString method;          // "SEND" / "REPORT" / other token, uppercased

    // Response fields
    int responseCode{0};
    QString responseComment;

    QString toPath;          // raw header value (space-separated URI list)
    QString fromPath;
    QString messageId;
    QString contentType;
    QString successReport;   // "yes" / "no"
    QString failureReport;   // "yes" / "no" / "partial"
    QString status;          // response "Status:" header, e.g. "000 200 OK"
    bool hasByteRange{false};
    MsrpByteRange byteRange;

    QMap<QString, QString> unknownHeaders; // preserved for diagnostics, never re-interpreted

    QByteArray body;
    MsrpContinuation continuation{MsrpContinuation::Complete};

    bool isSuccessResponse() const { return !isRequest && responseCode >= 200 && responseCode < 300; }
};

Q_DECLARE_METATYPE(MsrpFrame)
