#include <QtTest/QtTest>

#include "gui/panels/messaging/ConversationFilterProxyModel.h"
#include "gui/panels/messaging/ConversationListModel.h"

// Tests for ConversationListModel/ConversationFilterProxyModel (Task W112).
// Both are dependency-free (fed via setRows(), same pattern as
// CallHistoryListModel) — rows are synthesized directly here rather than
// derived from ContactStore/ConversationModel/PresenceStore/SipManager
// (that derivation lives in ConversationWorkspacePanel, a QWidget, and is
// exercised via manual GUI testing instead — see the W112 agent-result).

class TestConversationListModel : public QObject
{
    Q_OBJECT

private slots:
    void rowCountMatchesSetRows();
    void dataReturnsFieldsByRole();
    void filterMatchesDisplayNameUriOrPreview();
    void emptyFilterAcceptsEverything();
    void pinnedRowsSortFirst();
    void unpinnedRowsSortByLastActivityDescending();
};

namespace {
ConversationRow makeRow(const QString &peer, const QString &name, const QString &preview,
                        const QDateTime &activity, bool pinned = false, int unread = 0)
{
    ConversationRow row;
    row.peerUri = peer;
    row.displayName = name;
    row.lastMessagePreview = preview;
    row.lastActivity = activity;
    row.pinned = pinned;
    row.unreadCount = unread;
    return row;
}
} // namespace

void TestConversationListModel::rowCountMatchesSetRows()
{
    ConversationListModel model;
    QCOMPARE(model.rowCount(), 0);

    model.setRows({ makeRow(QStringLiteral("sip:a@x.com"), QStringLiteral("Alice"), QString(), QDateTime()),
                    makeRow(QStringLiteral("sip:b@x.com"), QStringLiteral("Bob"), QString(), QDateTime()) });
    QCOMPARE(model.rowCount(), 2);
}

void TestConversationListModel::dataReturnsFieldsByRole()
{
    ConversationListModel model;
    model.setRows({ makeRow(QStringLiteral("sip:a@x.com"), QStringLiteral("Alice"),
                            QStringLiteral("hello"), QDateTime::currentDateTime(), true, 3) });

    const QModelIndex idx = model.index(0, 0);
    QCOMPARE(idx.data(ConversationListModel::PeerUriRole).toString(), QStringLiteral("sip:a@x.com"));
    QCOMPARE(idx.data(ConversationListModel::DisplayNameRole).toString(), QStringLiteral("Alice"));
    QCOMPARE(idx.data(ConversationListModel::LastMessagePreviewRole).toString(), QStringLiteral("hello"));
    QCOMPARE(idx.data(ConversationListModel::UnreadCountRole).toInt(), 3);
    QCOMPARE(idx.data(ConversationListModel::PinnedRole).toBool(), true);
    QCOMPARE(idx.data(Qt::DisplayRole).toString(), QStringLiteral("Alice"));
}

void TestConversationListModel::filterMatchesDisplayNameUriOrPreview()
{
    ConversationListModel model;
    model.setRows({ makeRow(QStringLiteral("sip:alice@example.com"), QStringLiteral("Alice"),
                            QStringLiteral("see you soon"), QDateTime::currentDateTime()),
                    makeRow(QStringLiteral("sip:carol@example.com"), QStringLiteral("Carol"),
                            QStringLiteral("meeting notes"), QDateTime::currentDateTime()) });

    ConversationFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setSearchText(QStringLiteral("alice"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(ConversationListModel::DisplayNameRole).toString(),
             QStringLiteral("Alice"));

    proxy.setSearchText(QStringLiteral("meeting"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(ConversationListModel::DisplayNameRole).toString(),
             QStringLiteral("Carol"));

    proxy.setSearchText(QStringLiteral("carol@example.com"));
    QCOMPARE(proxy.rowCount(), 1);
}

void TestConversationListModel::emptyFilterAcceptsEverything()
{
    ConversationListModel model;
    model.setRows({ makeRow(QStringLiteral("sip:a@x.com"), QStringLiteral("Alice"), QString(), QDateTime()),
                    makeRow(QStringLiteral("sip:b@x.com"), QStringLiteral("Bob"), QString(), QDateTime()) });

    ConversationFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSearchText(QString());
    QCOMPARE(proxy.rowCount(), 2);
}

void TestConversationListModel::pinnedRowsSortFirst()
{
    ConversationListModel model;
    const QDateTime now = QDateTime::currentDateTime();
    model.setRows({ makeRow(QStringLiteral("sip:a@x.com"), QStringLiteral("Alice"), QString(),
                            now.addSecs(-10), false),
                    makeRow(QStringLiteral("sip:b@x.com"), QStringLiteral("Bob"), QString(),
                            now.addSecs(-100), true) });

    ConversationFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    // Bob is pinned but has older activity than Alice — pinned still wins.
    QCOMPARE(proxy.index(0, 0).data(ConversationListModel::DisplayNameRole).toString(),
             QStringLiteral("Bob"));
    QCOMPARE(proxy.index(1, 0).data(ConversationListModel::DisplayNameRole).toString(),
             QStringLiteral("Alice"));
}

void TestConversationListModel::unpinnedRowsSortByLastActivityDescending()
{
    ConversationListModel model;
    const QDateTime now = QDateTime::currentDateTime();
    model.setRows({ makeRow(QStringLiteral("sip:old@x.com"), QStringLiteral("Old"), QString(),
                            now.addSecs(-1000)),
                    makeRow(QStringLiteral("sip:new@x.com"), QStringLiteral("New"), QString(),
                            now) });

    ConversationFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    QCOMPARE(proxy.index(0, 0).data(ConversationListModel::DisplayNameRole).toString(),
             QStringLiteral("New"));
    QCOMPARE(proxy.index(1, 0).data(ConversationListModel::DisplayNameRole).toString(),
             QStringLiteral("Old"));
}

QTEST_GUILESS_MAIN(TestConversationListModel)
#include "test_conversation_list_model.moc"
