#include "MessagingDiagnosticsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextStream>
#include <QFile>

#include "core/AppSettings.h"
#include "gui/MessagingMessageDetailsDialog.h"
#include "sip/InteropTraceExporter.h"
#include "sip/MessageHistoryStore.h"
#include "sip/MessagingContentKind.h"
#include "sip/MessagingDiagnosticsStore.h"
#include "sip/MessagingEventStore.h"
#include "sip/SipManager.h"
#include "sip/SipMessageComposer.h"
#include "sip/SipProfileManager.h"

namespace {
QString normalizedKey(const QString &value)
{
    return value.trimmed().toLower();
}
} // namespace

MessagingDiagnosticsPage::MessagingDiagnosticsPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("MessagingDiagnosticsPage");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *title = new QLabel(tr("Messaging Diagnostics"), this);
    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    root->addWidget(title);

    auto *note = new QLabel(
        tr("Read-only diagnostics for SIP MESSAGE, CPIM, IMDN, is-composing and MSRP/SDP "
           "attributes, backed by the transport-independent messaging event store. MSRP "
           "sessions are never started from this feature — detection and logging only."),
        this);
    note->setWordWrap(true);
    note->setStyleSheet("color: #b7c4d6;");
    root->addWidget(note);

    // ---- SIP MESSAGE composer (Task W092) ---------------------------------
    auto *composeGroup = new QGroupBox(tr("Send SIP MESSAGE"), this);
    auto *composeLayout = new QVBoxLayout(composeGroup);

    auto *destRow = new QHBoxLayout();
    m_toUriEdit = new QLineEdit(composeGroup);
    m_toUriEdit->setPlaceholderText(tr("Recipient SIP URI, e.g. sip:user@example.com"));
    destRow->addWidget(new QLabel(tr("To:"), composeGroup));
    destRow->addWidget(m_toUriEdit, 1);

    m_composeContentType = new QComboBox(composeGroup);
    m_composeContentType->addItem(QStringLiteral("text/plain"),
                                   static_cast<int>(MessagingContentKind::PlainText));
    m_composeContentType->addItem(QStringLiteral("text/html"),
                                   static_cast<int>(MessagingContentKind::Html));
    m_composeContentType->addItem(QStringLiteral("message/cpim"),
                                   static_cast<int>(MessagingContentKind::Cpim));
    destRow->addWidget(new QLabel(tr("Content-Type:"), composeGroup));
    destRow->addWidget(m_composeContentType);
    composeLayout->addLayout(destRow);

    m_bodyEdit = new QPlainTextEdit(composeGroup);
    m_bodyEdit->setPlaceholderText(tr("Message body"));
    m_bodyEdit->setFixedHeight(70);
    composeLayout->addWidget(m_bodyEdit);

    auto *optionsRow = new QHBoxLayout();
    m_enableSipMessageCheck = new QCheckBox(tr("Enable SIP MESSAGE"), composeGroup);
    m_enableCpimCheck       = new QCheckBox(tr("Enable CPIM"), composeGroup);
    m_requestImdnCheck      = new QCheckBox(tr("Request IMDN"), composeGroup);
    m_enableSipMessageCheck->setChecked(AppSettings::enableSipMessage());
    m_enableCpimCheck->setChecked(AppSettings::enableCpim());
    m_requestImdnCheck->setChecked(AppSettings::requestImdnByDefault());
    optionsRow->addWidget(m_enableSipMessageCheck);
    optionsRow->addWidget(m_enableCpimCheck);
    optionsRow->addWidget(m_requestImdnCheck);
    optionsRow->addStretch(1);
    m_sendBtn = new QPushButton(tr("Send"), composeGroup);
    optionsRow->addWidget(m_sendBtn);
    composeLayout->addLayout(optionsRow);

    m_sendStatusLabel = new QLabel(composeGroup);
    m_sendStatusLabel->setWordWrap(true);
    composeLayout->addWidget(m_sendStatusLabel);

    root->addWidget(composeGroup);

    connect(m_enableSipMessageCheck, &QCheckBox::toggled,
            this, &MessagingDiagnosticsPage::onEnableSipMessageToggled);
    connect(m_enableCpimCheck, &QCheckBox::toggled,
            this, &MessagingDiagnosticsPage::onEnableCpimToggled);
    connect(m_requestImdnCheck, &QCheckBox::toggled,
            this, &MessagingDiagnosticsPage::onRequestImdnToggled);
    connect(m_toUriEdit, &QLineEdit::textChanged, this, &MessagingDiagnosticsPage::updateSendEnabled);
    connect(m_bodyEdit, &QPlainTextEdit::textChanged, this, &MessagingDiagnosticsPage::updateSendEnabled);
    connect(m_sendBtn, &QPushButton::clicked, this, &MessagingDiagnosticsPage::onSendClicked);

    // ---- Message History (Task W093) ---------------------------------------
    auto *historyGroup = new QGroupBox(tr("Message History"), this);
    auto *historyLayout = new QVBoxLayout(historyGroup);

    auto *historyToolbar = new QHBoxLayout();
    m_historyDirectionFilter = new QComboBox(historyGroup);
    m_historyDirectionFilter->addItem(tr("All"), QStringLiteral("all"));
    m_historyDirectionFilter->addItem(tr("Inbound"), QStringLiteral("inbound"));
    m_historyDirectionFilter->addItem(tr("Outbound"), QStringLiteral("outbound"));
    m_historyDirectionFilter->addItem(tr("Failed"), QStringLiteral("failed"));
    historyToolbar->addWidget(m_historyDirectionFilter);

    m_historyContentTypeFilter = new QComboBox(historyGroup);
    m_historyContentTypeFilter->addItem(tr("All content types"), QString());
    m_historyContentTypeFilter->addItem(QStringLiteral("text/plain; charset=utf-8"),
                                        QStringLiteral("text/plain; charset=utf-8"));
    m_historyContentTypeFilter->addItem(QStringLiteral("text/html; charset=utf-8"),
                                        QStringLiteral("text/html; charset=utf-8"));
    m_historyContentTypeFilter->addItem(QStringLiteral("message/cpim"),
                                        QStringLiteral("message/cpim"));
    historyToolbar->addWidget(m_historyContentTypeFilter);
    historyToolbar->addStretch(1);

    m_clearHistoryBtn = new QPushButton(tr("Clear History"), historyGroup);
    historyToolbar->addWidget(m_clearHistoryBtn);
    historyLayout->addLayout(historyToolbar);

    m_historyTable = new QTableWidget(0, 6, historyGroup);
    m_historyTable->setHorizontalHeaderLabels({
        tr("Time"), tr("Dir"), tr("Peer"), tr("Content-Type"), tr("Preview"), tr("Status")
    });
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyLayout->addWidget(m_historyTable);

    root->addWidget(historyGroup);

    connect(m_historyDirectionFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MessagingDiagnosticsPage::applyHistoryFilters);
    connect(m_historyContentTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MessagingDiagnosticsPage::applyHistoryFilters);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, &MessagingDiagnosticsPage::onClearHistory);
    connect(&MessageHistoryStore::instance(), &MessageHistoryStore::entryAppended,
            this, &MessagingDiagnosticsPage::onHistoryEntryAppended);
    connect(&MessageHistoryStore::instance(), &MessageHistoryStore::entryUpdated,
            this, &MessagingDiagnosticsPage::onHistoryEntryUpdated);
    connect(&MessageHistoryStore::instance(), &MessageHistoryStore::cleared,
            this, &MessagingDiagnosticsPage::onHistoryCleared);

    m_historyEntries = MessageHistoryStore::instance().snapshot();
    rebuildHistoryTable();

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);

    m_callIdFilter = new QLineEdit(this);
    m_callIdFilter->setPlaceholderText(tr("Call-ID"));
    toolbar->addWidget(m_callIdFilter, 1);

    m_kindFilter = new QComboBox(this);
    m_kindFilter->addItem(tr("All payload types"), QString());
    m_kindFilter->addItem(tr("plain"), QStringLiteral("plain"));
    m_kindFilter->addItem(tr("html"), QStringLiteral("html"));
    m_kindFilter->addItem(tr("cpim"), QStringLiteral("cpim"));
    m_kindFilter->addItem(tr("imdn"), QStringLiteral("imdn"));
    m_kindFilter->addItem(tr("is-composing"), QStringLiteral("is-composing"));
    m_kindFilter->addItem(tr("sdp"), QStringLiteral("sdp"));
    m_kindFilter->addItem(tr("rcs-ft-http"), QStringLiteral("rcs-ft-http"));
    toolbar->addWidget(m_kindFilter);

    m_directionFilter = new QComboBox(this);
    m_directionFilter->addItem(tr("All directions"), QString());
    m_directionFilter->addItem(tr("Outbound"), QStringLiteral("outbound"));
    m_directionFilter->addItem(tr("Inbound"), QStringLiteral("inbound"));
    toolbar->addWidget(m_directionFilter);

    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_exportTextBtn = new QPushButton(tr("Export Text"), this);
    m_exportJsonBtn = new QPushButton(tr("Export JSON"), this);
    m_exportInteropJsonBtn = new QPushButton(tr("Export Interop JSON"), this);
    m_exportInteropJsonBtn->setToolTip(
        tr("Export in the server-compatible schema used by "
           "scripts/interop/compare-client-server-trace.py (SIP-Server-RTT)."));
    toolbar->addWidget(m_clearBtn);
    toolbar->addWidget(m_exportTextBtn);
    toolbar->addWidget(m_exportJsonBtn);
    toolbar->addWidget(m_exportInteropJsonBtn);
    root->addLayout(toolbar);

    m_table = new QTableWidget(0, 10, this);
    m_table->setHorizontalHeaderLabels({
        tr("Time"), tr("Dir"), tr("Transport"), tr("From"),
        tr("To"), tr("Call-ID"), tr("Content-Type"), tr("Preview"), tr("Parse"), tr("Encoding")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);

    connect(m_callIdFilter, &QLineEdit::textChanged, this, &MessagingDiagnosticsPage::applyFilters);
    connect(m_kindFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MessagingDiagnosticsPage::applyFilters);
    connect(m_directionFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MessagingDiagnosticsPage::applyFilters);
    connect(m_clearBtn, &QPushButton::clicked, this, &MessagingDiagnosticsPage::onClear);
    connect(m_exportTextBtn, &QPushButton::clicked, this, &MessagingDiagnosticsPage::onExportText);
    connect(m_exportJsonBtn, &QPushButton::clicked, this, &MessagingDiagnosticsPage::onExportJson);
    connect(m_exportInteropJsonBtn, &QPushButton::clicked,
            this, &MessagingDiagnosticsPage::onExportInteropJson);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &MessagingDiagnosticsPage::onRowActivated);

    // Data comes exclusively from MessagingEventStore (Task W091), which
    // maps entries from MessagingDiagnosticsStore (Task W090) — this page
    // does not talk to MessagingDiagnosticsStore for its row data, only to
    // resolve structured CPIM/IMDN/is-composing/SDP-MSRP detail on demand
    // (see onRowActivated).
    connect(&MessagingEventStore::instance(), &MessagingEventStore::eventAppended,
            this, &MessagingDiagnosticsPage::onEventAppended);
    connect(&MessagingEventStore::instance(), &MessagingEventStore::cleared,
            this, &MessagingDiagnosticsPage::onCleared);

    m_events = MessagingEventStore::instance().snapshot();
    rebuildTable();
    updateSendEnabled();
}

