#include <QCoreApplication>
#include <QTest>

#include "emergency/EmergencyMultipartBuilder.h"
#include "sip/SipCallOptions.h"

class TestEmergencyMultipartBuilder : public QObject
{
    Q_OBJECT
private slots:

    // 1. No addPidfLo call → builder is empty
    void test_emptyBuilderIsEmpty()
    {
        EmergencyMultipartBuilder builder;
        QVERIFY(builder.isEmpty());
    }

    // 2. No addPidfLo → build() returns empty list
    void test_emptyBuilderProducesNoParts()
    {
        EmergencyMultipartBuilder builder;
        QVERIFY(builder.build().isEmpty());
    }

    // 3. addPidfLo with valid xml → one part
    void test_addPidfLoCreatesOnePart()
    {
        EmergencyMultipartBuilder builder;
        builder.addPidfLo(QStringLiteral("<presence/>"),
                          QStringLiteral("pidflo-1@ng112.local"));
        QCOMPARE(builder.build().size(), 1);
        QVERIFY(!builder.isEmpty());
    }

    // 4. Content-Type of PIDF-LO part is application/pidf+xml
    void test_pidfLoPartContentType()
    {
        EmergencyMultipartBuilder builder;
        builder.addPidfLo(QStringLiteral("<presence/>"),
                          QStringLiteral("pidflo-1@ng112.local"));
        const auto parts = builder.build();
        QCOMPARE(parts.at(0).contentType, QStringLiteral("application/pidf+xml"));
    }

    // 5. Body is preserved exactly
    void test_pidfLoBodyPreserved()
    {
        const QString xml = QStringLiteral("<presence><tuple/></presence>");
        EmergencyMultipartBuilder builder;
        builder.addPidfLo(xml, QStringLiteral("pidflo-1@ng112.local"));
        QCOMPARE(builder.build().at(0).body, xml);
    }

    // 6. contentId stored raw (no angle brackets in the field itself)
    void test_contentIdStoredRaw()
    {
        EmergencyMultipartBuilder builder;
        builder.addPidfLo(QStringLiteral("<presence/>"),
                          QStringLiteral("pidflo-1@ng112.local"));
        const auto &part = builder.build().at(0);
        QCOMPARE(part.contentId, QStringLiteral("pidflo-1@ng112.local"));
        QVERIFY(!part.contentId.startsWith(QLatin1Char('<')));
        QVERIFY(!part.contentId.endsWith(QLatin1Char('>')));
    }

    // 7. Content-ID header value has angle brackets (RFC 2183)
    void test_contentIdHeaderHasAngleBrackets()
    {
        EmergencyMultipartBuilder builder;
        builder.addPidfLo(QStringLiteral("<presence/>"),
                          QStringLiteral("pidflo-1@ng112.local"));
        const auto &part = builder.build().at(0);

        bool found = false;
        QString cidValue;
        for (const auto &h : part.headers) {
            if (h.first == QStringLiteral("Content-ID")) {
                found    = true;
                cidValue = h.second;
            }
        }
        QVERIFY2(found, "Content-ID header must be present in part");
        QVERIFY2(cidValue.startsWith(QLatin1Char('<')), "Content-ID must start with '<'");
        QVERIFY2(cidValue.endsWith(QLatin1Char('>')), "Content-ID must end with '>'");
        QCOMPARE(cidValue, QStringLiteral("<pidflo-1@ng112.local>"));
    }

    // 8. contentIdHeaderValue() helper returns "<id>"
    void test_contentIdHeaderValueMethod()
    {
        MultipartPart part;
        part.contentId = QStringLiteral("pidflo-42@ng112.local");
        QCOMPARE(part.contentIdHeaderValue(),
                 QStringLiteral("<pidflo-42@ng112.local>"));
    }

    // 9. contentIdHeaderValue() returns empty when contentId is empty
    void test_contentIdHeaderValueEmptyWhenNoId()
    {
        MultipartPart part;
        QVERIFY(part.contentIdHeaderValue().isEmpty());
    }

    // 10. addPidfLo with empty xml → no part added (guard)
    void test_emptyXmlNotAdded()
    {
        EmergencyMultipartBuilder builder;
        builder.addPidfLo(QString{}, QStringLiteral("pidflo-1@ng112.local"));
        QVERIFY(builder.isEmpty());
        QVERIFY(builder.build().isEmpty());
    }

    // 11. Normal SipCallOptions body is empty → no multipart trigger
    void test_normalSipCallOptionsBodyEmpty()
    {
        SipCallOptions opts = SipCallOptions::normal();
        QVERIFY(opts.body.isEmpty());
    }

    // 12. Emergency SipCallOptions without location → body still empty
    void test_emergencyWithoutBodyNoMultipart()
    {
        SipCallOptions opts;
        opts.emergencyCall = true;
        // body not set
        QVERIFY(opts.body.isEmpty());
        // multipart should NOT be triggered by SipCall (body.isEmpty() guard)
    }

    // 13. Two addPidfLo calls with non-empty xml → two parts
    void test_multiplePidfLoPartsSupported()
    {
        EmergencyMultipartBuilder builder;
        builder.addPidfLo(QStringLiteral("<presence/>"),
                          QStringLiteral("pidflo-1@ng112.local"));
        builder.addPidfLo(QStringLiteral("<presence2/>"),
                          QStringLiteral("pidflo-2@ng112.local"));
        QCOMPARE(builder.build().size(), 2);
    }

    // 14. Part without contentId has no Content-ID header
    void test_partWithoutContentIdHasNoCidHeader()
    {
        EmergencyMultipartBuilder builder;
        builder.addPidfLo(QStringLiteral("<presence/>"), QString{});
        const auto &part = builder.build().at(0);
        bool hasCid = false;
        for (const auto &h : part.headers) {
            if (h.first == QStringLiteral("Content-ID"))
                hasCid = true;
        }
        QVERIFY2(!hasCid, "No Content-ID header expected when contentId is empty");
    }
};

QTEST_GUILESS_MAIN(TestEmergencyMultipartBuilder)
#include "test_emergency_multipart_builder.moc"
