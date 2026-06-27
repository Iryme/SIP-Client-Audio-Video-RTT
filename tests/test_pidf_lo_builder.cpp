#include <QCoreApplication>
#include <QTest>

// No PJSIP headers — PidfLoBuilder must be testable without PJSIP.
#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyInviteBuilder.h"
#include "emergency/EmergencyLocation.h"
#include "emergency/PidfLoBuilder.h"

class TestPidfLoBuilder : public QObject
{
    Q_OBJECT
private slots:

    // -----------------------------------------------------------------------
    // PidfLoBuilder
    // -----------------------------------------------------------------------

    // 1. Valid location produces non-empty XML
    void test_buildsNonEmptyXml()
    {
        auto loc = EmergencyLocation::makeStatic(47.6062, -122.3321,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QVERIFY(!r.xml.isEmpty());
    }

    // 2. XML contains latitude and longitude values
    void test_xmlContainsLatLon()
    {
        auto loc = EmergencyLocation::makeStatic(47.606200, -122.332100,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QVERIFY2(r.xml.contains(QStringLiteral("47.606200")),
                 "XML must contain latitude");
        QVERIFY2(r.xml.contains(QStringLiteral("-122.332100")),
                 "XML must contain longitude");
    }

    // 3. XML contains the timestamp
    void test_xmlContainsTimestamp()
    {
        auto loc = EmergencyLocation::makeStatic(0.0, 0.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QVERIFY(r.xml.contains(QStringLiteral("2026-06-27T12:00:00Z")));
    }

    // 4. contentType is application/pidf+xml
    void test_contentType()
    {
        auto loc = EmergencyLocation::makeStatic(0.0, 0.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QCOMPARE(r.contentType, QStringLiteral("application/pidf+xml"));
    }

    // 5. Static contentType() method matches result
    void test_staticContentType()
    {
        QCOMPARE(PidfLoBuilder::contentType(), QStringLiteral("application/pidf+xml"));
    }

    // 6. Same input → same XML (deterministic)
    void test_deterministicOutput()
    {
        auto loc = EmergencyLocation::makeStatic(47.6062, -122.3321,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r1 = PidfLoBuilder(loc).setContentId(QStringLiteral("test@ng112")).build();
        PidfLoResult r2 = PidfLoBuilder(loc).setContentId(QStringLiteral("test@ng112")).build();

        QVERIFY(r1.success);
        QCOMPARE(r1.xml, r2.xml);
    }

    // 7. Invalid location returns success=false with error message
    void test_invalidLocationFails()
    {
        EmergencyLocation loc;
        loc.latitude  = 200.0;  // out of range
        loc.longitude = 0.0;
        loc.timestamp = QStringLiteral("2026-06-27T12:00:00Z");

        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(!r.success);
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.xml.isEmpty());
    }

    // 8. XML escaping: entity with & is escaped in attribute
    void test_xmlEscapingAmpersand()
    {
        auto loc = EmergencyLocation::makeStatic(0.0, 0.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc)
                             .setEntity(QStringLiteral("pres:user&admin@example.com"))
                             .build();

        QVERIFY(r.success);
        QVERIFY2(!r.xml.contains(QStringLiteral("user&admin")),
                 "Raw & must not appear unescaped in XML");
        QVERIFY(r.xml.contains(QStringLiteral("user&amp;admin")));
    }

    // 9. XML escaping: < and > in text
    void test_xmlEscapingBrackets()
    {
        QCOMPARE(PidfLoBuilder::escapeXml(QStringLiteral("a<b>c")),
                 QStringLiteral("a&lt;b&gt;c"));
    }

    // 10. XML attribute escaping: " escaped
    void test_xmlEscapingQuote()
    {
        QCOMPARE(PidfLoBuilder::escapeXmlAttr(QStringLiteral("say \"hello\"")),
                 QStringLiteral("say &quot;hello&quot;"));
    }

    // 11. Without uncertainty → uses gml:Point
    void test_withoutUncertaintyUsesPoint()
    {
        auto loc = EmergencyLocation::makeStatic(47.0, 10.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        // uncertaintyMeters is -1 (not set) by default from makeStatic
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QVERIFY(r.xml.contains(QStringLiteral("gml:Point")));
        QVERIFY(!r.xml.contains(QStringLiteral("gs:Circle")));
    }

    // 12. With uncertainty → uses gs:Circle with radius
    void test_withUncertaintyUsesCircle()
    {
        auto loc = EmergencyLocation::makeStatic(47.0, 10.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        loc.uncertaintyMeters = 50.0;
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QVERIFY(r.xml.contains(QStringLiteral("gs:Circle")));
        QVERIFY(r.xml.contains(QStringLiteral("50.0")));
        QVERIFY(!r.xml.contains(QStringLiteral("gml:Point")));
    }

    // 13. Custom contentId appears in result
    void test_customContentId()
    {
        auto loc = EmergencyLocation::makeStatic(47.0, 10.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc)
                             .setContentId(QStringLiteral("my-cid@example.com"))
                             .build();

        QVERIFY(r.success);
        QCOMPARE(r.contentId, QStringLiteral("my-cid@example.com"));
    }

    // 14. Default contentId is non-empty when not specified
    void test_defaultContentIdNonEmpty()
    {
        auto loc = EmergencyLocation::makeStatic(47.0, 10.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QVERIFY(!r.contentId.isEmpty());
    }

    // 15. XML starts with XML declaration
    void test_xmlDeclaration()
    {
        auto loc = EmergencyLocation::makeStatic(47.0, 10.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QVERIFY(r.xml.startsWith(QStringLiteral("<?xml")));
    }

    // 16. XML contains PIDF presence namespace
    void test_xmlPidfNamespace()
    {
        auto loc = EmergencyLocation::makeStatic(47.0, 10.0,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult r = PidfLoBuilder(loc).build();

        QVERIFY(r.success);
        QVERIFY(r.xml.contains(QStringLiteral("urn:ietf:params:xml:ns:pidf")));
    }

    // -----------------------------------------------------------------------
    // EmergencyInviteBuilder integration with pidfLo
    // -----------------------------------------------------------------------

    // 17. Profile with pidfLo → invite body and contentType populated
    void test_profileWithPidfLoPopulatesBody()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        profile.pidfLo = QStringLiteral("<presence>placeholder</presence>");

        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        QCOMPARE(inv.body, QStringLiteral("<presence>placeholder</presence>"));
        QCOMPARE(inv.contentType, QStringLiteral("application/pidf+xml"));
        QVERIFY(inv.hasLocation);
    }

    // 18. Profile with pidfLo → Geolocation header uses cid: format
    void test_profileWithPidfLoGeolocationCid()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        profile.pidfLo = QStringLiteral("<presence/>");

        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setContentId(QStringLiteral("pidflo-test@ng112"))
                                  .build();

        bool found = false;
        for (const auto &h : inv.headers) {
            if (h.name == QStringLiteral("Geolocation")) {
                QVERIFY2(h.value.startsWith(QStringLiteral("<cid:")),
                         "Geolocation value must use cid: scheme");
                QVERIFY(h.value.contains(QStringLiteral("pidflo-test@ng112")));
                found = true;
            }
        }
        QVERIFY2(found, "Geolocation header must be present");
    }

    // 19. Profile with pidfLo + locationRequired → valid invite
    void test_pidfLoPresentWithLocationRequired()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        profile.pidfLo = QStringLiteral("<presence/>");

        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setLocationRequired(true)
                                  .build();
        auto result = EmergencyInviteBuilder::validate(inv);

        // hasLocation=true (from pidfLo) + locationRequired=true → no location error
        QVERIFY(result.isValid());
        bool hasLocationError = false;
        for (const auto &e : result.errors)
            if (e.contains(QStringLiteral("location"))) hasLocationError = true;
        QVERIFY(!hasLocationError);
    }

    // 20. Profile without pidfLo → body and contentType empty
    void test_profileWithoutPidfLoHasEmptyBody()
    {
        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        // pidfLo is empty by default

        EmergencyInvite inv = EmergencyInviteBuilder(profile).build();

        QVERIFY(inv.body.isEmpty());
        QVERIFY(inv.contentType.isEmpty());
        QVERIFY(!inv.hasLocation);
    }

    // 21. End-to-end: build location → PIDF-LO → profile → invite with body
    void test_endToEndLocationToInvite()
    {
        auto loc = EmergencyLocation::makeStatic(47.6062, -122.3321,
                                                  QStringLiteral("2026-06-27T12:00:00Z"));
        PidfLoResult pidf = PidfLoBuilder(loc)
                                .setContentId(QStringLiteral("pidflo-e2e@ng112"))
                                .build();
        QVERIFY(pidf.success);

        auto profile = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"));
        profile.pidfLo = pidf.xml;

        EmergencyInvite inv = EmergencyInviteBuilder(profile)
                                  .setContentId(pidf.contentId)
                                  .build();

        QVERIFY(inv.hasLocation);
        QVERIFY(!inv.body.isEmpty());
        QCOMPARE(inv.contentType, QStringLiteral("application/pidf+xml"));
        QCOMPARE(inv.contentId, QStringLiteral("pidflo-e2e@ng112"));

        bool geoFound = false;
        for (const auto &h : inv.headers) {
            if (h.name == QStringLiteral("Geolocation")) {
                QVERIFY(h.value.contains(QStringLiteral("pidflo-e2e@ng112")));
                geoFound = true;
            }
        }
        QVERIFY(geoFound);

        auto result = EmergencyInviteBuilder::validate(inv);
        QVERIFY(result.isValid());
    }
};

QTEST_GUILESS_MAIN(TestPidfLoBuilder)
#include "test_pidf_lo_builder.moc"
