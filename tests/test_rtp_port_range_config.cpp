#include <QCoreApplication>
#include <QTest>

#include "sip/RtpPortRangeConfig.h"

class TestRtpPortRangeConfig : public QObject
{
    Q_OBJECT
private slots:
    void cleanup()
    {
        // Never leak a session override across tests.
        setRtpPortRangeSessionOverride(0, 0);
    }

    // 1. A sane range validates cleanly.
    void test_validRange()
    {
        QString error, warning;
        QVERIFY(validateRtpPortRange(4000, 4998, &error, &warning));
        QVERIFY(error.isEmpty());
    }

    // 2. start > end is rejected.
    void test_startGreaterThanEndRejected()
    {
        QString error;
        QVERIFY(!validateRtpPortRange(5000, 4000, &error, nullptr));
        QVERIFY(!error.isEmpty());
    }

    // 3. Odd start port is rejected (RTCP = RTP + 1 convention).
    void test_oddStartRejected()
    {
        QString error;
        QVERIFY(!validateRtpPortRange(4001, 4998, &error, nullptr));
        QVERIFY(!error.isEmpty());
    }

    // 4. Out-of-range ports (below 1024 / above 65535) are rejected.
    void test_outOfRangePortsRejected()
    {
        QString error;
        QVERIFY(!validateRtpPortRange(80, 4998, &error, nullptr));
        QVERIFY(!validateRtpPortRange(4000, 70000, &error, nullptr));
    }

    // 5. A range too small to fit audio+video+text is rejected outright
    // (not just warned about) — this is what prevents "the range is
    // technically non-empty but two calls' streams will still collide".
    void test_tooSmallRangeRejected()
    {
        QString error;
        QVERIFY(!validateRtpPortRange(4000, 4004, &error, nullptr));
        QVERIFY(!error.isEmpty());
    }

    // 6. A small-but-valid range produces a non-fatal warning.
    void test_smallRangeWarns()
    {
        QString error, warning;
        QVERIFY(validateRtpPortRange(4000, 4020, &error, &warning));
        QVERIFY(!warning.isEmpty());
    }

    // 7. A generously sized range has no warning.
    void test_generousRangeNoWarning()
    {
        QString error, warning;
        QVERIFY(validateRtpPortRange(4000, 4998, &error, &warning));
        QVERIFY(warning.isEmpty());
    }

    // 8. Two distinct, non-overlapping ranges (e.g. Alice/Bob on the same
    // host) both validate independently — this is the actual same-host
    // scenario the task is about.
    void test_twoDistinctRangesBothValid()
    {
        QString error;
        QVERIFY(validateRtpPortRange(4000, 4998, &error, nullptr)); // "Alice"
        QVERIFY(validateRtpPortRange(6000, 6998, &error, nullptr)); // "Bob"
    }

    // 9. Session override takes priority over AppSettings/defaults.
    void test_sessionOverrideTakesPriority()
    {
        QVERIFY(!hasRtpPortRangeSessionOverride());
        setRtpPortRangeSessionOverride(8000, 8998);
        QVERIFY(hasRtpPortRangeSessionOverride());

        const RtpPortRangeConfig cfg = resolveEffectiveRtpPortRange();
        QCOMPARE(cfg.start, 8000);
        QCOMPARE(cfg.end, 8998);
    }

    // 10. Clearing the override (start<=0 or end<=0) falls back to defaults.
    void test_clearingOverrideFallsBack()
    {
        setRtpPortRangeSessionOverride(8000, 8998);
        QVERIFY(hasRtpPortRangeSessionOverride());

        setRtpPortRangeSessionOverride(0, 0);
        QVERIFY(!hasRtpPortRangeSessionOverride());
    }

    // 11. portRange() is end - start, matching pj::TransportConfig::portRange
    // semantics ("available ports are [port, port+portRange]").
    void test_portRangeComputation()
    {
        RtpPortRangeConfig cfg;
        cfg.start = 4000;
        cfg.end   = 4998;
        QCOMPARE(cfg.portRange(), 998);
    }

    // 12. Defaults are sane on their own (start even, start<=end, non-trivial size).
    void test_defaultsAreValid()
    {
        RtpPortRangeConfig cfg;
        QString error;
        QVERIFY(validateRtpPortRange(cfg.start, cfg.end, &error, nullptr));
    }
};

QTEST_GUILESS_MAIN(TestRtpPortRangeConfig)
#include "test_rtp_port_range_config.moc"
