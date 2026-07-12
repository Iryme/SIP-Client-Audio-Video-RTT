#pragma once
#include <QByteArray>
#include <QString>

// Task W103 (LMPE Foundation) — neutral seam only.
//
// This interface intentionally assumes NOTHING about the real LMPE
// (ETSI TS 103 698) wire format: no content-type, no framing, no field
// list, no version. Task W103's own starting rule required confirming the
// real format against the SIP-Server-RTT repository or another authoritative
// source before writing any codec logic. That repository does not exist on
// the configured GitHub account (confirmed 404) and is not present anywhere
// in this workspace; no other authoritative LMPE wire-format source was
// found either (see docs/agent-results/W103-lmpe-foundation-result.md).
//
// Per that rule, W103 does not implement the LMPE codec. This interface
// exists only so a future task (once the format is confirmed) has a seam to
// implement against, and so nothing in this codebase silently pretends LMPE
// compatibility in the meantime — see LmpeCodec::isFormatConfirmed().
struct LmpeCodecResult
{
    bool success{false};
    QString error;
    QByteArray data;
};

class LmpeCodec
{
public:
    virtual ~LmpeCodec() = default;

    // Human-readable identifier for the format this codec claims to speak.
    // Never "ETSI TS 103 698" unless the format has actually been confirmed
    // and implemented against a real, cited authoritative source.
    virtual QString formatName() const = 0;

    // False until a real LMPE codec (backed by a confirmed format) exists.
    virtual bool isFormatConfirmed() const = 0;

    virtual LmpeCodecResult encode(const QByteArray &payload) const = 0;
    virtual LmpeCodecResult decode(const QByteArray &wireData) const = 0;
};
