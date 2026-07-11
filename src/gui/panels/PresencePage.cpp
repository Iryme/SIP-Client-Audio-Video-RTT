#include "PresencePage.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "sip/PresenceDiagnosticsStore.h"
#include "sip/PresenceStore.h"
#include "sip/SipManager.h"

namespace {
constexpr int kColEntity = 0;
constexpr int kColBasic = 1;
constexpr int kColExtended = 2;
constexpr int kColNote = 3;
constexpr int kColSubState = 4;
constexpr int kColExpires = 5;
constexpr int kColLastUpdate = 6;
constexpr int kColStatus = 7;
constexpr int kColCount = 8;
} // namespace

PresencePage::PresencePage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    // ---- Feature toggles (Task W098 requirement 13: everything off by default) ----
    auto *toggleBox = new QGroupBox(tr("Presence"), this);
    auto *toggleLayout = new QHBoxLayout(toggleBox);
    m_enablePresenceCheck = new QCheckBox(tr("Enable Presence"), toggleBox);
    m_enableSubscribeCheck = new QCheckBox(tr("Enable Subscribe"), toggleBox);
    m_enablePublishCheck = new QCheckBox(tr("Enable Publish (experimental)"), toggleBox);
    m_autoResubscribeCheck = new QCheckBox(tr("Auto-resubscribe"), toggleBox);
    m_enablePresenceCheck->setChecked(AppSettings::enablePresence());
    m_enableSubscribeCheck->setChecked(AppSettings::enablePresenceSubscribe());
    m_enablePublishCheck->setChecked(AppSettings::enablePresencePublish());
    m_autoResubscribeCheck->setChecked(AppSettings::presenceAutoResubscribe());
    toggleLayout->addWidget(m_enablePresenceCheck);
    toggleLayout->addWidget(m_enableSubscribeCheck);
    toggleLayout->addWidget(m_enablePublishCheck);
    toggleLayout->addWidget(m_autoResubscribeCheck);
    toggleLayout->addStretch(1);
    root->addWidget(toggleBox);

    connect(m_enablePresenceCheck, &QCheckBox::toggled, this, &PresencePage::onEnablePresenceToggled);
    connect(m_enableSubscribeCheck, &QCheckBox::toggled, this, &PresencePage::onEnableSubscribeToggled);
    connect(m_enablePublishCheck, &QCheckBox::toggled, this, &PresencePage::onEnablePublishToggled);
    connect(m_autoResubscribeCheck, &QCheckBox::toggled, this, &PresencePage::onAutoResubscribeToggled);

    // ---- Subscription controls ----
    auto *subBox = new QGroupBox(tr("Subscribe to a SIP URI"), this);
    auto *subLayout = new QHBoxLayout(subBox);
    subLayout->addWidget(new QLabel(tr("Target SIP URI:"), subBox));
    m_targetUriEdit = new QLineEdit(subBox);
    m_targetUriEdit->setPlaceholderText(QStringLiteral("sip:user@domain"));
    subLayout->addWidget(m_targetUriEdit, 1);
    subLayout->addWidget(new QLabel(tr("Expires (s):"), subBox));
    m_expiresSpin = new QSpinBox(subBox);
    m_expiresSpin->setRange(30, 86400);
    m_expiresSpin->setValue(AppSettings::presenceDefaultExpiresSeconds());
    subLayout->addWidget(m_expiresSpin);
    m_subscribeBtn = new QPushButton(tr("Subscribe"), subBox);
    m_unsubscribeBtn = new QPushButton(tr("Unsubscribe"), subBox);
    m_refreshBtn = new QPushButton(tr("Refresh"), subBox);
    subLayout->addWidget(m_subscribeBtn);
    subLayout->addWidget(m_unsubscribeBtn);
    subLayout->addWidget(m_refreshBtn);
    root->addWidget(subBox);

    connect(m_subscribeBtn, &QPushButton::clicked, this, &PresencePage::onSubscribeClicked);
    connect(m_unsubscribeBtn, &QPushButton::clicked, this, &PresencePage::onUnsubscribeClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &PresencePage::onRefreshClicked);
    connect(m_expiresSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [](int value) { AppSettings::setPresenceDefaultExpiresSeconds(value); });

    // ---- Own state (experimental Publish) ----
    auto *ownBox = new QGroupBox(tr("Own presence status"), this);
    auto *ownLayout = new QHBoxLayout(ownBox);
    m_ownStateCombo = new QComboBox(ownBox);
    m_ownStateCombo->addItem(tr("Available"), QStringLiteral("available"));
    m_ownStateCombo->addItem(tr("Away"), QStringLiteral("away"));
    m_ownStateCombo->addItem(tr("Busy"), QStringLiteral("busy"));
    m_ownStateCombo->addItem(tr("Do Not Disturb"), QStringLiteral("do-not-disturb"));
    m_ownStateCombo->addItem(tr("Offline"), QStringLiteral("offline"));
    ownLayout->addWidget(m_ownStateCombo);
    m_setOwnStateBtn = new QPushButton(tr("Set"), ownBox);
    ownLayout->addWidget(m_setOwnStateBtn);
    m_publishExperimentalLabel = new QLabel(
        tr("Experimental: requires \"Enable Publish\" and re-registration; depends on server support for PUBLISH. See docs/presence.md."),
        ownBox);
    m_publishExperimentalLabel->setWordWrap(true);
    ownLayout->addWidget(m_publishExperimentalLabel, 1);
    root->addWidget(ownBox);

    connect(m_setOwnStateBtn, &QPushButton::clicked, this, &PresencePage::onSetOwnStateClicked);

    // ---- Status + Clear ----
    auto *statusLayout = new QHBoxLayout;
    m_statusLabel = new QLabel(this);
    m_clearBtn = new QPushButton(tr("Clear"), this);
    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_clearBtn);
    root->addLayout(statusLayout);
    connect(m_clearBtn, &QPushButton::clicked, this, &PresencePage::onClearClicked);

    // ---- Contacts / subscriptions table ----
    m_table = new QTableWidget(0, kColCount, this);
    m_table->setHorizontalHeaderLabels({
        tr("Entity"), tr("Basic"), tr("Extended"), tr("Note"),
        tr("Subscription"), tr("Expires"), tr("Last Update"), tr("Status/Error")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &PresencePage::onSelectionChanged);

    connect(&PresenceStore::instance(), &PresenceStore::presenceUpdated,
            this, &PresencePage::onPresenceUpdated);
    connect(&PresenceStore::instance(), &PresenceStore::cleared,
            this, &PresencePage::onCleared);

    // Ensures the raw SUBSCRIBE/NOTIFY diagnostics feed (SIP Ladder /
    // Interop JSON export "presenceEvents") starts capturing as soon as
    // this page is opened, rather than only on first export.
    PresenceDiagnosticsStore::instance();

    rebuildTable();
    updateControlsEnabled();
}

