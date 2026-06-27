#include "DiagnosticsPanel.h"
#include "core/Logger.h"
#include "core/AppSettings.h"
#include "gui/SipLadderWidget.h"
#include "sip/SipTraceLogger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
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

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(4);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);

    // -----------------------------------------------------------------------
    // Tab 0: Log
    // -----------------------------------------------------------------------
    auto *logTab = new QWidget(m_tabs);
    auto *logLayout = new QVBoxLayout(logTab);
    logLayout->setContentsMargins(0, 4, 0, 0);
    logLayout->setSpacing(4);

    // Toolbar row
    auto *toolbarRow = new QHBoxLayout();
    auto *title = new QLabel(tr("Diagnostics / Logs"), logTab);
    title->setStyleSheet("font-weight: bold; font-size: 12px;");
    toolbarRow->addWidget(title);
    toolbarRow->addSpacing(12);
    buildToolbar(toolbarRow);
    toolbarRow->addStretch();

    m_search = new QLineEdit(logTab);
    m_search->setPlaceholderText(tr("Filter..."));
    m_search->setFixedWidth(160);
    toolbarRow->addWidget(m_search);

    auto *btnClear  = new QPushButton(tr("Clear"),    logTab);
    auto *btnCopy   = new QPushButton(tr("Copy Sel"), logTab);
    auto *btnExport = new QPushButton(tr("Export"),   logTab);
    auto *btnBundle = new QPushButton(tr("Bundle"),   logTab);
    toolbarRow->addWidget(btnClear);
    toolbarRow->addWidget(btnCopy);
    toolbarRow->addWidget(btnExport);
    toolbarRow->addWidget(btnBundle);
    logLayout->addLayout(toolbarRow);

    // Log table
    m_table = new QTableWidget(0, 5, logTab);
    m_table->setHorizontalHeaderLabels(
        {tr("Time"), tr("Level"), tr("Category"), tr("Message"), tr("Payload")});
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
    logLayout->addWidget(m_table, 1);

    connect(&Logger::instance(), &Logger::entryAdded, this, &DiagnosticsPanel::onEntryAdded);
    connect(btnClear,  &QPushButton::clicked, this, &DiagnosticsPanel::onClear);
    connect(btnCopy,   &QPushButton::clicked, this, &DiagnosticsPanel::onCopySelected);
    connect(btnExport, &QPushButton::clicked, this, &DiagnosticsPanel::onExportVisible);
    connect(btnBundle, &QPushButton::clicked, this, &DiagnosticsPanel::onExportBundle);

    m_tabs->addTab(logTab, tr("Log"));

    // -----------------------------------------------------------------------
    // Tab 1: SIP Ladder
    // -----------------------------------------------------------------------
    auto *ladderTab = new QWidget(m_tabs);
    auto *ladderLayout = new QVBoxLayout(ladderTab);
    ladderLayout->setContentsMargins(0, 4, 0, 0);
    ladderLayout->setSpacing(4);

    auto *ladderToolbar = new QHBoxLayout();
    auto *ladderTitle = new QLabel(tr("SIP Message Ladder"), ladderTab);
    ladderTitle->setStyleSheet("font-weight: bold; font-size: 12px;");
    ladderToolbar->addWidget(ladderTitle);
    ladderToolbar->addStretch();
    auto *btnLadderClear    = new QPushButton(tr("Clear"),       ladderTab);
    auto *btnLadderExportTx = new QPushButton(tr("Export Text"), ladderTab);
    auto *btnLadderExportJs = new QPushButton(tr("Export JSON"), ladderTab);
    ladderToolbar->addWidget(btnLadderClear);
    ladderToolbar->addWidget(btnLadderExportTx);
    ladderToolbar->addWidget(btnLadderExportJs);
    ladderLayout->addLayout(ladderToolbar);

    m_ladderScroll = new QScrollArea(ladderTab);
    m_ladderScroll->setWidgetResizable(true);
    m_ladderScroll->setFrameShape(QFrame::NoFrame);

    m_ladder = new SipLadderWidget(m_ladderScroll);
    m_ladderScroll->setWidget(m_ladder);
    ladderLayout->addWidget(m_ladderScroll, 1);

    connect(btnLadderClear, &QPushButton::clicked,
            this, &DiagnosticsPanel::onClearLadder);

    connect(btnLadderExportTx, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Export SIP Trace"),
            QStringLiteral("sip-trace.txt"),
            tr("Text files (*.txt);;All (*.*)"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QFile::WriteOnly | QFile::Text)) return;
        QTextStream(&f) << SipTraceLogger::instance().exportToText();
    });

    connect(btnLadderExportJs, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Export SIP Trace"),
            QStringLiteral("sip-trace.json"),
            tr("JSON files (*.json);;All (*.*)"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QFile::WriteOnly | QFile::Text)) return;
        QTextStream(&f) << SipTraceLogger::instance().exportToJson();
    });

    // Connect SipTraceLogger → ladder widget
    connect(&SipTraceLogger::instance(), &SipTraceLogger::messageLogged,
            m_ladder, &SipLadderWidget::onMessageLogged);
    connect(&SipTraceLogger::instance(), &SipTraceLogger::cleared,
            m_ladder, &SipLadderWidget::onCleared);

    // Auto-scroll ladder to bottom when a new message arrives (queued so
    // the widget geometry has updated before we query the scroll maximum).
    connect(&SipTraceLogger::instance(), &SipTraceLogger::messageLogged,
            this, [this]() {
                QMetaObject::invokeMethod(m_ladderScroll->verticalScrollBar(),
                    [this]() {
                        m_ladderScroll->verticalScrollBar()->setValue(
                            m_ladderScroll->verticalScrollBar()->maximum());
                    }, Qt::QueuedConnection);
            });

    m_tabs->addTab(ladderTab, tr("SIP Ladder"));

    rootLayout->addWidget(m_tabs, 1);
}

void DiagnosticsPanel::showLogsTab()
{
    if (m_tabs)
        m_tabs->setCurrentIndex(0);
}

void DiagnosticsPanel::showLadderTab()
{
    if (m_tabs)
        m_tabs->setCurrentIndex(1);
}

void DiagnosticsPanel::setActiveTab(int index)
{
    if (m_tabs)
        m_tabs->setCurrentIndex(index);
}

QTableWidget *DiagnosticsPanel::logTable() const
{
    return m_table;
}

SipLadderWidget *DiagnosticsPanel::ladderWidget() const
{
    return m_ladder;
}

// ---------------------------------------------------------------------------

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
    if (!isLevelVisible(entry.level))
        return;

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
    QMessageBox::information(this, tr("Debug Bundle"),
        tr("Debug bundle export is not yet implemented.\n"
           "It will package logs, SIP traces, and system info.\n\n"
           "To export SIP traces now, switch to the SIP Ladder tab and use\n"
           "Export Text or Export JSON.\n\n"
           "Passwords and secrets are never included."));
}

void DiagnosticsPanel::onClearLadder()
{
    SipTraceLogger::instance().clear();
}

void DiagnosticsPanel::onLevelToggled(bool)
{
    // Individual level buttons handle their own logic in buildToolbar lambdas.
}
