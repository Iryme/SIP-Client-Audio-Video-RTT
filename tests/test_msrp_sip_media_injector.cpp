// Task W101 Phase 2 — proves MsrpSipMediaInjector actually writes
// "m=message" bytes into a real pjmedia_sdp_session, the exact struct
// pjsua2 hands to the transport layer as the outgoing SDP. This is not a
// test of the internal MsrpSessionInfo model — it builds a real
// pjmedia_sdp_session (parsed the same way PJSIP itself would), injects,
// then re-serializes with pjmedia_sdp_print() and inspects the raw text,
// so a pass here is direct evidence the SDP bytes on the wire would
// contain m=message, not just an internal Qt model believing they would.
#include <QtTest>

#include <cstring>

#include <pjlib.h>
#include <pjmedia/sdp.h>

#include "msrp/MsrpPath.h"
#include "msrp/MsrpSipMediaInjector.h"

namespace {

// Existing audio+video SDP an INVITE would already carry — the injector
// must never disturb this, only append after it.
const char *kBaseSdp =
    "v=0\r\n"
    "o=- 123456 654321 IN IP4 192.0.2.10\r\n"
    "s=-\r\n"
    "c=IN IP4 192.0.2.10\r\n"
    "t=0 0\r\n"
    "m=audio 49170 RTP/AVP 0\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "m=video 51372 RTP/AVP 96\r\n"
    "a=rtpmap:96 H264/90000\r\n";

} // namespace

