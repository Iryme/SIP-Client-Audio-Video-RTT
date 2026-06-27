#include "DiagnosticsPanel.h"

#include "core/Logger.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
static QString categoryKey(LogCategory category)
{
    return Logger::categoryName(category).toLower();
}
}

DiagnosticsPanel::DiagnosticsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DiagnosticsPanel");

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    auto *title = new QLabel(tr("Logs / Capture / Debug"), this);
    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    rootLayout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Filter backend activity, SIP events, media traces, and diagnostic output."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet("color: #b7c4d6;");
    rootLayout->addWidget(subtitle);

    auto *toolbarRow = new QHBoxLayout();
    toolbarRow->setSpacing(6);

    buildToolbar(toolbarRow);
    toolbarRow->addStretch();

    m_componentFilter = new QComboBox(this);
    m_componentFilter->addItem(tr("All components"), QString());
    m_componentFilter->addItem(tr("APP"), QStringLiteral("app"));
    m_componentFilter->addItem(tr("SIP"), QStringLiteral("sip"));
    m_componentFilter->addItem(tr("SDP"), QStringLiteral("sdp"));
    m_componentFilter->addItem(tr("MEDIA"), QStringLiteral("media"));
    m_componentFilter->addItem(tr("RTT"), QStringLiteral("rtt"));
    m_componentFilter->addItem(tr("LMPE"), QStringLiteral("lmpe"));
    m_componentFilter->addItem(tr("ETSI"), QStringLiteral("etsi"));
    m_componentFilter->addItem(tr("PLATFORM"), QStringLiteral("platform"));
    toolbarRow->addWidget(m_componentFilter);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search logs..."));
    m_search->setFixedWidth(220);
    toolbarRow->addWidget(m_search);

    auto *btnClear  = new QPushButton(tr("Clear"),    this);
    auto *btnCopy   = new QPushButton(tr("Copy"),     this);
    auto *btnExport = new QPushButton(tr("Export"),   this);
    auto *btnBundle = new QPushButton(tr("Bundle"),   this);
    toolbarRow->addWidget(btnClear);
    toolbarRow->addWidget(btnCopy);
    toolbarRow->addWidget(btnExport);
    toolbarRow->addWidget(btnBundle);
    rootLayout->addLayout(toolbarRow);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Time"), tr("Level"), tr("Category"), tr("Message"), tr("Payload")});
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(0, 100);
    m_table->setColumnWidth(1, 55);
    m_table->setColumnWidth(2, 80);
    m_table->setColumnWidth(4, 180);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    rootLayout->addWidget(m_table, 1);

    connect(&Logger::instance(), &Logger::entryAdded,
            this, &DiagnosticsPanel::onEntryAdded);
    connect(btnClear,  &QPushButton::clicked, this, &DiagnosticsPanel::onClear);
    connect(btnCopy,   &QPushButton::clicked, this, &DiagnosticsPanel::onCopySelected);
    connect(btnExport, &QPushButton::clicked, this, &DiagnosticsPanel::onExportVisible);
    connect(btnBundle, &QPushButton::clicked, this, &DiagnosticsPanel::onExportBundle);
    connect(m_search, &QLineEdit::textChanged, this, &DiagnosticsPanel::onFilterChanged);
    connect(m_componentFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiagnosticsPanel::onFilterChanged);

    onFilterChanged();
}

QTableWidget *DiagnosticsPanel::logTable() const
{
    return m_table;
}