void MessagingDiagnosticsPage::onEventAppended(const MessagingEvent &event)
{
    m_events.append(event);
    if (passesFilters(event))
        addRow(event);
}

void MessagingDiagnosticsPage::onCleared()
{
    m_events.clear();
    m_table->setRowCount(0);
}

bool MessagingDiagnosticsPage::passesFilters(const MessagingEvent &event) const
{
    const QString callIdFilter = normalizedKey(m_callIdFilter->text());
    if (!callIdFilter.isEmpty() && !normalizedKey(event.callId).contains(callIdFilter))
        return false;

    const QString kindFilter = m_kindFilter->currentData().toString();
    if (!kindFilter.isEmpty()
        && MessagingEvent::payloadTypeToString(event.payloadType).compare(
               kindFilter, Qt::CaseInsensitive) != 0)
        return false;

    const QString directionFilter = m_directionFilter->currentData().toString();
    if (!directionFilter.isEmpty()
        && MessagingEvent::directionToString(event.direction) != directionFilter)
        return false;

    return true;
}

void MessagingDiagnosticsPage::applyFilters()
{
    rebuildTable();
}

void MessagingDiagnosticsPage::rebuildTable()
{
    m_table->setRowCount(0);
    for (const MessagingEvent &event : m_events) {
        if (passesFilters(event))
            addRow(event);
    }
}

