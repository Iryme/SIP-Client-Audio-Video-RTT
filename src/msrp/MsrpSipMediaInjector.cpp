#include "MsrpSipMediaInjector.h"

#ifdef HAVE_PJSIP
#include <pjlib.h>
#include <pjmedia/sdp.h>
#endif

namespace MsrpSipMediaInjector {

#ifdef HAVE_PJSIP

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

    auto *m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
    m->desc.media = pj_str(const_cast<char *>("message"));
    m->desc.port = static_cast<pj_uint16_t>(port);
    m->desc.port_count = 0;
    const bool useTls = localUri.transportProtocol() == MsrpTransportProtocol::Tls;
    m->desc.transport = useTls ? pj_str(const_cast<char *>("TCP/TLS/MSRP"))
                                : pj_str(const_cast<char *>("TCP/MSRP"));
    m->desc.fmt_count = 1;
    m->desc.fmt[0] = pj_str(const_cast<char *>("*"));
    m->conn = nullptr;
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

    sdp->media[sdp->media_count++] = m;
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

QString resolveSessionConnectionHost(void *)
{
    return QString();
}

#endif

} // namespace MsrpSipMediaInjector
