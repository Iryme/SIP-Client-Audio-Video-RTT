#include <QtTest/QtTest>

#include "gui/panels/messaging/ConversationModel.h"
#include "sip/MessageHistoryStore.h"
#include "sip/MessagingContentKind.h"
#include "sip/SipMessageComposer.h"
#include "sip/TypingIndicatorController.h"

// Tests for ConversationModel (Task W111) — groups MessageHistoryStore's
// flat entry list by normalized peer URI for the Client Messaging View.
// Deliberately exercised against the real MessageHistoryStore singleton
// (cleared in init()/cleanup()), same pattern as test_message_history.cpp,
// since ConversationModel is a thin read-only view over it, not a
// self-contained store.

class TestConversationModel : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void normalizePeerFoldsCase();
    void conversationPeersTracksDistinctPeersOnly();
    void conversationListChangedEmittedOncePerNewPeer();
    void historyForFiltersByPeerOnly();
    void conversationUpdatedCarriesNormalizedPeer();
    void typingControllerForReturnsSameInstancePerPeer();
    void typingControllerForIsIndependentAcrossPeers();
    void typingSendRequestedCarriesOwningPeer();
    void remoteTypingStateReflectsMostRecentInboundEntry();
    void remoteTypingStateEmptyWhenNeverSeen();

    // Task W112 (Conversation Workspace): unread/last-message/last-activity.
    void lastMessageForReturnsMostRecentEntry();
    void lastMessageForDefaultWhenNoHistory();
    void lastActivityForMatchesLastMessageTimestamp();
    void unreadCountCountsAllInboundBeforeAnyRead();
    void unreadCountIgnoresOutboundEntries();
    void markReadZerosUnreadCount();
    void markReadDoesNotAffectOtherConversations();
    void newInboundAfterMarkReadIncrementsUnreadAgain();

    // Task W113F: protocol events (IMDN reports, is-composing notifications,
    // unsupported/malformed payloads) must never surface as chat bubbles,
    // unread count, or conversation preview.
    void userVisibleHistoryExcludesImdnReports();
    void userVisibleHistoryExcludesTypingNotifications();
    void userVisibleHistoryExcludesUnsupportedPayloads();
    void userVisibleHistoryKeepsPlainMessages();
    void historyForStillIncludesProtocolEvents();
    void lastMessageForSkipsTrailingImdnReport();
    void unreadCountIgnoresImdnReportsAndTypingNotifications();
};

void TestConversationModel::init()
{
    MessageHistoryStore::instance().clear();
}

void TestConversationModel::cleanup()
{
    MessageHistoryStore::instance().clear();
}

void TestConversationModel::normalizePeerFoldsCase()
{
    QCOMPARE(ConversationModel::normalizePeer(QStringLiteral(" sip:Alice@Example.com ")),
             ConversationModel::normalizePeer(QStringLiteral("sip:alice@example.com")));
}

void TestConversationModel::conversationPeersTracksDistinctPeersOnly()
{
    ConversationModel model;
    QCOMPARE(model.conversationPeers().size(), 0);

    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hi"), QStringLiteral("c1"), QString());
    QCOMPARE(model.conversationPeers().size(), 1);

    // Same peer, different case — must not create a second conversation.
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:Alice@Example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hi again"), QStringLiteral("c2"), QString());
    QCOMPARE(model.conversationPeers().size(), 1);

    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:carol@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hey"), QStringLiteral("c3"), QString());
    QCOMPARE(model.conversationPeers().size(), 2);
}

void TestConversationModel::conversationListChangedEmittedOncePerNewPeer()
{
    ConversationModel model;
    QSignalSpy spy(&model, &ConversationModel::conversationListChanged);

    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("m1"), QStringLiteral("c1"), QString());
    QCOMPARE(spy.count(), 1);

    // A second message from the SAME peer must not re-fire "new conversation".
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("m2"), QStringLiteral("c2"), QString());
    QCOMPARE(spy.count(), 1);
}

void TestConversationModel::historyForFiltersByPeerOnly()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("from alice"), QStringLiteral("c1"), QString());
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:carol@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("from carol"), QStringLiteral("c2"), QString());

    const auto aliceHistory = model.historyFor(QStringLiteral("sip:alice@example.com"));
    QCOMPARE(aliceHistory.size(), 1);
    QCOMPARE(aliceHistory.first().bodyPreview, QStringLiteral("from alice"));

    const auto carolHistory = model.historyFor(QStringLiteral("SIP:Carol@Example.com"));
    QCOMPARE(carolHistory.size(), 1);
    QCOMPARE(carolHistory.first().bodyPreview, QStringLiteral("from carol"));
}

