#include <QtTest/QtTest>
#include "diagnostics/LogFilterModel.h"
#include "diagnostics/DiagnosticsLogger.h"

// Helpers to build LogEntry without going through the singleton
static LogEntry makeEntry(LogLevel level, LogCategory category,
                          const QString &message, const QString &payload = {})
{
    LogEntry e;
    e.timestamp = QDateTime::currentDateTime();
    e.level     = level;
    e.category  = category;
    e.message   = message;
    e.payload   = payload;
    return e;
}

class LogFilterModelTests : public QObject
{
    Q_OBJECT

private:
    LogFilterModel *m_model{nullptr};

private slots:
    void init()
    {
        m_model = new LogFilterModel();
    }

    void cleanup()
    {
        delete m_model;
        m_model = nullptr;
    }

    // --- Default toggle states ---

    void test_defaults_infoVisible()
    {
        QVERIFY(m_model->isLevelVisible(LogLevel::Info));
    }

    void test_defaults_warnVisible()
    {
        QVERIFY(m_model->isLevelVisible(LogLevel::Warn));
    }

    void test_defaults_errorVisible()
    {
        QVERIFY(m_model->isLevelVisible(LogLevel::Error));
    }

    void test_defaults_debugHidden()
    {
        QVERIFY(!m_model->isLevelVisible(LogLevel::Debug));
    }

    void test_defaults_rawHidden()
    {
        // RAW must never be visible by default.
        QVERIFY(!m_model->isLevelVisible(LogLevel::Raw));
        QVERIFY(!m_model->isRawVisible());
    }

    void test_defaults_categoryAll()
    {
        QCOMPARE(m_model->categoryFilter(), QStringLiteral("All"));
    }

    void test_defaults_searchEmpty()
    {
        QVERIFY(m_model->searchText().isEmpty());
    }

    // --- Level visibility filter ---

    void test_levelFilter_infoPassesWhenVisible()
    {
        auto e = makeEntry(LogLevel::Info, LogCategory::App, "hello");
        QVERIFY(m_model->matchesFilter(e));
    }

    void test_levelFilter_infoBlockedWhenHidden()
    {
        m_model->setLevelVisible(LogLevel::Info, false);
        auto e = makeEntry(LogLevel::Info, LogCategory::App, "hello");
        QVERIFY(!m_model->matchesFilter(e));
    }

    void test_levelFilter_debugBlockedByDefault()
    {
        auto e = makeEntry(LogLevel::Debug, LogCategory::App, "debug msg");
        QVERIFY(!m_model->matchesFilter(e));
    }

    void test_levelFilter_debugPassesAfterEnable()
    {
        m_model->setLevelVisible(LogLevel::Debug, true);
        auto e = makeEntry(LogLevel::Debug, LogCategory::App, "debug msg");
        QVERIFY(m_model->matchesFilter(e));
    }

    // --- RAW visibility ---

    void test_raw_blockedByDefault()
    {
        auto e = makeEntry(LogLevel::Raw, LogCategory::Sip, "raw dump", "0x00");
        QVERIFY(!m_model->matchesFilter(e));
    }

    void test_raw_passesAfterSetRawVisible()
    {
        m_model->setRawVisible(true);
        auto e = makeEntry(LogLevel::Raw, LogCategory::Sip, "raw dump", "0x00");
        QVERIFY(m_model->matchesFilter(e));
    }

    void test_raw_setRawVisibleAlsoSetsLevelVisible()
    {
        m_model->setRawVisible(true);
        QVERIFY(m_model->isLevelVisible(LogLevel::Raw));
    }

    void test_raw_disableRawHidesRawEntries()
    {
        m_model->setRawVisible(true);
        m_model->setRawVisible(false);
        auto e = makeEntry(LogLevel::Raw, LogCategory::Sip, "raw dump");
        QVERIFY(!m_model->matchesFilter(e));
    }

    // --- Category filter ---

    void test_category_allPassesEverything()
    {
        m_model->setCategoryFilter("All");
        QVERIFY(m_model->matchesFilter(makeEntry(LogLevel::Info, LogCategory::Sip,      "sip")));
        QVERIFY(m_model->matchesFilter(makeEntry(LogLevel::Info, LogCategory::Media,    "media")));
        QVERIFY(m_model->matchesFilter(makeEntry(LogLevel::Info, LogCategory::Platform, "platform")));
    }

