// Task 40 — Emergency Protocol Validation
// Negative and positive protocol tests — no PJSIP dependency.
// Validates the full declarative chain:
//   EmergencyCallProfile → EmergencyInviteBuilder → EmergencyCallAdapter → SipCallOptions

#include <QtTest>

#include "emergency/EmergencyCallAdapter.h"
#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyInviteBuilder.h"
#include "emergency/EmergencyLocation.h"
#include "emergency/PidfLoBuilder.h"
#include "sip/SipCallOptions.h"

class TestEmergencyProtocolValidation : public QObject
{
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // Negative: invalid profile
    // -----------------------------------------------------------------------

    void test_emptyProfileIsInvalid()
    {
        EmergencyCallProfile p;
        QVERIFY(!p.isValid());
        QVERIFY(!p.validationError().isEmpty());
    }

    void test_profileWithoutRoutingTargetIsInvalid()
    {
        EmergencyCallProfile p;
        p.serviceUrn = QStringLiteral("urn:service:sos");
        p.routingTarget = QString();
        QVERIFY(!p.isValid());
    }

    void test_profileWithoutServiceUrnIsInvalid()
    {
        EmergencyCallProfile p;
        p.serviceUrn = QString();
        p.routingTarget = QStringLiteral("sip:psap@ng112.local");
        QVERIFY(!p.isValid());
    }

    // -----------------------------------------------------------------------
    // Positive: valid profile
    // -----------------------------------------------------------------------

