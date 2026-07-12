#pragma once
#include <QWidget>

#include "msrp/MsrpDiagnosticsEvent.h"
#include "msrp/MsrpSessionInfo.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class MsrpSession;

// "MSRP" nav page (Task W100): a clearly separate zone from Call Control/
// Presence/XCAP. Configuration + a manual test session (active connector or
// passive listener, driven entirely by user/config-supplied host/port/path
// — never an arbitrary un-negotiated raw-frame injection by default) +
// active-session table (from MsrpSessionStore, includes live-call-detected
// sessions via MsrpSipIntegration) + diagnostics frame log (from
// MsrpDiagnosticsStore). See docs/msrp-foundation.md for why this page's
// manual session is independent of the live call SDP pipeline.
class MsrpPage : public QWidget
{
    Q_OBJECT
public:
    explicit MsrpPage(QWidget *parent = nullptr);
    ~MsrpPage() override;

private slots:
    void onEnableToggled(bool on);
    void onConfigFieldChanged();
    void onStartActiveClicked();
    void onStartPassiveClicked();
    void onSendTestMessageClicked();
    void onDisconnectClicked();
    void onClearDiagnosticsClicked();
    void onExportJsonClicked();
    void onExportTxtClicked();

    void onSessionUpdated(const MsrpSessionInfo &info);
    void onSessionRemoved(const QString &sessionKey);
    void onDiagnosticEvent(const MsrpDiagnosticsEvent &event);

private:
    void addOrUpdateSessionRow(const MsrpSessionInfo &info);
    void addDiagnosticRow(const MsrpDiagnosticsEvent &event);
    void rebuildSessionTable();
    void rebuildDiagnosticsTable();
    void updateControlsEnabled();

    QCheckBox *m_enableCheck{nullptr};
    QComboBox *m_transportModeCombo{nullptr};
    QCheckBox *m_enableTcpCheck{nullptr};
    QCheckBox *m_enableTlsCheck{nullptr};
    QLineEdit *m_bindAddressEdit{nullptr};
    QLineEdit *m_advertisedHostEdit{nullptr};
    QComboBox *m_portModeCombo{nullptr};
    QSpinBox  *m_fixedPortSpin{nullptr};
    QCheckBox *m_requestReportsCheck{nullptr};
    QSpinBox  *m_chunkSizeSpin{nullptr};
    QSpinBox  *m_connectTimeoutSpin{nullptr};

    QLineEdit   *m_remoteHostEdit{nullptr};
    QSpinBox    *m_remotePortSpin{nullptr};
    QLineEdit   *m_remoteSessionIdEdit{nullptr};
    QPushButton *m_startActiveBtn{nullptr};
    QPushButton *m_startPassiveBtn{nullptr};
    QPlainTextEdit *m_testMessageEdit{nullptr};
    QPushButton *m_sendTestBtn{nullptr};
    QPushButton *m_disconnectBtn{nullptr};
    QLabel      *m_statusLabel{nullptr};

    QTableWidget *m_sessionsTable{nullptr};
    QTableWidget *m_diagnosticsTable{nullptr};
    QPushButton  *m_clearDiagBtn{nullptr};
    QPushButton  *m_exportJsonBtn{nullptr};
    QPushButton  *m_exportTxtBtn{nullptr};

    MsrpSession *m_testSession{nullptr};
    QList<MsrpSessionInfo> m_sessionRows;
    QList<MsrpDiagnosticsEvent> m_diagRows;
};
