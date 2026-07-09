#include "MessagingDiagnosticsPage.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextStream>
#include <QFile>

#include "gui/MessagingMessageDetailsDialog.h"
#include "sip/MessagingDiagnosticsStore.h"
#include "sip/MessagingEventStore.h"

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
           "attributes, backed by the transport-independent messaging event store. No MSRP "
           "session is ever started from this view — detection and logging only."),
        this);
    note->setWordWrap(true);
    note->setStyleSheet("color: #b7c4d6;");
    root->addWidget(note);

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
    toolbar->addWidget(m_kindFilter);

    m_directionFilter = new QComboBox(this);
    m_directionFilter->addItem(tr("All directions"), QString());
    m_directionFilter->addItem(tr("Outbound"), QStringLiteral("outbound"));
    m_directionFilter->addItem(tr("Inbound"), QStringLiteral("inbound"));
    toolbar->addWidget(m_directionFilter);

    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_exportTextBtn = new QPushButton(tr("Export Text"), this);
    m_exportJsonBtn = new QPushButton(tr("Export JSON"), this);
    toolbar->addWidget(m_clearBtn);
    toolbar->addWidget(m_exportTextBtn);
    toolbar->addWidget(m_exportJsonBtn);
    root->addLayout(toolbar);

    m_table = new QTableWidget(0, 9, this);
    m_table->setHorizontalHeaderLabels({
        tr("Time"), tr("Dir"), tr("Transport"), tr("From"),
        tr("To"), tr("Call-ID"), tr("Content-Type"), tr("Preview"), tr("Parse")
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
