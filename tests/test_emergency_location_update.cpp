// Task 43 — Manual PIDF-LO Generator + SIP Location Update tests
//
// Tests the manual location entry → PIDF-LO → location update chain without
// requiring a real SIP stack or Qt GUI widgets (QTEST_GUILESS_MAIN).
//
// Coverage:
//   1. Valid manual location → PidfLoBuilder produces non-empty PIDF-LO.
//   2. Latitude out of range → EmergencyLocation.isValid() = false.
//   3. Longitude out of range → EmergencyLocation.isValid() = false.
//   4. Emergency call uses manual location from StaticLocationProvider.
//   5. locationRequired=true, no pidfLo → EmergencyInviteBuilder validate() fails.
//   6. toLocationUpdateOptions() sets emergencyCall=true; default SipCallOptions has false.
//   7. toLocationUpdateOptions() includes Geolocation + Geolocation-Routing + Supported.
//   8. generateContentId() returns a different value for each sequential call.

#include <QtTest/QtTest>

#include "emergency/EmergencyCallAdapter.h"
#include "emergency/EmergencyCallController.h"
#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyCallStateMachine.h"
#include "emergency/EmergencyInviteBuilder.h"
#include "emergency/EmergencyLocation.h"
#include "emergency/EmergencyLocationProvider.h"
#include "emergency/PidfLoBuilder.h"
#include "emergency/StaticLocationProvider.h"
#include "sip/SipCallOptions.h"

class TestEmergencyLocationUpdate : public QObject
{
    Q_OBJECT

private slots:
    // 1. Valid manual location (lat=44.4268, lon=26.1025, unc=50) → valid PIDF-LO.
    void test_validManualLocationProducesPidfLo()
    {
        QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!ts.endsWith('Z')) ts += 'Z';

        EmergencyLocation loc = EmergencyLocation::makeStatic(44.4268, 26.1025, ts);
        loc.uncertaintyMeters = 50.0;
        loc.source = LocationSource::Manual;

        QVERIFY(loc.isValid());

        const PidfLoResult r = PidfLoBuilder(loc)
            .setContentId(QStringLiteral("pidflo-test@ng112.local"))
            .build();

