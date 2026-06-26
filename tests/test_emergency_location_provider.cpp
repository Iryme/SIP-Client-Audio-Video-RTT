#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>

#include "emergency/EmergencyLocationProvider.h"
#include "emergency/EmergencyCallController.h"
#include "emergency/EmergencyCallProfile.h"

class TestEmergencyLocationProvider : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("IrymeTest_emergency"));
        QCoreApplication::setApplicationName(QStringLiteral("test_emergency_location_provider"));
    }

    // 1. NullLocationProvider status is NotImplemented
    void test_nullProviderStatus()
    {
        NullLocationProvider p;
        QCOMPARE(p.status(), LocationStatus::NotImplemented);
    }

    // 2. NullLocationProvider pidfLo is empty
    void test_nullProviderPidfLoEmpty()
    {
        NullLocationProvider p;
        QVERIFY(p.pidfLo().isEmpty());
    }

    // 3. requestLocation() on NullLocationProvider emits locationStatusChanged(NotImplemented)
    void test_nullProviderRequestLocationEmitsSignal()
    {
        NullLocationProvider p;
        QSignalSpy spy(&p, &EmergencyLocationProvider::locationStatusChanged);

        p.requestLocation();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<LocationStatus>(), LocationStatus::NotImplemented);
    }

    // 4. locationStatusName() returns correct strings
    void test_locationStatusNames()
    {
        QCOMPARE(locationStatusName(LocationStatus::Unavailable),    QStringLiteral("Unavailable"));
        QCOMPARE(locationStatusName(LocationStatus::NotImplemented), QStringLiteral("NotImplemented"));
    }

    // 5. Controller with NullLocationProvider skips to ReadyToDial on prepare()
    void test_controllerWithNullProviderReachesReadyToDial()
    {
        EmergencyCallController ctrl;
        ctrl.setProfile(EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@example.com")));

        QSignalSpy spy(&ctrl, &EmergencyCallController::readyToDial);
        const bool ok = ctrl.prepare();

        QVERIFY(ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.state(), EmergencyCallState::ReadyToDial);
    }

    // 6. Controller emits preparationFailed for invalid profile
    void test_controllerInvalidProfileFails()
    {
        EmergencyCallController ctrl;
        // Default profile is invalid (empty serviceUrn, empty routingTarget)
        QSignalSpy spy(&ctrl, &EmergencyCallController::preparationFailed);

        const bool ok = ctrl.prepare();

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.state(), EmergencyCallState::Idle);
    }

    // 7. Controller abort() resets to Idle and emits aborted
    void test_controllerAbort()
    {
        EmergencyCallController ctrl;
        ctrl.setProfile(EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@example.com")));
        ctrl.prepare();

        QSignalSpy spy(&ctrl, &EmergencyCallController::aborted);
        ctrl.abort(QStringLiteral("user cancelled"));

        QCOMPARE(ctrl.state(), EmergencyCallState::Idle);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("user cancelled"));
    }

    // 8. setLocationProvider(nullptr) reverts to NullLocationProvider
    void test_setProviderNullRevertsToNull()
    {
        EmergencyCallController ctrl;
        ctrl.setLocationProvider(nullptr);
        QVERIFY(ctrl.locationProvider() != nullptr);
        QCOMPARE(ctrl.locationProvider()->status(), LocationStatus::NotImplemented);
    }

    // 9. Controller profile is stored and retrievable
    void test_controllerProfileStored()
    {
        EmergencyCallController ctrl;
        auto p = EmergencyCallProfile::makeSos(QStringLiteral("sip:112@psap.local"),
                                               QStringLiteral("Bob"));
        ctrl.setProfile(p);
        QCOMPARE(ctrl.profile().serviceUrn, QStringLiteral("urn:service:sos"));
        QCOMPARE(ctrl.profile().routingTarget, QStringLiteral("sip:112@psap.local"));
        QCOMPARE(ctrl.profile().callerDisplayName, QStringLiteral("Bob"));
    }

    // 10. readyToDial signal carries the profile
    void test_readyToDialCarriesProfile()
    {
        EmergencyCallController ctrl;
        auto p = EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@example.com"),
                                               QStringLiteral("Carol"));
        ctrl.setProfile(p);

        QSignalSpy spy(&ctrl, &EmergencyCallController::readyToDial);
        ctrl.prepare();

        QCOMPARE(spy.count(), 1);
        const auto received = spy.first().first().value<EmergencyCallProfile>();
        QCOMPARE(received.serviceUrn, QStringLiteral("urn:service:sos"));
        QCOMPARE(received.routingTarget, QStringLiteral("sip:psap@example.com"));
        QCOMPARE(received.callerDisplayName, QStringLiteral("Carol"));
    }
};

QTEST_GUILESS_MAIN(TestEmergencyLocationProvider)
#include "test_emergency_location_provider.moc"
