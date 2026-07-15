#include <QTest>

#include "sip/RtpPortDiagnostics.h"

class TestRtpPortDiagnostics : public QObject
{
    Q_OBJECT
private slots:
    // 1. Windows WSAEADDRINUSE-style reason classifies as RtpPortInUse.
    void test_wsaeaddrinuseClassifiesAsInUse()
    {
        QCOMPARE(classifyRtpPortError(QStringLiteral(
                     "Transport error: Address already in use (WSAEADDRINUSE)")),
                 RtpPortErrorKind::RtpPortInUse);
    }

    // 2. POSIX EADDRINUSE-style reason also classifies as RtpPortInUse.
    void test_eaddrinuseClassifiesAsInUse()
    {
        QCOMPARE(classifyRtpPortError(QStringLiteral("bind() failed: EADDRINUSE")),
                 RtpPortErrorKind::RtpPortInUse);
    }

    // 3. Plain "already in use" text is recognized case-insensitively.
    void test_alreadyInUseCaseInsensitive()
    {
        QCOMPARE(classifyRtpPortError(QStringLiteral("Address ALREADY IN USE")),
                 RtpPortErrorKind::RtpPortInUse);
    }

    // 4. Exhausted-range wording classifies distinctly from a simple in-use port.
    void test_exhaustedRangeClassifies()
    {
        QCOMPARE(classifyRtpPortError(QStringLiteral(
                     "Unable to find a free port in the configured range")),
                 RtpPortErrorKind::RtpPortRangeExhausted);
    }

    // 5. A generic port/transport/bind failure that isn't specifically "in
    // use" or "exhausted" still gets a port-related classification, not
    // NotPortRelated.
    void test_genericPortFailureClassifiesAsAllocationFailed()
    {
        QCOMPARE(classifyRtpPortError(QStringLiteral("Failed to create media transport")),
                 RtpPortErrorKind::RtpPortAllocationFailed);
    }

    // 6. An unrelated error (e.g. a plain SIP timeout) is never misclassified
    // as a port problem.
    void test_unrelatedErrorNotPortRelated()
    {
        QCOMPARE(classifyRtpPortError(QStringLiteral("Request timed out")),
                 RtpPortErrorKind::NotPortRelated);
    }

    // 7. rtpPortErrorKindName returns a distinct, non-empty name per kind
    // (used in log lines so operators can grep for a specific failure mode).
    void test_kindNamesAreDistinct()
    {
        const QString a = rtpPortErrorKindName(RtpPortErrorKind::NotPortRelated);
        const QString b = rtpPortErrorKindName(RtpPortErrorKind::RtpPortInUse);
        const QString c = rtpPortErrorKindName(RtpPortErrorKind::RtpPortRangeExhausted);
        const QString d = rtpPortErrorKindName(RtpPortErrorKind::RtpPortAllocationFailed);
        QVERIFY(!a.isEmpty() && !b.isEmpty() && !c.isEmpty() && !d.isEmpty());
        QVERIFY(a != b && a != c && a != d && b != c && b != d && c != d);
    }

    // 8. Empty reason string never crashes and classifies as NotPortRelated.
    void test_emptyReasonIsSafe()
    {
        QCOMPARE(classifyRtpPortError(QString()), RtpPortErrorKind::NotPortRelated);
    }
};

QTEST_GUILESS_MAIN(TestRtpPortDiagnostics)
#include "test_rtp_port_diagnostics.moc"
