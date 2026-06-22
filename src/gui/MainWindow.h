#pragma once
#include <QMainWindow>
#include <QCloseEvent>

class NavRail;
class SidebarPanel;
class CallPanel;
class VideoPanel;
class RttPanel;
class DiagnosticsPanel;
class MediaPanel;
class AppStatusBar;
class QSplitter;
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

private:
    void buildMenuBar();
    void buildCentralWidget();
    void buildStatusBar();
    void restoreLayout();
    void saveLayout();

    // Panels
    NavRail          *m_navRail{nullptr};
    SidebarPanel     *m_sidebar{nullptr};
    CallPanel        *m_callPanel{nullptr};
    VideoPanel       *m_videoPanel{nullptr};
    RttPanel         *m_rttPanel{nullptr};
    DiagnosticsPanel *m_diagnostics{nullptr};
    MediaPanel       *m_mediaPanel{nullptr};
    AppStatusBar     *m_statusBar{nullptr};

    // Splitters (saved for layout persistence)
    QSplitter *m_vertSplitter{nullptr}; // main area vs diagnostics
    QSplitter *m_horzSplitter{nullptr}; // sidebar | center | right
};
