#include <QtTest/QtTest>

#include "sip/CpimBuilder.h"
#include "sip/CpimInfo.h"
#include "sip/CpimParser.h"
#include "sip/MessagingDiagnosticsStore.h"
#include "sip/MessagingEventStore.h"
#include "sip/SipMessageComposer.h"
#include "sip/SipTraceLogger.h"

// Tests for the SIP MESSAGE Foundation (Task W092): SipMessageComposer (pure
// Qt, no PJSIP/network dependency) and CpimBuilder. Also verifies that a
// composed outbound message's synthetic rawSip flows unchanged through the
// existing W090/W091 pipeline (SipTraceLogger -> MessagingDiagnosticsStore ->
// MessagingEventStore) without any new parsing logic.
class TestSipMessageFoundation : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void buildsPlainTextMessage();
    void buildsHtmlMessage();
    void buildsCpimWrapper();
    void cpimRequiresEnableCpim();
    void handlesUtf8Body();
    void handlesLargeBody();
    void requestImdnAddsHeaders();
    void rejectsEmptyDestination();
    void rejectsInvalidDestination();
    void rejectsEmptyBody();
    void outboundMessageMapsToMessagingEvent();

    // Task W096: IMDN report composition + diagnostics mapping.
    void composeImdnReportBuildsDeliveredBody();
    void composeImdnReportBuildsDisplayedBody();
    void composeImdnReportRejectsEmptyOriginalMessageId();
    void generatedImdnMapsToMessagingEventDeliveryState();
};

void TestSipMessageFoundation::init()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
    MessagingEventStore::instance().setMaxEventsRetained(1000);
}

void TestSipMessageFoundation::cleanup()
{
    SipTraceLogger::instance().clear();
    MessagingDiagnosticsStore::instance().clear();
    MessagingEventStore::instance().setMaxEventsRetained(1000);
}

void TestSipMessageFoundation::buildsPlainTextMessage()
{
    SipMessageComposer::Options opts;
    opts.toUri   = QStringLiteral("sip:bob@example.com");
    opts.fromUri = QStringLiteral("sip:alice@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Hello Bob!");

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);
    QCOMPARE(msg.contentType, QStringLiteral("text/plain; charset=utf-8"));
    QCOMPARE(msg.body, QStringLiteral("Hello Bob!"));
    QVERIFY(msg.rawSip.startsWith(QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0")));
    QVERIFY(msg.rawSip.contains(QStringLiteral("Hello Bob!")));
    QVERIFY(!msg.callId.isEmpty());
}

void TestSipMessageFoundation::buildsHtmlMessage()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::Html;
    opts.body = QStringLiteral("<b>Hello</b>");

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);
    QCOMPARE(msg.contentType, QStringLiteral("text/html; charset=utf-8"));
    QCOMPARE(msg.body, QStringLiteral("<b>Hello</b>"));
}

void TestSipMessageFoundation::buildsCpimWrapper()
{
    SipMessageComposer::Options opts;
    opts.toUri   = QStringLiteral("sip:bob@example.com");
    opts.fromUri = QStringLiteral("sip:alice@example.com");
    opts.contentType = MessagingContentKind::Cpim;
    opts.cpimEnabled = true;
    opts.body = QStringLiteral("Wheee!");

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);
    QCOMPARE(msg.contentType, QStringLiteral("message/cpim"));
    QVERIFY(msg.body.contains(QStringLiteral("From: sip:alice@example.com")));
    QVERIFY(msg.body.contains(QStringLiteral("To: sip:bob@example.com")));
    QVERIFY(msg.body.contains(QStringLiteral("Content-Type: text/plain; charset=utf-8")));
    QVERIFY(msg.body.endsWith(QStringLiteral("Wheee!")));

    // Round-trips through the existing (unmodified) parser.
    const CpimInfo parsed = CpimParser::parse(msg.body);
    QVERIFY(parsed.present);
    QCOMPARE(parsed.wrappedBody, QStringLiteral("Wheee!"));
}

