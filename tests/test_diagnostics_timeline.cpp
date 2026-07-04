#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/DiagnosticsTimelineEntry.h"
#include "core/DiagnosticsTimelineExporter.h"
#include "core/DiagnosticsTimelineFilterProxyModel.h"
#include "core/DiagnosticsTimelineModel.h"

namespace {
DiagnosticsTimelineEntry makeEntry(const QString &id, TimelineCategory category,
                                    TimelineSeverity severity, const QString &title,
                                    const QString &details = {})
{
    DiagnosticsTimelineEntry e;
    e.id = id;
    e.timestamp = QDateTime::currentDateTime();
    e.category = category;
    e.severity = severity;
    e.title = title;
    e.details = details;
    e.colorHint = timelineSeverityColor(severity);
    e.iconHint = timelineCategoryName(category).left(4).toUpper();
    return e;
}
}

class TestDiagnosticsTimeline : public QObject
{
    Q_OBJECT

private slots:
    void appendKeepsOrder();
    void appendRemovesOldestAtCap();
    void searchFilter();
    void categoryFilter();
    void jsonRoundTrip();
    void txtExport();
    void jsonExportString();
};

void TestDiagnosticsTimeline::appendKeepsOrder()
{
    DiagnosticsTimelineModel model;
    QCOMPARE(model.count(), 0);

    model.append(makeEntry(QStringLiteral("1"), TimelineCategory::Call, TimelineSeverity::Info, QStringLiteral("first")));
    model.append(makeEntry(QStringLiteral("2"), TimelineCategory::Call, TimelineSeverity::Info, QStringLiteral("second")));
    model.append(makeEntry(QStringLiteral("3"), TimelineCategory::Call, TimelineSeverity::Info, QStringLiteral("third")));

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.entryAt(0).id, QStringLiteral("1"));
    QCOMPARE(model.entryAt(2).id, QStringLiteral("3"));
}

void TestDiagnosticsTimeline::appendRemovesOldestAtCap()
{
    DiagnosticsTimelineModel model;

    const int total = DiagnosticsTimelineModel::kMaxEntries + 5;
    for (int i = 0; i < total; ++i)
        model.append(makeEntry(QString::number(i), TimelineCategory::System, TimelineSeverity::Info, QStringLiteral("e%1").arg(i)));

    QCOMPARE(model.count(), DiagnosticsTimelineModel::kMaxEntries);
    // The first 5 entries (0..4) should have been dropped as the oldest.
    QCOMPARE(model.entryAt(0).id, QStringLiteral("5"));
    QCOMPARE(model.entryAt(model.count() - 1).id, QString::number(total - 1));
}

void TestDiagnosticsTimeline::searchFilter()
{
    DiagnosticsTimelineModel model;
    model.append(makeEntry(QStringLiteral("1"), TimelineCategory::Registration, TimelineSeverity::Success,
                            QStringLiteral("Registration: Registered"), QStringLiteral("OK")));
    model.append(makeEntry(QStringLiteral("2"), TimelineCategory::Call, TimelineSeverity::Error,
                            QStringLiteral("Call failed"), QStringLiteral("Busy Here")));

    DiagnosticsTimelineFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setSearchText(QStringLiteral("busy"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(DiagnosticsTimelineModel::IdRole).toString(), QStringLiteral("2"));

    proxy.setSearchText(QString());
    QCOMPARE(proxy.rowCount(), 2);
}

void TestDiagnosticsTimeline::categoryFilter()
{
    DiagnosticsTimelineModel model;
    model.append(makeEntry(QStringLiteral("1"), TimelineCategory::Rtp, TimelineSeverity::Warning, QStringLiteral("RTP quality degraded")));
    model.append(makeEntry(QStringLiteral("2"), TimelineCategory::Call, TimelineSeverity::Error, QStringLiteral("Call failed")));
    model.append(makeEntry(QStringLiteral("3"), TimelineCategory::Camera, TimelineSeverity::Info, QStringLiteral("Camera enabled")));

    DiagnosticsTimelineFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setCategoryFilter(DiagnosticsTimelineFilterProxyModel::CategoryFilter::Warnings);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(DiagnosticsTimelineModel::IdRole).toString(), QStringLiteral("1"));

    proxy.setCategoryFilter(DiagnosticsTimelineFilterProxyModel::CategoryFilter::Errors);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(DiagnosticsTimelineModel::IdRole).toString(), QStringLiteral("2"));

    proxy.setCategoryFilter(DiagnosticsTimelineFilterProxyModel::CategoryFilter::Camera);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(DiagnosticsTimelineModel::IdRole).toString(), QStringLiteral("3"));

    proxy.setCategoryFilter(DiagnosticsTimelineFilterProxyModel::CategoryFilter::All);
    QCOMPARE(proxy.rowCount(), 3);
}

