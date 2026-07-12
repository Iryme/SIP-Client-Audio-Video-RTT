#pragma once
#include <QHash>
#include <QObject>

#include "msrp/MsrpSessionInfo.h"

// Tracks all known MSRP sessions (Task W100, section Q) — one entry per
// sessionKey, from first detection through close. Sole writer is whatever
// drives negotiation/transport (MsrpSession / the SIP integration hook);
// the UI only ever reads via snapshot()/signals, mirroring PresenceStore's
// established pattern (Task W098).
class MsrpSessionStore : public QObject
{
    Q_OBJECT
public:
    static MsrpSessionStore &instance();

    void upsert(const MsrpSessionInfo &info);
    MsrpSessionInfo get(const QString &sessionKey) const;
    QList<MsrpSessionInfo> snapshot() const;
    void remove(const QString &sessionKey);
    void clear();

    static constexpr int kMaxConcurrentSessionsHardCap = 64;

signals:
    void sessionUpdated(const MsrpSessionInfo &info);
    void sessionRemoved(const QString &sessionKey);
    void cleared();

private:
    MsrpSessionStore();

    QHash<QString, MsrpSessionInfo> m_sessions;
};
