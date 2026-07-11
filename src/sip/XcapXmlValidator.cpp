#include "XcapXmlValidator.h"

#include <QRegularExpression>
#include <QXmlStreamReader>

namespace XcapXmlValidator {

namespace {

const QStringList &knownEncodings()
{
    static const QStringList kKnown = {
        QStringLiteral("utf-8"), QStringLiteral("utf-16"),
        QStringLiteral("us-ascii"), QStringLiteral("iso-8859-1")
    };
    return kKnown;
}

// QXmlStreamReader itself never fetches external DTD subsets/entities (it
// has no such support), but a DOCTYPE declaration is rejected outright here
// as defense in depth — this client must never even attempt to interpret
// one, regardless of parser internals.
bool containsDoctype(const QString &xml)
{
    static const QRegularExpression re(
        QStringLiteral("<!DOCTYPE"), QRegularExpression::CaseInsensitiveOption);
    return re.match(xml).hasMatch();
}

QString declaredEncoding(const QString &xml)
{
    static const QRegularExpression re(
        QStringLiteral("<\\?xml[^>]*\\bencoding\\s*=\\s*['\"]([^'\"]+)['\"]"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(xml);
    return m.hasMatch() ? m.captured(1) : QString();
}

} // namespace

Result validate(const QString &xml, bool allowEmpty)
{
    Result result;

    const QString trimmed = xml.trimmed();
    if (trimmed.isEmpty()) {
        result.empty = true;
        result.ok = allowEmpty;
        if (!allowEmpty)
            result.errorMessage = QStringLiteral("document is empty");
        return result;
    }

    if (containsDoctype(trimmed)) {
        result.errorMessage = QStringLiteral("DOCTYPE/external DTD declarations are not permitted");
        return result;
    }

    const QString encoding = declaredEncoding(trimmed);
    if (!encoding.isEmpty() && !knownEncodings().contains(encoding.toLower())) {
        result.errorMessage = QStringLiteral("unknown encoding declared: %1").arg(encoding);
        return result;
    }

    QXmlStreamReader reader(trimmed);
    int depth = 0;
    bool sawRootElement = false;

    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType tok = reader.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            ++depth;
            sawRootElement = true;
        } else if (tok == QXmlStreamReader::EndElement) {
            --depth;
        } else if (tok == QXmlStreamReader::Invalid) {
            break;
        }
    }

    if (reader.hasError()) {
        result.errorMessage = QStringLiteral("XML parse error at line %1, column %2: %3")
            .arg(reader.lineNumber()).arg(reader.columnNumber()).arg(reader.errorString());
        return result;
    }

    if (!sawRootElement) {
        result.errorMessage = QStringLiteral("no root element found");
        return result;
    }
    if (depth != 0) {
        result.errorMessage = QStringLiteral("unbalanced elements");
        return result;
    }

    result.wellFormed = true;
    result.ok = true;
    return result;
}

} // namespace XcapXmlValidator
