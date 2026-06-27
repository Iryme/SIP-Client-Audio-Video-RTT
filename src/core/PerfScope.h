#pragma once
#include <QElapsedTimer>
#include <QString>
#include "core/Logger.h"

// RAII scoped timer — logs "[PERF] <tag> took N ms" on destruction.
// Also provides a shared app-wide monotonic clock: call initAppClock() once
// in main() before creating QApplication; then query msecsSinceAppStart()
// anywhere to get time elapsed since program start.
//
// Usage:
//   { PerfScope s("MyClass constructor"); ... }
//   // → [PERF] MyClass constructor took 312 ms
class PerfScope
{
public:
    // Call once in main() before QApplication is constructed.
    static void initAppClock() { s_appClock.start(); }

    // Milliseconds since initAppClock() was called. Returns -1 if not initialised.
    static qint64 msecsSinceAppStart() { return s_appClock.isValid() ? s_appClock.elapsed() : -1; }

    explicit PerfScope(const QString &tag) : m_tag(tag) { m_timer.start(); }

    ~PerfScope()
    {
        Logger::instance().info(LogCategory::Perf,
            QStringLiteral("[PERF] %1 took %2 ms").arg(m_tag).arg(m_timer.elapsed()));
    }

    qint64 elapsed() const { return m_timer.elapsed(); }

private:
    QString       m_tag;
    QElapsedTimer m_timer;

    // C++17 inline static — single definition across all TUs, no .cpp needed.
    inline static QElapsedTimer s_appClock{};
};
