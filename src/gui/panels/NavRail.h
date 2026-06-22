#pragma once
#include <QWidget>

class QVBoxLayout;
class QToolButton;

class NavRail : public QWidget
{
    Q_OBJECT
public:
    explicit NavRail(QWidget *parent = nullptr);

signals:
    void pageRequested(const QString &page);

private:
    QToolButton *addNavButton(QVBoxLayout *layout, const QString &label, const QString &page);
};