void TestConversationModel::conversationUpdatedCarriesNormalizedPeer()
{
    ConversationModel model;
    QSignalSpy spy(&model, &ConversationModel::conversationUpdated);

    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:Alice@Example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hi"), QStringLiteral("c1"), QString());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), ConversationModel::normalizePeer(QStringLiteral("sip:Alice@Example.com")));
}

void TestConversationModel::typingControllerForReturnsSameInstancePerPeer()
{
    ConversationModel model;
    auto *first = model.typingControllerFor(QStringLiteral("sip:alice@example.com"));
    auto *second = model.typingControllerFor(QStringLiteral("sip:Alice@Example.com"));
    QVERIFY(first != nullptr);
    QCOMPARE(first, second);
}

void TestConversationModel::typingControllerForIsIndependentAcrossPeers()
{
    ConversationModel model;
    auto *alice = model.typingControllerFor(QStringLiteral("sip:alice@example.com"));
    auto *bob = model.typingControllerFor(QStringLiteral("sip:bob@example.com"));
    QVERIFY(alice != bob);

    // Typing in Alice's conversation must never affect Bob's controller
    // state (no duplicate/shared timers across conversations).
    alice->onTextChanged(true);
    QCOMPARE(alice->currentState(), IsComposingInfo::State::Active);
    QCOMPARE(bob->currentState(), IsComposingInfo::State::Unknown);
}

void TestConversationModel::typingSendRequestedCarriesOwningPeer()
{
    ConversationModel model;
    QSignalSpy spy(&model, &ConversationModel::typingSendRequested);

    auto *alice = model.typingControllerFor(QStringLiteral("sip:alice@example.com"));
    alice->onTextChanged(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(),
             ConversationModel::normalizePeer(QStringLiteral("sip:alice@example.com")));
    QCOMPARE(spy.at(0).at(1).value<IsComposingInfo::State>(), IsComposingInfo::State::Active);
}

void TestConversationModel::remoteTypingStateReflectsMostRecentInboundEntry()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInboundTyping(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("<isComposing><state>active</state></isComposing>"),
        QStringLiteral("c-typing-1"), QString(), QStringLiteral("active"));

    QCOMPARE(model.remoteTypingState(QStringLiteral("sip:alice@example.com")), QStringLiteral("active"));
}

void TestConversationModel::remoteTypingStateEmptyWhenNeverSeen()
{
    ConversationModel model;
    QVERIFY(model.remoteTypingState(QStringLiteral("sip:nobody@example.com")).isEmpty());
}

void TestConversationModel::lastMessageForReturnsMostRecentEntry()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("first"), QStringLiteral("c1"), QString());
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("second"), QStringLiteral("c2"), QString());

    QCOMPARE(model.lastMessageFor(QStringLiteral("sip:alice@example.com")).bodyPreview,
             QStringLiteral("second"));
}

void TestConversationModel::lastMessageForDefaultWhenNoHistory()
{
    ConversationModel model;
    QCOMPARE(model.lastMessageFor(QStringLiteral("sip:nobody@example.com")).id, qint64(0));
}

void TestConversationModel::lastActivityForMatchesLastMessageTimestamp()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hi"), QStringLiteral("c1"), QString());

    const auto last = model.lastMessageFor(QStringLiteral("sip:alice@example.com"));
    QCOMPARE(model.lastActivityFor(QStringLiteral("sip:alice@example.com")), last.timestamp);
}

void TestConversationModel::unreadCountCountsAllInboundBeforeAnyRead()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("m1"), QStringLiteral("c1"), QString());
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("m2"), QStringLiteral("c2"), QString());

    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:alice@example.com")), 2);
}

void TestConversationModel::unreadCountIgnoresOutboundEntries()
{
    ConversationModel model;
    SipMessageComposer::Options opts;
    opts.toUri = QStringLiteral("sip:alice@example.com");
    opts.contentType = MessagingContentKind::PlainText;
    opts.body = QStringLiteral("outbound only");
    MessageHistoryStore::instance().appendOutbound(SipMessageComposer::compose(opts));

    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:alice@example.com")), 0);
}

void TestConversationModel::markReadZerosUnreadCount()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("m1"), QStringLiteral("c1"), QString());

    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:alice@example.com")), 1);
    model.markRead(QStringLiteral("sip:alice@example.com"));
    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:alice@example.com")), 0);
}

