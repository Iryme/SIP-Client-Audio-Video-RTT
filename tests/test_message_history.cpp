#include <QtTest/QtTest>

#include "sip/MessageHistoryStore.h"
#include "sip/MessagingContentKind.h"
#include "sip/SipMessageComposer.h"

// Tests for MessageHistoryStore — the simple conversational Message History
// (Task W093). Deliberately independent of MessagingEventStore/PJSIP: the
// dedicated incoming-message callback (SipAccount::instantMessageReceived)
// is a thin pass-through straight into MessageHistoryStore::appendInbound()
// (see SipManager::onAccountInstantMessageReceived), so exercising
// appendInbound() directly covers the callback's mapping logic without a
// live PJSIP stack.

class TestMessageHistory : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void incomingMessageMapsAllFields();
    void dedupSuppressesRepeatedInbound();
    void historyAppendInbound();
    void historyAppendOutbound();
    void failedOutboundStatus();
    void statusUpgradesFromSubmittedToSent();
    void filteringByDirectionAndFailed();
    void filteringByContentType();
    void previewLimit();
    void utf8Body();

    // Task W096: IMDN correlation / delivery state.
    void inboundCapturesMessageIdAndDispositionNotification();
    void outboundCapturesMessageIdWhenImdnRequested();
    void correlateDeliveryUpgradesOutboundEntry();
    void correlateDeliveryDisplayedAfterDelivered();
    void correlateDeliveryIgnoresUnknownMessageId();
    void duplicateInboundImdnReportIsDeduped();
    void markImdnSentSetsFlagsIndependently();
    void appendInboundImdnStoresCorrelatedMessageId();

    // Task W097: is-composing history rows.
    void appendInboundTypingStoresState();
    void duplicateInboundTypingIsDeduped();
};

void TestMessageHistory::init()
{
    MessageHistoryStore::instance().clear();
}

void TestMessageHistory::cleanup()
{
    MessageHistoryStore::instance().clear();
}

void TestMessageHistory::incomingMessageMapsAllFields()
{
    QSignalSpy spy(&MessageHistoryStore::instance(), &MessageHistoryStore::entryAppended);
    const qint64 id = MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"),
        QStringLiteral("sip:alice@10.0.0.1:5060"), QStringLiteral("text/plain"),
        QStringLiteral("Hello Bob!"), QStringLiteral("call-abc"), QStringLiteral("profile-1"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(id > 0);

    const MessageHistoryEntry e = MessageHistoryStore::instance().snapshot().first();
    QCOMPARE(e.direction, MessageHistoryEntry::Direction::Inbound);
    QCOMPARE(e.peerUri, QStringLiteral("sip:alice@example.com"));
    QCOMPARE(e.contentType, QStringLiteral("text/plain"));
    QCOMPARE(e.bodyPreview, QStringLiteral("Hello Bob!"));
    QCOMPARE(e.callId, QStringLiteral("call-abc"));
    QCOMPARE(e.contactUri, QStringLiteral("sip:alice@10.0.0.1:5060"));
    QCOMPARE(e.profileId, QStringLiteral("profile-1"));
    QCOMPARE(e.outboundStatus, MessageHistoryEntry::OutboundStatus::Unknown);
}

void TestMessageHistory::dedupSuppressesRepeatedInbound()
{
    for (int i = 0; i < 3; ++i) {
        MessageHistoryStore::instance().appendInbound(
            QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"),
            QString(), QStringLiteral("text/plain"), QStringLiteral("Same message"),
            QStringLiteral("call-dup"), QString());
    }

    // All three calls describe the exact same physical message (identical
    // from/to/contentType/body/callId) arriving within the dedup window —
    // only one row should be recorded.
    QCOMPARE(MessageHistoryStore::instance().count(), 1);
}

void TestMessageHistory::historyAppendInbound()
{
    QCOMPARE(MessageHistoryStore::instance().count(), 0);
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:a@x.com"), QStringLiteral("sip:b@x.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hi"), QStringLiteral("c1"), QString());
    QCOMPARE(MessageHistoryStore::instance().count(), 1);
    QCOMPARE(MessageHistoryStore::instance().snapshot().first().direction,
             MessageHistoryEntry::Direction::Inbound);
}

void TestMessageHistory::historyAppendOutbound()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.fromUri = QStringLiteral("sip:alice@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Ping");
    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);

    const qint64 id = MessageHistoryStore::instance().appendOutbound(msg);
    QVERIFY(id > 0);
    QCOMPARE(MessageHistoryStore::instance().count(), 1);

    const MessageHistoryEntry e = MessageHistoryStore::instance().snapshot().first();
    QCOMPARE(e.direction, MessageHistoryEntry::Direction::Outbound);
    QCOMPARE(e.peerUri, QStringLiteral("sip:bob@example.com"));
    QCOMPARE(e.outboundStatus, MessageHistoryEntry::OutboundStatus::Queued);
}

