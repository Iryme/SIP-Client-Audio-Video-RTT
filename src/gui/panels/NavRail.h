#pragma once
#include <QWidget>
#include <QMap>
#include <QString>

class QVBoxLayout;
class QToolButton;

class NavRail : public QWidget
{
    Q_OBJECT
public:
    explicit NavRail(QWidget *parent = nullptr);

    // Enable or disable a nav button by page id.
    void setPageEnabled(const QString &page, bool enabled);

signals:
    void pageRequested(const QString &page);

private:
    QToolButton *addNavButton(QVBoxLayout *layout, const QString &label, const QString &page);

    QMap<QString, QToolButton *> m_buttons;
};