void DiagnosticsPanel::buildToolbar(QHBoxLayout *row)
{
    auto makeLevel = [&](const QString &label, LogLevel level, bool defaultOn) -> QToolButton* {
        auto *btn = new QToolButton(this);
        btn->setText(label);
        btn->setCheckable(true);
        btn->setChecked(defaultOn);
        btn->setObjectName("LogLevelBtn");
        btn->setFixedHeight(24);
        Logger::instance().setLevelEnabled(level, defaultOn);
        connect(btn, &QToolButton::toggled, this, [this, level](bool on) {
            Logger::instance().setLevelEnabled(level, on);
            onFilterChanged();
        });
        row->addWidget(btn);
        return btn;
    };

    m_btnInfo  = makeLevel("INFO",  LogLevel::Info,  true);
    m_btnWarn  = makeLevel("WARN",  LogLevel::Warn,  true);
    m_btnError = makeLevel("ERROR", LogLevel::Error, true);
    m_btnDebug = makeLevel("DEBUG", LogLevel::Debug, false);
    m_btnRaw   = makeLevel("RAW",   LogLevel::Raw,   false);
}

void DiagnosticsPanel::onEntryAdded(const LogEntry &entry)
{
    m_entries.append(entry);
    if (entryMatchesFilters(entry))
        refreshTable();
}

void DiagnosticsPanel::refreshTable()
{
    m_table->setRowCount(0);

    for (const auto &entry : std::as_const(m_entries)) {
        if (!entryMatchesFilters(entry))
            continue;

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *tTime  = new QTableWidgetItem(entry.timestamp.toString("hh:mm:ss.zzz"));
        auto *tLevel = new QTableWidgetItem(Logger::levelName(entry.level));
        auto *tCat   = new QTableWidgetItem(Logger::categoryName(entry.category));
        auto *tMsg   = new QTableWidgetItem(entry.message);
        auto *tPay   = new QTableWidgetItem(entry.payload);

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
        m_table->setItem(row, 2, tCat);
        m_table->setItem(row, 3, tMsg);
        m_table->setItem(row, 4, tPay);
    }
    m_table->scrollToBottom();
}

bool DiagnosticsPanel::isLevelVisible(LogLevel level) const
{
    switch (level) {
    case LogLevel::Info:  return m_btnInfo->isChecked();
    case LogLevel::Warn:  return m_btnWarn->isChecked();
    case LogLevel::Error: return m_btnError->isChecked();
    case LogLevel::Debug: return m_btnDebug->isChecked();
    case LogLevel::Raw:   return m_btnRaw->isChecked();
    }
    return false;
}

bool DiagnosticsPanel::entryMatchesFilters(const LogEntry &entry) const
{
    if (!isLevelVisible(entry.level))
        return false;

    const QString component = m_componentFilter->currentData().toString();
    if (!component.isEmpty() && categoryKey(entry.category) != component)
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

void DiagnosticsPanel::onClear()
{
    m_entries.clear();
    m_table->setRowCount(0);
}

void DiagnosticsPanel::onCopySelected()
{
    QStringList lines;
    const auto selected = m_table->selectedRanges();
    for (const auto &range : selected) {
        for (int r = range.topRow(); r <= range.bottomRow(); ++r) {
            QStringList cols;
            for (int c = 0; c < m_table->columnCount(); ++c) {
                auto *item = m_table->item(r, c);
                cols << (item ? item->text() : QString());
            }
            lines << cols.join('\t');
        }
    }
    if (!lines.isEmpty())
        QApplication::clipboard()->setText(lines.join('\n'));
}

void DiagnosticsPanel::onExportVisible()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Logs"), QString(), tr("Text files (*.txt);;All (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text))
        return;

    QTextStream out(&f);
    for (int r = 0; r < m_table->rowCount(); ++r) {
        for (int c = 0; c < m_table->columnCount(); ++c) {
            auto *item = m_table->item(r, c);
            out << (item ? item->text() : QString());
            if (c < m_table->columnCount() - 1)
                out << '\t';
        }
        out << '\n';
    }
}

void DiagnosticsPanel::onExportBundle()
{
    QMessageBox::information(this, tr("Debug Bundle"),
        tr("Debug bundle export is not yet implemented.\n"
           "It will package logs, SIP traces, and system info.\n\n"
           "Passwords and secrets are never included."));
}

void DiagnosticsPanel::onLevelToggled(bool)
{
    onFilterChanged();
}

void DiagnosticsPanel::onFilterChanged()
{
    refreshTable();
}
