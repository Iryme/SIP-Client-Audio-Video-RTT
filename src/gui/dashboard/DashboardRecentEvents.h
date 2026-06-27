#pragma once
#include <QWidget>
#include "core/Logger.h"

class QVBoxLayout;

class DashboardRecentEvents : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardRecentEvents(QWidget *parent = nullptr);

private slots:
    void onEntryAdded(const LogEntry &entry);

private:
    static bool isRelevant(const LogEntry &entry);
    static QString colorFor(const LogEntry &entry);
    void insertEventRow(const QString &time, const QString &message, const QString &color);

    QVBoxLayout *m_list{nullptr};
    int          m_count{0};
    static constexpr int kMaxEvents = 10;
};
