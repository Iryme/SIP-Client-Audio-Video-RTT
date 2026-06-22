#pragma once
#include <QApplication>

class MainWindow;

class Application : public QApplication
{
    Q_OBJECT
public:
    Application(int &argc, char **argv);
    ~Application() override;

private:
    void loadStyleSheet();

    MainWindow *m_mainWindow{nullptr};
};
