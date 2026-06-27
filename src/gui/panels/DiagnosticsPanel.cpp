#include "DiagnosticsPanel.h"

#include "core/Logger.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
static QString levelFilterLabel(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:  return QStringLiteral("Info");
    case LogLevel::Warn:  return QStringLiteral("Warning");
    case LogLevel::Error: return QStringLiteral("Error");
    case LogLevel::Debug: return QStringLiteral("Debug");
    case LogLevel::Raw:   return QStringLiteral("Raw");
    }
    return QStringLiteral("All");
}
}

DiagnosticsPanel::DiagnosticsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DiagnosticsPanel");

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    auto *title = new QLabel(tr("Logs / Capture / Debug"), this);
    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    rootLayout->addWidget(title);

    auto *toolbarRow = new QHBoxLayout();
    toolbarRow->setSpacing(6);
    buildToolbar(toolbarRow);
    rootLayout->addLayout(toolbarRow);

    m_viewStack = new QStackedWidget(this);
    m_table = new QTableWidget(0, 5, m_viewStack);
    m_table->setHorizontalHeaderLabels(
        {tr("Time"), tr("Level"), tr("Component"), tr("Message"), tr("Payload")});
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(0, 100);
    m_table->setColumnWidth(1, 70);
    m_table->setColumnWidth(2, 90);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setSelectionMode(QTableWidget::ExtendedSelection);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    m_emptyLabel = new QLabel(tr("No logs available"), m_viewStack);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: #7a8796; font-size: 14px;");
    m_emptyLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_viewStack->addWidget(m_table);
    m_viewStack->addWidget(m_emptyLabel);
    rootLayout->addWidget(m_viewStack, 1);

    auto *statusRow = new QHBoxLayout();
    statusRow->setContentsMargins(0, 0, 0, 0);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #b7c4d6; font-size: 11px;");
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();
    rootLayout->addLayout(statusRow);

    connect(&Logger::instance(), &Logger::entryAdded,
            this, &DiagnosticsPanel::onEntryAdded);
    connect(m_search, &QLineEdit::textChanged, this, &DiagnosticsPanel::onFilterChanged);
    connect(m_severityFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiagnosticsPanel::onFilterChanged);
    connect(m_componentFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiagnosticsPanel::onFilterChanged);
    connect(m_autoScroll, &QCheckBox::toggled,
            this, &DiagnosticsPanel::onAutoScrollChanged);

    updateStatus();
    onFilterChanged();
}

QTableWidget *DiagnosticsPanel::logTable() const
{
    return m_table;
}

void DiagnosticsPanel::buildToolbar(QHBoxLayout *row)
{
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search logs..."));
    m_search->setMinimumWidth(220);
    row->addWidget(m_search, 1);

    m_severityFilter = new QComboBox(this);
    m_severityFilter->addItem(tr("All"), QString());
    m_severityFilter->addItem(tr("Debug"), QStringLiteral("debug"));
    m_severityFilter->addItem(tr("Info"), QStringLiteral("info"));
    m_severityFilter->addItem(tr("Warning"), QStringLiteral("warn"));
    m_severityFilter->addItem(tr("Error"), QStringLiteral("error"));
    m_severityFilter->addItem(tr("Raw"), QStringLiteral("raw"));
    row->addWidget(m_severityFilter);

    m_componentFilter = new QComboBox(this);
    m_componentFilter->addItem(tr("All"), QString());
    m_componentFilter->addItem(tr("SIP"), QStringLiteral("sip"));
    m_componentFilter->addItem(tr("RTT"), QStringLiteral("rtt"));
    m_componentFilter->addItem(tr("Media"), QStringLiteral("media"));
    m_componentFilter->addItem(tr("UI"), QStringLiteral("ui"));
    m_componentFilter->addItem(tr("Network"), QStringLiteral("network"));
    m_componentFilter->addItem(tr("Capture"), QStringLiteral("capture"));
    row->addWidget(m_componentFilter);

    m_autoScroll = new QCheckBox(tr("Auto-scroll"), this);
    m_autoScroll->setChecked(true);
    row->addWidget(m_autoScroll);

    auto *btnClear = new QPushButton(tr("Clear"), this);
    auto *btnCopy = new QPushButton(tr("Copy"), this);
    auto *btnExport = new QPushButton(tr("Export"), this);
    row->addWidget(btnClear);
    row->addWidget(btnCopy);
    row->addWidget(btnExport);

    connect(btnClear, &QPushButton::clicked, this, &DiagnosticsPanel::onClear);
    connect(btnCopy, &QPushButton::clicked, this, &DiagnosticsPanel::onCopySelected);
    connect(btnExport, &QPushButton::clicked, this, &DiagnosticsPanel::onExportVisible);
}

