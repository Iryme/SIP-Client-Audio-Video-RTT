#pragma once
#include <QByteArray>
#include <QList>
#include <QString>

#include "msrp/MsrpFrame.h"

// Incremental, binary-safe MSRP frame parser (Task W100, section I).
// Never converts the accumulation buffer to QString and never searches for
// the delimiter via text conversion — all scanning is done on raw
// QByteArray. Feed arbitrarily-chunked TCP reads via feed(); each call
// returns zero or more completed/invalid results without blocking.
class MsrpFrameParser
{
public:
    enum class Status { NeedMoreData, Complete, Invalid, LimitExceeded };

    struct Result
    {
        Status status{Status::NeedMoreData};
        MsrpFrame frame;
        QString errorMessage;
    };

    // maxFrameBytes bounds a single frame's total wire size (start-line +
    // headers + body + end-line) — protects against unbounded memory
    // growth from a malicious/broken peer that never sends a delimiter.
    explicit MsrpFrameParser(int maxFrameBytes = 16384);

    void reset();

    // Appends data and extracts as many complete frames as are currently
    // available. After an Invalid/LimitExceeded result, the parser
    // discards the offending bytes up to (and including) the next
    // recognizable start-line so a single malformed frame does not
    // permanently wedge the connection ("recovery after invalid frame").
    QList<Result> feed(const QByteArray &data);

private:
    Result tryParseOne();

    QByteArray m_buffer;
    int m_maxFrameBytes;
};