void TestSipMessageFoundation::cpimRequiresEnableCpim()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::Cpim;
    opts.cpimEnabled = false; // not enabled
    opts.body = QStringLiteral("Wheee!");

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(!msg.valid);
    QVERIFY(!msg.error.isEmpty());
}

void TestSipMessageFoundation::handlesUtf8Body()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QString::fromUtf8("Bun\xc4\x83 ziua! \xe2\x98\x83"); // "Bună ziua! ☃"

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);
    QCOMPARE(msg.body, opts.body);
    QVERIFY(msg.rawSip.toUtf8().contains(opts.body.toUtf8()));
}

void TestSipMessageFoundation::handlesLargeBody()
{
    const QString largeBody = QString(5000, QLatin1Char('x')); // > 4KB
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = largeBody;

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);
    QCOMPARE(msg.body.size(), 5000);
    QVERIFY(msg.rawSip.contains(largeBody));
}

void TestSipMessageFoundation::requestImdnAddsHeaders()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Hi");
    opts.requestImdn = true;

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);
    QVERIFY(msg.imdnRequested);
    QCOMPARE(msg.extraHeaders.size(), 2);
    QVERIFY(msg.rawSip.contains(QStringLiteral("Message-ID:")));
    QVERIFY(msg.rawSip.contains(QStringLiteral("Disposition-Notification: positive-delivery, positive-display")));

    // This is metadata only — no IMDN document is generated by composing a request.
    QVERIFY(!msg.body.contains(QStringLiteral("<imdn")));
}

void TestSipMessageFoundation::rejectsEmptyDestination()
{
    SipMessageComposer::Options opts;
    opts.toUri = QString();
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Hi");

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(!msg.valid);
    QVERIFY(!msg.error.isEmpty());
}

void TestSipMessageFoundation::rejectsInvalidDestination()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("not a valid uri with spaces");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Hi");

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(!msg.valid);
}

void TestSipMessageFoundation::rejectsEmptyBody()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("   "); // whitespace-only

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(!msg.valid);
    QVERIFY(!msg.error.isEmpty());
}

void TestSipMessageFoundation::outboundMessageMapsToMessagingEvent()
{
    SipMessageComposer::Options opts;
    opts.toUri   = QStringLiteral("sip:bob@example.com");
    opts.fromUri = QStringLiteral("sip:alice@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Ping");

    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);

    // Exactly what SipManager::sendSipMessage() does: log the synthetic
    // outbound trace, then let the existing (unmodified) W090/W091 pipeline
    // parse and map it — no bespoke mapping logic here.
    SipMessageTrace trace;
    trace.direction   = SipMessageTrace::Direction::Outbound;
    trace.method      = QStringLiteral("MESSAGE");
    trace.fromUri     = msg.fromUri;
    trace.toUri       = msg.toUri;
    trace.callId      = msg.callId;
    trace.cSeq        = msg.cSeq;
    trace.contentType = msg.contentType;
    trace.rawSip      = msg.rawSip;

    QSignalSpy appendSpy(&MessagingEventStore::instance(), &MessagingEventStore::eventAppended);
    SipTraceLogger::instance().logMessage(trace);

    QCOMPARE(appendSpy.count(), 1);
    const MessagingEvent e = MessagingEventStore::instance().snapshot().first();
    QCOMPARE(e.transport, MessagingEvent::Transport::SipMessage);
    QCOMPARE(e.payloadType, MessagingEvent::PayloadType::Plain);
    QCOMPARE(e.direction, MessagingEvent::Direction::Outbound);
    QCOMPARE(e.from, QStringLiteral("sip:alice@example.com"));
    QCOMPARE(e.to, QStringLiteral("sip:bob@example.com"));
    QCOMPARE(e.callId, msg.callId);
    QCOMPARE(e.bodyPreview, QStringLiteral("Ping"));
    QCOMPARE(e.parseStatus, MessagingEvent::ParseStatus::Ok);
}

