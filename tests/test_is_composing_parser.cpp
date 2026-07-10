#include <QtTest/QtTest>

#include "sip/IsComposingGenerator.h"
#include "sip/IsComposingParser.h"

// Tests for IsComposingParser (RFC 3994 application/im-iscomposing+xml) —
// pure text/XML parsing, no PJSIP/network needed. Also covers
// IsComposingGenerator (Task W097) and its round-trip through this parser.

class TestIsComposingParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesActiveStateWithRefresh();
    void parsesIdleState();
    void parsesGoneState();
    void parsesTimeoutElementIfPresent();
    void parsesContentTypeElementIfPresent();
    void emptyBodyIsNotPresent();
    void nonComposingXmlIsNotPresent();

    // Task W097: IsComposingGenerator
    void generatesActiveWithRefreshAndContentTypeAndRoundTrips();
    void generatesIdleAndRoundTrips();
    void generatesGoneAndRoundTrips();
    void generatorOmitsRefreshWhenZeroOrNegative();
    void generatorRejectsUnknownState();
    void generatorEscapesContentType();
};

void TestIsComposingParser::parsesActiveStateWithRefresh()
{
    const QString xml =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">\n"
                       "  <state>active</state>\n"
                       "  <refresh>60</refresh>\n"
                       "  <contenttype>text/plain</contenttype>\n"
                       "</isComposing>\n");

    const IsComposingInfo info = IsComposingParser::parse(xml);

    QVERIFY(info.present);
    QCOMPARE(info.state, IsComposingInfo::State::Active);
    QCOMPARE(info.refresh, QStringLiteral("60"));
}

void TestIsComposingParser::parsesIdleState()
{
    const QString xml =
        QStringLiteral("<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">\n"
                       "  <state>idle</state>\n"
                       "</isComposing>\n");

    const IsComposingInfo info = IsComposingParser::parse(xml);

    QVERIFY(info.present);
    QCOMPARE(info.state, IsComposingInfo::State::Idle);
}

void TestIsComposingParser::parsesGoneState()
{
    const QString xml =
        QStringLiteral("<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">\n"
                       "  <state>gone</state>\n"
                       "</isComposing>\n");

    const IsComposingInfo info = IsComposingParser::parse(xml);

    QVERIFY(info.present);
    QCOMPARE(info.state, IsComposingInfo::State::Gone);
}

void TestIsComposingParser::parsesTimeoutElementIfPresent()
{
    const QString xml =
        QStringLiteral("<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">\n"
                       "  <state>active</state>\n"
                       "  <timeout>120</timeout>\n"
                       "</isComposing>\n");

    const IsComposingInfo info = IsComposingParser::parse(xml);

    QVERIFY(info.present);
    QCOMPARE(info.timeout, QStringLiteral("120"));
}

void TestIsComposingParser::parsesContentTypeElementIfPresent()
{
    const QString xml =
        QStringLiteral("<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">\n"
                       "  <state>active</state>\n"
                       "  <contenttype>text/plain</contenttype>\n"
                       "</isComposing>\n");
    const IsComposingInfo info = IsComposingParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.contentType, QStringLiteral("text/plain"));
}

void TestIsComposingParser::emptyBodyIsNotPresent()
{
    const IsComposingInfo info = IsComposingParser::parse(QString());
    QVERIFY(!info.present);
}

void TestIsComposingParser::nonComposingXmlIsNotPresent()
{
    const IsComposingInfo info = IsComposingParser::parse(QStringLiteral("<foo><bar/></foo>"));
    QVERIFY(!info.present);
}

void TestIsComposingParser::generatesActiveWithRefreshAndContentTypeAndRoundTrips()
{
    const QString xml = IsComposingGenerator::generate(
        IsComposingInfo::State::Active, 60, QStringLiteral("text/plain"));
    QVERIFY(!xml.isEmpty());
    QVERIFY(xml.contains(QStringLiteral("<state>active</state>")));

    const IsComposingInfo info = IsComposingParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.state, IsComposingInfo::State::Active);
    QCOMPARE(info.refresh, QStringLiteral("60"));
    QCOMPARE(info.contentType, QStringLiteral("text/plain"));
}

void TestIsComposingParser::generatesIdleAndRoundTrips()
{
    const QString xml = IsComposingGenerator::generate(IsComposingInfo::State::Idle);
    const IsComposingInfo info = IsComposingParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.state, IsComposingInfo::State::Idle);
    QVERIFY(info.refresh.isEmpty());
}

void TestIsComposingParser::generatesGoneAndRoundTrips()
{
    const QString xml = IsComposingGenerator::generate(IsComposingInfo::State::Gone);
    const IsComposingInfo info = IsComposingParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.state, IsComposingInfo::State::Gone);
}

void TestIsComposingParser::generatorOmitsRefreshWhenZeroOrNegative()
{
    const QString xmlZero = IsComposingGenerator::generate(IsComposingInfo::State::Active, 0);
    QVERIFY(!xmlZero.contains(QStringLiteral("<refresh>")));
    const QString xmlNeg = IsComposingGenerator::generate(IsComposingInfo::State::Active, -5);
    QVERIFY(!xmlNeg.contains(QStringLiteral("<refresh>")));
}

void TestIsComposingParser::generatorRejectsUnknownState()
{
    const QString xml = IsComposingGenerator::generate(IsComposingInfo::State::Unknown);
    QVERIFY(xml.isEmpty());
}

void TestIsComposingParser::generatorEscapesContentType()
{
    const QString contentType = QStringLiteral("text/plain;x=<a>&\"b\"");
    const QString xml = IsComposingGenerator::generate(IsComposingInfo::State::Active, 30, contentType);
    QVERIFY(!xml.contains(QStringLiteral("<a>&\"b\""))); // raw special chars must be escaped

    const IsComposingInfo info = IsComposingParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.contentType, contentType);
}

QTEST_GUILESS_MAIN(TestIsComposingParser)
#include "test_is_composing_parser.moc"
