#include "msrp/MsrpDigestAuth.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QStringList>

namespace MsrpDigestAuth {

namespace {

QString md5Hex(const QString &input)
{
    return QString::fromLatin1(QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Md5).toHex());
}

// Splits a Digest challenge/credentials parameter list on top-level commas,
// respecting quoted-string boundaries (so a comma inside realm="a,b" is not
// treated as a parameter separator).
QStringList splitDigestParams(const QString &text)
{
    QStringList parts;
    QString current;
    bool inQuotes = false;
    for (const QChar &c : text) {
        if (c == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            current += c;
        } else if (c == QLatin1Char(',') && !inQuotes) {
            parts << current;
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.trimmed().isEmpty())
        parts << current;
    return parts;
}

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.length() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
        value = value.mid(1, value.length() - 2);
    return value;
}

} // namespace

Challenge parseChallenge(const QString &headerValue)
{
    Challenge challenge;
    QString body = headerValue.trimmed();
    if (body.startsWith(QLatin1String("Digest "), Qt::CaseInsensitive))
        body = body.mid(7);

    const QStringList params = splitDigestParams(body);
    for (const QString &rawParam : params) {
        const int eq = rawParam.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = rawParam.left(eq).trimmed().toLower();
        const QString value = unquote(rawParam.mid(eq + 1));
        if (key == QLatin1String("realm")) challenge.realm = value;
        else if (key == QLatin1String("nonce")) challenge.nonce = value;
        else if (key == QLatin1String("algorithm")) challenge.algorithm = value;
        else if (key == QLatin1String("qop")) challenge.qop = value.split(QLatin1Char(',')).value(0).trimmed();
        else if (key == QLatin1String("opaque")) challenge.opaque = value;
        else if (key == QLatin1String("stale")) challenge.stale = (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
    }

    challenge.ok = !challenge.realm.isEmpty() && !challenge.nonce.isEmpty();
    if (challenge.algorithm.isEmpty())
        challenge.algorithm = QStringLiteral("MD5");
    return challenge;
}

QString computeHA1(const QString &username, const QString &realm, const QString &password)
{
    return md5Hex(username + QLatin1Char(':') + realm + QLatin1Char(':') + password);
}

QString computeHA2(const QString &method, const QString &digestUri)
{
    return md5Hex(method + QLatin1Char(':') + digestUri);
}

QString computeResponse(const QString &ha1, const QString &nonce, const QString &nc,
                        const QString &cnonce, const QString &qop, const QString &ha2)
{
    if (qop.isEmpty())
        return md5Hex(ha1 + QLatin1Char(':') + nonce + QLatin1Char(':') + ha2);
    return md5Hex(ha1 + QLatin1Char(':') + nonce + QLatin1Char(':') + nc + QLatin1Char(':') +
                  cnonce + QLatin1Char(':') + qop + QLatin1Char(':') + ha2);
}

QString generateCnonce()
{
    QByteArray raw(16, Qt::Uninitialized);
    for (int i = 0; i < raw.size(); i += 4) {
        const quint32 word = QRandomGenerator::global()->generate();
        for (int b = 0; b < 4 && i + b < raw.size(); ++b)
            raw[i + b] = static_cast<char>((word >> (8 * b)) & 0xFF);
    }
    return QString::fromLatin1(raw.toHex());
}

QString buildAuthorizationHeader(const AuthorizationParams &p)
{
    QStringList parts;
    parts << QStringLiteral("username=\"%1\"").arg(p.username);
    parts << QStringLiteral("realm=\"%1\"").arg(p.realm);
    parts << QStringLiteral("nonce=\"%1\"").arg(p.nonce);
    parts << QStringLiteral("uri=\"%1\"").arg(p.uri);
    parts << QStringLiteral("response=\"%1\"").arg(p.response);
    if (!p.algorithm.isEmpty())
        parts << QStringLiteral("algorithm=%1").arg(p.algorithm);
    if (!p.qop.isEmpty()) {
        parts << QStringLiteral("qop=%1").arg(p.qop);
        parts << QStringLiteral("cnonce=\"%1\"").arg(p.cnonce);
        parts << QStringLiteral("nc=%1").arg(p.nc);
    }
    if (!p.opaque.isEmpty())
        parts << QStringLiteral("opaque=\"%1\"").arg(p.opaque);
    return QStringLiteral("Digest ") + parts.join(QStringLiteral(", "));
}

QString buildAuthorizationHeader(const Challenge &challenge, const QString &username,
                                 const QString &password, const QString &digestUri,
                                 const QString &cnonce, const QString &nc)
{
    const QString ha1 = computeHA1(username, challenge.realm, password);
    const QString ha2 = computeHA2(QStringLiteral("AUTH"), digestUri);
    const QString response = computeResponse(ha1, challenge.nonce, nc, cnonce, challenge.qop, ha2);

    AuthorizationParams params;
    params.username = username;
    params.realm = challenge.realm;
    params.nonce = challenge.nonce;
    params.uri = digestUri;
    params.response = response;
    params.algorithm = challenge.algorithm;
    params.cnonce = cnonce;
    params.nc = nc;
    params.qop = challenge.qop;
    params.opaque = challenge.opaque;
    return buildAuthorizationHeader(params);
}

} // namespace MsrpDigestAuth
