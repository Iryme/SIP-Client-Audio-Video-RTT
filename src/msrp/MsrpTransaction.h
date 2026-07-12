#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

#include "msrp/MsrpTypes.h"

// One outbound (or inbound) MSRP transaction — a SEND or REPORT request and
// its eventual response/report correlation (Task W100, section J).
struct MsrpTransaction
{
    QString transactionId;
    QString sessionKey;
    MsrpMethod method{MsrpMethod::Unknown};
    QString messageId;
    bool outbound{true};

    MsrpTransactionStatus status{MsrpTransactionStatus::Queued};
    int responseCode{0};
    QString responseComment;
    QString reportStatus;   // REPORT's Status header, when applicable

    QDateTime createdAt;
    QDateTime updatedAt;
};

Q_DECLARE_METATYPE(MsrpTransaction)
