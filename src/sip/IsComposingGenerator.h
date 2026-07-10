#pragma once
#include <QString>

#include "sip/IsComposingInfo.h"

// Builds RFC 3994 (application/im-iscomposing+xml) notification bodies.
// Pure Qt/text — no PJSIP or network dependency — so it is fully
// unit-testable.
class IsComposingGenerator
{
public:
    // state must be Active/Idle/Gone (Unknown is rejected — returns an
    // empty string). refreshSeconds <= 0 omits the <refresh> element
    // (meaningful only for Active, per RFC 3994 §5, but left to the caller
    // to decide — this is a pure builder). contentType empty omits
    // <contenttype>.
    static QString generate(IsComposingInfo::State state,
                            int refreshSeconds = 0,
                            const QString &contentType = QString());
};
