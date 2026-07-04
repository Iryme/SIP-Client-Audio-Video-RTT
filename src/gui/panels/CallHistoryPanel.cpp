#include "CallHistoryPanel.h"

#include "core/CallHistoryStore.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

static constexpr int kEntryIdRole = Qt::UserRole + 1;

static QString formatDuration(int secs)
{
    const int h = secs / 3600;
    const int m = (secs % 3600) / 60;
    const int s = secs % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h, 2, 10, QLatin1Char('0'))
                                          .arg(m, 2, 10, QLatin1Char('0'))
                                          .arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0'))
                                   .arg(s, 2, 10, QLatin1Char('0'));
}

static QString badges(const CallHistoryEntry &e)
{
    QStringList b;
    if (e.hadAudio) b << QStringLiteral("Audio");
    if (e.hadVideo) b << QStringLiteral("Video");
    if (e.hadRtt)   b << QStringLiteral("RTT");
    return b.isEmpty() ? QStringLiteral("—") : b.join(QStringLiteral(" · "));
}

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

    m_emptyState = new QLabel(tr("No calls recorded yet."), this);
    m_emptyState->setObjectName("CallHistoryEmptyState");
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setWordWrap(true);
    m_emptyState->setStyleSheet("color: #8899aa; padding: 12px; border: 1px dashed #3b4d63;");
    layout->addWidget(m_emptyState);

    m_list = new QListWidget(this);
    m_list->setObjectName("CallHistoryList");
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);
    m_clearBtn = new QPushButton(tr("Clear History"), this);
    m_clearBtn->setObjectName("ClearCallHistoryBtn");
    m_exportBtn = new QPushButton(tr("Export JSON"), this);
    m_exportBtn->setObjectName("ExportCallHistoryBtn");
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_exportBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(m_clearBtn, &QPushButton::clicked, this, &CallHistoryPanel::onClearHistory);
    connect(m_exportBtn, &QPushButton::clicked, this, &CallHistoryPanel::onExportJson);
    connect(m_list, &QListWidget::itemActivated, this, &CallHistoryPanel::onItemActivated);

    connect(&CallHistoryStore::instance(), &CallHistoryStore::historyChanged,
            this, &CallHistoryPanel::refresh);

    refresh();
}

void CallHistoryPanel::refresh()
{
    const auto history = CallHistoryStore::instance().entries();

    m_list->clear();
    for (const CallHistoryEntry &e : history) {
        const QString who = e.displayName.isEmpty() ? e.remoteUri
                                                     : QStringLiteral("%1  <%2>").arg(e.displayName, e.remoteUri);
        const QString when = e.startTime.isNull() ? QString()
                                                    : e.startTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        const QString label = QStringLiteral("[%1] %2 — %3 — %4 — %5 — %6")
                                   .arg(callDirectionName(e.direction),
                                        who,
                                        when,
                                        formatDuration(e.durationSec),
                                        callResultName(e.result),
                                        badges(e));

        auto *item = new QListWidgetItem(label, m_list);
        item->setData(kEntryIdRole, e.id);
    }

    const bool hasEntries = !history.isEmpty();
    m_emptyState->setVisible(!hasEntries);
    m_list->setVisible(hasEntries);
    m_clearBtn->setEnabled(hasEntries);
    m_exportBtn->setEnabled(hasEntries);
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

void CallHistoryPanel::onItemActivated(QListWidgetItem *item)
{
    if (!item)
        return;
    showDetails(item->data(kEntryIdRole).toString());
}

void CallHistoryPanel::showDetails(const QString &entryId)
{
    const CallHistoryEntry e = CallHistoryStore::instance().entry(entryId);
    if (e.isNull())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Call Details"));
    dlg.setMinimumWidth(360);

    auto *form = new QFormLayout(&dlg);
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
    form->addRow(tr("Duration:"), new QLabel(formatDuration(e.durationSec), &dlg));
    form->addRow(tr("Result:"), new QLabel(callResultName(e.result), &dlg));
    form->addRow(tr("Media:"), new QLabel(badges(e), &dlg));
    form->addRow(tr("Last SIP code:"), new QLabel(e.lastSipCode > 0 ? QString::number(e.lastSipCode) : tr("—"), &dlg));
    form->addRow(tr("Reason:"), new QLabel(e.reason.isEmpty() ? tr("—") : e.reason, &dlg));

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    form->addRow(btns);

    dlg.exec();
}