void TestDiagnosticsTimeline::jsonRoundTrip()
{
    DiagnosticsTimelineEntry e = makeEntry(QStringLiteral("abc-123"), TimelineCategory::Registration,
                                            TimelineSeverity::Error, QStringLiteral("Registration: RegistrationFailed"),
                                            QStringLiteral("Forbidden"));
    e.remoteUri = QStringLiteral("sip:bob@example.com");
    e.sipCode = 403;
    e.profileId = QStringLiteral("profile-1");
    e.callId = QStringLiteral("call-1");

    const QJsonObject json = e.toJson();
    const DiagnosticsTimelineEntry roundTripped = DiagnosticsTimelineEntry::fromJson(json);

    QCOMPARE(roundTripped.id, e.id);
    QCOMPARE(roundTripped.category, e.category);
    QCOMPARE(roundTripped.severity, e.severity);
    QCOMPARE(roundTripped.title, e.title);
    QCOMPARE(roundTripped.details, e.details);
    QCOMPARE(roundTripped.remoteUri, e.remoteUri);
    QCOMPARE(roundTripped.sipCode, e.sipCode);
    QCOMPARE(roundTripped.profileId, e.profileId);
    QCOMPARE(roundTripped.callId, e.callId);
    QCOMPARE(roundTripped.colorHint, e.colorHint);
    QCOMPARE(roundTripped.iconHint, e.iconHint);
}

void TestDiagnosticsTimeline::txtExport()
{
    QList<DiagnosticsTimelineEntry> entries;
    entries.append(makeEntry(QStringLiteral("1"), TimelineCategory::Call, TimelineSeverity::Success,
                              QStringLiteral("Call connected")));
    DiagnosticsTimelineEntry e2 = makeEntry(QStringLiteral("2"), TimelineCategory::Call, TimelineSeverity::Error,
                                             QStringLiteral("Call failed"), QStringLiteral("Busy Here"));
    e2.sipCode = 486;
    e2.remoteUri = QStringLiteral("sip:alice@example.com");
    entries.append(e2);

    const QString txt = DiagnosticsTimelineExporter::toTxtString(entries);
    QVERIFY(txt.contains(QStringLiteral("Call connected")));
    QVERIFY(txt.contains(QStringLiteral("Call failed")));
    QVERIFY(txt.contains(QStringLiteral("Busy Here")));
    QVERIFY(txt.contains(QStringLiteral("sip=486")));
    QVERIFY(txt.contains(QStringLiteral("uri=sip:alice@example.com")));

    const QString path = QDir::temp().filePath(QStringLiteral("test_diagnostics_timeline_export.txt"));
    QString error;
    QVERIFY(DiagnosticsTimelineExporter::exportTxtToFile(entries, path, &error));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(f.readAll()), txt);
    f.close();
    QFile::remove(path);
}

void TestDiagnosticsTimeline::jsonExportString()
{
    QList<DiagnosticsTimelineEntry> entries;
    entries.append(makeEntry(QStringLiteral("1"), TimelineCategory::Rtt, TimelineSeverity::Info, QStringLiteral("RTT: Active")));
    entries.append(makeEntry(QStringLiteral("2"), TimelineCategory::Video, TimelineSeverity::Info, QStringLiteral("Local video started")));

    const QString json = DiagnosticsTimelineExporter::toJsonString(entries);
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QVERIFY(doc.isObject());
    const QJsonArray arr = doc.object().value(QStringLiteral("entries")).toArray();
    QCOMPARE(arr.size(), 2);
    QCOMPARE(arr.at(0).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("1"));
}

QTEST_GUILESS_MAIN(TestDiagnosticsTimeline)
#include "test_diagnostics_timeline.moc"
