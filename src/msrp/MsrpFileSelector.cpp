#include "MsrpFileSelector.h"

namespace MsrpFileSelector {

namespace {

QStringList tokenizeRespectingQuotes(const QString &text, bool *unterminatedQuote)
{
    QStringList tokens;
    QString current;
    bool inQuotes = false;
    for (const QChar &c : text) {
        if (c == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            current += c;
            continue;
        }
        if (c == QLatin1Char(' ') && !inQuotes) {
            if (!current.isEmpty()) {
                tokens << current;
                current.clear();
            }
            continue;
        }
        current += c;
    }
    if (!current.isEmpty())
        tokens << current;
    if (unterminatedQuote)
        *unterminatedQuote = inQuotes;
    return tokens;
}

} // namespace

MsrpFileSelectorInfo parse(const QString &attributeValue)
{
    MsrpFileSelectorInfo info;
    const QString text = attributeValue.trimmed();
    if (text.isEmpty()) {
        info.warnings << QStringLiteral("empty file-selector value");
        return info;
    }

    bool unterminated = false;
    const QStringList tokens = tokenizeRespectingQuotes(text, &unterminated);
    if (unterminated)
        info.warnings << QStringLiteral("unterminated quoted name in file-selector");

    for (const QString &token : tokens) {
        const int colonIdx = token.indexOf(QLatin1Char(':'));
        if (colonIdx <= 0) {
            info.warnings << QStringLiteral("malformed file-selector token: %1").arg(token);
            continue;
        }
        const QString key = token.left(colonIdx);
        QString value = token.mid(colonIdx + 1);

        if (key == QLatin1String("name")) {
            if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                value = value.mid(1, value.size() - 2);
            value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
            info.fileName = value;
        } else if (key == QLatin1String("size")) {
            bool sizeOk = false;
            const qint64 size = value.toLongLong(&sizeOk);
            if (sizeOk && size >= 0)
                info.fileSize = size;
            else
                info.warnings << QStringLiteral("invalid size in file-selector: %1").arg(value);
        } else if (key == QLatin1String("type")) {
            info.fileType = value;
        } else if (key == QLatin1String("hash")) {
            const int algoColon = value.indexOf(QLatin1Char(':'));
            if (algoColon > 0) {
                info.hashAlgorithm = value.left(algoColon).toLower();
                QString hex = value.mid(algoColon + 1);
                hex.remove(QLatin1Char(':'));
                info.hashValueHex = hex.toUpper();
            } else {
                info.warnings << QStringLiteral("malformed hash token in file-selector: %1").arg(value);
            }
        } else {
            info.warnings << QStringLiteral("unrecognized file-selector field: %1").arg(key);
        }
    }

    info.ok = !info.fileName.isEmpty() || info.fileSize >= 0
        || !info.fileType.isEmpty() || !info.hashValueHex.isEmpty();
    return info;
}

QString build(const QString &fileName, qint64 fileSize, const QString &fileType,
              const QString &hashAlgorithm, const QString &hashValueHex)
{
    QStringList parts;
    if (!fileName.isEmpty()) {
        QString escaped = fileName;
        escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        parts << QStringLiteral("name:\"%1\"").arg(escaped);
    }
    if (fileSize >= 0)
        parts << QStringLiteral("size:%1").arg(fileSize);
    if (!fileType.isEmpty())
        parts << QStringLiteral("type:%1").arg(fileType);
    if (!hashAlgorithm.isEmpty() && !hashValueHex.isEmpty()) {
        const QString hex = hashValueHex.toUpper();
        QString colonHex;
        for (int i = 0; i + 1 < hex.size(); i += 2) {
            if (!colonHex.isEmpty())
                colonHex += QLatin1Char(':');
            colonHex += hex.mid(i, 2);
        }
        if (hex.size() % 2 != 0) {
            if (!colonHex.isEmpty())
                colonHex += QLatin1Char(':');
            colonHex += hex.right(1);
        }
        parts << QStringLiteral("hash:%1:%2").arg(hashAlgorithm.toLower(), colonHex);
    }
    return parts.join(QLatin1Char(' '));
}

QString sanitizeFileNameForDisplay(const QString &rawName)
{
    QString name = rawName;
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int lastSlash = name.lastIndexOf(QLatin1Char('/'));
    if (lastSlash >= 0)
        name = name.mid(lastSlash + 1);
    name = name.trimmed();
    if (name == QLatin1String(".") || name == QLatin1String(".."))
        name.clear();
    if (name.isEmpty())
        name = QStringLiteral("received-file");
    return name;
}

} // namespace MsrpFileSelector
