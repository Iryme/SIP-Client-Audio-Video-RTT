#pragma once
#include <QMainWindow>
#include <QCloseEvent>

class NavRail;
class SidebarPanel;
class ContactsPanel;
class CallPanel;
class VideoPanel;
class RttPanel;
class DiagnosticsPanel;
class MediaPanel;
class AppStatusBar;
class QAction;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public:
    void updateSipBackendStatus();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNavPageRequested(const QString &page);

private:
    void buildMenuBar();
    void buildCentralWidget();
    void buildStatusBar();
    void restoreLayout();
    void saveLayout();

    // Panels
    NavRail          *m_navRail{nullptr};
    QStackedWidget   *m_sidebarStack{nullptr};
    SidebarPanel     *m_sidebar{nullptr};
    ContactsPanel    *m_contactsPanel{nullptr};
    CallPanel        *m_callPanel{nullptr};
    VideoPanel       *m_videoPanel{nullptr};
    RttPanel         *m_rttPanel{nullptr};
    DiagnosticsPanel *m_diagnostics{nullptr};
    MediaPanel       *m_mediaPanel{nullptr};
    AppStatusBar     *m_statusBar{nullptr};
    QTabWidget       *m_infoTabs{nullptr};

    // Menu actions that need to be stored
    QAction *m_actNewCall{nullptr};
    QAction *m_actAddContact{nullptr};

    // Splitters (saved for layout persistence)
    QSplitter *m_vertSplitter{nullptr};
    QSplitter *m_horzSplitter{nullptr};
};
