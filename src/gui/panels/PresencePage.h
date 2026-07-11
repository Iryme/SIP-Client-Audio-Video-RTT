#pragma once

#include <QWidget>

#include "sip/PresenceInfo.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// "Presence" nav page (Task W098): a clearly separate zone from Messaging
// Diagnostics / Call History, per the task's explicit "do not cram Presence
// into call control or Messaging Diagnostics" requirement. Lets the user
// subscribe/unsubscribe to a configurable SIP URI's presence, shows the
// resulting per-entity state table (sourced from PresenceStore — no PIDF
// parsing performed in this class, only display), and offers an
// experimental control for the account's own published state.
class PresencePage : public QWidget
{
    Q_OBJECT
public:
    explicit PresencePage(QWidget *parent = nullptr);

private slots:
    void onPresenceUpdated(const PresenceInfo &info);
    void onCleared();

    void onEnablePresenceToggled(bool on);
    void onEnableSubscribeToggled(bool on);
    void onEnablePublishToggled(bool on);
    void onAutoResubscribeToggled(bool on);

    void onSubscribeClicked();
    void onUnsubscribeClicked();
    void onRefreshClicked();
    void onSetOwnStateClicked();
    void onClearClicked();
    void onSelectionChanged();

private:
    void rebuildTable();
    void addOrUpdateRow(const PresenceInfo &info);
    void updateControlsEnabled();

    QCheckBox *m_enablePresenceCheck{nullptr};
    QCheckBox *m_enableSubscribeCheck{nullptr};
    QCheckBox *m_enablePublishCheck{nullptr};
    QCheckBox *m_autoResubscribeCheck{nullptr};

    QLineEdit   *m_targetUriEdit{nullptr};
    QSpinBox    *m_expiresSpin{nullptr};
    QPushButton *m_subscribeBtn{nullptr};
    QPushButton *m_unsubscribeBtn{nullptr};
    QPushButton *m_refreshBtn{nullptr};
    QPushButton *m_clearBtn{nullptr};
    QLabel      *m_statusLabel{nullptr};

    // Own-state (experimental Publish) controls.
    QComboBox   *m_ownStateCombo{nullptr};
    QPushButton *m_setOwnStateBtn{nullptr};
    QLabel      *m_publishExperimentalLabel{nullptr};

    QTableWidget *m_table{nullptr};

    QList<PresenceInfo> m_rows; // current per-entity snapshot, row-aligned with m_table
};
