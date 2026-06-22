#include "DiagnosticsPanel.h"
#include "diagnostics/DiagnosticsLogger.h"
#include "diagnostics/LogFilterModel.h"
#include "core/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QTextStream>
#include <QFile>
#include <QMessageBox>

static constexpr int kColTime     = 0;
static constexpr int kColLevel    = 1;
static constexpr int kColCategory = 2;
static constexpr int kColMessage  = 3;
static constexpr int kColDetails  = 4;

DiagnosticsPanel::DiagnosticsPanel(QWidget *parent)
    : QWidget(parent)
    , m_filterModel(new LogFilterModel(this))
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
    toolbarRow->addSpacing(8);

    buildToolbar(toolbarRow);
    toolbarRow->addSpacing(8);

    // Category filter
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setFixedHeight(24);
    for (const char *cat : {"All","APP","SIP","SDP","MEDIA","RTT","LMPE","ETSI","PLATFORM"})
        m_categoryCombo->addItem(QString::fromLatin1(cat));
    toolbarRow->addWidget(m_categoryCombo);
    toolbarRow->addSpacing(4);

    // Search
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setFixedWidth(160);
    m_search->setClearButtonEnabled(true);
    toolbarRow->addWidget(m_search);

    toolbarRow->addStretch();

    // Action buttons
    auto *btnClear  = new QPushButton(tr("Clear"),       this);
    auto *btnCopy   = new QPushButton(tr("Copy Sel"),    this);
    auto *btnExport = new QPushButton(tr("Export Logs"), this);
    auto *btnBundle = new QPushButton(tr("Debug Bundle"),this);
    btnBundle->setEnabled(false); // placeholder — not yet implemented
    btnBundle->setToolTip(tr("Debug bundle export is not yet implemented"));
    toolbarRow->addWidget(btnClear);
    toolbarRow->addWidget(btnCopy);
    toolbarRow->addWidget(btnExport);
    toolbarRow->addWidget(btnBundle);

    layout->addLayout(toolbarRow);

    // Log table
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("Level"), tr("Category"), tr("Message"), tr("Details")});
    m_table->horizontalHeader()->setSectionResizeMode(kColMessage, QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(kColTime,     100);
    m_table->setColumnWidth(kColLevel,     55);
    m_table->setColumnWidth(kColCategory,  72);
    m_table->setColumnWidth(kColDetails,  200);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    // Details column hidden until RAW is enabled
    m_table->setColumnHidden(kColDetails, true);
    layout->addWidget(m_table, 1);

    // Restore persisted toggle states and sync filter model + logger
    loadToggleStates();

    // Wire signals
    connect(&DiagnosticsLogger::instance(), &DiagnosticsLogger::entryAdded,
            this, &DiagnosticsPanel::onEntryAdded);
    connect(m_filterModel, &LogFilterModel::filterChanged,
            this, &DiagnosticsPanel::onFilterChanged);

    connect(m_categoryCombo, &QComboBox::currentTextChanged, this, [this](const QString &cat){
        m_filterModel->setCategoryFilter(cat);
    });
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text){
        m_filterModel->setSearchText(text);
    });

    connect(btnClear,  &QPushButton::clicked, this, &DiagnosticsPanel::onClear);
    connect(btnCopy,   &QPushButton::clicked, this, &DiagnosticsPanel::onCopySelected);
    connect(btnExport, &QPushButton::clicked, this, &DiagnosticsPanel::onExportVisible);
    connect(btnBundle, &QPushButton::clicked, this, &DiagnosticsPanel::onExportBundle);
}

// ---------------------------------------------------------------------------
// Toolbar: level toggle buttons
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
        row->addWidget(btn);
        return btn;
    };

    m_btnInfo  = makeLevel("INFO",  LogLevel::Info,  true);
    m_btnWarn  = makeLevel("WARN",  LogLevel::Warn,  true);
    m_btnError = makeLevel("ERROR", LogLevel::Error, true);
    m_btnDebug = makeLevel("DEBUG", LogLevel::Debug, false);
    m_btnRaw   = makeLevel("RAW",   LogLevel::Raw,   false);

    // INFO / WARN / ERROR / DEBUG: straightforward toggles.
    // rebuildTable() is driven by LogFilterModel::filterChanged → onFilterChanged.
    auto wireSimple = [this](QToolButton *btn, LogLevel level) {
        connect(btn, &QToolButton::clicked, this, [this, level](bool checked) {
            m_filterModel->setLevelVisible(level, checked);
            DiagnosticsLogger::instance().setLevelEnabled(level, checked);
            saveToggleStates();
        });
    };
    wireSimple(m_btnInfo,  LogLevel::Info);
    wireSimple(m_btnWarn,  LogLevel::Warn);
    wireSimple(m_btnError, LogLevel::Error);
    wireSimple(m_btnDebug, LogLevel::Debug);

    // RAW: requires explicit confirmation before enabling
    connect(m_btnRaw, &QToolButton::clicked, this, &DiagnosticsPanel::onRawToggled);
}

// ---------------------------------------------------------------------------
// Slot: new entry from DiagnosticsLogger
// ---------------------------------------------------------------------------
void DiagnosticsPanel::onEntryAdded(const LogEntry &entry)
{
    m_filterModel->addEntry(entry);
    if (m_filterModel->matchesFilter(entry))
        appendRowToTable(entry);
}

// ---------------------------------------------------------------------------
// Slot: filter model changed — rebuild table from stored entries
// ---------------------------------------------------------------------------
void DiagnosticsPanel::onFilterChanged()
{
    rebuildTable();
}

