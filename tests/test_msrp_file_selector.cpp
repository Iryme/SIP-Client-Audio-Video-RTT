#include <QtTest/QtTest>

#include "msrp/MsrpFileSelector.h"

// RFC 5547 file-selector attribute parse/build (Task W104) — pure text,
// no filesystem access.
class TestMsrpFileSelector : public QObject
{
    Q_OBJECT

private slots:
    void parsesAllFields();
    void parseIsToleratesMissingFields();
    void parseFlagsMalformedTokens();
    void buildRoundTrips();
    void buildOmitsEmptyFields();
    void sanitizeStripsDirectoryComponents();
    void sanitizeRejectsDotDot();
    void sanitizeFallsBackWhenEmpty();
};

void TestMsrpFileSelector::parsesAllFields()
{
    const auto info = MsrpFileSelector::parse(
        QStringLiteral("name:\"report.pdf\" size:2022 type:application/pdf hash:sha-1:2A:AE"));
    QVERIFY(info.ok);
    QCOMPARE(info.fileName, QStringLiteral("report.pdf"));
    QCOMPARE(info.fileSize, static_cast<qint64>(2022));
    QCOMPARE(info.fileType, QStringLiteral("application/pdf"));
    QCOMPARE(info.hashAlgorithm, QStringLiteral("sha-1"));
    QCOMPARE(info.hashValueHex, QStringLiteral("2AAE"));
    QVERIFY(info.warnings.isEmpty());
}

void TestMsrpFileSelector::parseIsToleratesMissingFields()
{
    const auto info = MsrpFileSelector::parse(QStringLiteral("name:\"a b c.txt\""));
    QVERIFY(info.ok);
    QCOMPARE(info.fileName, QStringLiteral("a b c.txt")); // quoted name may contain spaces
    QCOMPARE(info.fileSize, static_cast<qint64>(-1));
    QVERIFY(info.fileType.isEmpty());
}

void TestMsrpFileSelector::parseFlagsMalformedTokens()
{
    const auto empty = MsrpFileSelector::parse(QString());
    QVERIFY(!empty.ok);
    QVERIFY(!empty.warnings.isEmpty());

    const auto badSize = MsrpFileSelector::parse(QStringLiteral("size:notanumber"));
    QVERIFY(!badSize.warnings.isEmpty());
    QCOMPARE(badSize.fileSize, static_cast<qint64>(-1));

    const auto noColon = MsrpFileSelector::parse(QStringLiteral("garbage"));
    QVERIFY(!noColon.ok);
    QVERIFY(!noColon.warnings.isEmpty());
}

void TestMsrpFileSelector::buildRoundTrips()
{
    const QString built = MsrpFileSelector::build(QStringLiteral("photo.jpg"), 4096,
        QStringLiteral("image/jpeg"), QStringLiteral("sha-1"), QStringLiteral("2AAE00FF1"));
    const auto reparsed = MsrpFileSelector::parse(built);
    QVERIFY(reparsed.ok);
    QCOMPARE(reparsed.fileName, QStringLiteral("photo.jpg"));
    QCOMPARE(reparsed.fileSize, static_cast<qint64>(4096));
    QCOMPARE(reparsed.fileType, QStringLiteral("image/jpeg"));
    QCOMPARE(reparsed.hashAlgorithm, QStringLiteral("sha-1"));
    QCOMPARE(reparsed.hashValueHex, QStringLiteral("2AAE00FF1"));
}

void TestMsrpFileSelector::buildOmitsEmptyFields()
{
    const QString built = MsrpFileSelector::build(QStringLiteral("x.bin"), -1, QString(), QString(), QString());
    QCOMPARE(built, QStringLiteral("name:\"x.bin\""));
}

void TestMsrpFileSelector::sanitizeStripsDirectoryComponents()
{
    QCOMPARE(MsrpFileSelector::sanitizeFileNameForDisplay(QStringLiteral("../../etc/passwd")),
             QStringLiteral("passwd"));
    QCOMPARE(MsrpFileSelector::sanitizeFileNameForDisplay(QStringLiteral("C:\\Windows\\evil.exe")),
             QStringLiteral("evil.exe"));
}

void TestMsrpFileSelector::sanitizeRejectsDotDot()
{
    QCOMPARE(MsrpFileSelector::sanitizeFileNameForDisplay(QStringLiteral("..")),
             QStringLiteral("received-file"));
    QCOMPARE(MsrpFileSelector::sanitizeFileNameForDisplay(QStringLiteral(".")),
             QStringLiteral("received-file"));
}

void TestMsrpFileSelector::sanitizeFallsBackWhenEmpty()
{
    QCOMPARE(MsrpFileSelector::sanitizeFileNameForDisplay(QString()), QStringLiteral("received-file"));
    QCOMPARE(MsrpFileSelector::sanitizeFileNameForDisplay(QStringLiteral("  ")), QStringLiteral("received-file"));
}

QTEST_APPLESS_MAIN(TestMsrpFileSelector)
#include "test_msrp_file_selector.moc"
