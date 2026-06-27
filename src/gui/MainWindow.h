#pragma once
#include <QMainWindow>
#include <QCloseEvent>

class DashboardPage;
class NavRail;
class ContactsPanel;
class VideoPanel;
class RttPanel;
class DiagnosticsPanel;
class SettingsPanel;
class SipLadderPage;
class AppStatusBar;
class QDialog;
class QSplitter;
class QStackedWidget;
class QLabel;
class QLineEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public:
    void updateSipBackendStatus();
    void setInitializingStatus(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNavPageRequested(const QString &page);
    void showSettingsDialog();
    void exportConfiguration();
    void importConfiguration();
    void showAboutDialog();
    void showDiagnosticsInfo();

private:
    void buildCentralWidget();
    void buildStatusBar();
    void requestApplicationShutdown();
    void restoreLayout();
    void saveLayout();
    QWidget *buildDashboardPage();
    QWidget *buildClientsPage();
    QWidget *buildLogsPage();

    // Build page[index] on first request and replace its placeholder.
    void ensurePage(int index);

    // Tracks which pages have been built (false = still a placeholder).
    static constexpr int kPageCount = 5;
    bool m_pageBuilt[kPageCount]{};

    // Dashboard page (kept for signal wiring in buildDashboardPage)
    DashboardPage    *m_dashboardPage{nullptr};

    // Panels
    NavRail          *m_navRail{nullptr};
    QStackedWidget   *m_pageStack{nullptr};
    ContactsPanel    *m_contactsPanel{nullptr};
    VideoPanel       *m_clientsVideoPanel{nullptr};
    VideoPanel       *m_videoPanel{nullptr};
    RttPanel         *m_rttPanel{nullptr};
    DiagnosticsPanel *m_diagnostics{nullptr};
    SettingsPanel    *m_settingsPanel{nullptr};
    SipLadderPage    *m_ladderPage{nullptr};
    AppStatusBar     *m_statusBar{nullptr};
    QDialog          *m_settingsDialog{nullptr};

    QLineEdit *m_clientsTargetInput{nullptr};
    bool m_shutdownRequested{false};

    // Splitters (saved for layout persistence)
    QSplitter *m_vertSplitter{nullptr};
    QSplitter *m_horzSplitter{nullptr};
};
