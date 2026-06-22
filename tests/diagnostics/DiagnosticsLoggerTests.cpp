#include <QtTest/QtTest>
#include "diagnostics/DiagnosticsLogger.h"

class DiagnosticsLoggerTests : public QObject
{
    Q_OBJECT

private:
    DiagnosticsLogger &logger() { return DiagnosticsLogger::instance(); }

    // Reset to known state before each test case.
    void resetLogger()
    {
        logger().clear();
        logger().setLevelEnabled(LogLevel::Info,  true);
        logger().setLevelEnabled(LogLevel::Warn,  true);
        logger().setLevelEnabled(LogLevel::Error, true);
        logger().setLevelEnabled(LogLevel::Debug, false);
        logger().setLevelEnabled(LogLevel::Raw,   false);
    }

private slots:
    void init() { resetLogger(); }

    // --- Default levels ---

    void test_defaultLevels_infoEnabled()
    {
        QVERIFY(logger().isLevelEnabled(LogLevel::Info));
    }

    void test_defaultLevels_warnEnabled()
    {
        QVERIFY(logger().isLevelEnabled(LogLevel::Warn));
    }

    void test_defaultLevels_errorEnabled()
    {
        QVERIFY(logger().isLevelEnabled(LogLevel::Error));
    }

    void test_defaultLevels_debugDisabled()
    {
        QVERIFY(!logger().isLevelEnabled(LogLevel::Debug));
    }

    void test_defaultLevels_rawDisabledByDefault()
    {
        // RAW must never be enabled accidentally.
        QVERIFY(!logger().isLevelEnabled(LogLevel::Raw));
    }

    // --- Enable/disable levels ---

    void test_enableDisable_debugCanBeEnabled()
    {
        logger().setLevelEnabled(LogLevel::Debug, true);
        QVERIFY(logger().isLevelEnabled(LogLevel::Debug));
    }

    void test_enableDisable_infoCanBeDisabled()
    {
        logger().setLevelEnabled(LogLevel::Info, false);
        QVERIFY(!logger().isLevelEnabled(LogLevel::Info));
    }

    void test_enableDisable_disabledLevelDoesNotStore()
    {
        logger().setLevelEnabled(LogLevel::Debug, false);
        logger().log(LogLevel::Debug, LogCategory::App, "should not appear");
        QCOMPARE(logger().entryCount(), 0);
    }

    void test_enableDisable_enabledLevelStoresEntry()
    {
        logger().log(LogLevel::Info, LogCategory::App, "hello");
        QCOMPARE(logger().entryCount(), 1);
    }

    // --- RAW level ---

    void test_raw_notStoredWhenDisabled()
    {
        // RAW defaults to off — log call must be silently dropped.
        logger().raw(LogCategory::Sip, "raw dump", "0x00 0x01 0x02");
        QCOMPARE(logger().entryCount(), 0);
    }

    void test_raw_storedOnlyAfterExplicitEnable()
    {
        logger().setLevelEnabled(LogLevel::Raw, true);
        logger().raw(LogCategory::Sip, "raw dump", "0x00 0x01 0x02");
        QCOMPARE(logger().entryCount(), 1);
        QCOMPARE(logger().entries().first().level, LogLevel::Raw);
    }

    // --- Category filtering ---

    void test_category_filterReturnsSingleCategory()
    {
        logger().info(LogCategory::Sip,   "sip msg");
        logger().info(LogCategory::Media, "media msg");
        logger().warn(LogCategory::App,   "app warn");

        auto sipEntries = logger().entriesForCategory(LogCategory::Sip);
        QCOMPARE(sipEntries.size(), 1);
        QCOMPARE(sipEntries.first().category, LogCategory::Sip);
    }

    void test_category_filterEmptyWhenNoMatch()
    {
        logger().info(LogCategory::App, "app msg");
        auto rttEntries = logger().entriesForCategory(LogCategory::Rtt);
        QVERIFY(rttEntries.isEmpty());
    }

    void test_category_entriesForLevelFiltersCorrectly()
    {
        logger().info(LogCategory::App, "info msg");
        logger().warn(LogCategory::App, "warn msg");
        logger().error(LogCategory::App, "error msg");

        auto warns = logger().entriesForLevel(LogLevel::Warn);
        QCOMPARE(warns.size(), 1);
        QCOMPARE(warns.first().level, LogLevel::Warn);
    }

