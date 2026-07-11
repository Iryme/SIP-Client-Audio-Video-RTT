#pragma once
#include <QString>
#include <QStringList>

// XCAP XML validation (Task W099, requirement 4): validates a document body
// before it is ever sent as a PUT, using QXmlStreamReader only — no DOM, no
// external entity/DTD resolution, no network access. Also used to classify
// GET response bodies for diagnostics (parseStatus/warnings), independent of
// any AUID-specific schema (none is implemented at this stage).
namespace XcapXmlValidator {

struct Result
{
    bool ok{false};
    bool wellFormed{false};
    bool empty{false};
    QStringList warnings;
    QString errorMessage;
};

// allowEmpty: some operations (e.g. a HEAD/GET result classification) may
// tolerate an empty body; PUT bodies never do (empty PUT bodies are always
// rejected regardless of this flag having no effect there — callers pass
// allowEmpty=false for PUT validation).
Result validate(const QString &xml, bool allowEmpty = false);

} // namespace XcapXmlValidator
