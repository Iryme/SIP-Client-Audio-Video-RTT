#include <QtTest/QtTest>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "msrp/MsrpFileReceiver.h"

// MSRP file-transfer receive-side helpers (Task W104): atomic disk write +
// hash verification. No MsrpSession/network dependency.
class TestMsrpFileReceiver : public QObject
{
    Q_OBJECT

private slots:
    void saveWritesExactBytes();
    void saveFailsForEmptyPath();
    void saveNeverLeavesPartialFileOnCancel();
    void verifyHashAcceptsMatchingSha1();
    void verifyHashRejectsMismatch();
    void verifyHashPassesWhenExpectedEmpty();
    void verifyHashRejectsUnknownAlgorithm();
};

void TestMsrpFileReceiver::saveWritesExactBytes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("out.bin"));
    const QByteArray body("some file contents, including \r\n and \x00 bytes", 45);

    const auto result = MsrpFileReceiver::saveToPath(body, path);
    QVERIFY(result.ok);
    QCOMPARE(result.bytesWritten, static_cast<qint64>(body.size()));

    QFile check(path);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), body);
}

void TestMsrpFileReceiver::saveFailsForEmptyPath()
{
    const auto result = MsrpFileReceiver::saveToPath(QByteArray("x"), QString());
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

void TestMsrpFileReceiver::saveNeverLeavesPartialFileOnCancel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A directory that does not exist as a parent: open() should fail
    // cleanly rather than partially writing anything.
    const QString path = dir.filePath(QStringLiteral("nested/does/not/exist/out.bin"));
    const auto result = MsrpFileReceiver::saveToPath(QByteArray("data"), path);
    QVERIFY(!result.ok);
    QVERIFY(!QFile::exists(path));
}

void TestMsrpFileReceiver::verifyHashAcceptsMatchingSha1()
{
    const QByteArray body("hello world");
    const QString hex = QString::fromLatin1(QCryptographicHash::hash(body, QCryptographicHash::Sha1).toHex());
    QVERIFY(MsrpFileReceiver::verifyHash(body, QStringLiteral("sha-1"), hex));
    QVERIFY(MsrpFileReceiver::verifyHash(body, QStringLiteral("SHA1"), hex.toUpper()));
}

void TestMsrpFileReceiver::verifyHashRejectsMismatch()
{
    const QByteArray body("hello world");
    QVERIFY(!MsrpFileReceiver::verifyHash(body, QStringLiteral("sha-1"), QStringLiteral("deadbeef")));
}

void TestMsrpFileReceiver::verifyHashPassesWhenExpectedEmpty()
{
    QVERIFY(MsrpFileReceiver::verifyHash(QByteArray("anything"), QStringLiteral("sha-1"), QString()));
}

void TestMsrpFileReceiver::verifyHashRejectsUnknownAlgorithm()
{
    QVERIFY(!MsrpFileReceiver::verifyHash(QByteArray("x"), QStringLiteral("md5"), QStringLiteral("abcd")));
}

QTEST_APPLESS_MAIN(TestMsrpFileReceiver)
#include "test_msrp_file_receiver.moc"