void MessagingDiagnosticsPage::addRow(const MessagingEvent &event)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto setCell = [this, row](int col, const QString &text) {
        auto *item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, col, item);
    };

    setCell(0, event.timestamp.toString(QStringLiteral("hh:mm:ss.zzz")));
    setCell(1, event.direction == MessagingEvent::Direction::Outbound ? tr("Out")
             : event.direction == MessagingEvent::Direction::Inbound ? tr("In") : tr("?"));
    setCell(2, MessagingEvent::transportToString(event.transport));
    setCell(3, event.from);
    setCell(4, event.to);
    setCell(5, event.callId);
    setCell(6, event.contentType.isEmpty()
        ? MessagingEvent::payloadTypeToString(event.payloadType)
        : event.contentType);
    setCell(7, event.bodyPreview);

    // Parse warnings are surfaced as a compact cell (with full text in the
    // tooltip) rather than a blocking dialog/message box, so a malformed
    // body never interrupts the UI thread or the live feed.
    const QString parseStatus = MessagingEvent::parseStatusToString(event.parseStatus);
    const QString warningCount = event.parseWarnings.isEmpty()
        ? QString() : QStringLiteral(" (%1)").arg(event.parseWarnings.size());
    setCell(8, parseStatus + warningCount);
    if (!event.parseWarnings.isEmpty())
        m_table->item(row, 8)->setToolTip(event.parseWarnings.join(QStringLiteral("\n")));

    // Content-Encoding diagnostics (Task W095) — compact "deflate: decoded
    // (zlib, 42 -> 128 B)" style summary, empty for the (overwhelming
    // majority of) events with no Content-Encoding at all.
    if (event.contentEncoding.isEmpty()) {
        setCell(9, QString());
    } else {
        QString summary = QStringLiteral("%1: %2").arg(event.contentEncoding, event.decodeStatus);
        if (event.decodeStatus == QStringLiteral("decoded")) {
            summary += QStringLiteral(" (%1, %2 -> %3 B)")
                           .arg(event.decodeVariant)
                           .arg(event.compressedBodyLength)
                           .arg(event.decodedBodyLength);
        }
        setCell(9, summary);
        if (!event.decodeError.isEmpty())
            m_table->item(row, 9)->setToolTip(event.decodeError);
    }

    // Stash the event id so onRowActivated can resolve the structured
    // CPIM/IMDN/is-composing/SDP-MSRP detail from MessagingDiagnosticsStore.
    m_table->item(row, 0)->setData(Qt::UserRole, static_cast<qlonglong>(event.id));
}

