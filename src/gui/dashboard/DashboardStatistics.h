#pragma once
#include <QHash>
#include <QWidget>

class QGridLayout;
class QLabel;

class DashboardStatistics : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardStatistics(QWidget *parent = nullptr);

    void addSection(const QString &title);
    void addStat(const QString &key, const QString &label);
    void setValue(const QString &key, const QString &value);

private:
    QGridLayout             *m_grid{nullptr};
    QHash<QString, QLabel *> m_valueLabels;
    int                      m_row{0};
};
