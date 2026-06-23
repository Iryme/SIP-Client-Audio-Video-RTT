#pragma once
#include <QList>
#include <QObject>

#include "sip/SipMessageTrace.h"

// Application-layer SIP signaling trace store.  Captures SIP messages
// (synthetic in stub mode, from PJSIP callbacks when HAVE_PJSIP), strips
// Authorization / Proxy-Authorization header values before storage, and
// re-emits each entry so SipLadderWidget can display the ladder diagram.
// Qt-only — no pjsua2.hpp included.
class SipTraceLogger : public QObject
{
    Q_OBJECT
public:
    static SipTraceLogger &instance();

    // Store trace (redacts credentials, sets timestamp if not already set).
    void logMessage(const SipMessageTrace &trace);

    const QList<SipMessageTrace> &messages() const;
    void clear();

    // Basic export helpers.
    QString exportToText() const;
    QString exportToJson() const;

signals:
    void messageLogged(const SipMessageTrace &trace);
    void cleared();

private:
    SipTraceLogger();

    static QString redactCredentials(const QString &raw);

    QList<SipMessageTrace> m_messages;
};
