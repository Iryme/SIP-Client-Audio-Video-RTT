#include <QCoreApplication>
#include <QTest>

#include "emergency/EmergencyCallProfile.h"

class TestEmergencyCallProfile : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("IrymeTest_emergency"));
        QCoreApplication::setApplicationName(QStringLiteral("test_emergency_profile"));
    }

    // 1. Default-constructed profile is invalid (all fields empty)
    void test_defaultInvalid()
    {
        EmergencyCallProfile p;
        QVERIFY(!p.isValid());
        QVERIFY(!p.validationError().isEmpty());
    }

    // 2. Missing serviceUrn is invalid
    void test_missingServiceUrn()
    {
        EmergencyCallProfile p;
        p.routingTarget = QStringLiteral("sip:psap@example.com");
        QVERIFY(!p.isValid());
        QVERIFY(p.validationError().contains(QStringLiteral("serviceUrn")));
    }

    // 3. Malformed serviceUrn (not urn:service:) is invalid
    void test_badServiceUrn()
    {
        EmergencyCallProfile p;
        p.serviceUrn    = QStringLiteral("sos");
        p.routingTarget = QStringLiteral("sip:psap@example.com");
        QVERIFY(!p.isValid());
        QVERIFY(p.validationError().contains(QStringLiteral("urn:service:")));
    }

    // 4. Missing routingTarget is invalid
    void test_missingRoutingTarget()
    {
        EmergencyCallProfile p;
        p.serviceUrn = QStringLiteral("urn:service:sos");
        QVERIFY(!p.isValid());
        QVERIFY(p.validationError().contains(QStringLiteral("routingTarget")));
    }

    // 5. Valid minimal profile
    void test_validMinimal()
    {
        EmergencyCallProfile p;
        p.serviceUrn    = QStringLiteral("urn:service:sos");
        p.routingTarget = QStringLiteral("sip:psap@example.com");
        QVERIFY(p.isValid());
        QVERIFY(p.validationError().isEmpty());
    }

    // 6. makeSos() factory creates a valid SOS profile
    void test_makeSos()
    {
        const auto p = EmergencyCallProfile::makeSos(
            QStringLiteral("sip:psap@ng112.example.com"),
            QStringLiteral("Alice"));
        QVERIFY(p.isValid());
        QCOMPARE(p.serviceUrn, QStringLiteral("urn:service:sos"));
        QCOMPARE(p.routingTarget, QStringLiteral("sip:psap@ng112.example.com"));
        QCOMPARE(p.callerDisplayName, QStringLiteral("Alice"));
        QVERIFY(p.pidfLo.isEmpty());
    }

    // 7. makeSos() without callerDisplayName is still valid
    void test_makeSosNoDisplayName()
    {
        const auto p = EmergencyCallProfile::makeSos(QStringLiteral("sip:112@psap.local"));
        QVERIFY(p.isValid());
        QVERIFY(p.callerDisplayName.isEmpty());
    }

    // 8. pidfLo field is optional (does not affect validity)
    void test_pidfLoOptional()
    {
        auto p = EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@example.com"));
        QVERIFY(p.isValid());
        p.pidfLo = QStringLiteral("<presence>...</presence>");
        QVERIFY(p.isValid());
    }

    // 9. Valid sub-service URNs (sos.police, sos.fire)
    void test_subServiceUrns()
    {
        for (const auto &urn : {
            QStringLiteral("urn:service:sos.police"),
            QStringLiteral("urn:service:sos.fire"),
            QStringLiteral("urn:service:sos.ambulance"),
        }) {
            EmergencyCallProfile p;
            p.serviceUrn    = urn;
            p.routingTarget = QStringLiteral("sip:psap@example.com");
            QVERIFY2(p.isValid(), qPrintable(p.validationError()));
        }
    }

    // 10. additionalDataUris is optional
    void test_additionalDataUrisOptional()
    {
        auto p = EmergencyCallProfile::makeSos(QStringLiteral("sip:psap@example.com"));
        QVERIFY(p.additionalDataUris.isEmpty());
        p.additionalDataUris << QStringLiteral("https://example.com/callerdata");
        QVERIFY(p.isValid());
    }
};

QTEST_GUILESS_MAIN(TestEmergencyCallProfile)
#include "test_emergency_profile.moc"
