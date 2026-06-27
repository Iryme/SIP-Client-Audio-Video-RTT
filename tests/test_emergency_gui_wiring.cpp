// Task 42 — Emergency GUI Wiring tests
//
// Tests the logic behind the emergency button / controller wiring without
// requiring a real SIP stack or Qt GUI widgets (QTEST_GUILESS_MAIN).
//
// Coverage:
//   1. emergencyTestModeEnabled() defaults to false (button hidden by default).
//   2. emergencyTarget() returns a non-empty SIP lab URI by default.
//   3. Full chain: controller + StaticLocationProvider → valid SipCallOptions
//      with emergencyCall=true and all NG112 headers present.
//   4. Invalid profile (empty routingTarget) → prepare() returns false.
//   5. Normal SipCallOptions is not an emergency call (normal flow unchanged).
//   6. Without prepare() no call is issued (controller stays Idle).

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "emergency/EmergencyCallAdapter.h"
#include "emergency/EmergencyCallController.h"
#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyCallStateMachine.h"
#include "emergency/EmergencyInviteBuilder.h"
#include "emergency/EmergencyLocation.h"
#include "emergency/EmergencyLocationProvider.h"
#include "emergency/StaticLocationProvider.h"
#include "sip/SipCallOptions.h"

class TestEmergencyGuiWiring : public QObject
{
    Q_OBJECT

private slots:
    // 1. Button hidden by default — AppSettings flag defaults to false.
    void test_emergencyTestModeDefaultFalse()
    {
        // Temporarily clear the setting so we read the real default.
        AppSettings::settings().remove("emergency/testMode");
        QCOMPARE(AppSettings::emergencyTestModeEnabled(), false);
    }

    // 2. Default emergency target is a lab SIP URI, not a real PSAP.
    void test_emergencyTargetDefault()
    {
        AppSettings::settings().remove("emergency/target");
        const QString target = AppSettings::emergencyTarget();
        QVERIFY(!target.isEmpty());
        QVERIFY2(target.startsWith("sip:"),
                 "Default emergency target must be a SIP URI");
        // Must not be a public PSAP number.
        QVERIFY2(!target.contains("112") || target.contains("@"),
                 "Default target must be a SIP URI, not a PSTN number");
    }

    // 3. Full chain with StaticLocationProvider produces valid SipCallOptions.
    void test_controllerWithStaticLocationProducesValidOpts()
    {
        // DEMO coordinates (Bucharest), same as used in CallPanel.
        const QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
                           + (QDateTime::currentDateTimeUtc()
                                  .toString(Qt::ISODate)
                                  .endsWith('Z') ? QString{} : QString{"Z"});
        EmergencyLocation demoLoc = EmergencyLocation::makeStatic(44.4268, 26.1025, ts);
        demoLoc.uncertaintyMeters = 50.0;
        demoLoc.source            = LocationSource::Static;

        StaticLocationProvider provider(demoLoc);
        QCOMPARE(provider.status(), LocationStatus::Available);
        QVERIFY(!provider.pidfLo().isEmpty());

        const QString target = QStringLiteral("sip:psap@10.2.0.180");
        EmergencyCallProfile profile = EmergencyCallProfile::makeSos(
            target, QStringLiteral("NG112-TEST"));
        QVERIFY(profile.isValid());

        // Wire controller.
        EmergencyCallController controller;
        controller.setLocationProvider(&provider);
        controller.setProfile(profile);
        QCOMPARE(controller.state(), EmergencyCallState::Idle);

        // Capture the readyToDial signal.
        EmergencyCallProfile capturedProfile;
        bool readyFired = false;
        connect(&controller, &EmergencyCallController::readyToDial,
                &controller, [&](const EmergencyCallProfile &p) {
                    capturedProfile = p;
                    readyFired = true;
                });

        QVERIFY(controller.prepare());
        QVERIFY2(readyFired, "readyToDial must fire synchronously via StaticLocationProvider");
        QVERIFY(!capturedProfile.pidfLo.isEmpty());

        // Build invite and convert to SipCallOptions.
        const QString contentId = EmergencyCallAdapter::generateContentId();
        const EmergencyInvite invite = EmergencyInviteBuilder(capturedProfile)
            .setLocationAvailable(true)
            .setLocationRequired(false)
            .setContentId(contentId)
            .build();

        const EmergencyInviteValidationResult vr = EmergencyInviteBuilder::validate(invite);
        QVERIFY2(vr.isValid(),
                 qPrintable(vr.errors.join(", ")));

        const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(invite);
        QCOMPARE(opts.emergencyCall, true);
        QVERIFY(!opts.body.isEmpty());
        QVERIFY(!opts.contentId.isEmpty());
        QCOMPARE(opts.requireAudio, true);
        QCOMPARE(opts.requireRtt,   true);
        QCOMPARE(opts.allowVideo,   false);

        // Verify NG112 headers are present.
        bool hasGeolocation        = false;
        bool hasGeolocationRouting = false;
        bool hasSupported          = false;
        for (const auto &h : opts.customHeaders) {
            if (h.first == QStringLiteral("Geolocation"))         hasGeolocation        = true;
            if (h.first == QStringLiteral("Geolocation-Routing")) hasGeolocationRouting = true;
            if (h.first == QStringLiteral("Supported"))           hasSupported          = true;
        }
        QVERIFY2(hasGeolocation,        "Geolocation header must be present");
        QVERIFY2(hasGeolocationRouting, "Geolocation-Routing header must be present");
        QVERIFY2(hasSupported,          "Supported: geolocation header must be present");
    }

    // 4. Invalid profile (empty routingTarget) blocks call preparation.
    void test_invalidProfileBlocksCallPreparation()
    {
        EmergencyCallProfile bad;
        bad.serviceUrn = QStringLiteral("urn:service:sos");
        bad.routingTarget.clear(); // invalid — missing target
        QVERIFY(!bad.isValid());

        EmergencyCallController controller;
        controller.setProfile(bad);

        bool failedFired = false;
        connect(&controller, &EmergencyCallController::preparationFailed,
                &controller, [&](const QString &) { failedFired = true; });

        const bool result = controller.prepare();
        QCOMPARE(result, false);
        QVERIFY2(failedFired, "preparationFailed must be emitted for invalid profile");
        QCOMPARE(controller.state(), EmergencyCallState::Idle);
    }

    // 5. Normal SipCallOptions is not an emergency call (normal flow unchanged).
    void test_normalSipCallOptionsIsNotEmergency()
    {
        const SipCallOptions normal;
        QCOMPARE(normal.emergencyCall, false);
        QVERIFY2(normal.isEmpty(),
                 "Default SipCallOptions must be empty (no emergency headers)");
    }

    // 6. Without prepare(), the controller stays Idle (no call is issued).
    void test_withoutPrepareControllerStaysIdle()
    {
        EmergencyCallController controller;
        controller.setProfile(EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@10.2.0.180")));

        // No prepare() call — state must remain Idle.
        QCOMPARE(controller.state(), EmergencyCallState::Idle);

        // Simulate what CallPanel does when the confirm dialog returns No:
        // nothing — prepare() is never called, so no INVITE is produced.
        // There is no SipCallOptions to verify here; the state machine being
        // Idle is the guarantee that no call path was entered.
        QCOMPARE(controller.state(), EmergencyCallState::Idle);
    }
};

QTEST_GUILESS_MAIN(TestEmergencyGuiWiring)
#include "test_emergency_gui_wiring.moc"
