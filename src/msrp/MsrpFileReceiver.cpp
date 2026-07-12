#include "MsrpFileReceiver.h"

#include <QCryptographicHash>
#include <QSaveFile>

namespace MsrpFileReceiver {

SaveResult saveToPath(const QByteArray &body, const QString &savePath)
{
    SaveResult result;
    if (savePath.isEmpty()) {
        result.error = QStringLiteral("save path is empty");
        return result;
    }

    QSaveFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = QStringLiteral("failed to open save path: %1").arg(file.errorString());
        return result;
    }
    const qint64 written = file.write(body);
    if (written != body.size()) {
        result.error = QStringLiteral("short write to save path: %1").arg(file.errorString());
        file.cancelWriting();
        return result;
    }
    if (!file.commit()) {
        result.error = QStringLiteral("failed to commit save path: %1").arg(file.errorString());
        return result;
    }

    result.ok = true;
    result.bytesWritten = written;
    return result;
}

bool verifyHash(const QByteArray &body, const QString &algorithm, const QString &expectedHex)
{
    if (expectedHex.isEmpty())
        return true;

    QCryptographicHash::Algorithm algo;
    const QString norm = algorithm.toLower();
    if (norm == QLatin1String("sha-1") || norm == QLatin1String("sha1"))
        algo = QCryptographicHash::Sha1;
    else if (norm == QLatin1String("sha-256") || norm == QLatin1String("sha256"))
        algo = QCryptographicHash::Sha256;
    else
        return false; // unrecognized algorithm: never silently "pass"

    const QByteArray actual = QCryptographicHash::hash(body, algo).toHex();
    return actual.compare(expectedHex.toUtf8(), Qt::CaseInsensitive) == 0;
}

} // namespace MsrpFileReceiver
