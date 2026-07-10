#include <QtTest/QtTest>

#include "sip/RcsFtHttpParser.h"

// Fixtures are sanitized/minimal, structurally representative of GSMA
// RCC.07 / OMA CPM file-transfer descriptors — no real hosts, tokens, or
// personal data.

class TestRcsFtHttpParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesCompleteDescriptor();
    void parsesDescriptorWithoutThumbnail();
    void incompleteXmlYieldsPartialOrAbsentInfo();
    void emptyBodyYieldsNotPresent();
    void nonXmlBodyDoesNotCrash();
};

void TestRcsFtHttpParser::parsesCompleteDescriptor()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<file xmlns=\"urn:gsma:params:xml:ns:rcs:rcs:fthttp\">"
        "  <file-info type=\"file\">"
        "    <file-size>204800</file-size>"
        "    <file-name>sample-photo.jpg</file-name>"
        "    <content-type>image/jpeg</content-type>"
        "    <data url=\"https://files.example.test/dl/abc123?token=sample-token\" until=\"2030-01-01T00:00:00.000Z\"/>"
        "  </file-info>"
        "  <file-info type=\"thumbnail\">"
        "    <file-size>2048</file-size>"
        "    <content-type>image/jpeg</content-type>"
        "    <data url=\"https://files.example.test/dl/abc123-thumb?token=sample-token\" until=\"2030-01-01T00:00:00.000Z\"/>"
        "  </file-info>"
        "</file>");

    const RcsFtHttpInfo info = RcsFtHttpParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.fileInfoType, QStringLiteral("file"));
    QCOMPARE(info.fileName, QStringLiteral("sample-photo.jpg"));
    QCOMPARE(info.fileSize, static_cast<qint64>(204800));
    QCOMPARE(info.contentType, QStringLiteral("image/jpeg"));
    QCOMPARE(info.expiresAt, QStringLiteral("2030-01-01T00:00:00.000Z"));
    QVERIFY(info.dataUrl.contains(QStringLiteral("files.example.test")));
    QVERIFY(info.thumbnailPresent);
}

void TestRcsFtHttpParser::parsesDescriptorWithoutThumbnail()
{
    const QString xml = QStringLiteral(
        "<file>"
        "  <file-info type=\"file\">"
        "    <file-size>512</file-size>"
        "    <file-name>note.txt</file-name>"
        "    <content-type>text/plain</content-type>"
        "    <data url=\"https://files.example.test/dl/note\" until=\"2030-06-01T00:00:00.000Z\"/>"
        "  </file-info>"
        "</file>");

    const RcsFtHttpInfo info = RcsFtHttpParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.fileName, QStringLiteral("note.txt"));
    QVERIFY(!info.thumbnailPresent);
}

void TestRcsFtHttpParser::incompleteXmlYieldsPartialOrAbsentInfo()
{
    // <file> root present but no <file-info> at all — the parser must not
    // throw/crash and must not fabricate fields.
    const QString xml = QStringLiteral("<file></file>");

    const RcsFtHttpInfo info = RcsFtHttpParser::parse(xml);
    QVERIFY(info.present); // root element alone is enough to mark "present"
    QVERIFY(info.fileName.isEmpty());
    QCOMPARE(info.fileSize, static_cast<qint64>(-1));
}

void TestRcsFtHttpParser::emptyBodyYieldsNotPresent()
{
    const RcsFtHttpInfo info = RcsFtHttpParser::parse(QString());
    QVERIFY(!info.present);
}

void TestRcsFtHttpParser::nonXmlBodyDoesNotCrash()
{
    const RcsFtHttpInfo info = RcsFtHttpParser::parse(QStringLiteral("not xml at all { garbage </>"));
    QVERIFY(!info.present);
    QVERIFY(info.fileName.isEmpty());
}

QTEST_GUILESS_MAIN(TestRcsFtHttpParser)
#include "test_rcs_ft_http_parser.moc"
