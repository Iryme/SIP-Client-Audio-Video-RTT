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

// RTT / T.140 — listed here for the codec model only.
// PJSIP handles T.140 and RED natively via the text media stream
// (m=text in SDP); they are NOT enumerated by codecEnum2() / videoCodecEnum2().
// Redundancy level is configured via AccountTextConfig.redundancyLevel (see
// SipAccount.cpp and src/rtt/RttConfig.h).
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
    // T.140 and RED are not audio/video codecs and are not enumerated by
    // PJSIP's codecEnum2()/videoCodecEnum2(). Returned here as a model so
    // callers can display codec information consistently.
    // Actual SDP negotiation uses pj::AccountConfig.textConfig.redundancyLevel
    // (see SipAccount.cpp + src/rtt/RttConfig.h).
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

void CodecManager::applyVideoCodecOrder(const QStringList &order)
{
#if defined(HAVE_PJSIP) && defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
    try {
        pj::CodecInfoVector2 pjVideo = pj::Endpoint::instance().videoCodecEnum2();
        for (const auto &c : pjVideo) {
            const QString id = QString::fromStdString(c.codecId);
            int rank = -1;
            for (int i = 0; i < order.size(); ++i) {
                if (id.startsWith(order[i], Qt::CaseInsensitive)) { rank = i; break; }
            }
            const int pri = (rank >= 0) ? qMax(1, 240 - rank * 10) : 1;
            try {
                pj::Endpoint::instance().videoCodecSetPriority(
                    c.codecId, static_cast<pj_uint8_t>(pri));
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("VideoCodec priority: %1 → %2 (rank %3)")
                        .arg(id).arg(pri).arg(rank));
            } catch (const pj::Error &e) {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("Video setting not applied to PJSIP: priority set failed for %1: %2")
                        .arg(id, QString::fromStdString(e.reason)));
            }
        }
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("Video setting not applied to PJSIP: codec order enum failed: %1")
                .arg(QString::fromStdString(e.reason)));
    }
#else
    Q_UNUSED(order)
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Video setting not applied to PJSIP: PJMEDIA_HAS_VIDEO=0 or HAVE_PJSIP not defined"));
#endif
}

void CodecManager::applyVideoCodecBitrate(const QString &preferredCodec, int bitrateKbps)
{
#if defined(HAVE_PJSIP) && defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
    try {
        pj::CodecInfoVector2 pjVideo = pj::Endpoint::instance().videoCodecEnum2();
        for (const auto &c : pjVideo) {
            const QString id = QString::fromStdString(c.codecId);
            if (!id.startsWith(preferredCodec, Qt::CaseInsensitive))
                continue;

            pjmedia_vid_codec_param param;
            pj_str_t pjId = pj_str(const_cast<char *>(c.codecId.c_str()));
            pj_status_t st = pjsua_vid_codec_get_param(&pjId, &param);
            if (st == PJ_SUCCESS) {
                param.enc_fmt.det.vid.avg_bps = static_cast<pj_uint32_t>(bitrateKbps) * 1000u;
                param.enc_fmt.det.vid.max_bps = static_cast<pj_uint32_t>(bitrateKbps) * 2000u;
                st = pjsua_vid_codec_set_param(&pjId, &param);
                if (st == PJ_SUCCESS) {
                    Logger::instance().info(LogCategory::Media,
                        QStringLiteral("VideoCodec bitrate: %1 → %2 kbps "
                                       "(avg_bps=%3 max_bps=%4)")
                            .arg(id).arg(bitrateKbps)
                            .arg(param.enc_fmt.det.vid.avg_bps)
                            .arg(param.enc_fmt.det.vid.max_bps));
                } else {
                    Logger::instance().warn(LogCategory::Media,
                        QStringLiteral("Video setting not applied to PJSIP: "
                                       "bitrate set_param failed for %1 (status %2)")
                            .arg(id).arg(static_cast<int>(st)));
                }
            } else {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("Video setting not applied to PJSIP: "
                                   "bitrate get_param failed for %1 (status %2)")
                        .arg(id).arg(static_cast<int>(st)));
            }
            break;
        }
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("Video setting not applied to PJSIP: bitrate exception: %1")
                .arg(QString::fromStdString(e.reason)));
    }
