#include "MessageHistoryStore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QMutexLocker>
#include <QRegularExpression>

#include "core/AppSettings.h"

namespace {
// Duplicate inbound callback invocations for the same physical message
// (e.g. a defensive re-fire at the PJSIP layer) are only ever expected to
// land milliseconds apart, so a short window is enough to catch them
// without risking merging two genuinely different messages sent close
// together.
constexpr qint64 kDedupWindowMs = 2000;
} // namespace

MessageHistoryStore &MessageHistoryStore::instance()
{
    static MessageHistoryStore s;
    return s;
}

MessageHistoryStore::MessageHistoryStore(int maxEntriesRetained)
    : QObject(nullptr)
    , m_maxEntriesRetained(maxEntriesRetained > 0
          ? maxEntriesRetained : AppSettings::loadMaxMessagingEventsRetained())
{
    qRegisterMetaType<MessageHistoryEntry>("MessageHistoryEntry");
}

QString MessageHistoryStore::makePreview(const QString &body, int maxLen)
{
    QString collapsed = body;
    collapsed.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    collapsed = collapsed.trimmed();
    if (collapsed.size() > maxLen)
        return collapsed.left(maxLen) + QStringLiteral("…");
    return collapsed;
}

QString MessageHistoryStore::inboundFingerprint(const QString &fromUri, const QString &toUri,
                                                const QString &contentType, const QString &body,
                                                const QString &callId)
{
    // Call-ID alone would be enough for a well-behaved peer, but folding in
    // the other fields keeps the fingerprint meaningful even when Call-ID is
    // missing/empty (some synthetic or malformed traces omit it).
    const QString basis = fromUri + QLatin1Char('|') + toUri + QLatin1Char('|')
        + contentType + QLatin1Char('|') + body + QLatin1Char('|') + callId;
    return QString::fromLatin1(
        QCryptographicHash::hash(basis.toUtf8(), QCryptographicHash::Sha1).toHex());
}

qint64 MessageHistoryStore::appendInbound(const QString &fromUri, const QString &toUri,
                                          const QString &contactUri, const QString &contentType,
                                          const QString &body, const QString &callId,
                                          const QString &profileId)
{
    const QString fp = inboundFingerprint(fromUri, toUri, contentType, body, callId);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    MessageHistoryEntry entry;
    {
        QMutexLocker locker(&m_mutex);

        // Opportunistic prune of stale fingerprints so this map never grows
        // unbounded across a long-running session.
        for (auto it = m_recentInboundFingerprints.begin(); it != m_recentInboundFingerprints.end();) {
            if (nowMs - it.value() > kDedupWindowMs * 4)
                it = m_recentInboundFingerprints.erase(it);
            else
                ++it;
        }

        const auto seenIt = m_recentInboundFingerprints.constFind(fp);
        if (seenIt != m_recentInboundFingerprints.constEnd()
            && (nowMs - seenIt.value()) < kDedupWindowMs) {
            return m_recentInboundFingerprintToEntryId.value(fp, 0);
        }

        entry.id          = m_nextId++;
        entry.timestamp   = QDateTime::currentDateTimeUtc();
        entry.direction   = MessageHistoryEntry::Direction::Inbound;
        entry.peerUri     = fromUri;
        entry.contentType = contentType;
        entry.bodyPreview = makePreview(body);
        entry.callId      = callId;
        entry.contactUri  = contactUri;
        entry.profileId   = profileId;
        Q_UNUSED(toUri) // kept in the fingerprint; peerUri intentionally uses fromUri for inbound

        m_entries.append(entry);
        m_recentInboundFingerprints.insert(fp, nowMs);
        m_recentInboundFingerprintToEntryId.insert(fp, entry.id);

        while (m_entries.size() > m_maxEntriesRetained)
            m_entries.removeFirst();
    }

    emit entryAppended(entry);
    return entry.id;
}

qint64 MessageHistoryStore::appendOutbound(const ComposedSipMessage &msg)
{
    MessageHistoryEntry entry;
    {
        QMutexLocker locker(&m_mutex);
        entry.id             = m_nextId++;
        entry.timestamp      = QDateTime::currentDateTimeUtc();
        entry.direction      = MessageHistoryEntry::Direction::Outbound;
        entry.peerUri        = msg.toUri;
        entry.contentType    = msg.contentType;
        entry.bodyPreview    = makePreview(msg.body);
        entry.outboundStatus = MessageHistoryEntry::OutboundStatus::Queued;
        entry.callId         = msg.callId;
        entry.profileId      = QString(); // filled in by SipManager if/when known

        m_entries.append(entry);

        while (m_entries.size() > m_maxEntriesRetained)
            m_entries.removeFirst();
    }

    emit entryAppended(entry);
    return entry.id;
}

void MessageHistoryStore::updateOutboundStatus(qint64 id, MessageHistoryEntry::OutboundStatus status)
{
    MessageHistoryEntry updated;
    bool found = false;
    {
        QMutexLocker locker(&m_mutex);
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries.at(i).id == id) {
                m_entries[i].outboundStatus = status;
                updated = m_entries.at(i);
                found = true;
                break;
            }
        }
    }
    if (found)
        emit entryUpdated(updated);
}

void MessageHistoryStore::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_entries.clear();
        m_recentInboundFingerprints.clear();
        m_recentInboundFingerprintToEntryId.clear();
        m_nextId = 1;
    }
    emit cleared();
}

int MessageHistoryStore::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.size();
}

QList<MessageHistoryEntry> MessageHistoryStore::snapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_entries;
}

int MessageHistoryStore::maxEntriesRetained() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxEntriesRetained;
}

void MessageHistoryStore::setMaxEntriesRetained(int max)
{
    QMutexLocker locker(&m_mutex);
    m_maxEntriesRetained = max;
    while (m_entries.size() > m_maxEntriesRetained)
        m_entries.removeFirst();
}