void MessagingDiagnosticsPage::onRowActivated(int row, int /*column*/)
{
    if (row < 0 || row >= m_table->rowCount())
        return;

    QTableWidgetItem *item = m_table->item(row, 0);
    if (!item)
        return;

    // MessagingEvent ids are assigned sequentially starting at 1 and stay
    // aligned with MessagingDiagnosticsStore's own (never independently
    // evicted) entry list — both stores are cleared together, from the same
    // Clear action. See MessagingEventStore::mapFromTraceEntry / clear().
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    const int index = static_cast<int>(id) - 1;
    const auto &traceEntries = MessagingDiagnosticsStore::instance().entries();
    if (index < 0 || index >= traceEntries.size())
        return;

    auto *dlg = new MessagingMessageDetailsDialog(this);
    dlg->setEntry(traceEntries.at(index));
    dlg->open();
}

void MessagingDiagnosticsPage::onClear()
{
    // Single source of truth: clearing MessagingDiagnosticsStore cascades to
    // MessagingEventStore (connected in its constructor), which in turn
    // notifies this page via the "cleared" signal.
    MessagingDiagnosticsStore::instance().clear();
}

void MessagingDiagnosticsPage::onExportText()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Messaging Diagnostics"), QStringLiteral("messaging-diagnostics.txt"),
        tr("Text files (*.txt);;All (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text))
        return;
    QTextStream(&f) << MessagingEventStore::instance().exportToText();
}