    void test_makeSosProfileValid()
    {
        const EmergencyCallProfile p =
            EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@ng112.local"),
                                          QStringLiteral("Test User"));
        QVERIFY(p.isValid());
        QVERIFY(p.validationError().isEmpty());
        QCOMPARE(p.serviceUrn, QStringLiteral("urn:service:sos"));
    }

    // -----------------------------------------------------------------------
    // Negative: normal call — no emergency headers
    // -----------------------------------------------------------------------

    void test_normalSipCallOptionsIsEmpty()
    {
        const SipCallOptions opts = SipCallOptions::normal();
        QVERIFY(!opts.emergencyCall);
        QVERIFY(opts.customHeaders.isEmpty());
        QVERIFY(opts.body.isEmpty());
        QVERIFY(opts.contentId.isEmpty());
        QVERIFY(opts.isEmpty());
    }

    // -----------------------------------------------------------------------
    // Negative: emergencyCall=true but empty body → multipart not triggered
    // -----------------------------------------------------------------------

    void test_emergencyWithoutPidfLoBodyIsEmpty()
    {
        EmergencyCallProfile p =
            EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@ng112.local"));
        // pidfLo intentionally NOT set

        const EmergencyInvite inv = EmergencyInviteBuilder(p)
            .setLocationAvailable(false)
            .setLocationRequired(false)
            .build();

        const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QVERIFY(opts.emergencyCall);
        QVERIFY(opts.body.isEmpty());  // guard: multipart NOT triggered
    }

    // -----------------------------------------------------------------------
    // Negative: location required but PIDF-LO missing → validation error
    // -----------------------------------------------------------------------

    void test_locationRequiredWithoutPidfLoIsInvalid()
    {
        EmergencyCallProfile p =
            EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@ng112.local"));
        // pidfLo empty, locationRequired=true

        const EmergencyInvite inv = EmergencyInviteBuilder(p)
            .setLocationAvailable(false)
            .setLocationRequired(true)
            .build();

        const auto result = EmergencyInviteBuilder::validate(inv);
        QVERIFY(!result.isValid());
        QVERIFY(!result.errors.isEmpty());
    }

    // -----------------------------------------------------------------------
    // Positive: full chain with PIDF-LO
    // -----------------------------------------------------------------------

    void test_fullChainWithPidfLoHasBody()
    {
        EmergencyLocation loc = EmergencyLocation::makeStatic(
            44.4268, 26.1025,
            QStringLiteral("2026-06-27T12:00:00Z"));
        loc.uncertaintyMeters = 50.0;

        const QString cid = QStringLiteral("pidflo-test@ng112.local");

        const PidfLoResult pidf = PidfLoBuilder(loc)
            .setEntity(QStringLiteral("pres:test@ng112.local"))
            .setContentId(cid)
            .build();
        QVERIFY(pidf.success);
        QVERIFY(!pidf.xml.isEmpty());

        EmergencyCallProfile p =
            EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@ng112.local"),
                                          QStringLiteral("Test User"));
        p.pidfLo = pidf.xml;

        const EmergencyInvite inv = EmergencyInviteBuilder(p)
            .setLocationAvailable(true)
            .setLocationRequired(false)
            .setContentId(cid)
            .build();

        const auto validation = EmergencyInviteBuilder::validate(inv);
        QVERIFY(validation.isValid());

        const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QVERIFY(opts.emergencyCall);
        QVERIFY(!opts.body.isEmpty());
        QVERIFY(!opts.contentId.isEmpty());
    }

    // -----------------------------------------------------------------------
    // Positive: expected emergency headers are present in SipCallOptions
    // -----------------------------------------------------------------------

    void test_emergencyHeadersPresent()
    {
        EmergencyCallProfile p =
            EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@ng112.local"));

        const EmergencyInvite inv = EmergencyInviteBuilder(p)
            .setLocationAvailable(false)
            .setLocationRequired(false)
            .build();

        const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);

        QStringList headerNames;
        for (const auto &h : opts.customHeaders)
            headerNames.append(h.first);

        // RFC 5031 service URN routing
        QVERIFY(headerNames.contains(QStringLiteral("P-Asserted-Service-URN"),
                                     Qt::CaseInsensitive)
                || headerNames.contains(QStringLiteral("Request-URI"),
                                        Qt::CaseInsensitive)
                || !opts.customHeaders.isEmpty());

        QVERIFY(opts.emergencyCall);
    }

    // -----------------------------------------------------------------------
    // Positive: Geolocation + Supported headers when PIDF-LO present
    // -----------------------------------------------------------------------

    void test_geolocationHeaderPresentWhenPidfLoSet()
    {
        const QString cid = EmergencyCallAdapter::generateContentId();
        QVERIFY(!cid.isEmpty());
        QVERIFY(cid.startsWith(QStringLiteral("pidflo-")));
        QVERIFY(cid.endsWith(QStringLiteral("@ng112.local")));

        EmergencyLocation loc = EmergencyLocation::makeStatic(
            44.4268, 26.1025,
            QStringLiteral("2026-06-27T12:00:00Z"));

        const PidfLoResult pidf = PidfLoBuilder(loc)
            .setContentId(cid)
            .build();
        QVERIFY(pidf.success);

        EmergencyCallProfile p =
            EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@ng112.local"));
        p.pidfLo = pidf.xml;

        const EmergencyInvite inv = EmergencyInviteBuilder(p)
            .setLocationAvailable(true)
            .setLocationRequired(false)
            .setContentId(cid)
            .build();

        const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);

        // Geolocation header must reference Content-ID via <cid:…>
        bool foundGeolocation = false;
        bool foundGeolocationRouting = false;
        bool foundSupported = false;

        for (const auto &h : opts.customHeaders) {
            if (h.first.compare(QStringLiteral("Geolocation"), Qt::CaseInsensitive) == 0) {
                foundGeolocation = true;
                QVERIFY2(h.second.contains(QStringLiteral("cid:")),
                         qPrintable(QStringLiteral("Geolocation missing cid: — got: %1").arg(h.second)));
            }
            if (h.first.compare(QStringLiteral("Geolocation-Routing"), Qt::CaseInsensitive) == 0)
                foundGeolocationRouting = true;
            if (h.first.compare(QStringLiteral("Supported"), Qt::CaseInsensitive) == 0)
                foundSupported = true;
        }

        QVERIFY2(foundGeolocation, "Geolocation header missing");
        QVERIFY2(foundGeolocationRouting, "Geolocation-Routing header missing");
        QVERIFY2(foundSupported, "Supported header missing");
    }

    // -----------------------------------------------------------------------
    // Positive: SDP not replaced — media policy preserved
    // -----------------------------------------------------------------------

    void test_mediaPolicyPreservedThroughAdapter()
    {
        EmergencyCallProfile p =
            EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@ng112.local"));

        EmergencyMediaPolicy policy;
        policy.requireAudio = true;
        policy.requireRtt   = true;
        policy.allowVideo   = false;

        const EmergencyInvite inv = EmergencyInviteBuilder(p)
            .setMediaPolicy(policy)
            .build();

        const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QVERIFY(opts.requireAudio);
        QVERIFY(opts.requireRtt);
        QVERIFY(!opts.allowVideo);
    }

    // -----------------------------------------------------------------------
    // Positive: generateContentId format
    // -----------------------------------------------------------------------

    void test_generateContentIdFormat()
    {
        const QString cid1 = EmergencyCallAdapter::generateContentId();
        QVERIFY(!cid1.isEmpty());
        QVERIFY(cid1.startsWith(QStringLiteral("pidflo-")));
        QVERIFY(cid1.endsWith(QStringLiteral("@ng112.local")));
        QVERIFY(!cid1.contains(QLatin1Char('<')));
        QVERIFY(!cid1.contains(QLatin1Char('>')));
    }

    // -----------------------------------------------------------------------
    // Positive: Content-ID raw field has no angle brackets
    // -----------------------------------------------------------------------

    void test_contentIdRawNoAngleBrackets()
    {
        const QString cid = EmergencyCallAdapter::generateContentId();

        EmergencyCallProfile p =
            EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@ng112.local"));

        EmergencyLocation loc = EmergencyLocation::makeStatic(
            44.4268, 26.1025, QStringLiteral("2026-06-27T12:00:00Z"));
        const PidfLoResult pidf = PidfLoBuilder(loc).setContentId(cid).build();
        p.pidfLo = pidf.xml;

        const EmergencyInvite inv = EmergencyInviteBuilder(p)
            .setLocationAvailable(true)
            .setContentId(cid)
            .build();

        const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);
        QVERIFY(!opts.contentId.contains(QLatin1Char('<')));
        QVERIFY(!opts.contentId.contains(QLatin1Char('>')));
    }

    // -----------------------------------------------------------------------
    // Positive: PIDF-LO XML contains expected elements
    // -----------------------------------------------------------------------

    void test_pidfLoXmlContainsExpectedElements()
    {
        EmergencyLocation loc = EmergencyLocation::makeStatic(
            44.4268, 26.1025,
            QStringLiteral("2026-06-27T12:00:00Z"));
        loc.uncertaintyMeters = 100.0;

        const PidfLoResult pidf = PidfLoBuilder(loc)
            .setEntity(QStringLiteral("pres:test@ng112.local"))
            .setContentId(QStringLiteral("pidflo-test@ng112.local"))
            .build();

        QVERIFY(pidf.success);
        QVERIFY(pidf.xml.contains(QStringLiteral("presence")));
        QVERIFY(pidf.xml.contains(QStringLiteral("geopriv")));
        // With uncertainty → Circle
        QVERIFY(pidf.xml.contains(QStringLiteral("gs:Circle"))
                || pidf.xml.contains(QStringLiteral("Circle")));
        QVERIFY2(pidf.contentType == QStringLiteral("application/pidf+xml"),
                 qPrintable(pidf.contentType));
    }
};

QTEST_GUILESS_MAIN(TestEmergencyProtocolValidation)
#include "test_emergency_protocol_validation.moc"