// ---------------------------------------------------------------------------
// RAW toggle with confirmation dialog
// ---------------------------------------------------------------------------
void DiagnosticsPanel::onRawToggled(bool checked)
{
    if (checked) {
        const auto ret = QMessageBox::warning(
            this,
            tr("Enable RAW Logging"),
            tr("RAW mode captures full protocol payloads: SIP messages, SDP bodies, "
               "and RTP traces.\n\n"
               "This may expose call metadata and session details. "
               "Passwords are always redacted, but other sensitive data may appear.\n\n"
               "Enable RAW logging?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (ret != QMessageBox::Yes) {
            // User declined — revert button without triggering this slot again
            QSignalBlocker blocker(m_btnRaw);
            m_btnRaw->setChecked(false);
            return;
        }
    }

    // setRawVisible emits filterChanged → onFilterChanged → rebuildTable
    m_filterModel->setRawVisible(checked);
    DiagnosticsLogger::instance().setLevelEnabled(LogLevel::Raw, checked);
    m_table->setColumnHidden(kColDetails, !checked);
    saveToggleStates();
}

// ---------------------------------------------------------------------------
// Rebuild table from filter model (called when filters change)
// ---------------------------------------------------------------------------
void DiagnosticsPanel::rebuildTable()
{
    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(0);
    for (const auto &entry : m_filterModel->filteredEntries())
        appendRowToTable(entry);
    m_table->setUpdatesEnabled(true);
    if (m_table->rowCount() > 0)
        m_table->scrollToBottom();
}

// ---------------------------------------------------------------------------
// Append a single row (already known to pass filter)
// ---------------------------------------------------------------------------
void DiagnosticsPanel::appendRowToTable(const LogEntry &entry)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *tTime  = new QTableWidgetItem(entry.timestamp.toString("hh:mm:ss.zzz"));
    auto *tLevel = new QTableWidgetItem(DiagnosticsLogger::levelName(entry.level));
    auto *tCat   = new QTableWidgetItem(DiagnosticsLogger::categoryName(entry.category));
    auto *tMsg   = new QTableWidgetItem(entry.message);
    auto *tDet   = new QTableWidgetItem(entry.payload);

    QColor levelColor;
    switch (entry.level) {
    case LogLevel::Info:  levelColor = QColor("#50b8e0"); break;
    case LogLevel::Warn:  levelColor = QColor("#e0b850"); break;
    case LogLevel::Error: levelColor = QColor("#e05050"); break;
    case LogLevel::Debug: levelColor = QColor("#888888"); break;
    case LogLevel::Raw:   levelColor = QColor("#606060"); break;
    }
    tLevel->setForeground(levelColor);

    m_table->setItem(row, kColTime,     tTime);
    m_table->setItem(row, kColLevel,    tLevel);
    m_table->setItem(row, kColCategory, tCat);
    m_table->setItem(row, kColMessage,  tMsg);
    m_table->setItem(row, kColDetails,  tDet);

    m_table->scrollToBottom();
}

// ---------------------------------------------------------------------------
// Persist / restore level toggle states via AppSettings
// ---------------------------------------------------------------------------
void DiagnosticsPanel::saveToggleStates()
{
    AppSettings::setLogLevelEnabled("INFO",  m_btnInfo->isChecked());
    AppSettings::setLogLevelEnabled("WARN",  m_btnWarn->isChecked());
    AppSettings::setLogLevelEnabled("ERROR", m_btnError->isChecked());
    AppSettings::setLogLevelEnabled("DEBUG", m_btnDebug->isChecked());
    AppSettings::setLogLevelEnabled("RAW",   m_btnRaw->isChecked());
}

void DiagnosticsPanel::loadToggleStates()
{
    const bool info  = AppSettings::isLogLevelEnabled("INFO",  true);
    const bool warn  = AppSettings::isLogLevelEnabled("WARN",  true);
    const bool error = AppSettings::isLogLevelEnabled("ERROR", true);
    const bool debug = AppSettings::isLogLevelEnabled("DEBUG", false);
    // RAW is always loaded as false for safety — must be re-confirmed each session
    const bool raw   = false;

    auto applyToggle = [](QToolButton *btn, LogLevel level, bool on,
                          LogFilterModel *model) {
        QSignalBlocker blocker(btn);
        btn->setChecked(on);
        model->setLevelVisible(level, on);
        DiagnosticsLogger::instance().setLevelEnabled(level, on);
    };

    applyToggle(m_btnInfo,  LogLevel::Info,  info,  m_filterModel);
    applyToggle(m_btnWarn,  LogLevel::Warn,  warn,  m_filterModel);
    applyToggle(m_btnError, LogLevel::Error, error, m_filterModel);
    applyToggle(m_btnDebug, LogLevel::Debug, debug, m_filterModel);
    applyToggle(m_btnRaw,   LogLevel::Raw,   raw,   m_filterModel);

    m_filterModel->setRawVisible(raw);
    DiagnosticsLogger::instance().setLevelEnabled(LogLevel::Raw, raw);
    m_table->setColumnHidden(kColDetails, !raw);
}

// ---------------------------------------------------------------------------
// Action buttons
// ---------------------------------------------------------------------------
void DiagnosticsPanel::onClear()
{
    m_filterModel->clear();
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
                if (m_table->isColumnHidden(c))
                    continue;
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
            if (m_table->isColumnHidden(c))
                continue;
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
    // Placeholder — future implementation packages logs + SIP traces + system info
    QMessageBox::information(this, tr("Debug Bundle"),
        tr("Debug bundle export is not yet implemented.\n"
           "It will package logs, SIP traces, and system info.\n\n"
           "Passwords and secrets are never included."));
}
