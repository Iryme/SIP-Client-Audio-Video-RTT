#include "CodecManager.h"

#include "core/Logger.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <pjsua-lib/pjsua.h>
#endif

// ---------------------------------------------------------------------------
// Default priority tables
// Priority 0 → disable; 1–255 → enabled (higher = preferred).
// PJSIP audio/video codec IDs are matched by prefix, case-insensitive.
// ---------------------------------------------------------------------------

namespace {

struct PriorityEntry {
    const char *idPrefix;
    int         priority;
};

constexpr PriorityEntry kAudioDefaults[] = {
    { "opus",             240 },
    { "G722",             230 },
    { "PCMA",             220 },
    { "PCMU",             210 },
    { "GSM",              200 },
    { "telephone-event",  195 },
    { nullptr,              0 }
};

constexpr PriorityEntry kVideoDefaults[] = {
    { "H264",  240 },
    { "VP8",   230 },
    { "VP9",   220 },
    { "H263",  210 },
    { nullptr,   0 }
};

// RTT / T.140 — model only; PJSIP has no native T.140 codec support.
// Listed here so callers can reason about the codec model without PJSIP.
constexpr PriorityEntry kRttDefaults[] = {
    { "red/90000/1",  240 },
    { "t140/1000/1",  230 },
    { nullptr,          0 }
};

int lookupPriority(const QString &id, const PriorityEntry *table)
{
    for (int i = 0; table[i].idPrefix != nullptr; ++i) {
        if (id.startsWith(QLatin1String(table[i].idPrefix), Qt::CaseInsensitive))
            return table[i].priority;
    }
    return -1; // not in table — leave PJSIP default
}

} // namespace

// ---------------------------------------------------------------------------
// CodecManager
// ---------------------------------------------------------------------------

CodecManager &CodecManager::instance()
{
    static CodecManager s_instance;
    return s_instance;
}

void CodecManager::initialize()
{
    m_audioCodecs.clear();
    m_videoCodecs.clear();

#ifdef HAVE_PJSIP
    // --- Audio codecs (codecEnum2 = pjsua2 audio codec list) ---
    try {
        pj::CodecInfoVector2 pjAudio = pj::Endpoint::instance().codecEnum2();
        for (const auto &c : pjAudio) {
            const QString id = QString::fromStdString(c.codecId);
            const int desired = lookupPriority(id, kAudioDefaults);
            int finalPriority = static_cast<int>(c.priority);

            if (desired >= 0) {
                try {
                    pj::Endpoint::instance().codecSetPriority(
                        c.codecId, static_cast<pj_uint8_t>(desired));
                    finalPriority = desired;
                } catch (...) {}
            }

            m_audioCodecs.append({ id, SipMediaType::Audio, finalPriority, true });
        }
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("CodecManager: audio codec enumeration failed: %1")
                .arg(QString::fromStdString(e.reason)));
    }

    // --- Video codecs ---
#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
    try {
        pj::CodecInfoVector2 pjVideo = pj::Endpoint::instance().videoCodecEnum2();
        for (const auto &c : pjVideo) {
            const QString id = QString::fromStdString(c.codecId);
            const int desired = lookupPriority(id, kVideoDefaults);
            int finalPriority = static_cast<int>(c.priority);

            if (desired > 0) {
                try {
                    pj::Endpoint::instance().videoCodecSetPriority(
                        c.codecId, static_cast<pj_uint8_t>(desired));
                    finalPriority = desired;
                } catch (...) {}
            }

            m_videoCodecs.append({ id, SipMediaType::Video, finalPriority, true });
        }
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("CodecManager: video codec enumeration failed: %1")
                .arg(QString::fromStdString(e.reason)));
    }
#endif

#else
    // Stub mode: known audio codecs with their expected default priorities.
    // available=false signals these are not backed by a real PJSIP instance.
    m_audioCodecs = {
        { QStringLiteral("PCMU/8000/1"),  SipMediaType::Audio, 210, false },
        { QStringLiteral("PCMA/8000/1"),  SipMediaType::Audio, 220, false },
        { QStringLiteral("G722/16000/1"), SipMediaType::Audio, 230, false },
    };
#endif

    logCodecMatrix();
}

QList<CodecEntry> CodecManager::audioCodecs() const { return m_audioCodecs; }
QList<CodecEntry> CodecManager::videoCodecs() const { return m_videoCodecs; }

QList<CodecEntry> CodecManager::rttCodecs() const
{
    // T.140 and RED are not PJSIP codecs — model only.
    // Presence here does not imply any PJSIP-level support.
    return {
        { QStringLiteral("red/90000/1"),  SipMediaType::Text, 240, false },
        { QStringLiteral("t140/1000/1"),  SipMediaType::Text, 230, false },
    };
}

bool CodecManager::hasUsableAudioCodec() const
{
    for (const auto &e : m_audioCodecs) {
        if (e.available && e.priority > 0)
            return true;
    }
    return false;
}

bool CodecManager::hasUsableVideoCodec() const
{
    for (const auto &e : m_videoCodecs) {
        if (e.available && e.priority > 0)
            return true;
    }
    return false;
}

void CodecManager::logCodecMatrix() const
{
    // --- Audio ---
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("CodecManager: audio codecs (%1 available):")
            .arg(m_audioCodecs.size()));
    for (const auto &e : m_audioCodecs) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("  %1  %-32s priority=%2")
                .arg(e.priority > 0 ? QStringLiteral("ENABLED ") : QStringLiteral("DISABLED"))
                .arg(e.codecId)
                .arg(e.priority));
    }
    if (m_audioCodecs.isEmpty())
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("CodecManager: no audio codecs detected — calls will fail"));
    else if (!hasUsableAudioCodec())
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("CodecManager: no enabled audio codecs — calls will fail"));

    // --- Video ---
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("CodecManager: video codecs (%1 available):")
            .arg(m_videoCodecs.size()));
    for (const auto &e : m_videoCodecs) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("  %1  %-32s priority=%2")
                .arg(e.priority > 0 ? QStringLiteral("ENABLED ") : QStringLiteral("DISABLED"))
                .arg(e.codecId)
                .arg(e.priority));
    }
    if (m_videoCodecs.isEmpty()) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("CodecManager: no video codecs available "
                           "(VPX/OpenH264/FFmpeg not compiled) — "
                           "INVITE will NOT include a video m-line"));
    }

    // --- RTT/Text (model only) ---
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("CodecManager: RTT/text codecs (model only — "
                       "not negotiated via PJSIP in this build):"));
    for (const auto &e : rttCodecs()) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("  MODEL  %-32s priority=%2")
                .arg(e.codecId)
                .arg(e.priority));
    }
}