    void test_category_specificFilterPassesMatchingCategory()
    {
        m_model->setCategoryFilter("SIP");
        auto e = makeEntry(LogLevel::Info, LogCategory::Sip, "sip msg");
        QVERIFY(m_model->matchesFilter(e));
    }

    void test_category_specificFilterBlocksNonMatching()
    {
        m_model->setCategoryFilter("SIP");
        auto e = makeEntry(LogLevel::Info, LogCategory::App, "app msg");
        QVERIFY(!m_model->matchesFilter(e));
    }

    void test_category_filteredEntriesCountMatchesFilter()
    {
        m_model->addEntry(makeEntry(LogLevel::Info, LogCategory::Sip,  "sip 1"));
        m_model->addEntry(makeEntry(LogLevel::Info, LogCategory::Sip,  "sip 2"));
        m_model->addEntry(makeEntry(LogLevel::Info, LogCategory::App,  "app 1"));
        m_model->addEntry(makeEntry(LogLevel::Warn, LogCategory::Media,"media warn"));

        m_model->setCategoryFilter("SIP");
        QCOMPARE(m_model->filteredEntries().size(), 2);
    }

    // --- Search text ---

    void test_search_emptyMatchesAll()
    {
        m_model->setSearchText("");
        auto e = makeEntry(LogLevel::Info, LogCategory::App, "anything");
        QVERIFY(m_model->matchesFilter(e));
    }

    void test_search_matchesMessageCaseInsensitive()
    {
        m_model->setSearchText("hello");
        auto e = makeEntry(LogLevel::Info, LogCategory::App, "Hello World");
        QVERIFY(m_model->matchesFilter(e));
    }

    void test_search_matchesPayload()
    {
        m_model->setRawVisible(true);
        m_model->setSearchText("0xdeadbeef");
        auto e = makeEntry(LogLevel::Raw, LogCategory::Sip, "raw dump", "0xDEADBEEF");
        QVERIFY(m_model->matchesFilter(e));
    }

    void test_search_noMatchBlocks()
    {
        m_model->setSearchText("xyz_not_present");
        auto e = makeEntry(LogLevel::Info, LogCategory::App, "completely different text");
        QVERIFY(!m_model->matchesFilter(e));
    }

    // --- Combined filters ---

    void test_combined_categoryAndSearchBothApply()
    {
        m_model->setCategoryFilter("APP");
        m_model->setSearchText("start");

        m_model->addEntry(makeEntry(LogLevel::Info, LogCategory::App, "Application started"));
        m_model->addEntry(makeEntry(LogLevel::Info, LogCategory::App, "Application stopped"));
        m_model->addEntry(makeEntry(LogLevel::Info, LogCategory::Sip, "SIP started"));

        // Only APP category + "start" in message
        const auto filtered = m_model->filteredEntries();
        QCOMPARE(filtered.size(), 1);
        QVERIFY(filtered.first().message.contains("started"));
    }

    // --- Entry management ---

    void test_clear_removesAllEntries()
    {
        m_model->addEntry(makeEntry(LogLevel::Info, LogCategory::App, "a"));
        m_model->addEntry(makeEntry(LogLevel::Info, LogCategory::App, "b"));
        m_model->clear();
        QVERIFY(m_model->allEntries().isEmpty());
        QVERIFY(m_model->filteredEntries().isEmpty());
    }

    void test_filterChanged_emittedOnLevelChange()
    {
        QSignalSpy spy(m_model, &LogFilterModel::filterChanged);
        m_model->setLevelVisible(LogLevel::Debug, true);
        QCOMPARE(spy.count(), 1);
    }

    void test_filterChanged_emittedOnCategoryChange()
    {
        QSignalSpy spy(m_model, &LogFilterModel::filterChanged);
        m_model->setCategoryFilter("SIP");
        QCOMPARE(spy.count(), 1);
    }

    void test_filterChanged_emittedOnSearchChange()
    {
        QSignalSpy spy(m_model, &LogFilterModel::filterChanged);
        m_model->setSearchText("test");
        QCOMPARE(spy.count(), 1);
    }

    void test_filterChanged_notEmittedOnSameValue()
    {
        m_model->setCategoryFilter("SIP");
        QSignalSpy spy(m_model, &LogFilterModel::filterChanged);
        m_model->setCategoryFilter("SIP"); // same value
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(LogFilterModelTests)
#include "LogFilterModelTests.moc"
