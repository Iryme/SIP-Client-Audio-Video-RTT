#include "CallHistoryPanel.h"

#include "core/CallHistoryFilterProxyModel.h"
#include "core/CallHistoryListModel.h"
#include "core/CallHistoryStore.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QStringList>
#include <QVBoxLayout>

CallHistoryPanel::CallHistoryPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("CallHistoryPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(tr("Call History"), this);
    title->setStyleSheet("font-weight: bold; font-size: 13px;");
    layout->addWidget(title);

    auto *filterRow = new QHBoxLayout();
    filterRow->setSpacing(6);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("CallHistorySearch");
    m_searchEdit->setPlaceholderText(tr("Search name, URI, profile, result, reason, SIP code..."));
    filterRow->addWidget(m_searchEdit, 1);

    m_kindFilterCombo = new QComboBox(this);
    m_kindFilterCombo->setObjectName("CallHistoryKindFilter");
    m_kindFilterCombo->addItem(tr("All"),        int(CallHistoryFilterProxyModel::KindFilter::All));
    m_kindFilterCombo->addItem(tr("Incoming"),   int(CallHistoryFilterProxyModel::KindFilter::Incoming));
    m_kindFilterCombo->addItem(tr("Outgoing"),   int(CallHistoryFilterProxyModel::KindFilter::Outgoing));
    m_kindFilterCombo->addItem(tr("Missed"),     int(CallHistoryFilterProxyModel::KindFilter::Missed));
    m_kindFilterCombo->addItem(tr("Failed"),     int(CallHistoryFilterProxyModel::KindFilter::Failed));
    m_kindFilterCombo->addItem(tr("With Video"), int(CallHistoryFilterProxyModel::KindFilter::WithVideo));
    m_kindFilterCombo->addItem(tr("With RTT"),   int(CallHistoryFilterProxyModel::KindFilter::WithRtt));
    filterRow->addWidget(m_kindFilterCombo);

    m_dateFilterCombo = new QComboBox(this);
    m_dateFilterCombo->setObjectName("CallHistoryDateFilter");
    m_dateFilterCombo->addItem(tr("All time"),     int(CallHistoryFilterProxyModel::DateFilter::AllTime));
    m_dateFilterCombo->addItem(tr("Today"),        int(CallHistoryFilterProxyModel::DateFilter::Today));
    m_dateFilterCombo->addItem(tr("Last 7 days"),  int(CallHistoryFilterProxyModel::DateFilter::Last7Days));
    m_dateFilterCombo->addItem(tr("Last 30 days"), int(CallHistoryFilterProxyModel::DateFilter::Last30Days));
    filterRow->addWidget(m_dateFilterCombo);

    layout->addLayout(filterRow);

    m_resultsLabel = new QLabel(this);
    m_resultsLabel->setObjectName("CallHistoryResultsLabel");
    m_resultsLabel->setStyleSheet("color: #8899aa; font-size: 10px;");
    layout->addWidget(m_resultsLabel);

    m_emptyState = new QLabel(tr("No call history yet"), this);
    m_emptyState->setObjectName("CallHistoryEmptyState");
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setWordWrap(true);
    m_emptyState->setStyleSheet("color: #8899aa; padding: 12px; border: 1px dashed #3b4d63;");
    layout->addWidget(m_emptyState);

    m_model = new CallHistoryListModel(this);
    m_proxy = new CallHistoryFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_list = new QListView(this);
    m_list->setObjectName("CallHistoryList");
    m_list->setAlternatingRowColors(true);
    m_list->setModel(m_proxy);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);
    m_redialBtn = new QPushButton(tr("Call Back"), this);
    m_redialBtn->setObjectName("RedialCallHistoryBtn");
    m_redialBtn->setEnabled(false);
    m_clearBtn = new QPushButton(tr("Clear History"), this);
    m_clearBtn->setObjectName("ClearCallHistoryBtn");
    m_exportJsonBtn = new QPushButton(tr("Export JSON"), this);
    m_exportJsonBtn->setObjectName("ExportCallHistoryJsonBtn");
    m_exportCsvBtn = new QPushButton(tr("Export CSV"), this);
    m_exportCsvBtn->setObjectName("ExportCallHistoryCsvBtn");
    btnRow->addWidget(m_redialBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_exportJsonBtn);
    btnRow->addWidget(m_exportCsvBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(m_clearBtn, &QPushButton::clicked, this, &CallHistoryPanel::onClearHistory);
    connect(m_exportJsonBtn, &QPushButton::clicked, this, &CallHistoryPanel::onExportJson);
    connect(m_exportCsvBtn, &QPushButton::clicked, this, &CallHistoryPanel::onExportCsv);
    connect(m_redialBtn, &QPushButton::clicked, this, &CallHistoryPanel::onRedialSelected);
    connect(m_list, &QListView::activated, this, &CallHistoryPanel::onItemActivated);
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &) {
        m_redialBtn->setEnabled(!currentSelection().remoteUri.isEmpty());
    });

    connect(m_searchEdit, &QLineEdit::textChanged, this, &CallHistoryPanel::onSearchTextChanged);
    connect(m_kindFilterCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &CallHistoryPanel::onKindFilterChanged);
    connect(m_dateFilterCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &CallHistoryPanel::onDateFilterChanged);

    connect(&CallHistoryStore::instance(), &CallHistoryStore::historyChanged,
            this, &CallHistoryPanel::refresh);

    refresh();
}