void TestMessageHistory::failedOutboundStatus()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Ping");
    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);

    const qint64 id = MessageHistoryStore::instance().appendOutbound(msg);

    QSignalSpy spy(&MessageHistoryStore::instance(), &MessageHistoryStore::entryUpdated);
    MessageHistoryStore::instance().updateOutboundStatus(id, MessageHistoryEntry::OutboundStatus::Failed);

    QCOMPARE(spy.count(), 1);
    const MessageHistoryEntry e = MessageHistoryStore::instance().snapshot().first();
    QCOMPARE(e.outboundStatus, MessageHistoryEntry::OutboundStatus::Failed);
}

void TestMessageHistory::statusUpgradesFromSubmittedToSent()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Ping");
    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    const qint64 id = MessageHistoryStore::instance().appendOutbound(msg);

    MessageHistoryStore::instance().updateOutboundStatus(id, MessageHistoryEntry::OutboundStatus::Submitted);
    QCOMPARE(MessageHistoryStore::instance().snapshot().first().outboundStatus,
             MessageHistoryEntry::OutboundStatus::Submitted);

    MessageHistoryStore::instance().updateOutboundStatus(id, MessageHistoryEntry::OutboundStatus::Sent);
    QCOMPARE(MessageHistoryStore::instance().snapshot().first().outboundStatus,
             MessageHistoryEntry::OutboundStatus::Sent);
}

void TestMessageHistory::filteringByDirectionAndFailed()
{
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:a@x.com"), QStringLiteral("sip:b@x.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("in-1"), QStringLiteral("c-in"), QString());

    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("out-1");
    const qint64 sentId = MessageHistoryStore::instance().appendOutbound(SipMessageComposer::compose(opts));
    MessageHistoryStore::instance().updateOutboundStatus(sentId, MessageHistoryEntry::OutboundStatus::Sent);

    opts.body = QStringLiteral("out-2 (will fail)");
    const qint64 failedId = MessageHistoryStore::instance().appendOutbound(SipMessageComposer::compose(opts));
    MessageHistoryStore::instance().updateOutboundStatus(failedId, MessageHistoryEntry::OutboundStatus::Failed);

    const QList<MessageHistoryEntry> all = MessageHistoryStore::instance().snapshot();
    QCOMPARE(all.size(), 3);

    int inboundCount = 0, outboundCount = 0, failedCount = 0;
    for (const auto &e : all) {
        if (e.direction == MessageHistoryEntry::Direction::Inbound) ++inboundCount;
        if (e.direction == MessageHistoryEntry::Direction::Outbound) ++outboundCount;
        if (e.outboundStatus == MessageHistoryEntry::OutboundStatus::Failed) ++failedCount;
    }
    QCOMPARE(inboundCount, 1);
    QCOMPARE(outboundCount, 2);
    QCOMPARE(failedCount, 1);
}