void MessagingDiagnosticsPage::onExportJson()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Messaging Diagnostics"), QStringLiteral("messaging-diagnostics.json"),
        tr("JSON files (*.json);;All (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text))
        return;
    QTextStream(&f) << MessagingEventStore::instance().exportToJson();
}

void MessagingDiagnosticsPage::onExportInteropJson()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Interop-Compatible Messaging/MSRP Trace"),
        QStringLiteral("windows-trace-export.json"),
        tr("JSON files (*.json);;All (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text))
        return;
    QTextStream(&f) << InteropTraceExporter::exportToJson();
}

void MessagingDiagnosticsPage::onEnableSipMessageToggled(bool on)
{
    AppSettings::setEnableSipMessage(on);
    updateSendEnabled();
}

void MessagingDiagnosticsPage::onEnableCpimToggled(bool on)
{
    AppSettings::setEnableCpim(on);
    if (!on && m_composeContentType->currentData().toInt()
                   == static_cast<int>(MessagingContentKind::Cpim)) {
        m_composeContentType->setCurrentIndex(0); // fall back to text/plain
    }
}

void MessagingDiagnosticsPage::onRequestImdnToggled(bool on)
{
    AppSettings::setRequestImdnByDefault(on);
}

void MessagingDiagnosticsPage::updateSendEnabled()
{
    const bool enabled = m_enableSipMessageCheck->isChecked()
        && !m_toUriEdit->text().trimmed().isEmpty()
        && !m_bodyEdit->toPlainText().trimmed().isEmpty();
    m_sendBtn->setEnabled(enabled);
}

void MessagingDiagnosticsPage::onSendClicked()
{
    SipMessageComposer::Options opts;
    opts.toUri = m_toUriEdit->text();
    const SipProfile activeProfile = SipProfileManager::instance().activeProfile();
    opts.fromUri = activeProfile.isNull() ? QString() : activeProfile.effectiveSipUri();
    opts.contentType = static_cast<MessagingContentKind>(m_composeContentType->currentData().toInt());
    opts.body = m_bodyEdit->toPlainText();
    opts.cpimEnabled = m_enableCpimCheck->isChecked();
    opts.requestImdn = m_requestImdnCheck->isChecked();

    const ComposedSipMessage composed = SipMessageComposer::compose(opts);
    if (!composed.valid) {
        m_sendStatusLabel->setText(tr("Not sent: %1").arg(composed.error));
        m_sendStatusLabel->setStyleSheet(QStringLiteral("color: #e08080;"));
        return;
    }

    QString error;
    const bool ok = SipManager::instance().sendSipMessage(composed, error);
    if (ok) {
        m_sendStatusLabel->setText(tr("Submitted to %1").arg(composed.toUri));
        m_sendStatusLabel->setStyleSheet(QStringLiteral("color: #7fd08a;"));
        m_bodyEdit->clear();
    } else {
        m_sendStatusLabel->setText(tr("Send failed: %1").arg(error));
        m_sendStatusLabel->setStyleSheet(QStringLiteral("color: #e08080;"));
    }
}

