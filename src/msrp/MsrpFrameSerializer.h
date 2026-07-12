#pragma once
#include <QByteArray>

#include "msrp/MsrpFrame.h"

// MSRP frame serialization (Task W100, section H). Produces exact
// CRLF/delimiter wire bytes; body is always copied byte-for-byte, never
// routed through QString. Header values are validated (no embedded
// CR/LF/NUL) before being written, so a caller can never inject an extra
// header/line via an unvalidated value.
namespace MsrpFrameSerializer {

// Returns true and rejects (returns empty QByteArray via *ok=false) if any
// header value would allow header/line injection (contains \r, \n, or a
// NUL byte).
QByteArray serialize(const MsrpFrame &frame, bool *ok = nullptr);

} // namespace MsrpFrameSerializer
