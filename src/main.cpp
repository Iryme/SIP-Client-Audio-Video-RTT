#include <QApplication>
#include "app/Application.h"
#include "core/PerfScope.h"

int main(int argc, char *argv[])
{
    PerfScope::initAppClock();

    Application app(argc, argv);

    Logger::instance().info(LogCategory::Perf,
        QStringLiteral("[PERF] main(): startup complete, entering event loop at T+%1 ms")
            .arg(PerfScope::msecsSinceAppStart()));

    return app.exec();
}