namespace {
QString historyStatusText(const MessageHistoryEntry &entry)
{
    if (entry.direction == MessageHistoryEntry::Direction::Inbound)
        return QObject::tr("received");
    return MessageHistoryEntry::outboundStatusToString(entry.outboundStatus);
}
} // namespace

void MessagingDiagnosticsPage::onHistoryEntryAppended(const MessageHistoryEntry &entry)
{
    m_historyEntries.append(entry);
    if (passesHistoryFilters(entry))
        addHistoryRow(entry);
}

void MessagingDiagnosticsPage::onHistoryEntryUpdated(const MessageHistoryEntry &entry)
{
    for (int i = 0; i < m_historyEntries.size(); ++i) {
        if (m_historyEntries.at(i).id == entry.id) {
            m_historyEntries[i] = entry;
            break;
        }
    }
    // Status is the only field that changes post-append; find the row (if
    // currently visible under the active filters) and update its cell
    // in place rather than rebuilding the whole table.
    for (int row = 0; row < m_historyTable->rowCount(); ++row) {
        QTableWidgetItem *item = m_historyTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toLongLong() == entry.id) {
            m_historyTable->item(row, 5)->setText(historyStatusText(entry));
            return;
        }
    }
    // Row not currently shown (e.g. filtered out) — nothing further to do.
}

void MessagingDiagnosticsPage::onHistoryCleared()
{
    m_historyEntries.clear();
    m_historyTable->setRowCount(0);
}

bool MessagingDiagnosticsPage::passesHistoryFilters(const MessageHistoryEntry &entry) const
{
    const QString directionFilter = m_historyDirectionFilter->currentData().toString();
    if (directionFilter == QStringLiteral("inbound")
        && entry.direction != MessageHistoryEntry::Direction::Inbound)
        return false;
    if (directionFilter == QStringLiteral("outbound")
        && entry.direction != MessageHistoryEntry::Direction::Outbound)
        return false;
    if (directionFilter == QStringLiteral("failed")
        && entry.outboundStatus != MessageHistoryEntry::OutboundStatus::Failed)
        return false;

    const QString contentTypeFilter = m_historyContentTypeFilter->currentData().toString();
    if (!contentTypeFilter.isEmpty() && entry.contentType != contentTypeFilter)
        return false;

    return true;
}

void MessagingDiagnosticsPage::applyHistoryFilters()
{
    rebuildHistoryTable();
}

void MessagingDiagnosticsPage::rebuildHistoryTable()
{
    m_historyTable->setRowCount(0);
    for (const MessageHistoryEntry &entry : m_historyEntries) {
        if (passesHistoryFilters(entry))
            addHistoryRow(entry);
    }
}

void MessagingDiagnosticsPage::addHistoryRow(const MessageHistoryEntry &entry)
{
    const int row = m_historyTable->rowCount();
    m_historyTable->insertRow(row);

    auto setCell = [this, row](int col, const QString &text) {
        auto *item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_historyTable->setItem(row, col, item);
    };

    setCell(0, entry.timestamp.toString(QStringLiteral("hh:mm:ss.zzz")));
    setCell(1, entry.direction == MessageHistoryEntry::Direction::Outbound ? tr("Out") : tr("In"));
    setCell(2, entry.peerUri);
    setCell(3, entry.contentType);
    setCell(4, entry.bodyPreview);
    setCell(5, historyStatusText(entry));

    m_historyTable->item(row, 0)->setData(Qt::UserRole, static_cast<qlonglong>(entry.id));
}

void MessagingDiagnosticsPage::onClearHistory()
{
    MessageHistoryStore::instance().clear();
}
