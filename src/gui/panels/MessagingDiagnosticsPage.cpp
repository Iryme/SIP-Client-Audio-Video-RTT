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
#include <QVBoxLayout>
#include <QFile>

#include "gui/MessagingMessageDetailsDialog.h"
#include "sip/MessagingContentKind.h"
#include "sip/MessagingDiagnosticsStore.h"

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
           "attributes. No MSRP session is ever started from this view — detection and "
           "logging only."),
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
    m_kindFilter->addItem(tr("All content kinds"), QString());
    m_kindFilter->addItem(tr("text/plain"), QStringLiteral("text/plain"));
    m_kindFilter->addItem(tr("text/html"), QStringLiteral("text/html"));
    m_kindFilter->addItem(tr("CPIM"), QStringLiteral("message/cpim"));
    m_kindFilter->addItem(tr("IMDN"), QStringLiteral("message/imdn+xml"));
    m_kindFilter->addItem(tr("is-composing"), QStringLiteral("application/im-iscomposing+xml"));
    m_kindFilter->addItem(tr("SDP (MSRP)"), QStringLiteral("application/sdp"));
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

    m_table = new QTableWidget(0, 8, this);
    m_table->setHorizontalHeaderLabels({
        tr("Time"), tr("Dir"), tr("Method/Status"), tr("From"),
        tr("To"), tr("Call-ID"), tr("Content-Type"), tr("Preview")
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

    connect(&MessagingDiagnosticsStore::instance(), &MessagingDiagnosticsStore::entryLogged,
            this, &MessagingDiagnosticsPage::onEntryLogged);
    connect(&MessagingDiagnosticsStore::instance(), &MessagingDiagnosticsStore::cleared,
            this, &MessagingDiagnosticsPage::onCleared);

    for (const MessagingTraceEntry &entry : MessagingDiagnosticsStore::instance().entries())
        m_entries.append(entry);

    rebuildTable();
}

void MessagingDiagnosticsPage::onEntryLogged(const MessagingTraceEntry &entry)
{
    m_entries.append(entry);
    if (passesFilters(entry))
        addRow(entry, m_entries.size() - 1);
}

void MessagingDiagnosticsPage::onCleared()
{
    m_entries.clear();
    m_table->setRowCount(0);
}

bool MessagingDiagnosticsPage::passesFilters(const MessagingTraceEntry &entry) const
{
    const QString callIdFilter = normalizedKey(m_callIdFilter->text());
    if (!callIdFilter.isEmpty() && !normalizedKey(entry.callId).contains(callIdFilter))
        return false;

    const QString kindFilter = m_kindFilter->currentData().toString();
    if (!kindFilter.isEmpty()
        && MessagingContentKindDetector::toString(entry.contentKind).compare(
               kindFilter, Qt::CaseInsensitive) != 0)
        return false;

    const QString directionFilter = m_directionFilter->currentData().toString();
    if (!directionFilter.isEmpty()) {
        const QString entryDirection = entry.direction == SipMessageTrace::Direction::Outbound
            ? QStringLiteral("outbound") : QStringLiteral("inbound");
        if (entryDirection != directionFilter)
            return false;
    }

    return true;
}

void MessagingDiagnosticsPage::applyFilters()
{
    rebuildTable();
}

void MessagingDiagnosticsPage::rebuildTable()
{
    m_table->setRowCount(0);
    for (int i = 0; i < m_entries.size(); ++i) {
        if (passesFilters(m_entries.at(i)))
            addRow(m_entries.at(i), i);
    }
}

void MessagingDiagnosticsPage::addRow(const MessagingTraceEntry &entry, int entryIndex)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto setCell = [this, row](int col, const QString &text) {
        auto *item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, col, item);
    };

    setCell(0, entry.timestamp.toString(QStringLiteral("hh:mm:ss.zzz")));
    setCell(1, entry.direction == SipMessageTrace::Direction::Outbound ? tr("Out") : tr("In"));
    setCell(2, entry.summary());
    setCell(3, entry.fromUri);
    setCell(4, entry.toUri);
    setCell(5, entry.callId);
    setCell(6, entry.contentType.isEmpty()
        ? MessagingContentKindDetector::toString(entry.contentKind)
        : entry.contentType);
    setCell(7, entry.bodyPreview);

    // Stash the entry index so onRowActivated can retrieve full data.
    m_table->item(row, 0)->setData(Qt::UserRole, entryIndex);
}

void MessagingDiagnosticsPage::onRowActivated(int row, int /*column*/)
{
    if (row < 0 || row >= m_table->rowCount())
        return;

    QTableWidgetItem *item = m_table->item(row, 0);
    if (!item)
        return;

    const int index = item->data(Qt::UserRole).toInt();
    if (index < 0 || index >= m_entries.size())
        return;

    auto *dlg = new MessagingMessageDetailsDialog(this);
    dlg->setEntry(m_entries.at(index));
    dlg->open();
}

void MessagingDiagnosticsPage::onClear()
{
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
    QTextStream(&f) << MessagingDiagnosticsStore::instance().exportToText();
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
    QTextStream(&f) << MessagingDiagnosticsStore::instance().exportToJson();
}