void TestMessageHistory::filteringByContentType()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.body = QStringLiteral("plain body");
    opts.contentType = MessagingContentKind::PlainText;
    MessageHistoryStore::instance().appendOutbound(SipMessageComposer::compose(opts));

    opts.body = QStringLiteral("<b>html body</b>");
    opts.contentType = MessagingContentKind::Html;
    MessageHistoryStore::instance().appendOutbound(SipMessageComposer::compose(opts));

    const QList<MessageHistoryEntry> all = MessageHistoryStore::instance().snapshot();
    QCOMPARE(all.size(), 2);

    int plainCount = 0;
    for (const auto &e : all) {
        if (e.contentType == QStringLiteral("text/plain; charset=utf-8"))
            ++plainCount;
    }
    QCOMPARE(plainCount, 1);
}

void TestMessageHistory::previewLimit()
{
    const QString longBody = QString(500, QLatin1Char('y'));
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:a@x.com"), QStringLiteral("sip:b@x.com"), QString(),
        QStringLiteral("text/plain"), longBody, QStringLiteral("c-long"), QString());

    const MessageHistoryEntry e = MessageHistoryStore::instance().snapshot().first();
    // Capped well below the raw 500-char body — large raw bodies never reach
    // the UI/history row directly, only a bounded preview.
    QVERIFY(e.bodyPreview.size() <= 201);
    QVERIFY(e.bodyPreview.size() < longBody.size());
}

void TestMessageHistory::utf8Body()
{
    const QString body = QString::fromUtf8("Bun\xc4\x83 ziua! \xe2\x98\x83"); // "Bună ziua! ☃"
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:a@x.com"), QStringLiteral("sip:b@x.com"), QString(),
        QStringLiteral("text/plain"), body, QStringLiteral("c-utf8"), QString());

    const MessageHistoryEntry e = MessageHistoryStore::instance().snapshot().first();
    QCOMPARE(e.bodyPreview, body);
}

void TestMessageHistory::inboundCapturesMessageIdAndDispositionNotification()
{
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:a@x.com"), QStringLiteral("sip:b@x.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hi"), QStringLiteral("c1"), QString(),
        QStringLiteral("msg-in-1"), QStringLiteral("positive-delivery, positive-display"));

    const MessageHistoryEntry e = MessageHistoryStore::instance().snapshot().first();
    QCOMPARE(e.messageId, QStringLiteral("msg-in-1"));
    QVERIFY(e.deliveryNotificationRequested);
    QVERIFY(e.displayNotificationRequested);
    QVERIFY(!e.deliveredImdnSent);
    QVERIFY(!e.displayedImdnSent);
}

void TestMessageHistory::outboundCapturesMessageIdWhenImdnRequested()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Ping");
    opts.requestImdn = true;
    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    QVERIFY(msg.valid);
    QVERIFY(!msg.messageId.isEmpty());

    MessageHistoryStore::instance().appendOutbound(msg);
    const MessageHistoryEntry e = MessageHistoryStore::instance().snapshot().first();
    QCOMPARE(e.messageId, msg.messageId);
    QCOMPARE(e.deliveryState, MessageHistoryEntry::DeliveryState::None);
}

void TestMessageHistory::correlateDeliveryUpgradesOutboundEntry()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Ping");
    opts.requestImdn = true;
    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    const qint64 id = MessageHistoryStore::instance().appendOutbound(msg);

    QSignalSpy spy(&MessageHistoryStore::instance(), &MessageHistoryStore::entryUpdated);
    MessageHistoryStore::instance().correlateDelivery(msg.messageId,
        MessageHistoryEntry::DeliveryState::Delivered);

    QCOMPARE(spy.count(), 1);
    const MessageHistoryEntry e = MessageHistoryStore::instance().entryById(id);
    QCOMPARE(e.deliveryState, MessageHistoryEntry::DeliveryState::Delivered);
}

void TestMessageHistory::correlateDeliveryDisplayedAfterDelivered()
{
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:bob@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("Ping");
    opts.requestImdn = true;
    const ComposedSipMessage msg = SipMessageComposer::compose(opts);
    const qint64 id = MessageHistoryStore::instance().appendOutbound(msg);

    MessageHistoryStore::instance().correlateDelivery(msg.messageId,
        MessageHistoryEntry::DeliveryState::Delivered);
    MessageHistoryStore::instance().correlateDelivery(msg.messageId,
        MessageHistoryEntry::DeliveryState::Displayed);

    QCOMPARE(MessageHistoryStore::instance().entryById(id).deliveryState,
             MessageHistoryEntry::DeliveryState::Displayed);
}

