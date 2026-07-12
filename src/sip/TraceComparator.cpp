#include "TraceComparator.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QMultiHash>

namespace TraceComparator {

namespace {

QString correlationKey(const QJsonObject &ev)
{
    return ev.value(QStringLiteral("callId")).toString()
         + QLatin1Char('|')
         + QString::number(ev.value(QStringLiteral("cseq")).toInt());
}

QString oppositeDirection(const QString &d)
{
    if (d == QLatin1String("outbound")) return QStringLiteral("inbound");
    if (d == QLatin1String("inbound"))  return QStringLiteral("outbound");
    return QString();
}

} // namespace

QJsonObject compareTraces(const QJsonObject &left, const QJsonObject &right, int timestampToleranceMs)
{
    QJsonArray mismatches;
    int correlatedCount = 0;

    const QJsonArray leftEvents = left.value(QStringLiteral("events")).toArray();
    const QJsonArray rightEvents = right.value(QStringLiteral("events")).toArray();

    QMultiHash<QString, QJsonObject> rightByKey;
    for (const QJsonValue &v : rightEvents)
        rightByKey.insert(correlationKey(v.toObject()), v.toObject());

    QHash<QString, int> leftKeyCount;
    for (const QJsonValue &v : leftEvents)
        leftKeyCount[correlationKey(v.toObject())]++;

    QHash<QString, int> consumedFromRight;

    for (const QJsonValue &lv : leftEvents) {
        const QJsonObject le = lv.toObject();
        const QString key = correlationKey(le);

        if (leftKeyCount.value(key) > 1) {
            QJsonObject m;
            m[QStringLiteral("kind")] = QStringLiteral("duplicate");
            m[QStringLiteral("side")] = QStringLiteral("left");
            m[QStringLiteral("callId")] = le.value(QStringLiteral("callId"));
            m[QStringLiteral("cseq")] = le.value(QStringLiteral("cseq"));
            mismatches.append(m);
        }

        const auto candidates = rightByKey.values(key);
        const int already = consumedFromRight.value(key);
        if (already >= candidates.size()) {
            QJsonObject m;
            m[QStringLiteral("kind")] = QStringLiteral("missingOnRight");
            m[QStringLiteral("callId")] = le.value(QStringLiteral("callId"));
            m[QStringLiteral("cseq")] = le.value(QStringLiteral("cseq"));
            m[QStringLiteral("direction")] = le.value(QStringLiteral("direction"));
            mismatches.append(m);
            continue;
        }

        const QJsonObject re = candidates.at(candidates.size() - 1 - already);
        consumedFromRight[key] = already + 1;
        correlatedCount++;

        const QString leftDir = le.value(QStringLiteral("direction")).toString();
        const QString rightDir = re.value(QStringLiteral("direction")).toString();
        if (rightDir != oppositeDirection(leftDir)) {
            QJsonObject m;
            m[QStringLiteral("kind")] = QStringLiteral("directionMismatch");
            m[QStringLiteral("callId")] = le.value(QStringLiteral("callId"));
            m[QStringLiteral("cseq")] = le.value(QStringLiteral("cseq"));
            m[QStringLiteral("leftDirection")] = leftDir;
            m[QStringLiteral("rightDirection")] = rightDir;
            mismatches.append(m);
        }

        const QString leftPayload = le.value(QStringLiteral("payloadType")).toString();
        const QString rightPayload = re.value(QStringLiteral("payloadType")).toString();
        if (!leftPayload.isEmpty() && !rightPayload.isEmpty() && leftPayload != rightPayload) {
            QJsonObject m;
            m[QStringLiteral("kind")] = QStringLiteral("payloadTypeMismatch");
            m[QStringLiteral("callId")] = le.value(QStringLiteral("callId"));
            m[QStringLiteral("cseq")] = le.value(QStringLiteral("cseq"));
            m[QStringLiteral("leftPayloadType")] = leftPayload;
            m[QStringLiteral("rightPayloadType")] = rightPayload;
            mismatches.append(m);
        }

        const QDateTime leftTs = QDateTime::fromString(
            le.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
        const QDateTime rightTs = QDateTime::fromString(
            re.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
        if (leftTs.isValid() && rightTs.isValid()) {
            const qint64 diffMs = qAbs(leftTs.msecsTo(rightTs));
            if (diffMs > timestampToleranceMs) {
                QJsonObject m;
                m[QStringLiteral("kind")] = QStringLiteral("timestampOutOfTolerance");
                m[QStringLiteral("callId")] = le.value(QStringLiteral("callId"));
                m[QStringLiteral("cseq")] = le.value(QStringLiteral("cseq"));
                m[QStringLiteral("differenceMs")] = diffMs;
                mismatches.append(m);
            }
        }
    }

    // Anything on the right never consumed is missing on the left. Reported
    // once per key (not once per duplicate right-side entry), using a
    // call-local (not function-static) set so this stays deterministic and
    // free of cross-call state.
    QHash<QString, bool> reportedMissingOnLeft;
    for (const QJsonValue &rv : rightEvents) {
        const QJsonObject re = rv.toObject();
        const QString key = correlationKey(re);
        const int totalForKey = rightByKey.values(key).size();
        const int consumed = consumedFromRight.value(key);
        if (consumed < totalForKey && !reportedMissingOnLeft.value(key)) {
            reportedMissingOnLeft[key] = true;
            for (int i = consumed; i < totalForKey; ++i) {
                QJsonObject m;
                m[QStringLiteral("kind")] = QStringLiteral("missingOnLeft");
                m[QStringLiteral("callId")] = re.value(QStringLiteral("callId"));
                m[QStringLiteral("cseq")] = re.value(QStringLiteral("cseq"));
                m[QStringLiteral("direction")] = re.value(QStringLiteral("direction"));
                mismatches.append(m);
            }
        }
    }

    QJsonObject summary;
    summary[QStringLiteral("leftEventCount")] = leftEvents.size();
    summary[QStringLiteral("rightEventCount")] = rightEvents.size();
    summary[QStringLiteral("correlatedCount")] = correlatedCount;
    summary[QStringLiteral("mismatchCount")] = mismatches.size();
    summary[QStringLiteral("deterministic")] = true;

    QJsonObject result;
    result[QStringLiteral("summary")] = summary;
    result[QStringLiteral("mismatches")] = mismatches;
    return result;
}

} // namespace TraceComparator
