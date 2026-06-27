#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include "emergency/EmergencyLocation.h"
#include "emergency/StaticLocationProvider.h"

class TestEmergencyLocation : public QObject
{
    Q_OBJECT
private slots:

    // -----------------------------------------------------------------------
    // EmergencyLocation validation
    // -----------------------------------------------------------------------

    // 1. Valid lat/lon passes validation
    void test_validLatLon()
    {
        auto loc = EmergencyLocation::makeStatic(47.6062, -122.3321,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        QVERIFY(loc.isValid());
        QVERIFY(loc.validationErrors().isEmpty());
    }

    // 2. Latitude below -90 fails
    void test_invalidLatitudeTooLow()
    {
        auto loc = EmergencyLocation::makeStatic(-91.0, 0.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        QVERIFY(!loc.isValid());
        QVERIFY(loc.validationErrors().contains(
            QStringLiteral("latitude must be in range -90..90")));
    }

    // 3. Latitude above 90 fails
    void test_invalidLatitudeTooHigh()
    {
        auto loc = EmergencyLocation::makeStatic(91.0, 0.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        QVERIFY(!loc.isValid());
        QVERIFY(loc.validationErrors().contains(
            QStringLiteral("latitude must be in range -90..90")));
    }

    // 4. Longitude below -180 fails
    void test_invalidLongitudeTooLow()
    {
        auto loc = EmergencyLocation::makeStatic(0.0, -181.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        QVERIFY(!loc.isValid());
        QVERIFY(loc.validationErrors().contains(
            QStringLiteral("longitude must be in range -180..180")));
    }

    // 5. Longitude above 180 fails
    void test_invalidLongitudeTooHigh()
    {
        auto loc = EmergencyLocation::makeStatic(0.0, 181.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        QVERIFY(!loc.isValid());
        QVERIFY(loc.validationErrors().contains(
            QStringLiteral("longitude must be in range -180..180")));
    }

    // 6. Missing timestamp fails
    void test_invalidEmptyTimestamp()
    {
        EmergencyLocation loc;
        loc.latitude  = 47.0;
        loc.longitude = 10.0;
        loc.timestamp = QString();

        QVERIFY(!loc.isValid());
        QVERIFY(loc.validationErrors().contains(
            QStringLiteral("timestamp must not be empty")));
    }

    // 7. Boundary values -90 and 90 are valid
    void test_boundaryLatitudeValid()
    {
        auto locMin = EmergencyLocation::makeStatic(-90.0, 0.0, QStringLiteral("2026-01-01T00:00:00Z"));
        auto locMax = EmergencyLocation::makeStatic( 90.0, 0.0, QStringLiteral("2026-01-01T00:00:00Z"));
        QVERIFY(locMin.isValid());
        QVERIFY(locMax.isValid());
    }

    // 8. makeStatic factory sets source to Static
    void test_makeStaticSetsSourceStatic()
    {
        auto loc = EmergencyLocation::makeStatic(0.0, 0.0, QStringLiteral("2026-06-27T12:00:00Z"));
        QCOMPARE(loc.source, LocationSource::Static);
    }

    // 9. uncertaintyMeters < 0 is the "not set" sentinel — allowed
    void test_uncertaintyNotSetIsAllowed()
    {
        auto loc = EmergencyLocation::makeStatic(47.0, 10.0, QStringLiteral("2026-06-27T12:00:00Z"));
        loc.uncertaintyMeters = -1.0;
        QVERIFY(loc.isValid());
    }

    // 10. uncertaintyMeters >= 0 is valid
    void test_uncertaintyZeroIsValid()
    {
        auto loc = EmergencyLocation::makeStatic(47.0, 10.0, QStringLiteral("2026-06-27T12:00:00Z"));
        loc.uncertaintyMeters = 0.0;
        QVERIFY(loc.isValid());
    }

    // -----------------------------------------------------------------------
    // StaticLocationProvider
    // -----------------------------------------------------------------------

    // 11. Valid location → status Available
    void test_validLocationStatusAvailable()
    {
        auto loc = EmergencyLocation::makeStatic(47.6062, -122.3321,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        StaticLocationProvider provider(loc);

        QCOMPARE(provider.status(), LocationStatus::Available);
    }

    // 12. Invalid location → status Unavailable
    void test_invalidLocationStatusUnavailable()
    {
        EmergencyLocation loc;
        loc.latitude  = 200.0;  // invalid
        loc.longitude = 0.0;
        loc.timestamp = QStringLiteral("2026-06-27T12:00:00Z");
        StaticLocationProvider provider(loc);

        QCOMPARE(provider.status(), LocationStatus::Unavailable);
    }

    // 13. Invalid location → unavailableReason non-empty
    void test_invalidLocationReasonNonEmpty()
    {
        EmergencyLocation loc;
        loc.latitude  = 200.0;
        loc.longitude = 0.0;
        loc.timestamp = QStringLiteral("2026-06-27T12:00:00Z");
        StaticLocationProvider provider(loc);

        QVERIFY(!provider.unavailableReason().isEmpty());
    }

    // 14. Valid location → pidfLo non-empty
    void test_validLocationPidfLoNonEmpty()
    {
        auto loc = EmergencyLocation::makeStatic(47.6062, -122.3321,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        StaticLocationProvider provider(loc);

        QVERIFY(!provider.pidfLo().isEmpty());
    }

    // 15. Invalid location → pidfLo empty
    void test_invalidLocationPidfLoEmpty()
    {
        EmergencyLocation loc;
        loc.latitude  = 200.0;
        loc.longitude = 0.0;
        loc.timestamp = QStringLiteral("2026-06-27T12:00:00Z");
        StaticLocationProvider provider(loc);

        QVERIFY(provider.pidfLo().isEmpty());
    }

    // 16. requestLocation on valid location emits locationAvailable
    void test_requestLocationEmitsSignal()
    {
        auto loc = EmergencyLocation::makeStatic(47.6062, -122.3321,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        StaticLocationProvider provider(loc);
        QSignalSpy spy(&provider, &EmergencyLocationProvider::locationAvailable);

        provider.requestLocation();

        QCOMPARE(spy.count(), 1);
        const QString emittedPidfLo = spy.first().first().toString();
        QVERIFY(!emittedPidfLo.isEmpty());
    }

    // 17. requestLocation on invalid location emits locationStatusChanged(Unavailable)
    void test_requestLocationInvalidEmitsUnavailable()
    {
        EmergencyLocation loc;
        loc.latitude  = 200.0;
        loc.longitude = 0.0;
        loc.timestamp = QStringLiteral("2026-06-27T12:00:00Z");
        StaticLocationProvider provider(loc);
        QSignalSpy spyStatus(&provider, &EmergencyLocationProvider::locationStatusChanged);
        QSignalSpy spyAvail(&provider, &EmergencyLocationProvider::locationAvailable);

        provider.requestLocation();

        QVERIFY(spyAvail.isEmpty());
        QCOMPARE(spyStatus.count(), 1);
        QCOMPARE(spyStatus.first().first().value<LocationStatus>(),
                 LocationStatus::Unavailable);
    }

    // 18. setLocation updates status dynamically
    void test_setLocationUpdatesStatus()
    {
        EmergencyLocation invalid;
        invalid.latitude  = 200.0;
        invalid.longitude = 0.0;
        invalid.timestamp = QStringLiteral("2026-06-27T12:00:00Z");
        StaticLocationProvider provider(invalid);
        QCOMPARE(provider.status(), LocationStatus::Unavailable);

        auto valid = EmergencyLocation::makeStatic(47.0, 10.0,
                                                    QStringLiteral("2026-06-27T12:00:00Z"));
        provider.setLocation(valid);
        QCOMPARE(provider.status(), LocationStatus::Available);
    }
};

QTEST_GUILESS_MAIN(TestEmergencyLocation)
#include "test_emergency_location.moc"