void TestConversationModel::markReadDoesNotAffectOtherConversations()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("from alice"), QStringLiteral("c1"), QString());
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:carol@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("from carol"), QStringLiteral("c2"), QString());

    model.markRead(QStringLiteral("sip:alice@example.com"));

    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:alice@example.com")), 0);
    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:carol@example.com")), 1);
}

void TestConversationModel::newInboundAfterMarkReadIncrementsUnreadAgain()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("m1"), QStringLiteral("c1"), QString());
    model.markRead(QStringLiteral("sip:alice@example.com"));
    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:alice@example.com")), 0);

    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("m2"), QStringLiteral("c2"), QString());
    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:alice@example.com")), 1);
}

void TestConversationModel::userVisibleHistoryExcludesImdnReports()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hello"), QStringLiteral("c1"), QString());
    MessageHistoryStore::instance().appendInboundImdn(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("<imdn/>"), QStringLiteral("c2"), QString(), QStringLiteral("msg-1"));

    const auto visible = model.userVisibleHistoryFor(QStringLiteral("sip:alice@example.com"));
    QCOMPARE(visible.size(), 1);
    QCOMPARE(visible.first().bodyPreview, QStringLiteral("hello"));
}

void TestConversationModel::userVisibleHistoryExcludesTypingNotifications()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hello"), QStringLiteral("c1"), QString());
    MessageHistoryStore::instance().appendInboundTyping(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("<isComposing/>"), QStringLiteral("c2"), QString(), QStringLiteral("active"));

    const auto visible = model.userVisibleHistoryFor(QStringLiteral("sip:alice@example.com"));
    QCOMPARE(visible.size(), 1);
    QCOMPARE(visible.first().bodyPreview, QStringLiteral("hello"));
}

void TestConversationModel::userVisibleHistoryExcludesUnsupportedPayloads()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hello"), QStringLiteral("c1"), QString());
    MessageHistoryStore::instance().appendInboundUnsupported(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("message/cpim"), QStringLiteral("c2"), QString());

    const auto visible = model.userVisibleHistoryFor(QStringLiteral("sip:alice@example.com"));
    QCOMPARE(visible.size(), 1);
    QCOMPARE(visible.first().bodyPreview, QStringLiteral("hello"));
}

void TestConversationModel::userVisibleHistoryKeepsPlainMessages()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("hi"), QStringLiteral("c1"), QString());
    QCOMPARE(model.userVisibleHistoryFor(QStringLiteral("sip:alice@example.com")).size(), 1);
}

void TestConversationModel::historyForStillIncludesProtocolEvents()
{
    // historyFor() stays unfiltered -- remoteTypingState() and Tools' full
    // Message History table both rely on seeing every row.
    ConversationModel model;
    MessageHistoryStore::instance().appendInboundTyping(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("<isComposing/>"), QStringLiteral("c1"), QString(), QStringLiteral("active"));
    QCOMPARE(model.historyFor(QStringLiteral("sip:alice@example.com")).size(), 1);
}

void TestConversationModel::lastMessageForSkipsTrailingImdnReport()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("real message"), QStringLiteral("c1"), QString());
    // An IMDN report arrives after the real message and would otherwise
    // become the "most recent entry" for preview purposes.
    MessageHistoryStore::instance().appendInboundImdn(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("<imdn/>"), QStringLiteral("c2"), QString(), QStringLiteral("msg-1"));

    QCOMPARE(model.lastMessageFor(QStringLiteral("sip:alice@example.com")).bodyPreview,
             QStringLiteral("real message"));
}

void TestConversationModel::unreadCountIgnoresImdnReportsAndTypingNotifications()
{
    ConversationModel model;
    MessageHistoryStore::instance().appendInbound(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("text/plain"), QStringLiteral("real message"), QStringLiteral("c1"), QString());
    MessageHistoryStore::instance().appendInboundImdn(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("<imdn/>"), QStringLiteral("c2"), QString(), QStringLiteral("msg-1"));
    MessageHistoryStore::instance().appendInboundTyping(
        QStringLiteral("sip:alice@example.com"), QStringLiteral("sip:bob@example.com"), QString(),
        QStringLiteral("<isComposing/>"), QStringLiteral("c3"), QString(), QStringLiteral("active"));

    QCOMPARE(model.unreadCountFor(QStringLiteral("sip:alice@example.com")), 1);
}

QTEST_GUILESS_MAIN(TestConversationModel)
#include "test_conversation_model.moc"
