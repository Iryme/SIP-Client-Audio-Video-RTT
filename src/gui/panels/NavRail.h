#pragma once
#include <QWidget>
#include <QMap>
#include <QString>
#include <QIcon>

class QVBoxLayout;
class QToolButton;

class NavRail : public QWidget
{
    Q_OBJECT
public:
    explicit NavRail(QWidget *parent = nullptr);

    void setPageEnabled(const QString &page, bool enabled);
    void setPageActive(const QString &page);

signals:
    void pageRequested(const QString &page);
    void importConfigRequested();
    void exportConfigRequested();
    void helpRequested();
    void aboutRequested();
    void exitRequested();

private:
    QToolButton *addNavButton(QVBoxLayout *layout, const QString &svgPath,
                              const QString &label, const QString &page);
    QToolButton *addActionButton(QVBoxLayout *layout, const QString &svgPath,
                                 const QString &label, const QString &objectName);
    static QIcon makeIconFromSvg(const QByteArray &svgData, int size = 28);

    QMap<QString, QToolButton *> m_buttons;
    QString m_activePage;
};
