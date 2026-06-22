#pragma once
#include "LogLevel.h"
#include "LogCategory.h"
#include <QDateTime>
#include <QString>

struct LogEntry {
    QDateTime   timestamp;
    LogLevel    level;
    LogCategory category;
    QString     message;
    QString     payload;
};