        QVERIFY2(r.success, qPrintable(r.error));
        QVERIFY2(!r.xml.isEmpty(), "PIDF-LO XML must not be empty");
        QVERIFY2(r.xml.contains(QStringLiteral("44.4268")),
                 "PIDF-LO must contain entered latitude");
        QVERIFY2(r.xml.contains(QStringLiteral("26.1025")),
                 "PIDF-LO must contain entered longitude");
    }

    // 2. Latitude out of range → EmergencyLocation.isValid() = false.
    void test_invalidLatBlocksValidation()
    {
        QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!ts.endsWith('Z')) ts += 'Z';

        EmergencyLocation loc = EmergencyLocation::makeStatic(91.0, 26.1025, ts);
        QCOMPARE(loc.isValid(), false);
        QVERIFY2(!loc.validationErrors().isEmpty(), "validationErrors() must explain why");
    }

    // 3. Longitude out of range → EmergencyLocation.isValid() = false.
    void test_invalidLonBlocksValidation()
    {
        QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!ts.endsWith('Z')) ts += 'Z';

        EmergencyLocation loc = EmergencyLocation::makeStatic(44.4268, 181.0, ts);
        QCOMPARE(loc.isValid(), false);
        QVERIFY2(!loc.validationErrors().isEmpty(), "validationErrors() must explain why");
    }

    // 4. Emergency call chain uses manual location from StaticLocationProvider.
    void test_emergencyCallUsesManualLocation()
    {
        QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!ts.endsWith('Z')) ts += 'Z';

        EmergencyLocation loc = EmergencyLocation::makeStatic(44.4268, 26.1025, ts);
        loc.uncertaintyMeters = 50.0;
        loc.source = LocationSource::Manual;

        StaticLocationProvider provider(loc);
        QCOMPARE(provider.status(), LocationStatus::Available);
        QVERIFY2(!provider.pidfLo().isEmpty(), "StaticLocationProvider must return PIDF-LO");

        EmergencyCallProfile profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@10.2.0.180"), QStringLiteral("NG112-TEST"));
        QVERIFY(profile.isValid());

        EmergencyCallController controller;
        controller.setLocationProvider(&provider);
        controller.setProfile(profile);

        EmergencyCallProfile captured;
        connect(&controller, &EmergencyCallController::readyToDial,
                &controller, [&](const EmergencyCallProfile &p) { captured = p; });

        QVERIFY(controller.prepare());
        QVERIFY2(!captured.pidfLo.isEmpty(), "Profile pidfLo must be populated after prepare()");
        QVERIFY2(captured.pidfLo.contains(QStringLiteral("44.4268")),
                 "PIDF-LO must contain manually entered latitude");
    }

    // 5. locationRequired=true, no pidfLo → validate() fails.
    void test_locationRequiredBlocksWithoutPidfLo()
    {
        EmergencyCallProfile profile;
        profile.routingTarget = QStringLiteral("sip:psap@10.2.0.180");
        profile.serviceUrn    = QStringLiteral("urn:service:sos");
        // pidfLo intentionally empty

        const EmergencyInvite invite = EmergencyInviteBuilder(profile)
            .setLocationAvailable(false)
            .setLocationRequired(true)
            .setContentId(QString{})
            .build();

        const EmergencyInviteValidationResult vr = EmergencyInviteBuilder::validate(invite);
        QCOMPARE(vr.isValid(), false);
        QVERIFY2(!vr.errors.isEmpty(),
                 "Validation must report errors when location is required but absent");
    }

    // 6. toLocationUpdateOptions() → emergencyCall=true; default SipCallOptions → false.
    void test_locationUpdateOptionsIsEmergencyFlagged()
    {
        const SipCallOptions opts = EmergencyCallAdapter::toLocationUpdateOptions(
            QStringLiteral("<presence/>"),
            QStringLiteral("pidflo-1@ng112.local"));

        QCOMPARE(opts.emergencyCall, true);
        QVERIFY2(!opts.body.isEmpty(), "body must contain the PIDF-LO");

        const SipCallOptions normal;
        QCOMPARE(normal.emergencyCall, false);
        QVERIFY2(normal.isEmpty(), "Default SipCallOptions must be empty (guard for normal calls)");
    }

    // 7. toLocationUpdateOptions() includes Geolocation + Geolocation-Routing + Supported.
    void test_locationUpdateOptionsHaveGeolocationHeaders()
    {
        const QString pidfLo = QStringLiteral("<presence/>");
        const QString cid    = QStringLiteral("pidflo-123-0@ng112.local");
        const SipCallOptions opts = EmergencyCallAdapter::toLocationUpdateOptions(pidfLo, cid);

        bool hasGeolocation  = false;
        bool hasGeoRouting   = false;
        bool hasSupported    = false;
        for (const auto &h : opts.customHeaders) {
            if (h.first == QStringLiteral("Geolocation"))         hasGeolocation  = true;
            if (h.first == QStringLiteral("Geolocation-Routing")) hasGeoRouting   = true;
            if (h.first == QStringLiteral("Supported"))           hasSupported    = true;
        }
        QVERIFY2(hasGeolocation,  "Geolocation header must be present");
        QVERIFY2(hasGeoRouting,   "Geolocation-Routing header must be present");
        QVERIFY2(hasSupported,    "Supported: geolocation header must be present");

        // Geolocation value must reference the Content-ID.
        for (const auto &h : opts.customHeaders) {
            if (h.first == QStringLiteral("Geolocation")) {
                QVERIFY2(h.second.contains(cid),
                         "Geolocation header value must contain the content ID");
            }
        }

        // body and contentId must be set.
        QCOMPARE(opts.body, pidfLo);
        QCOMPARE(opts.contentId, cid);
    }

    // 8. generateContentId() returns a different value for each sequential call.
    void test_generateContentIdChanges()
    {
        const QString id1 = EmergencyCallAdapter::generateContentId();
        const QString id2 = EmergencyCallAdapter::generateContentId();

        // Atomic counter guarantees uniqueness regardless of timing.
        QVERIFY2(id1 != id2,
                 "Each generateContentId() call must return a unique value "
                 "(atomic counter ensures this even within the same millisecond)");

        // Both must conform to the expected format.
        QVERIFY2(id1.startsWith(QStringLiteral("pidflo-")),
                 "Content-ID must start with 'pidflo-'");
        QVERIFY2(id1.endsWith(QStringLiteral("@ng112.local")),
                 "Content-ID must end with '@ng112.local'");
        QVERIFY2(id2.startsWith(QStringLiteral("pidflo-")),
                 "Content-ID must start with 'pidflo-'");
        QVERIFY2(id2.endsWith(QStringLiteral("@ng112.local")),
                 "Content-ID must end with '@ng112.local'");
    }
};

QTEST_GUILESS_MAIN(TestEmergencyLocationUpdate)
#include "test_emergency_location_update.moc"
