#include <QtTest/QtTest>

#include "msrp/MsrpPayloadDispatcher.h"
#include "sip/MessageHistoryStore.h"

class TestMsrpPayloadDispatcher : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void dispatchesPlainText();
    void dispatchesHtml();
    void dispatchesCpimWrappedText();
    void dispatchesImdnReport();
    void dispatchesIsComposing();
    void unknownBinaryNotAddedToHistory();
    void unknownTextLikeAddedToHistory();
};

void TestMsrpPayloadDispatcher::init()
{
    MessageHistoryStore::instance().clear();
}

void TestMsrpPayloadDispatcher::dispatchesPlainText()
{
    const auto r = MsrpPayloadDispatcher::dispatch(
        QStringLiteral("text/plain"), QByteArray("hello"),
        QStringLiteral("sip:a@x"), QStringLiteral("sip:b@x"),
        QStringLiteral("call1"), QStringLiteral("prof1"));
    QCOMPARE(r.kind, MessagingContentKind::PlainText);
    QVERIFY(r.addedToHistory);
    QCOMPARE(MessageHistoryStore::instance().count(), 1);
}

void TestMsrpPayloadDispatcher::dispatchesHtml()
{
    const auto r = MsrpPayloadDispatcher::dispatch(
        QStringLiteral("text/html"), QByteArray("<b>hi</b>"),
        QStringLiteral("sip:a@x"), QStringLiteral("sip:b@x"),
        QStringLiteral("call1"), QStringLiteral("prof1"));
    QCOMPARE(r.kind, MessagingContentKind::Html);
    QVERIFY(r.addedToHistory);
}

void TestMsrpPayloadDispatcher::dispatchesCpimWrappedText()
{
    const QByteArray cpim =
        "From: <sip:a@x>\r\nTo: <sip:b@x>\r\n\r\nContent-Type: text/plain\r\n\r\nHi via CPIM";
    const auto r = MsrpPayloadDispatcher::dispatch(
        QStringLiteral("message/cpim"), cpim,
        QStringLiteral("sip:a@x"), QStringLiteral("sip:b@x"),
        QStringLiteral("call1"), QStringLiteral("prof1"));
    QCOMPARE(r.kind, MessagingContentKind::PlainText);
    QVERIFY(r.addedToHistory);
}

void TestMsrpPayloadDispatcher::dispatchesImdnReport()
{
    const QByteArray imdn =
        "<?xml version=\"1.0\"?><imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">"
        "<message-id>abc123</message-id><datetime>2026-01-01T00:00:00Z</datetime>"
        "<delivery-notification><status><delivered/></status></delivery-notification>"
        "</imdn>";
    const auto r = MsrpPayloadDispatcher::dispatch(
        QStringLiteral("message/imdn+xml"), imdn,
        QStringLiteral("sip:a@x"), QStringLiteral("sip:b@x"),
        QStringLiteral("call1"), QStringLiteral("prof1"));
    QCOMPARE(r.kind, MessagingContentKind::Imdn);
    QVERIFY(r.addedToHistory);
}

void TestMsrpPayloadDispatcher::dispatchesIsComposing()
{
    const QByteArray ic =
        "<?xml version=\"1.0\"?><isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">"
        "<state>active</state></isComposing>";
    const auto r = MsrpPayloadDispatcher::dispatch(
        QStringLiteral("application/im-iscomposing+xml"), ic,
        QStringLiteral("sip:a@x"), QStringLiteral("sip:b@x"),
        QStringLiteral("call1"), QStringLiteral("prof1"));
    QCOMPARE(r.kind, MessagingContentKind::IsComposing);
    QVERIFY(r.addedToHistory);
}

void TestMsrpPayloadDispatcher::unknownBinaryNotAddedToHistory()
{
    QByteArray binary;
    for (int i = 0; i < 64; ++i)
        binary.append(static_cast<char>(i));
    const auto r = MsrpPayloadDispatcher::dispatch(
        QStringLiteral("application/octet-stream"), binary,
        QStringLiteral("sip:a@x"), QStringLiteral("sip:b@x"),
        QStringLiteral("call1"), QStringLiteral("prof1"));
    QVERIFY(!r.addedToHistory);
    QVERIFY(!r.warning.isEmpty());
}

void TestMsrpPayloadDispatcher::unknownTextLikeAddedToHistory()
{
    const auto r = MsrpPayloadDispatcher::dispatch(
        QStringLiteral("application/x-custom-text"), QByteArray("just plain-ish text"),
        QStringLiteral("sip:a@x"), QStringLiteral("sip:b@x"),
        QStringLiteral("call1"), QStringLiteral("prof1"));
    QVERIFY(r.addedToHistory);
}

QTEST_GUILESS_MAIN(TestMsrpPayloadDispatcher)
#include "test_msrp_payload_dispatcher.moc"