void CallHistoryPanel::refresh()
{
    m_model->setEntries(CallHistoryStore::instance().entries());
    updateResultsLabel();
}

void CallHistoryPanel::updateResultsLabel()
{
    const int total = m_model->rowCount();
    const int shown = m_proxy->rowCount();

    m_resultsLabel->setText(tr("Showing %1 of %2 calls").arg(shown).arg(total));

    const bool hasVisibleEntries = shown > 0;
    m_emptyState->setVisible(!hasVisibleEntries);
    m_list->setVisible(hasVisibleEntries);
    m_emptyState->setText(total == 0 ? tr("No call history yet")
                                     : tr("No calls match the current filters"));

    m_clearBtn->setEnabled(total > 0);
    m_exportJsonBtn->setEnabled(total > 0);
    m_exportCsvBtn->setEnabled(total > 0);
    m_redialBtn->setEnabled(!currentSelection().remoteUri.isEmpty());
}

CallHistoryEntry CallHistoryPanel::currentSelection() const
{
    const QModelIndex proxyIdx = m_list->currentIndex();
    if (!proxyIdx.isValid())
        return {};
    return m_model->entryAt(m_proxy->mapToSource(proxyIdx).row());
}

void CallHistoryPanel::onSearchTextChanged(const QString &text)
{
    m_proxy->setSearchText(text);
    updateResultsLabel();
}

void CallHistoryPanel::onKindFilterChanged(int index)
{
    const auto filter = static_cast<CallHistoryFilterProxyModel::KindFilter>(
        m_kindFilterCombo->itemData(index).toInt());
    m_proxy->setKindFilter(filter);
    updateResultsLabel();
}

void CallHistoryPanel::onDateFilterChanged(int index)
{
    const auto filter = static_cast<CallHistoryFilterProxyModel::DateFilter>(
        m_dateFilterCombo->itemData(index).toInt());
    m_proxy->setDateFilter(filter);
    updateResultsLabel();
}

