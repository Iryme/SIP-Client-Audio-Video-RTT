#pragma once

#include <QWidget>

#include "sip/XcapModels.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// "XCAP" nav page (Task W099): a clearly separate zone from Presence /
// Messaging Diagnostics, per the task's explicit "do not cram into Call
// Control or Presence" requirement. Lets the user configure an XCAP server,
// run GET/PUT/DELETE against a document selector, view the result, and
// inspect the operation log (sourced from XcapDiagnosticsStore — no HTTP
// execution performed in this class, only display + XcapClient calls).
class XcapPage : public QWidget
{
    Q_OBJECT
public:
    explicit XcapPage(QWidget *parent = nullptr);

private slots:
    void onEnableToggled(bool on);
    void onConfigFieldChanged();
    void onTestConnectionClicked();
    void onGetClicked();
    void onPutClicked();
    void onDeleteClicked();
    void onClearLogClicked();
    void onOperationCompleted(const XcapResult &result);
    void onSelectionChanged();

private:
    XcapServerConfig currentConfig() const;
    XcapDocument     currentDocument() const;
    void addLogRow(const XcapResult &result);
    void rebuildLog();
    void updateControlsEnabled();
    void savePasswordIfChanged();

    QCheckBox   *m_enableCheck{nullptr};
    QLineEdit   *m_rootUriEdit{nullptr};
    QLineEdit   *m_xuiEdit{nullptr};
    QLineEdit   *m_usernameEdit{nullptr};
    QLineEdit   *m_passwordEdit{nullptr};
    QComboBox   *m_authModeCombo{nullptr};
    QCheckBox   *m_validateXmlCheck{nullptr};
    QSpinBox    *m_timeoutSpin{nullptr};
    QCheckBox   *m_verifyTlsCheck{nullptr};
    QPushButton *m_testConnectionBtn{nullptr};

    QComboBox   *m_auidCombo{nullptr};
    QLineEdit   *m_docXuiEdit{nullptr};
    QLineEdit   *m_documentNameEdit{nullptr};
    QLineEdit   *m_nodeSelectorEdit{nullptr};
    QPushButton *m_getBtn{nullptr};
    QPushButton *m_putBtn{nullptr};
    QPushButton *m_deleteBtn{nullptr};

    QPlainTextEdit *m_documentEdit{nullptr};
    QLabel          *m_resultLabel{nullptr};

    QTableWidget *m_logTable{nullptr};
    QPushButton  *m_clearLogBtn{nullptr};

    QList<XcapResult> m_logRows;
};