void TestMessageHistory::correlateDeliveryIgnoresUnknownMessageId()
{
    QSignalSpy spy(&MessageHistoryStore::instance(), &MessageHistoryStore::entryUpdated);
    MessageHistoryStore::instance().correlateDelivery(QStringLiteral("no-such-message-id"),
        MessageHistoryEntry::DeliveryState::Delivered);
    QCOMPARE(spy.count(), 0);
}

void TestMessageHistory::duplicateInboundImdnReportIsDeduped()
{
    for (int i = 0; i < 3; ++i) {
        MessageHistoryStore::instance().appendInboundImdn(
            QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"),
            QString(), QStringLiteral("<imdn>same report</imdn>"), QStringLiteral("call-imdn"),
            QString(), QStringLiteral("orig-msg-dup"));
    }
    QCOMPARE(MessageHistoryStore::instance().count(), 1);
    QVERIFY(MessageHistoryStore::instance().snapshot().first().isImdnReport);
}

void TestMessageHistory::markImdnSentSetsFlagsIndependently()
{
    const qint64 id = MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:a@x.com"), QStringLiteral("sip:b@x.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hi"), QStringLiteral("c1"), QString(),
        QStringLiteral("msg-in-2"), QStringLiteral("positive-delivery, positive-display"));

    MessageHistoryStore::instance().markImdnSent(id, ImdnInfo::Disposition::Delivered);
    MessageHistoryEntry e = MessageHistoryStore::instance().entryById(id);
    QVERIFY(e.deliveredImdnSent);
    QVERIFY(!e.displayedImdnSent);

    MessageHistoryStore::instance().markImdnSent(id, ImdnInfo::Disposition::Displayed);
    e = MessageHistoryStore::instance().entryById(id);
    QVERIFY(e.deliveredImdnSent);
    QVERIFY(e.displayedImdnSent);
}

void TestMessageHistory::appendInboundImdnStoresCorrelatedMessageId()
{
    const qint64 id = MessageHistoryStore::instance().appendInboundImdn(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"),
        QString(), QStringLiteral("<imdn>report</imdn>"), QStringLiteral("call-x"),
        QString(), QStringLiteral("orig-msg-9"));

    const MessageHistoryEntry e = MessageHistoryStore::instance().entryById(id);
    QVERIFY(e.isImdnReport);
    QCOMPARE(e.correlatedMessageId, QStringLiteral("orig-msg-9"));
    QCOMPARE(e.direction, MessageHistoryEntry::Direction::Inbound);
}

void TestMessageHistory::appendInboundTypingStoresState()
{
    const qint64 id = MessageHistoryStore::instance().appendInboundTyping(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"),
        QString(), QStringLiteral("<isComposing><state>active</state></isComposing>"),
        QStringLiteral("call-typing-1"), QString(), QStringLiteral("active"));

    const MessageHistoryEntry e = MessageHistoryStore::instance().entryById(id);
    QVERIFY(e.isTypingNotification);
    QCOMPARE(e.typingState, QStringLiteral("active"));
    QCOMPARE(e.direction, MessageHistoryEntry::Direction::Inbound);
    QCOMPARE(e.peerUri, QStringLiteral("sip:alice@example.com"));
}

void TestMessageHistory::duplicateInboundTypingIsDeduped()
{
    for (int i = 0; i < 3; ++i) {
        MessageHistoryStore::instance().appendInboundTyping(
            QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"),
            QString(), QStringLiteral("<isComposing><state>active</state></isComposing>"),
            QStringLiteral("call-typing-dup"), QString(), QStringLiteral("active"));
    }
    QCOMPARE(MessageHistoryStore::instance().count(), 1);
    QVERIFY(MessageHistoryStore::instance().snapshot().first().isTypingNotification);
}

QTEST_GUILESS_MAIN(TestMessageHistory)
#include "test_message_history.moc"
