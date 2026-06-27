#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

// Contacts / target / quick actions panel for the Clients page.
class ContactsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ContactsPanel(QWidget *parent = nullptr);

    void setTargetUri(const QString &uri);
    QString targetUri() const;

signals:
    // Emitted when the user double-clicks or presses Call on a contact.
    void dialRequested(const QString &uri);

private slots:
    void onAddContact();
    void onRemoveContact();
    void onCallSelected();
    void onCopyTarget();
    void onClearTarget();
    void refresh();

private:
    void syncActionButtons();
    QString selectedContactUri() const;

    QLabel *m_emptyState{nullptr};
    QLineEdit *m_targetEdit{nullptr};
    QListWidget *m_list{nullptr};
    QPushButton *m_addBtn{nullptr};
    QPushButton *m_removeBtn{nullptr};
    QPushButton *m_callBtn{nullptr};
    QPushButton *m_copyBtn{nullptr};
    QPushButton *m_clearBtn{nullptr};
};
