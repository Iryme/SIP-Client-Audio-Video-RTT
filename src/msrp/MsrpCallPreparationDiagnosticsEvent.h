#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

#include "msrp/MsrpCallPreparationController.h"

// One call-preparation diagnostic event (Task W108) — feeds the
// msrpCallPreparationEvents JSON export array. No credential/nonce/full
// path is ever stored here — see docs/msrp-relay-security.md, which this
// mirrors.
struct MsrpCallPreparationDiagnosticsEvent
{
    QDateTime timestamp;
    QString preparationId;
    MsrpCallPreparationController::State state{MsrpCallPreparationController::State::Idle};
    QString mode;              // "none" / "direct" / "relay" once resolved, else empty
    QString allocationId;      // internal correlation id only, once relay-allocated
    QString reason;            // failure/fallback reason text, never a credential
};

Q_DECLARE_METATYPE(MsrpCallPreparationDiagnosticsEvent)
