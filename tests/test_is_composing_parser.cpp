#include <QtTest/QtTest>

#include "sip/IsComposingParser.h"

// Tests for IsComposingParser (RFC 3994 application/im-iscomposing+xml) —
// pure text/XML parsing, no PJSIP/network needed.

class TestIsComposingParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesActiveStateWithRefresh();
    void parsesIdleState();
    void parsesGoneState();
    void parsesTimeoutElementIfPresent();
    void emptyBodyIsNotPresent();
    void nonComposingXmlIsNotPresent();
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

QTEST_GUILESS_MAIN(TestIsComposingParser)
#include "test_is_composing_parser.moc"
