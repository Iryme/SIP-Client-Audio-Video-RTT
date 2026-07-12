#pragma once
#include "etsi/LmpeCodec.h"

// Task W103 — fail-closed placeholder. Every operation always fails with an
// explanatory error; nothing here encodes or decodes real LMPE data. This
// exists so any future caller that forgets to check isFormatConfirmed()
// still fails safely instead of silently fabricating protocol compliance.
class UnconfirmedLmpeCodec : public LmpeCodec
{
public:
    QString formatName() const override { return QStringLiteral("unconfirmed"); }
    bool isFormatConfirmed() const override { return false; }

    LmpeCodecResult encode(const QByteArray & /*payload*/) const override
    {
        return blockedResult();
    }

    LmpeCodecResult decode(const QByteArray & /*wireData*/) const override
    {
        return blockedResult();
    }

private:
    static LmpeCodecResult blockedResult()
    {
        LmpeCodecResult result;
        result.success = false;
        result.error = QStringLiteral(
            "LMPE wire format not confirmed (Task W103 BLOCKED: no "
            "authoritative ETSI TS 103 698 source available in this "
            "environment). See docs/agent-results/"
            "W103-lmpe-foundation-result.md.");
        return result;
    }
};
