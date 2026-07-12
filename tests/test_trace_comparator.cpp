#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonObject>

#include "sip/TraceComparator.h"

namespace {
QJsonObject ev(const QString &callId, int cseq, const QString &direction,
               const QString &payloadType, const QString &timestamp)
{
    QJsonObject o;
    o[QStringLiteral("callId")] = callId;
    o[QStringLiteral("cseq")] = cseq;
    o[QStringLiteral("direction")] = direction;
    o[QStringLiteral("payloadType")] = payloadType;
    o[QStringLiteral("timestamp")] = timestamp;
    return o;
}

QJsonObject trace(const QList<QJsonObject> &events)
{
    QJsonArray arr;
    for (const auto &e : events) arr.append(e);
    QJsonObject root;
    root[QStringLiteral("events")] = arr;
    return root;
}
} // namespace

class TestTraceComparator : public QObject
{
    Q_OBJECT

private slots:
    void identicalTracesProduceNoMismatches()
    {
        const auto left = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });
        const auto right = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("inbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.500Z")),
        });

        const QJsonObject result = TraceComparator::compareTraces(left, right);
        QCOMPARE(result[QStringLiteral("summary")].toObject()
            .value(QStringLiteral("correlatedCount")).toInt(), 1);
        QCOMPARE(result[QStringLiteral("mismatches")].toArray().size(), 0);
    }

    void detectsMissingOnRight()
    {
        const auto left = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });
        const auto right = trace({});

        const QJsonObject result = TraceComparator::compareTraces(left, right);
        const QJsonArray mismatches = result[QStringLiteral("mismatches")].toArray();
        QCOMPARE(mismatches.size(), 1);
        QCOMPARE(mismatches.first().toObject().value(QStringLiteral("kind")).toString(),
                  QStringLiteral("missingOnRight"));
    }

    void detectsMissingOnLeft()
    {
        const auto left = trace({});
        const auto right = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("inbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });

        const QJsonObject result = TraceComparator::compareTraces(left, right);
        const QJsonArray mismatches = result[QStringLiteral("mismatches")].toArray();
        QCOMPARE(mismatches.size(), 1);
        QCOMPARE(mismatches.first().toObject().value(QStringLiteral("kind")).toString(),
                  QStringLiteral("missingOnLeft"));
    }

    void detectsDuplicateOnLeft()
    {
        const auto left = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });
        const auto right = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("inbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
            ev(QStringLiteral("call1"), 1, QStringLiteral("inbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });

        const QJsonObject result = TraceComparator::compareTraces(left, right);
        const QJsonArray mismatches = result[QStringLiteral("mismatches")].toArray();
        bool foundDuplicate = false;
        for (const auto &m : mismatches) {
            if (m.toObject().value(QStringLiteral("kind")).toString() == QStringLiteral("duplicate"))
                foundDuplicate = true;
        }
        QVERIFY(foundDuplicate);
    }

    void detectsDirectionMismatch()
    {
        const auto left = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });
        // Right side should be "inbound" (mirrored); it isn't.
        const auto right = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });

        const QJsonObject result = TraceComparator::compareTraces(left, right);
        const QJsonArray mismatches = result[QStringLiteral("mismatches")].toArray();
        QCOMPARE(mismatches.size(), 1);
        QCOMPARE(mismatches.first().toObject().value(QStringLiteral("kind")).toString(),
                  QStringLiteral("directionMismatch"));
    }

    void detectsPayloadTypeMismatch()
    {
        const auto left = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });
        const auto right = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("inbound"),
               QStringLiteral("imdn"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });

        const QJsonObject result = TraceComparator::compareTraces(left, right);
        const QJsonArray mismatches = result[QStringLiteral("mismatches")].toArray();
        QCOMPARE(mismatches.size(), 1);
        QCOMPARE(mismatches.first().toObject().value(QStringLiteral("kind")).toString(),
                  QStringLiteral("payloadTypeMismatch"));
    }

    void detectsTimestampOutsideTolerance()
    {
        const auto left = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });
        const auto right = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("inbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:01:00.000Z")), // +60s
        });

        const QJsonObject result = TraceComparator::compareTraces(left, right, /*toleranceMs=*/5000);
        const QJsonArray mismatches = result[QStringLiteral("mismatches")].toArray();
        QCOMPARE(mismatches.size(), 1);
        QCOMPARE(mismatches.first().toObject().value(QStringLiteral("kind")).toString(),
                  QStringLiteral("timestampOutOfTolerance"));
    }

    void isDeterministicAcrossRepeatedCalls()
    {
        const auto left = trace({
            ev(QStringLiteral("call1"), 1, QStringLiteral("outbound"),
               QStringLiteral("text-plain"), QStringLiteral("2026-01-01T10:00:00.000Z")),
        });
        const auto right = trace({});

        const QJsonObject r1 = TraceComparator::compareTraces(left, right);
        const QJsonObject r2 = TraceComparator::compareTraces(left, right);
        QCOMPARE(r1, r2);
    }
};

QTEST_APPLESS_MAIN(TestTraceComparator)
#include "test_trace_comparator.moc"