void CallHistoryPanel::onClearHistory()
{
    const auto reply = QMessageBox::question(this, tr("Clear History"),
        tr("Delete all recorded call history? This cannot be undone."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;
    CallHistoryStore::instance().clear();
}

void CallHistoryPanel::onExportJson()
{
    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Call History"),
        defaultDir + QStringLiteral("/call_history_export.json"),
        tr("JSON Files (*.json)"));
    if (path.isEmpty())
        return;

    if (!CallHistoryStore::instance().exportToJson(path)) {
        QMessageBox::warning(this, tr("Export Call History"),
            tr("Failed to write %1").arg(path));
    }
}

void CallHistoryPanel::onExportCsv()
{
    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Call History"),
        defaultDir + QStringLiteral("/call_history_export.csv"),
        tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    if (!CallHistoryStore::instance().exportToCsv(path)) {
        QMessageBox::warning(this, tr("Export Call History"),
            tr("Failed to write %1").arg(path));
    }
}

void CallHistoryPanel::onItemActivated(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    const CallHistoryEntry e = m_model->entryAt(m_proxy->mapToSource(index).row());
    if (!e.isNull())
        showDetails(e);
}

void CallHistoryPanel::onRedialSelected()
{
    const CallHistoryEntry e = currentSelection();
    if (e.remoteUri.isEmpty())
        return;
    emit redialRequested(e.remoteUri);
}

void CallHistoryPanel::showDetails(const CallHistoryEntry &e)
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Call Details"));
    dlg.setMinimumWidth(380);

    auto *outer = new QVBoxLayout(&dlg);

    auto *form = new QFormLayout();
    form->addRow(tr("Direction:"), new QLabel(callDirectionName(e.direction), &dlg));
    form->addRow(tr("Remote:"), new QLabel(e.displayName.isEmpty() ? e.remoteUri
                                            : QStringLiteral("%1 <%2>").arg(e.displayName, e.remoteUri), &dlg));
    form->addRow(tr("Profile:"), new QLabel(e.profileName, &dlg));
    form->addRow(tr("Start:"), new QLabel(e.startTime.isNull() ? tr("—")
                                           : e.startTime.toLocalTime().toString(Qt::TextDate), &dlg));
    form->addRow(tr("Answered:"), new QLabel(e.answerTime.isNull() ? tr("—")
                                              : e.answerTime.toLocalTime().toString(Qt::TextDate), &dlg));
    form->addRow(tr("Ended:"), new QLabel(e.endTime.isNull() ? tr("—")
                                           : e.endTime.toLocalTime().toString(Qt::TextDate), &dlg));
    form->addRow(tr("Duration:"), new QLabel(formatCallDuration(e.durationSec), &dlg));
    form->addRow(tr("Result:"), new QLabel(callResultName(e.result), &dlg));
    form->addRow(tr("Media:"), new QLabel(callHistoryBadges(e), &dlg));
    form->addRow(tr("Last SIP code:"), new QLabel(e.lastSipCode > 0 ? QString::number(e.lastSipCode) : tr("—"), &dlg));
    form->addRow(tr("Reason:"), new QLabel(e.reason.isEmpty() ? tr("—") : e.reason, &dlg));
    outer->addLayout(form);

    auto *actionRow = new QHBoxLayout();
    auto *redialBtn = new QPushButton(tr("Call Back"), &dlg);
    redialBtn->setEnabled(!e.remoteUri.isEmpty());
    auto *copyUriBtn = new QPushButton(tr("Copy URI"), &dlg);
    copyUriBtn->setEnabled(!e.remoteUri.isEmpty());
    auto *copySummaryBtn = new QPushButton(tr("Copy Summary"), &dlg);
    actionRow->addWidget(redialBtn);
    actionRow->addWidget(copyUriBtn);
    actionRow->addWidget(copySummaryBtn);
    actionRow->addStretch();
    outer->addLayout(actionRow);

    connect(redialBtn, &QPushButton::clicked, this, [this, &dlg, e]() {
        emit redialRequested(e.remoteUri);
        dlg.accept();
    });
    connect(copyUriBtn, &QPushButton::clicked, this, [e]() {
        QApplication::clipboard()->setText(e.remoteUri);
    });
    connect(copySummaryBtn, &QPushButton::clicked, this, [e]() {
        QStringList lines;
        lines << QStringLiteral("Direction: %1").arg(callDirectionName(e.direction));
        lines << QStringLiteral("Remote: %1").arg(e.displayName.isEmpty() ? e.remoteUri
            : QStringLiteral("%1 <%2>").arg(e.displayName, e.remoteUri));
        lines << QStringLiteral("Profile: %1").arg(e.profileName);
        lines << QStringLiteral("Start: %1").arg(e.startTime.isNull()
            ? QStringLiteral("—") : e.startTime.toLocalTime().toString(Qt::TextDate));
        lines << QStringLiteral("End: %1").arg(e.endTime.isNull()
            ? QStringLiteral("—") : e.endTime.toLocalTime().toString(Qt::TextDate));
        lines << QStringLiteral("Duration: %1").arg(formatCallDuration(e.durationSec));
        lines << QStringLiteral("Result: %1").arg(callResultName(e.result));
        lines << QStringLiteral("Media: %1").arg(callHistoryBadges(e));
        if (e.lastSipCode > 0)
            lines << QStringLiteral("SIP code: %1").arg(e.lastSipCode);
        if (!e.reason.isEmpty())
            lines << QStringLiteral("Reason: %1").arg(e.reason);
        QApplication::clipboard()->setText(lines.join(QStringLiteral("\n")));
    });

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    outer->addWidget(btns);

    dlg.exec();
}
