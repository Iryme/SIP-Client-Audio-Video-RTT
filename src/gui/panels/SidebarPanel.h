#pragma once
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class SidebarPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SidebarPanel(QWidget *parent = nullptr);

private:
    QLabel       *m_accountName{nullptr};
    QLabel       *m_accountUri{nullptr};
    QLabel       *m_regStatus{nullptr};
    QLineEdit    *m_search{nullptr};
    QListWidget  *m_contactList{nullptr};
    QPushButton  *m_addContact{nullptr};
    QPushButton  *m_addAccount{nullptr};
};