QString DiagnosticsPanel::componentKeyForCategory(LogCategory category) const
{
    switch (category) {
    case LogCategory::Sip:
    case LogCategory::Sdp:
        return QStringLiteral("sip");
    case LogCategory::Rtt:
    case LogCategory::Lmpe:
    case LogCategory::Etsi:
        return QStringLiteral("rtt");
    case LogCategory::Media:
        return QStringLiteral("media");
    case LogCategory::App:
        return QStringLiteral("ui");
    case LogCategory::Platform:
        return QStringLiteral("network");
    }
    return QStringLiteral("capture");
}

QString DiagnosticsPanel::severityKeyForLevel(LogLevel level) const
{
    switch (level) {
    case LogLevel::Debug: return QStringLiteral("debug");
    case LogLevel::Info:  return QStringLiteral("info");
    case LogLevel::Warn:  return QStringLiteral("warn");
    case LogLevel::Error: return QStringLiteral("error");
    case LogLevel::Raw:   return QStringLiteral("raw");
    }
    return QString();
}

bool DiagnosticsPanel::isLevelVisible(LogLevel level) const
{
    const QString filter = m_severityFilter->currentData().toString();
    if (filter.isEmpty())
        return true;
    return filter == severityKeyForLevel(level);
}

bool DiagnosticsPanel::entryMatchesFilters(const LogEntry &entry) const
{
    if (!isLevelVisible(entry.level))
        return false;

    const QString componentFilter = m_componentFilter->currentData().toString();
    if (!componentFilter.isEmpty() && componentKeyForCategory(entry.category) != componentFilter)
        return false;

    const QString search = m_search->text().trimmed();
    if (!search.isEmpty()) {
        const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
            .arg(entry.timestamp.toString(Qt::ISODateWithMs),
                 Logger::levelName(entry.level),
                 Logger::categoryName(entry.category),
                 entry.message,
                 entry.payload);
        if (!haystack.contains(search, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

QString DiagnosticsPanel::currentFilterSummary() const
{
    const QString severity = m_severityFilter->currentText();
    const QString component = m_componentFilter->currentText();
    const QString search = m_search->text().trimmed().isEmpty()
        ? tr("none")
        : m_search->text().trimmed();
    return tr("Severity: %1 | Component: %2 | Search: %3 | Auto-scroll: %4")
        .arg(severity,
             component,
             search,
             m_autoScroll->isChecked() ? tr("on") : tr("off"));
}

QString DiagnosticsPanel::formatLogLine(const LogEntry &entry) const
{
    return QStringLiteral("[%1] [%2] [%3] %4%5")
        .arg(entry.timestamp.toString(Qt::ISODateWithMs),
             Logger::levelName(entry.level),
             Logger::categoryName(entry.category),
             entry.message,
             entry.payload.isEmpty() ? QString() : QStringLiteral(" | ") + entry.payload);
}

QString DiagnosticsPanel::visibleLogsAsText() const
{
    QString out;
    QTextStream ts(&out);
    for (const auto &entry : m_entries) {
        if (entryMatchesFilters(entry))
            ts << formatLogLine(entry) << '\n';
    }
    return out;
}

void DiagnosticsPanel::appendTableRow(const LogEntry &entry)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *tTime  = new QTableWidgetItem(entry.timestamp.toString("hh:mm:ss.zzz"));
    auto *tLevel = new QTableWidgetItem(Logger::levelName(entry.level));
    auto *tComp  = new QTableWidgetItem(Logger::categoryName(entry.category));
    auto *tMsg   = new QTableWidgetItem(entry.message);
    auto *tPay   = new QTableWidgetItem(entry.payload);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    tTime->setFont(mono);
    tLevel->setFont(mono);
    tComp->setFont(mono);
    tMsg->setFont(mono);
    tPay->setFont(mono);

    QColor levelColor;
    switch (entry.level) {
    case LogLevel::Info:  levelColor = QColor("#50b8e0"); break;
    case LogLevel::Warn:  levelColor = QColor("#e0b850"); break;
    case LogLevel::Error: levelColor = QColor("#e05050"); break;
    case LogLevel::Debug: levelColor = QColor("#888888"); break;
    case LogLevel::Raw:   levelColor = QColor("#606060"); break;
    }
    tLevel->setForeground(levelColor);

    m_table->setItem(row, 0, tTime);
    m_table->setItem(row, 1, tLevel);
    m_table->setItem(row, 2, tComp);
    m_table->setItem(row, 3, tMsg);
    m_table->setItem(row, 4, tPay);
}

void DiagnosticsPanel::refreshTable()
{
    const int previousScroll = m_table->verticalScrollBar()->value();
    const int previousMax    = m_table->verticalScrollBar()->maximum();

    m_table->setRowCount(0);

    for (const auto &entry : m_entries) {
        if (entryMatchesFilters(entry))
            appendTableRow(entry);
    }

    if (m_table->rowCount() == 0) {
        m_viewStack->setCurrentWidget(m_emptyLabel);
    } else {
        m_viewStack->setCurrentWidget(m_table);
        if (m_autoScroll->isChecked() && previousScroll >= previousMax - 2) {
            m_table->scrollToBottom();
        } else {
            m_table->verticalScrollBar()->setValue(previousScroll);
        }
    }

    updateStatus();
}

void DiagnosticsPanel::updateStatus()
{
    const int visibleRows = m_table ? m_table->rowCount() : 0;
    QString lastTimestamp = tr("none");
    for (int r = visibleRows - 1; r >= 0; --r) {
        auto *item = m_table->item(r, 0);
        if (item) {
            lastTimestamp = item->text();
            break;
        }
    }

    m_statusLabel->setText(
        tr("Visible lines: %1 | Last timestamp: %2 | %3")
            .arg(visibleRows)
            .arg(lastTimestamp)
            .arg(currentFilterSummary()));
}

void DiagnosticsPanel::onEntryAdded(const LogEntry &entry)
{
    m_entries.append(entry);
    if (!entryMatchesFilters(entry))
        return;

    appendTableRow(entry);
    m_viewStack->setCurrentWidget(m_table);
    if (m_autoScroll->isChecked())
        m_table->scrollToBottom();
    updateStatus();
}

void DiagnosticsPanel::onClear()
{
    m_entries.clear();
    refreshTable();
}

void DiagnosticsPanel::onCopySelected()
{
    const QString text = visibleLogsAsText();
    if (!text.isEmpty())
        QApplication::clipboard()->setText(text.trimmed());
}

void DiagnosticsPanel::onExportVisible()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Logs"), QStringLiteral("logs.txt"),
        tr("Text files (*.txt);;All (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text))
        return;

    QTextStream out(&f);
    out << visibleLogsAsText();
}

void DiagnosticsPanel::onExportBundle()
{
    QMessageBox::information(this, tr("Debug Bundle"),
        tr("Debug bundle export is not yet implemented.\n"
           "It will package logs, SIP traces, and system info.\n\n"
           "Passwords and secrets are never included."));
}

void DiagnosticsPanel::onFilterChanged()
{
    refreshTable();
}

void DiagnosticsPanel::onAutoScrollChanged(bool)
{
    updateStatus();
}