void TestSipMessageFoundation::composeImdnReportBuildsDeliveredBody()
{
    SipMessageComposer::ImdnReportOptions opts;
    opts.toUri = QStringLiteral("sip:alice@example.com");
    opts.fromUri = QStringLiteral("sip:bob@example.com");
    opts.originalMessageId = QStringLiteral("orig-msg-1");
    opts.disposition = ImdnInfo::Disposition::Delivered;

    const ComposedSipMessage msg = SipMessageComposer::composeImdnReport(opts);
    QVERIFY(msg.valid);
    QVERIFY(msg.isImdnReport);
    QCOMPARE(msg.correlatedMessageId, QStringLiteral("orig-msg-1"));
    QCOMPARE(msg.contentType, QStringLiteral("message/imdn+xml"));
    QVERIFY(msg.body.contains(QStringLiteral("<delivered/>")));
    QVERIFY(msg.body.contains(QStringLiteral("orig-msg-1")));
    QVERIFY(msg.rawSip.contains(QStringLiteral("message/imdn+xml")));
}

void TestSipMessageFoundation::composeImdnReportBuildsDisplayedBody()
{
    SipMessageComposer::ImdnReportOptions opts;
    opts.toUri = QStringLiteral("sip:alice@example.com");
    opts.fromUri = QStringLiteral("sip:bob@example.com");
    opts.originalMessageId = QStringLiteral("orig-msg-2");
    opts.disposition = ImdnInfo::Disposition::Displayed;

    const ComposedSipMessage msg = SipMessageComposer::composeImdnReport(opts);
    QVERIFY(msg.valid);
    QVERIFY(msg.body.contains(QStringLiteral("<displayed/>")));
}

void TestSipMessageFoundation::composeImdnReportRejectsEmptyOriginalMessageId()
{
    SipMessageComposer::ImdnReportOptions opts;
    opts.toUri = QStringLiteral("sip:alice@example.com");
    opts.disposition = ImdnInfo::Disposition::Delivered;
    // opts.originalMessageId left empty

    const ComposedSipMessage msg = SipMessageComposer::composeImdnReport(opts);
    QVERIFY(!msg.valid);
    QVERIFY(!msg.error.isEmpty());
}

void TestSipMessageFoundation::generatedImdnMapsToMessagingEventDeliveryState()
{
    SipMessageComposer::ImdnReportOptions opts;
    opts.toUri = QStringLiteral("sip:alice@example.com");
    opts.fromUri = QStringLiteral("sip:bob@example.com");
    opts.originalMessageId = QStringLiteral("orig-msg-3");
    opts.disposition = ImdnInfo::Disposition::Delivered;
    const ComposedSipMessage msg = SipMessageComposer::composeImdnReport(opts);
    QVERIFY(msg.valid);

    SipMessageTrace trace;
    trace.direction   = SipMessageTrace::Direction::Outbound;
    trace.method      = QStringLiteral("MESSAGE");
    trace.fromUri     = msg.fromUri;
    trace.toUri       = msg.toUri;
    trace.callId      = msg.callId;
    trace.contentType = msg.contentType;
    trace.rawSip      = msg.rawSip;

    SipTraceLogger::instance().logMessage(trace);

    const MessagingEvent e = MessagingEventStore::instance().snapshot().first();
    QCOMPARE(e.payloadType, MessagingEvent::PayloadType::Imdn);
    QVERIFY(e.generatedImdn);
    QVERIFY(!e.receivedImdn);
    QCOMPARE(e.correlatedMessageId, QStringLiteral("orig-msg-3"));
    QCOMPARE(e.deliveryState, QStringLiteral("delivered"));
}

QTEST_GUILESS_MAIN(TestSipMessageFoundation)
#include "test_sip_message_foundation.moc"
