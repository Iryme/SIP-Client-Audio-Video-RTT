#include <QCoreApplication>
#include <QTest>

// No PJSIP headers — EmergencyInviteBuilder must be testable without PJSIP.
#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyInviteBuilder.h"

class TestEmergencyInviteBuilder : public QObject
{
    Q_OBJECT
private slots:

    // 1. Default SOS invite is valid with correct fields
    void test_buildDefaultSosInvite()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"),
            QStringLiteral("Test User"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        QCOMPARE(inv.requestUri, QStringLiteral("sip:psap@ng112.example.com"));
        QCOMPARE(inv.routeTarget, QStringLiteral("sip:psap@ng112.example.com"));
        QCOMPARE(inv.serviceUrn, QStringLiteral("urn:service:sos"));
        QVERIFY(inv.mediaPolicy.requireAudio);
        QVERIFY(inv.mediaPolicy.requireRtt);
        QVERIFY(!inv.mediaPolicy.allowVideo);
        QVERIFY(!inv.hasLocation);
        QVERIFY(!inv.locationRequired);

        auto result = EmergencyInviteBuilder::validate(inv);
        QVERIFY(result.isValid());
        QVERIFY(result.errors.isEmpty());
    }

    // 2. Missing serviceUrn is a hard error
    void test_invalidWithoutServiceUrn()
    {
        EmergencyCallProfile profile;
        profile.serviceUrn    = QString();
        profile.routingTarget = QStringLiteral("sip:psap@ng112.example.com");

        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(!result.isValid());
        QVERIFY(result.errors.contains(QStringLiteral("serviceUrn must not be empty")));
    }

    // 3. Missing routingTarget (requestUri) is a hard error
    void test_invalidWithoutRoutingTarget()
    {
        EmergencyCallProfile profile;
        profile.serviceUrn    = QStringLiteral("urn:service:sos");
        profile.routingTarget = QString();

        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(!result.isValid());
        QVERIFY(result.errors.contains(
            QStringLiteral("requestUri (routing target) must not be empty")));
    }

    // 4. requireAudio=false is a hard error
    void test_invalidWithoutAudio()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyMediaPolicy policy;
        policy.requireAudio = false;
        policy.requireRtt   = true;

        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setMediaPolicy(policy)
                                  .build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(!result.isValid());
        QVERIFY(result.errors.contains(
            QStringLiteral("emergency call must require audio")));
    }

    // 5. requireRtt=false is a hard error
    void test_invalidWithoutRtt()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyMediaPolicy policy;
        policy.requireAudio = true;
        policy.requireRtt   = false;

        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setMediaPolicy(policy)
                                  .build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(!result.isValid());
        QVERIFY(result.errors.contains(
            QStringLiteral("emergency call must require RTT (RFC 4103)")));
    }

    // 6. locationRequired=true + hasLocation=false is a hard error
    void test_locationRequiredButUnavailable()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setLocationRequired(true)
                                  .setLocationAvailable(false)
                                  .build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(!result.isValid());
        QVERIFY(result.errors.contains(
            QStringLiteral("location is required but unavailable")));
    }

    // 7. locationRequired=false + hasLocation=false → warning, not error
    void test_locationOptionalUnavailableIsWarning()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setLocationRequired(false)
                                  .setLocationAvailable(false)
                                  .build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(result.isValid());
        QVERIFY(result.warnings.contains(
            QStringLiteral("location is unavailable and not required")));
    }

    // 8. allowVideo=false (default) → warning, not error
    void test_videoDisabledIsWarning()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(result.isValid());
        QVERIFY(result.warnings.contains(
            QStringLiteral("video is disabled for this emergency call")));
    }

    // 9. Same input always produces identical header list (deterministic)
    void test_headersDeterministic()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv1 = EmergencyInviteBuilder(profile).build();
        EmergencyInvite inv2 = EmergencyInviteBuilder(profile).build();

        QCOMPARE(inv1.headers.count(), inv2.headers.count());
        for (int i = 0; i < inv1.headers.count(); ++i) {
            QCOMPARE(inv1.headers.at(i).name,  inv2.headers.at(i).name);
            QCOMPARE(inv1.headers.at(i).value, inv2.headers.at(i).value);
        }
    }

    // 10. Geolocation header present when location is available
    void test_locationAvailableAddsGeolocationHeader()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setLocationAvailable(true)
                                  .build();

        bool found = false;
        for (const auto &h : inv.headers) {
            if (h.name == QStringLiteral("Geolocation"))
                found = true;
        }
        QVERIFY(found);
        QVERIFY(inv.hasLocation);
    }

    // 11. No Geolocation header when location unavailable
    void test_noLocationNoGeolocationHeader()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        for (const auto &h : inv.headers) {
            QVERIFY2(h.name != QStringLiteral("Geolocation"),
                     "Geolocation header must not appear when hasLocation=false");
        }
    }

    // 12. Accept and Supported headers always present
    void test_mandatoryHeadersAlwaysPresent()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        bool hasAccept    = false;
        bool hasSupported = false;
        for (const auto &h : inv.headers) {
            if (h.name == QStringLiteral("Accept"))    hasAccept    = true;
            if (h.name == QStringLiteral("Supported")) hasSupported = true;
        }
        QVERIFY(hasAccept);
        QVERIFY(hasSupported);
    }

    // 13. serviceUrn with wrong prefix is a hard error
    void test_invalidServiceUrnPrefix()
    {
        EmergencyCallProfile profile;
        profile.serviceUrn    = QStringLiteral("tel:112");  // wrong scheme
        profile.routingTarget = QStringLiteral("sip:psap@ng112.example.com");

        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(!result.isValid());
        QVERIFY(result.errors.contains(
            QStringLiteral("serviceUrn must start with 'urn:service:'")));
    }

    // 14. body and contentType are empty (placeholders) by default
    void test_bodyAndContentTypePlaceholders()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        QVERIFY(inv.body.isEmpty());
        QVERIFY(inv.contentType.isEmpty());
    }

    // 15. locationAvailable=true + required=true → valid, no location error
    void test_locationAvailableAndRequired()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setLocationAvailable(true)
                                  .setLocationRequired(true)
                                  .build();
        auto result = EmergencyInviteBuilder::validate(inv);

        QVERIFY(result.isValid());
        bool hasLocationError = false;
        for (const auto &e : result.errors) {
            if (e.contains(QStringLiteral("location")))
                hasLocationError = true;
        }
        QVERIFY(!hasLocationError);
    }
};

QTEST_GUILESS_MAIN(TestEmergencyInviteBuilder)
#include "test_emergency_invite_builder.moc"
