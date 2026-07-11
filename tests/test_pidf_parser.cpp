#include <QTest>

#include "sip/PidfParser.h"

class TestPidfParser : public QObject
{
    Q_OBJECT

private slots:
    void openStatus();
    void closedStatus();
    void contactWithPriority();
    void withNote();
    void withNamespace();
    void multipleTuples();
    void incompletePidf();
    void invalidXml();
    void emptyBody();
    void oversizedBodyRejected();
    void extendedStatusAway();
    void extendedStatusBusy();
    void extendedStatusDoNotDisturb();
    void unknownExtensionKeepsBasicStatus();
};

void TestPidfParser::openStatus()
{
    const QString body = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <tuple id=\"t1\">"
        "    <status><basic>open</basic></status>"
        "  </tuple>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.entityUri, QStringLiteral("sip:alice@example.com"));
    QCOMPARE(info.tupleId, QStringLiteral("t1"));
    QCOMPARE(info.basicStatus, PresenceInfo::BasicStatus::Open);
    QCOMPARE(info.extendedStatus, PresenceInfo::ExtendedStatus::Available);
    QCOMPARE(info.parseStatus, PresenceInfo::ParseStatus::Ok);
}

void TestPidfParser::closedStatus()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:bob@example.com\">"
        "  <tuple id=\"t1\"><status><basic>closed</basic></status></tuple>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.basicStatus, PresenceInfo::BasicStatus::Closed);
    QCOMPARE(info.extendedStatus, PresenceInfo::ExtendedStatus::Offline);
    QCOMPARE(info.parseStatus, PresenceInfo::ParseStatus::Ok);
}

void TestPidfParser::contactWithPriority()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <tuple id=\"t1\">"
        "    <status><basic>open</basic></status>"
        "    <contact priority=\"0.8\">sip:alice-mobile@example.com</contact>"
        "  </tuple>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.contactUri, QStringLiteral("sip:alice-mobile@example.com"));
    QCOMPARE(info.priority, QStringLiteral("0.8"));
}

void TestPidfParser::withNote()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <tuple id=\"t1\"><status><basic>open</basic></status></tuple>"
        "  <note>In a meeting</note>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.note, QStringLiteral("In a meeting"));
}

void TestPidfParser::withNamespace()
{
    // Namespace-prefixed elements must still be recognized by local name.
    const QString body = QStringLiteral(
        "<pidf:presence xmlns:pidf=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <pidf:tuple id=\"t1\">"
        "    <pidf:status><pidf:basic>open</pidf:basic></pidf:status>"
        "  </pidf:tuple>"
        "</pidf:presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.entityUri, QStringLiteral("sip:alice@example.com"));
    QCOMPARE(info.basicStatus, PresenceInfo::BasicStatus::Open);
}

void TestPidfParser::multipleTuples()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <tuple id=\"t1\"><status><basic>open</basic></status></tuple>"
        "  <tuple id=\"t2\"><status><basic>closed</basic></status></tuple>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    // First tuple is taken as canonical.
    QCOMPARE(info.tupleId, QStringLiteral("t1"));
    QCOMPARE(info.basicStatus, PresenceInfo::BasicStatus::Open);
    QCOMPARE(info.parseStatus, PresenceInfo::ParseStatus::Ok);
}

void TestPidfParser::incompletePidf()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\"/>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.parseStatus, PresenceInfo::ParseStatus::Partial);
    QVERIFY(!info.parseWarnings.isEmpty());
}

void TestPidfParser::invalidXml()
{
    const QString body = QStringLiteral("<presence><tuple><status><basic>open</basic>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.parseStatus, PresenceInfo::ParseStatus::Error);
    QVERIFY(!info.parseWarnings.isEmpty());
}

void TestPidfParser::emptyBody()
{
    const PresenceInfo info = PidfParser::parse(QString());
    QCOMPARE(info.parseStatus, PresenceInfo::ParseStatus::Error);
}

void TestPidfParser::oversizedBodyRejected()
{
    QString body = QStringLiteral("<presence entity=\"sip:alice@example.com\">");
    body += QString(PidfParser::kMaxPidfBytes + 100, QLatin1Char('x'));
    body += QStringLiteral("</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.parseStatus, PresenceInfo::ParseStatus::Error);
    QVERIFY(info.parseWarnings.join(QString()).contains(QStringLiteral("exceeds")));
}

void TestPidfParser::extendedStatusAway()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <tuple id=\"t1\">"
        "    <status><basic>open</basic>"
        "      <ep:activities xmlns:ep=\"urn:ietf:params:xml:ns:pidf:rpid\"><ep:away/></ep:activities>"
        "    </status>"
        "  </tuple>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.extendedStatus, PresenceInfo::ExtendedStatus::Away);
}

void TestPidfParser::extendedStatusBusy()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <tuple id=\"t1\">"
        "    <status><basic>open</basic>"
        "      <ep:activities xmlns:ep=\"urn:ietf:params:xml:ns:pidf:rpid\"><ep:busy/></ep:activities>"
        "    </status>"
        "  </tuple>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.extendedStatus, PresenceInfo::ExtendedStatus::Busy);
}

void TestPidfParser::extendedStatusDoNotDisturb()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <tuple id=\"t1\">"
        "    <status><basic>closed</basic>"
        "      <ep:activities xmlns:ep=\"urn:ietf:params:xml:ns:pidf:rpid\"><ep:do-not-disturb/></ep:activities>"
        "    </status>"
        "  </tuple>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.extendedStatus, PresenceInfo::ExtendedStatus::DoNotDisturb);
}

void TestPidfParser::unknownExtensionKeepsBasicStatus()
{
    const QString body = QStringLiteral(
        "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"sip:alice@example.com\">"
        "  <tuple id=\"t1\">"
        "    <status><basic>open</basic>"
        "      <ep:activities xmlns:ep=\"urn:ietf:params:xml:ns:pidf:rpid\"><ep:vacationing/></ep:activities>"
        "    </status>"
        "  </tuple>"
        "</presence>");

    const PresenceInfo info = PidfParser::parse(body);
    QCOMPARE(info.basicStatus, PresenceInfo::BasicStatus::Open);
    // Unrecognized extension falls back to the basic-status-implied extended value.
    QCOMPARE(info.extendedStatus, PresenceInfo::ExtendedStatus::Available);
    QCOMPARE(info.parseStatus, PresenceInfo::ParseStatus::Ok);
}

QTEST_MAIN(TestPidfParser)
#include "test_pidf_parser.moc"
