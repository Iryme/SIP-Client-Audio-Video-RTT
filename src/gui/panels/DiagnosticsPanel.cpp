#include "DiagnosticsPanel.h"
#include "core/Logger.h"
#include "settings/ApplicationSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QTextStream>
#include <QFile>
#include <QMessageBox>

DiagnosticsPanel::DiagnosticsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DiagnosticsPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Title + toolbar row
    auto *toolbarRow = new QHBoxLayout();
    auto *title = new QLabel(tr("Diagnostics / Logs"), this);
    title->setStyleSheet("font-weight: bold; font-size: 12px;");
    toolbarRow->addWidget(title);
    toolbarRow->addSpacing(12);
    buildToolbar(toolbarRow);
    toolbarRow->addStretch();

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Filter..."));
    m_search->setFixedWidth(160);
    toolbarRow->addWidget(m_search);

    auto *btnClear  = new QPushButton(tr("Clear"),    this);
    auto *btnCopy   = new QPushButton(tr("Copy Sel"), this);
    auto *btnExport = new QPushButton(tr("Export"),   this);
    auto *btnBundle = new QPushButton(tr("Bundle"),   this);
    toolbarRow->addWidget(btnClear);
    toolbarRow->addWidget(btnCopy);
    toolbarRow->addWidget(btnExport);
    toolbarRow->addWidget(btnBundle);

    layout->addLayout(toolbarRow);

    // Log table
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("Level"), tr("Category"), tr("Message"), tr("Payload")});
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(0, 100);
    m_table->setColumnWidth(1, 55);
    m_table->setColumnWidth(2, 70);
    m_table->setColumnWidth(4, 180);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    layout->addWidget(m_table, 1);

    // Wire up signals
    connect(&Logger::instance(), &Logger::entryAdded, this, &DiagnosticsPanel::onEntryAdded);
    connect(btnClear,  &QPushButton::clicked, this, &DiagnosticsPanel::onClear);
    connect(btnCopy,   &QPushButton::clicked, this, &DiagnosticsPanel::onCopySelected);
    connect(btnExport, &QPushButton::clicked, this, &DiagnosticsPanel::onExportVisible);
    connect(btnBundle, &QPushButton::clicked, this, &DiagnosticsPanel::onExportBundle);
}

void DiagnosticsPanel::buildToolbar(QHBoxLayout *row)
{
    auto &settings = ApplicationSettings::instance();

    auto makeLevel = [&](const QString &label, LogLevel level) -> QToolButton* {
        auto *btn = new QToolButton(this);
        btn->setText(label);
        btn->setCheckable(true);
        btn->setObjectName("LogLevelBtn");
        btn->setFixedHeight(24);

        // Load persisted state; ApplicationSettings returns the safe default if missing.
        const bool persisted = settings.diagLevelEnabled(level);
        btn->setChecked(persisted);
        Logger::instance().setLevelEnabled(level, persisted);

        connect(btn, &QToolButton::toggled, this, [level](bool on) {
            Logger::instance().setLevelEnabled(level, on);
            ApplicationSettings::instance().setDiagLevelEnabled(level, on);
        });
        row->addWidget(btn);
        return btn;
    };

    m_btnInfo  = makeLevel("INFO",  LogLevel::Info);
    m_btnWarn  = makeLevel("WARN",  LogLevel::Warn);
    m_btnError = makeLevel("ERROR", LogLevel::Error);
    m_btnDebug = makeLevel("DEBUG", LogLevel::Debug);
    m_btnRaw   = makeLevel("RAW",   LogLevel::Raw);
}

void DiagnosticsPanel::onEntryAdded(const LogEntry &entry)
{
    if (!isLevelVisible(entry.level))
        return;

    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *tTime = new QTableWidgetItem(entry.timestamp.toString("hh:mm:ss.zzz"));
    auto *tLevel = new QTableWidgetItem(Logger::levelName(entry.level));
    auto *tCat   = new QTableWidgetItem(Logger::categoryName(entry.category));
    auto *tMsg   = new QTableWidgetItem(entry.message);
    auto *tPay   = new QTableWidgetItem(entry.payload);

    // Color coding
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

void DiagnosticsPanel::onClear()
{
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
    // Placeholder: future implementation will zip logs + SIP traces
    QMessageBox::information(this, tr("Debug Bundle"),
        tr("Debug bundle export is not yet implemented.\n"
           "It will package logs, SIP traces, and system info.\n\n"
           "Passwords and secrets are never included."));
}

void DiagnosticsPanel::onLevelToggled(bool)
{
    // Intentionally empty — individual level buttons handle their own logic
}
