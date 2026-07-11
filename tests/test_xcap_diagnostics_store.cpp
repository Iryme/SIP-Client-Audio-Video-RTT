#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "sip/InteropTraceExporter.h"
#include "sip/XcapDiagnosticsStore.h"

class TestXcapDiagnosticsStore : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void recordAppendsEntry();
    void clearEmptiesEntries();
    void boundsRetainedEntries();
    void exportsXcapEventsJson();
    void networkErrorEntryHasNoStatusButHasError();
    void redactedUrlNeverContainsCredentials();
};

namespace {
XcapResult makeResult(const QString &auid, int status)
{
    XcapResult r;
    r.method = XcapHttpMethod::Get;
    r.rootUri = QStringLiteral("https://xcap.example.test/xcap-root");
    r.auid = auid;
    r.xui = QStringLiteral("sip:alice@example.test");
    r.documentSelector = QStringLiteral("index");
    r.urlRedacted = QStringLiteral("https://xcap.example.test/xcap-root/%1/users/…").arg(auid);
    r.httpStatus = status;
    r.contentType = QStringLiteral("application/xml");
    r.etag = QStringLiteral("\"abc123\"");
    r.timestamp = QDateTime::currentDateTimeUtc();
    r.durationMs = 42;
    r.parseStatus = XcapParseStatus::Ok;
    return r;
}
} // namespace

void TestXcapDiagnosticsStore::init()
{
    XcapDiagnosticsStore::instance().clear();
}

void TestXcapDiagnosticsStore::recordAppendsEntry()
{
    XcapDiagnosticsStore::instance().recordResult(makeResult(QStringLiteral("resource-lists"), 200));
    QCOMPARE(XcapDiagnosticsStore::instance().entries().size(), 1);
}

void TestXcapDiagnosticsStore::clearEmptiesEntries()
{
    XcapDiagnosticsStore::instance().recordResult(makeResult(QStringLiteral("resource-lists"), 200));
    XcapDiagnosticsStore::instance().clear();
    QVERIFY(XcapDiagnosticsStore::instance().entries().isEmpty());
}

void TestXcapDiagnosticsStore::boundsRetainedEntries()
{
    for (int i = 0; i < XcapDiagnosticsStore::kMaxRetainedEntries + 10; ++i)
        XcapDiagnosticsStore::instance().recordResult(makeResult(QStringLiteral("resource-lists"), 200));
    QCOMPARE(XcapDiagnosticsStore::instance().entries().size(), XcapDiagnosticsStore::kMaxRetainedEntries);
}

void TestXcapDiagnosticsStore::exportsXcapEventsJson()
{
    const XcapResult r = makeResult(QStringLiteral("pres-rules"), 200);
    const QString json = InteropTraceExporter::exportToJson({}, {}, {r});

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(doc.object().contains(QStringLiteral("xcapEvents")));

    const QJsonArray events = doc.object().value(QStringLiteral("xcapEvents")).toArray();
    QCOMPARE(events.size(), 1);
    const QJsonObject ev = events.first().toObject();
    QCOMPARE(ev.value(QStringLiteral("auid")).toString(), QStringLiteral("pres-rules"));
    QCOMPARE(ev.value(QStringLiteral("method")).toString(), QStringLiteral("GET"));
    QCOMPARE(ev.value(QStringLiteral("status")).toInt(), 200);
    QCOMPARE(ev.value(QStringLiteral("etag")).toString(), QStringLiteral("\"abc123\""));
    QVERIFY(!ev.value(QStringLiteral("urlRedacted")).toString().isEmpty());
}

void TestXcapDiagnosticsStore::networkErrorEntryHasNoStatusButHasError()
{
    XcapResult r = makeResult(QStringLiteral("resource-lists"), 0);
    r.networkError = true;
    r.errorString = QStringLiteral("Connection timed out");
    const QString json = InteropTraceExporter::exportToJson({}, {}, {r});
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    const QJsonObject ev = doc.object().value(QStringLiteral("xcapEvents")).toArray().first().toObject();
    QVERIFY(ev.value(QStringLiteral("networkError")).toBool());
    QCOMPARE(ev.value(QStringLiteral("errorString")).toString(), QStringLiteral("Connection timed out"));
}

void TestXcapDiagnosticsStore::redactedUrlNeverContainsCredentials()
{
    XcapResult r = makeResult(QStringLiteral("resource-lists"), 200);
    r.urlRedacted = QStringLiteral("https://xcap.example.test/xcap-root/resource-lists/users/…+deadbeef");
    const QString json = InteropTraceExporter::exportToJson({}, {}, {r});
    QVERIFY(!json.contains(QStringLiteral("password"), Qt::CaseInsensitive));
}

QTEST_GUILESS_MAIN(TestXcapDiagnosticsStore)
#include "test_xcap_diagnostics_store.moc"