    // --- Clear ---

    void test_clear_removesAllEntries()
    {
        logger().info(LogCategory::App, "a");
        logger().info(LogCategory::App, "b");
        logger().info(LogCategory::App, "c");
        QCOMPARE(logger().entryCount(), 3);

        logger().clear();
        QCOMPARE(logger().entryCount(), 0);
    }

    void test_clear_loggingWorksAfterClear()
    {
        logger().info(LogCategory::App, "before");
        logger().clear();
        logger().info(LogCategory::App, "after");
        QCOMPARE(logger().entryCount(), 1);
    }

    // --- Redaction ---

    void test_redact_passwordEqualsValue()
    {
        logger().info(LogCategory::Sip, "auth: password=hunter2");
        const QString msg = logger().entries().first().message;
        QVERIFY(!msg.contains("hunter2"));
        QVERIFY(msg.contains("***"));
    }

    void test_redact_passwdColonValue()
    {
        logger().info(LogCategory::Sip, "passwd: s3cr3t");
        const QString msg = logger().entries().first().message;
        QVERIFY(!msg.contains("s3cr3t"));
        QVERIFY(msg.contains("***"));
    }

    void test_redact_secretKey()
    {
        logger().info(LogCategory::Sip, "secret=mysecretvalue");
        const QString msg = logger().entries().first().message;
        QVERIFY(!msg.contains("mysecretvalue"));
    }

    void test_redact_tokenValue()
    {
        logger().info(LogCategory::Sip, "token=Bearer xyz123abc");
        const QString msg = logger().entries().first().message;
        QVERIFY(!msg.contains("xyz123abc"));
    }

    void test_redact_authorizationHeader()
    {
        logger().info(LogCategory::Sip, "Authorization: Basic dXNlcjpwYXNz");
        const QString msg = logger().entries().first().message;
        QVERIFY(!msg.contains("dXNlcjpwYXNz"));
    }

    void test_redact_privateKey()
    {
        logger().info(LogCategory::Sip, "private key=SECRETKEYDATA");
        const QString msg = logger().entries().first().message;
        QVERIFY(!msg.contains("SECRETKEYDATA"));
    }

    void test_redact_caseInsensitive()
    {
        logger().info(LogCategory::Sip, "PASSWORD=TopSecret");
        const QString msg = logger().entries().first().message;
        QVERIFY(!msg.contains("TopSecret"));
    }

    void test_redact_payloadIsAlsoRedacted()
    {
        logger().info(LogCategory::Sip, "register", "auth=mysecret");
        const QString payload = logger().entries().first().payload;
        QVERIFY(!payload.contains("mysecret"));
        QVERIFY(payload.contains("***"));
    }

    void test_redact_safeMessageUnchanged()
    {
        logger().info(LogCategory::App, "connected to 192.168.1.1:5060");
        const QString msg = logger().entries().first().message;
        QCOMPARE(msg, QStringLiteral("connected to 192.168.1.1:5060"));
    }

    // --- Export ---

    void test_export_textIncludesAllEntries()
    {
        logger().info(LogCategory::App, "message one");
        logger().warn(LogCategory::Sip, "message two");
        const QString text = logger().exportAsText();
        QVERIFY(text.contains("message one"));
        QVERIFY(text.contains("message two"));
        QVERIFY(text.contains("INFO"));
        QVERIFY(text.contains("WARN"));
    }

    void test_export_jsonReadyHasRequiredKeys()
    {
        logger().info(LogCategory::App, "hello", "world");
        const auto list = logger().exportAsJsonReady();
        QCOMPARE(list.size(), 1);
        const QVariantMap &m = list.first();
        QVERIFY(m.contains("timestamp"));
        QVERIFY(m.contains("level"));
        QVERIFY(m.contains("category"));
        QVERIFY(m.contains("message"));
        QVERIFY(m.contains("payload"));
        QCOMPARE(m["level"].toString(), QStringLiteral("INFO"));
        QCOMPARE(m["category"].toString(), QStringLiteral("APP"));
    }

    void test_export_emptyWhenNoEntries()
    {
        QVERIFY(logger().exportAsText().isEmpty());
        QVERIFY(logger().exportAsJsonReady().isEmpty());
    }
};

QTEST_GUILESS_MAIN(DiagnosticsLoggerTests)
#include "DiagnosticsLoggerTests.moc"
