#include <QCoreApplication>
#include <QTest>

// No PJSIP headers — all tested classes are pure Qt/C++.
#include "emergency/EmergencyCallAdapter.h"
#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyInviteBuilder.h"
#include "emergency/EmergencyLocation.h"
#include "emergency/PidfLoBuilder.h"
#include "sip/SipCallOptions.h"

class TestEmergencySipIntegration : public QObject
{
    Q_OBJECT
private slots:

    // -----------------------------------------------------------------------
    // SipCallOptions
    // -----------------------------------------------------------------------

    // 1. Default-constructed options are empty (normal call)
    void test_defaultOptionsIsEmpty()
    {
        SipCallOptions opts;
        QVERIFY(opts.isEmpty());
    }

    // 2. Setting emergencyCall=true makes it non-empty
    void test_emergencyFlagMakesNonEmpty()
    {
        SipCallOptions opts;
        opts.emergencyCall = true;
        QVERIFY(!opts.isEmpty());
    }

    // 3. Adding a custom header makes it non-empty
    void test_customHeaderMakesNonEmpty()
    {
        SipCallOptions opts;
        opts.customHeaders.append({QStringLiteral("Geolocation"),
                                   QStringLiteral("<cid:pidflo@ng112.local>")});
        QVERIFY(!opts.isEmpty());
    }

    // 4. SipCallOptions::normal() is empty
    void test_normalOptionsIsEmpty()
    {
        QVERIFY(SipCallOptions::normal().isEmpty());
    }

    // 5. Default options have audio/rtt/video enabled (mirrors makeCall behavior)
    void test_defaultOptionsMediaDefaults()
    {
        SipCallOptions opts;
        QVERIFY(opts.requireAudio);
        QVERIFY(opts.requireRtt);
        QVERIFY(opts.allowVideo);
    }

    // -----------------------------------------------------------------------
    // EmergencyCallAdapter — generateContentId
    // -----------------------------------------------------------------------

    // 6. generateContentId returns non-empty string
    void test_generateContentIdNonEmpty()
    {
        QVERIFY(!EmergencyCallAdapter::generateContentId().isEmpty());
    }

    // 7. generateContentId contains '@'
    void test_generateContentIdContainsAt()
    {
        QVERIFY(EmergencyCallAdapter::generateContentId().contains(QLatin1Char('@')));
    }

    // 8. generateContentId contains "pidflo-"
    void test_generateContentIdContainsPidfloPrefix()
    {
        QVERIFY(EmergencyCallAdapter::generateContentId()
                    .startsWith(QStringLiteral("pidflo-")));
    }

    // 9. generateContentId ends with "@ng112.local"
    void test_generateContentIdEndsWithDomain()
    {
        QVERIFY(EmergencyCallAdapter::generateContentId()
                    .endsWith(QStringLiteral("@ng112.local")));
    }

    // -----------------------------------------------------------------------
    // EmergencyCallAdapter — toSipCallOptions
    // -----------------------------------------------------------------------

    // 10. Converted invite sets emergencyCall=true
    void test_convertedOptionsEmergencyFlag()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QVERIFY(opts.emergencyCall);
    }

    // 11. Geolocation header from invite is present in SipCallOptions
    void test_geolocationHeaderInOptions()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        profile.pidfLo = QStringLiteral("<presence/>");
        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setContentId(QStringLiteral("pidflo-test@ng112"))
                                  .build();

        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);

        bool found = false;
        for (const auto &h : opts.customHeaders) {
            if (h.first == QStringLiteral("Geolocation"))
                found = true;
        }
        QVERIFY2(found, "Geolocation header must be present in SipCallOptions");
    }

    // 12. Supported header from invite is present in SipCallOptions
    void test_supportedHeaderInOptions()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);

        bool found = false;
        for (const auto &h : opts.customHeaders) {
            if (h.first == QStringLiteral("Supported"))
                found = true;
        }
        QVERIFY2(found, "Supported header must be present in SipCallOptions");
    }

    // 13. Accept header from invite is present in SipCallOptions
    void test_acceptHeaderInOptions()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);

        bool found = false;
        for (const auto &h : opts.customHeaders) {
            if (h.first == QStringLiteral("Accept"))
                found = true;
        }
        QVERIFY2(found, "Accept header must be present in SipCallOptions");
    }

    // 14. mediaPolicy requireAudio is carried through
    void test_mediaPolicyRequireAudio()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyMediaPolicy policy;
        policy.requireAudio = true;
        policy.requireRtt   = true;
        policy.allowVideo   = false;
        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setMediaPolicy(policy)
                                  .build();

        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QVERIFY(opts.requireAudio);
        QVERIFY(opts.requireRtt);
        QVERIFY(!opts.allowVideo);
    }

    // 15. PIDF-LO body from invite is preserved in SipCallOptions
    void test_pidfLoBodyInOptions()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        profile.pidfLo = QStringLiteral("<presence>body</presence>");
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QCOMPARE(opts.body, QStringLiteral("<presence>body</presence>"));
        QCOMPARE(opts.contentType, QStringLiteral("application/pidf+xml"));
    }

    // 16. contentId from invite is carried through
    void test_contentIdInOptions()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        profile.pidfLo = QStringLiteral("<presence/>");
        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setContentId(QStringLiteral("pidflo-e2e@ng112"))
                                  .build();

        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QCOMPARE(opts.contentId, QStringLiteral("pidflo-e2e@ng112"));
    }

    // 17. Options from emergency invite are not isEmpty()
    void test_emergencyOptionsNotEmpty()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();
        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QVERIFY(!opts.isEmpty());
    }

    // -----------------------------------------------------------------------
    // End-to-end: EmergencyLocation → PIDF-LO → EmergencyInvite → SipCallOptions
    // -----------------------------------------------------------------------

    // 18. Full chain produces correct SipCallOptions with cid: Geolocation value
    void test_endToEndChain()
    {
        auto loc = EmergencyLocation::makeStatic(47.6062, -122.3321,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult pidf = PidfLoBuilder(loc)
                                .setContentId(QStringLiteral("pidflo-e2e@ng112.local"))
                                .build();
        QVERIFY(pidf.success);

        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        profile.pidfLo = pidf.xml;

        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setContentId(pidf.contentId)
                                  .build();
        QVERIFY(inv.hasLocation);

        SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QVERIFY(opts.emergencyCall);
        QVERIFY(!opts.body.isEmpty());
        QCOMPARE(opts.contentType, QStringLiteral("application/pidf+xml"));
        QCOMPARE(opts.contentId, QStringLiteral("pidflo-e2e@ng112.local"));

        QString geoValue;
        for (const auto &h : opts.customHeaders) {
            if (h.first == QStringLiteral("Geolocation"))
                geoValue = h.second;
        }
        QVERIFY2(!geoValue.isEmpty(), "Geolocation header must be present");
        QVERIFY2(geoValue.contains(QStringLiteral("cid:")),
                 "Geolocation value must use cid: scheme");
        QVERIFY2(geoValue.contains(QStringLiteral("pidflo-e2e@ng112.local")),
                 "Geolocation value must contain contentId");
    }
};

QTEST_GUILESS_MAIN(TestEmergencySipIntegration)
#include "test_emergency_sip_integration.moc"