void PresencePage::updateControlsEnabled()
{
    const bool presenceOn = AppSettings::enablePresence();
    const bool subscribeOn = presenceOn && AppSettings::enablePresenceSubscribe();
    m_targetUriEdit->setEnabled(subscribeOn);
    m_expiresSpin->setEnabled(subscribeOn);
    m_subscribeBtn->setEnabled(subscribeOn);
    m_unsubscribeBtn->setEnabled(subscribeOn);
    m_refreshBtn->setEnabled(subscribeOn);
    m_ownStateCombo->setEnabled(presenceOn);
    m_setOwnStateBtn->setEnabled(presenceOn);
}

void PresencePage::onEnablePresenceToggled(bool on)
{
    AppSettings::setEnablePresence(on);
    updateControlsEnabled();
}

void PresencePage::onEnableSubscribeToggled(bool on)
{
    AppSettings::setEnablePresenceSubscribe(on);
    updateControlsEnabled();
}

void PresencePage::onEnablePublishToggled(bool on)
{
    AppSettings::setEnablePresencePublish(on);
    m_statusLabel->setText(tr("Publish setting changed; re-register the active profile for it to take effect."));
}

void PresencePage::onAutoResubscribeToggled(bool on)
{
    AppSettings::setPresenceAutoResubscribe(on);
}

