#pragma once
#include <QString>

#include "sip/RcsFtHttpInfo.h"

// Safe, tolerant, read-only parser for RCS FT HTTP file-transfer descriptor
// bodies (application/vnd.gsma.rcs-ft-http+xml). Pure Qt/text-based
// (QXmlStreamReader, non-validating) — never touches the network, never
// resolves the referenced URL, never opens/saves/displays the referenced
// file. A malformed or incomplete document simply yields a partially
// populated (or absent, RcsFtHttpInfo::present == false) result; it never
// throws or aborts the caller.
class RcsFtHttpParser
{
public:
    static RcsFtHttpInfo parse(const QString &body);
};
