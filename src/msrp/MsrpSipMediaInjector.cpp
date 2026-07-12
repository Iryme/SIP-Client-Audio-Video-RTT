#include "MsrpSipMediaInjector.h"

#include <cstring>

#ifdef HAVE_PJSIP
#include <pjlib.h>
#include <pjmedia/sdp.h>
#endif

namespace MsrpSipMediaInjector {

#ifdef HAVE_PJSIP

namespace {

// Bug fix (found via live manual testing, 2026-07-12): pjmedia_sdp_validate
// (called by pjsip_inv_create_uac for every outgoing call, always in strict
// mode) requires every m= section to carry its own "c=" line whenever the
// session-level "c=" is absent — and in this app's actual pjsua-generated
// offers, the session-level "c=" is not reliably present. The original code
// left m->conn == nullptr, assuming a session-level fallback that isn't
// guaranteed to exist, which made pjmedia_sdp_validate return
// PJMEDIA_SDP_EMISSINGCONN — a hard PJ_ASSERT_RETURN in this Debug build,
// crashing the app on every outgoing call once MSRP is enabled. Always
// giving the injected section its own "c=" removes the dependency on that
// assumption entirely.
pjmedia_sdp_conn *buildConnInfo(pj_pool_t *pool, const MsrpUri &localUri)
{
    auto *conn = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
    conn->net_type = pj_str(const_cast<char *>("IN"));
    conn->addr_type = localUri.hostIsIpv6 ? pj_str(const_cast<char *>("IP6"))
                                           : pj_str(const_cast<char *>("IP4"));

    // 0.0.0.0 placeholder only when no real host is known yet (e.g. a
    // rejected port-0 section) — never left empty, since pjmedia_sdp_validate
    // requires a non-empty addr regardless of whether the section is active.
    const QString addr = localUri.host.isEmpty() ? QStringLiteral("0.0.0.0") : localUri.host;
    const QByteArray addrUtf8 = addr.toUtf8();
    char *addrCopy = static_cast<char *>(pj_pool_alloc(pool, addrUtf8.size() + 1));
    memcpy(addrCopy, addrUtf8.constData(), static_cast<size_t>(addrUtf8.size()) + 1);
    conn->addr = pj_str(addrCopy);

    return conn;
}

pjmedia_sdp_media *buildMessageMedia(pj_pool_t *pool, const MsrpUri &localUri, MsrpSetup setup,
                                      const QStringList &acceptTypes,
                                      const QStringList &acceptWrappedTypes,
                                      int port)
{
    auto *m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
    m->desc.media = pj_str(const_cast<char *>("message"));
    m->desc.port = static_cast<pj_uint16_t>(port);
    m->desc.port_count = 0;
    const bool useTls = localUri.transportProtocol() == MsrpTransportProtocol::Tls;
    m->desc.transport = useTls ? pj_str(const_cast<char *>("TCP/TLS/MSRP"))
                                : pj_str(const_cast<char *>("TCP/MSRP"));
    m->desc.fmt_count = 1;
    m->desc.fmt[0] = pj_str(const_cast<char *>("*"));
    // Never nullptr — see buildConnInfo's comment above.
    m->conn = buildConnInfo(pool, localUri);
    m->bandw_count = 0;
    m->attr_count = 0;

    auto addAttr = [&](const char *name, const QString &value) {
        if (m->attr_count >= PJMEDIA_MAX_SDP_ATTR)
            return;
        const QByteArray utf8 = value.toUtf8();
        pj_str_t pjValue = pj_str(const_cast<char *>(utf8.constData()));
        m->attr[m->attr_count++] = pjmedia_sdp_attr_create(pool, name, &pjValue);
    };

    // A rejected/disabled section (port 0) carries no negotiation
    // attributes — the m= line alone is enough to signal "not offered" per
    // RFC 3264, and keeps the offer valid instead of half-populated.
    if (port != 0) {
        addAttr("path", localUri.toString());
        addAttr("setup", msrpSetupToString(setup));
        if (!acceptTypes.isEmpty())
            addAttr("accept-types", acceptTypes.join(QLatin1Char(' ')));
        if (!acceptWrappedTypes.isEmpty())
            addAttr("accept-wrapped-types", acceptWrappedTypes.join(QLatin1Char(' ')));
    }

    return m;
}

} // namespace

InjectResult injectMessageMedia(void *pjSdpSessionPtr, void *pjPoolPtr,
                                 const MsrpUri &localUri, MsrpSetup setup,
                                 const QStringList &acceptTypes,
                                 const QStringList &acceptWrappedTypes,
                                 int port)
{
    InjectResult result;

    auto *sdp = static_cast<pjmedia_sdp_session *>(pjSdpSessionPtr);
    auto *pool = static_cast<pj_pool_t *>(pjPoolPtr);
    if (!sdp || !pool) {
        result.errorMessage = QStringLiteral("null SDP session or pool");
        return result;
    }
    if (sdp->media_count >= PJMEDIA_MAX_SDP_MEDIA) {
        result.errorMessage = QStringLiteral("SDP media section limit reached");
        return result;
    }

    sdp->media[sdp->media_count++] =
        buildMessageMedia(pool, localUri, setup, acceptTypes, acceptWrappedTypes, port);
    result.injected = true;
    return result;
}

InjectResult answerMessageMediaAtIndex(void *pjSdpSessionPtr, void *pjPoolPtr, int index,
                                        const MsrpUri &localUri, MsrpSetup setup,
                                        const QStringList &acceptTypes,
                                        const QStringList &acceptWrappedTypes,
                                        int port)
{
    InjectResult result;

    auto *sdp = static_cast<pjmedia_sdp_session *>(pjSdpSessionPtr);
    auto *pool = static_cast<pj_pool_t *>(pjPoolPtr);
    if (!sdp || !pool) {
        result.errorMessage = QStringLiteral("null SDP session or pool");
        return result;
    }
    if (index < 0 || static_cast<unsigned>(index) >= sdp->media_count) {
        result.errorMessage = QStringLiteral("answer media index out of range");
        return result;
    }
    const pjmedia_sdp_media *existing = sdp->media[index];
    if (!existing || pj_stricmp2(&existing->desc.media, "message") != 0) {
        result.errorMessage = QStringLiteral("media at index is not an m=message section");
        return result;
    }

    sdp->media[index] =
        buildMessageMedia(pool, localUri, setup, acceptTypes, acceptWrappedTypes, port);
    result.injected = true;
    return result;
}

QString resolveSessionConnectionHost(void *pjSdpSessionPtr)
{
    const auto *sdp = static_cast<const pjmedia_sdp_session *>(pjSdpSessionPtr);
    if (!sdp)
        return QString();

    const pjmedia_sdp_conn *conn = sdp->conn;
    if (!conn) {
        for (unsigned i = 0; i < sdp->media_count && !conn; ++i) {
            if (sdp->media[i])
                conn = sdp->media[i]->conn;
        }
    }
    if (!conn || conn->addr.slen == 0)
        return QString();
    return QString::fromLatin1(conn->addr.ptr, static_cast<int>(conn->addr.slen));
}

#else // !HAVE_PJSIP

InjectResult injectMessageMedia(void *, void *, const MsrpUri &, MsrpSetup,
                                 const QStringList &, const QStringList &, int)
{
    InjectResult result;
    result.errorMessage = QStringLiteral("PJSIP not enabled in this build");
    return result;
}

InjectResult answerMessageMediaAtIndex(void *, void *, int, const MsrpUri &, MsrpSetup,
                                        const QStringList &, const QStringList &, int)
{
    InjectResult result;
    result.errorMessage = QStringLiteral("PJSIP not enabled in this build");
    return result;
}

QString resolveSessionConnectionHost(void *)
{
    return QString();
}

#endif

} // namespace MsrpSipMediaInjector