void PresencePage::onSubscribeClicked()
{
    const QString target = m_targetUriEdit->text().trimmed();
    if (target.isEmpty()) {
        m_statusLabel->setText(tr("Enter a target SIP URI first."));
        return;
    }
    QString error;
    if (SipManager::instance().subscribePresence(target, error))
        m_statusLabel->setText(tr("Subscribe requested: %1").arg(target));
    else
        m_statusLabel->setText(tr("Subscribe failed: %1").arg(error));
}

void PresencePage::onUnsubscribeClicked()
{
    const QString target = m_targetUriEdit->text().trimmed();
    if (target.isEmpty()) {
        m_statusLabel->setText(tr("Enter a target SIP URI first."));
        return;
    }
    QString error;
    if (SipManager::instance().unsubscribePresence(target, error))
        m_statusLabel->setText(tr("Unsubscribed: %1").arg(target));
    else
        m_statusLabel->setText(tr("Unsubscribe failed: %1").arg(error));
}

void PresencePage::onRefreshClicked()
{
    const QString target = m_targetUriEdit->text().trimmed();
    if (target.isEmpty()) {
        m_statusLabel->setText(tr("Enter a target SIP URI first."));
        return;
    }
    QString error;
    if (SipManager::instance().refreshPresenceSubscription(target, error))
        m_statusLabel->setText(tr("Refresh requested: %1").arg(target));
    else
        m_statusLabel->setText(tr("Refresh failed: %1").arg(error));
}

void PresencePage::onSetOwnStateClicked()
{
    const QString activity = m_ownStateCombo->currentData().toString();
    const QString basic = activity == QStringLiteral("offline") ? QStringLiteral("closed") : QStringLiteral("open");
    AppSettings::setPresenceDefaultState(activity);

    QString error;
    if (SipManager::instance().setOwnPresenceState(basic, activity, QString(), error))
        m_statusLabel->setText(tr("Own presence status set to %1.").arg(m_ownStateCombo->currentText()));
    else
        m_statusLabel->setText(tr("Failed to set own presence status: %1").arg(error));
}

void PresencePage::onClearClicked()
{
    PresenceStore::instance().clear();
}

void PresencePage::onSelectionChanged()
{
    const auto selected = m_table->selectionModel() ? m_table->selectionModel()->selectedRows() : QModelIndexList();
    if (selected.isEmpty())
        return;
    const int row = selected.first().row();
    if (row < 0 || row >= m_rows.size())
        return;
    m_targetUriEdit->setText(m_rows.at(row).entityUri);
}

void PresencePage::onPresenceUpdated(const PresenceInfo &info)
{
    addOrUpdateRow(info);
}

void PresencePage::onCleared()
{
    m_rows.clear();
    m_table->setRowCount(0);
}

void PresencePage::rebuildTable()
{
    m_rows = PresenceStore::instance().snapshot();
    m_table->setRowCount(0);
    for (const PresenceInfo &info : m_rows)
        addOrUpdateRow(info);
}

void PresencePage::addOrUpdateRow(const PresenceInfo &info)
{
    int row = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).entityUri == info.entityUri) {
            row = i;
            break;
        }
    }
    if (row < 0) {
        row = m_rows.size();
        m_rows.append(info);
        m_table->insertRow(row);
        for (int col = 0; col < kColCount; ++col)
            m_table->setItem(row, col, new QTableWidgetItem());
    } else {
        m_rows[row] = info;
    }

    m_table->item(row, kColEntity)->setText(info.entityUri);
    m_table->item(row, kColBasic)->setText(PresenceInfo::basicStatusToString(info.basicStatus));
    m_table->item(row, kColExtended)->setText(PresenceInfo::extendedStatusToString(info.extendedStatus));
    m_table->item(row, kColNote)->setText(info.note);
    m_table->item(row, kColSubState)->setText(PresenceInfo::subscriptionStateToString(info.subscriptionState));
    m_table->item(row, kColExpires)->setText(info.expires >= 0 ? QString::number(info.expires) : QString());
    m_table->item(row, kColLastUpdate)->setText(
        info.timestamp.isValid() ? info.timestamp.toString(Qt::ISODateWithMs) : QString());
    m_table->item(row, kColStatus)->setText(
        info.subscriptionState == PresenceInfo::SubscriptionState::Terminated
            ? info.subscriptionReason : QString());
}