class TestMsrpSipMediaInjector : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(pj_init() == PJ_SUCCESS);
        pj_caching_pool_init(&m_cp, nullptr, 0);
        m_pool = pj_pool_create(&m_cp.factory, "test-base", 4096, 4096, nullptr);
        QVERIFY(m_pool != nullptr);
        m_attrPool = pj_pool_create(&m_cp.factory, "test-attrs", 4096, 4096, nullptr);
        QVERIFY(m_attrPool != nullptr);
    }

    void cleanupTestCase()
    {
        if (m_attrPool) pj_pool_release(m_attrPool);
        if (m_pool) pj_pool_release(m_pool);
        pj_caching_pool_destroy(&m_cp);
    }

    // The headline requirement: after injection, printing the SDP back to
    // text produces a literal "m=message" line — real transmitted bytes,
    // not an internal model field.
    void injectsRealMessageLineOntoWire()
    {
        pjmedia_sdp_session *sdp = parseBase();
        QVERIFY(sdp != nullptr);
        QCOMPARE(static_cast<int>(sdp->media_count), 2); // audio, video only so far

        const MsrpUri uri = MsrpPath::buildUri(false, QStringLiteral("192.0.2.10"), 2855,
                                                QStringLiteral("abc123sessionid"));
        const auto result = MsrpSipMediaInjector::injectMessageMedia(
            sdp, m_attrPool, uri, MsrpSetup::ActPass,
            {QStringLiteral("text/plain"), QStringLiteral("message/cpim")},
            {QStringLiteral("text/plain")}, 2855);

        QVERIFY(result.injected);
        QCOMPARE(static_cast<int>(sdp->media_count), 3);

        char buf[4096];
        const int printed = pjmedia_sdp_print(sdp, buf, sizeof(buf));
        QVERIFY(printed > 0);
        const QString wire = QString::fromLatin1(buf, printed);

        // Audio/video sections must survive untouched.
        QVERIFY(wire.contains(QStringLiteral("m=audio 49170 RTP/AVP 0")));
        QVERIFY(wire.contains(QStringLiteral("m=video 51372 RTP/AVP 96")));

        // The actual "on the wire" MSRP media line and attributes.
        QVERIFY(wire.contains(QStringLiteral("m=message 2855 TCP/MSRP *")));
        QVERIFY(wire.contains(QStringLiteral("a=path:msrp://192.0.2.10:2855/abc123sessionid;tcp")));
        QVERIFY(wire.contains(QStringLiteral("a=setup:actpass")));
        QVERIFY(wire.contains(QStringLiteral("a=accept-types:text/plain message/cpim")));
        QVERIFY(wire.contains(QStringLiteral("a=accept-wrapped-types:text/plain")));
    }

    void injectsTlsSchemeAndProto()
    {
        pjmedia_sdp_session *sdp = parseBase();
        const MsrpUri uri = MsrpPath::buildUri(true, QStringLiteral("192.0.2.10"), 2856,
                                                QStringLiteral("tlssession"));
        const auto result = MsrpSipMediaInjector::injectMessageMedia(
            sdp, m_attrPool, uri, MsrpSetup::Passive,
            {QStringLiteral("text/plain")}, {}, 2856);
        QVERIFY(result.injected);

        char buf[4096];
        const int printed = pjmedia_sdp_print(sdp, buf, sizeof(buf));
        const QString wire = QString::fromLatin1(buf, printed);
        QVERIFY(wire.contains(QStringLiteral("m=message 2856 TCP/TLS/MSRP *")));
        QVERIFY(wire.contains(QStringLiteral("a=path:msrps://192.0.2.10:2856/tlssession;tcp")));
        QVERIFY(wire.contains(QStringLiteral("a=setup:passive")));
        // No acceptWrappedTypes were supplied — must not print an empty attribute.
        QVERIFY(!wire.contains(QStringLiteral("a=accept-wrapped-types:")));
    }

    // Port 0 (listener failed / MSRP unavailable) must still produce a
    // valid, well-formed rejected section — never an invalid offer, and
    // never leak negotiation attributes for a session that doesn't exist.
    void port0RejectionOmitsNegotiationAttributes()
    {
        pjmedia_sdp_session *sdp = parseBase();
        const MsrpUri uri = MsrpPath::buildUri(false, QStringLiteral("192.0.2.10"), 0,
                                                QStringLiteral("unused"));
        const auto result = MsrpSipMediaInjector::injectMessageMedia(
            sdp, m_attrPool, uri, MsrpSetup::ActPass,
            {QStringLiteral("text/plain")}, {}, 0);
        QVERIFY(result.injected);

        char buf[4096];
        const int printed = pjmedia_sdp_print(sdp, buf, sizeof(buf));
        const QString wire = QString::fromLatin1(buf, printed);
        QVERIFY(wire.contains(QStringLiteral("m=message 0 TCP/MSRP *")));
        QVERIFY(!wire.contains(QStringLiteral("a=path:")));
        QVERIFY(!wire.contains(QStringLiteral("a=setup:")));
        QVERIFY(!wire.contains(QStringLiteral("a=accept-types:")));
    }

    void resolvesConnectionHostFromSessionLevelLine()
    {
        pjmedia_sdp_session *sdp = parseBase();
        const QString host = MsrpSipMediaInjector::resolveSessionConnectionHost(sdp);
        QCOMPARE(host, QStringLiteral("192.0.2.10"));
    }

    void nullSessionOrPoolIsRejected()
    {
        const MsrpUri uri = MsrpPath::buildUri(false, QStringLiteral("192.0.2.10"), 2855,
                                                QStringLiteral("x"));
        const auto r1 = MsrpSipMediaInjector::injectMessageMedia(
            nullptr, m_attrPool, uri, MsrpSetup::ActPass, {}, {}, 2855);
        QVERIFY(!r1.injected);
        QVERIFY(!r1.errorMessage.isEmpty());

        pjmedia_sdp_session *sdp = parseBase();
        const auto r2 = MsrpSipMediaInjector::injectMessageMedia(
            sdp, nullptr, uri, MsrpSetup::ActPass, {}, {}, 2855);
        QVERIFY(!r2.injected);
    }

private:
    pjmedia_sdp_session *parseBase()
    {
        // pjmedia_sdp_parse mutates the buffer in-place; give it a fresh
        // writable copy each time so tests don't interfere with each other.
        static char buf[2048];
        std::strncpy(buf, kBaseSdp, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        pjmedia_sdp_session *sdp = nullptr;
        const pj_status_t st = pjmedia_sdp_parse(m_pool, buf, std::strlen(buf), &sdp);
        if (st != PJ_SUCCESS)
            return nullptr;
        return sdp;
    }

    pj_caching_pool m_cp;
    pj_pool_t *m_pool{nullptr};
    pj_pool_t *m_attrPool{nullptr};
};

QTEST_APPLESS_MAIN(TestMsrpSipMediaInjector)
#include "test_msrp_sip_media_injector.moc"