#else
    Q_UNUSED(preferredCodec)
    Q_UNUSED(bitrateKbps)
#endif
}

void CodecManager::applyVideoCodecFormat(const QStringList &order,
                                         const QSize &resolution, int fps)
{
#if defined(HAVE_PJSIP) && defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
    try {
        pj::CodecInfoVector2 pjVideo = pj::Endpoint::instance().videoCodecEnum2();
        bool applied = false;
        for (const QString &preferred : order) {
            if (applied) break;
            for (const auto &c : pjVideo) {
                const QString id = QString::fromStdString(c.codecId);
                if (!id.startsWith(preferred, Qt::CaseInsensitive))
                    continue;

                pjmedia_vid_codec_param param;
                pj_str_t pjId = pj_str(const_cast<char *>(c.codecId.c_str()));
                pj_status_t st = pjsua_vid_codec_get_param(&pjId, &param);
                if (st == PJ_SUCCESS) {
                    param.enc_fmt.det.vid.size.w = static_cast<unsigned>(resolution.width());
                    param.enc_fmt.det.vid.size.h = static_cast<unsigned>(resolution.height());
                    param.enc_fmt.det.vid.fps.num   = fps;
                    param.enc_fmt.det.vid.fps.denum = 1;
                    st = pjsua_vid_codec_set_param(&pjId, &param);
                    if (st == PJ_SUCCESS) {
                        Logger::instance().info(LogCategory::Media,
                            QStringLiteral("VideoCodec format: %1 → %2x%3 @ %4 fps")
                                .arg(id)
                                .arg(resolution.width()).arg(resolution.height())
                                .arg(fps));
                        applied = true;
                    } else {
                        Logger::instance().warn(LogCategory::Media,
                            QStringLiteral("Video setting not applied to PJSIP: "
                                           "format set_param failed for %1 (status %2)")
                                .arg(id).arg(static_cast<int>(st)));
                    }
                } else {
                    Logger::instance().warn(LogCategory::Media,
                        QStringLiteral("Video setting not applied to PJSIP: "
                                       "format get_param failed for %1 (status %2)")
                            .arg(id).arg(static_cast<int>(st)));
                }
                break;
            }
        }
        if (!applied && !order.isEmpty()) {
            Logger::instance().warn(LogCategory::Media,
                QStringLiteral("Video setting not applied to PJSIP: "
                               "no matching codec for format (order: [%1])")
                    .arg(order.join(QStringLiteral(", "))));
        }
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("Video setting not applied to PJSIP: format exception: %1")
                .arg(QString::fromStdString(e.reason)));
    }
#else
    Q_UNUSED(order)
    Q_UNUSED(resolution)
    Q_UNUSED(fps)
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Video setting not applied to PJSIP: "
                       "PJMEDIA_HAS_VIDEO=0 or HAVE_PJSIP not defined"));
#endif
}

void CodecManager::logCodecMatrix() const
{
    // --- Audio ---
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("CodecManager: audio codecs (%1 available):")
            .arg(m_audioCodecs.size()));
    for (const auto &e : m_audioCodecs) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("  %1  %2 priority=%3")
                .arg(e.priority > 0 ? QStringLiteral("ENABLED ") : QStringLiteral("DISABLED"))
                .arg(e.codecId, -32)
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
            QStringLiteral("  %1  %2 priority=%3")
                .arg(e.priority > 0 ? QStringLiteral("ENABLED ") : QStringLiteral("DISABLED"))
                .arg(e.codecId, -32)
                .arg(e.priority));
    }
    if (m_videoCodecs.isEmpty()) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("CodecManager: no video codecs available "
                           "(VPX/OpenH264/FFmpeg not compiled) — "
                           "INVITE will NOT include a video m-line"));
    }

    // --- RTT/Text (model — actual negotiation is via pj::AccountTextConfig) ---
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("CodecManager: RTT/text codecs (model; PJSIP negotiates "
                       "red/t140 via AccountTextConfig, not codecEnum2):"));
    for (const auto &e : rttCodecs()) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("  MODEL  %1 priority=%2")
                .arg(e.codecId, -32)
                .arg(e.priority));
    }
}
