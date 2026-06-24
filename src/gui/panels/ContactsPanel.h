#pragma once

#include <QWidget>

class QListWidget;
class QPushButton;

// Minimal contacts list: Add / Remove buttons, click to dial.
class ContactsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ContactsPanel(QWidget *parent = nullptr);

signals:
    // Emitted when the user double-clicks or presses Call on a contact.
    void dialRequested(const QString &uri);

private slots:
    void onAddContact();
    void onRemoveContact();
    void onContactActivated(int row);
    void refresh();

private:
    QListWidget *m_list{nullptr};
    QPushButton *m_addBtn{nullptr};
    QPushButton *m_removeBtn{nullptr};
    QPushButton *m_callBtn{nullptr};
};
