#pragma once

#include <QObject>
#include <QString>
#include <QList>

struct Contact {
    QString name;
    QString uri;
};

// Persists SIP contacts (name + URI) in QSettings.
// Thread: main thread only.
class ContactStore : public QObject
{
    Q_OBJECT
public:
    static ContactStore &instance();

    QList<Contact> contacts() const;
    void add(const Contact &c);
    void remove(int index);

signals:
    void contactsChanged();

private:
    ContactStore();
    void load();
    void save() const;

    QList<Contact> m_contacts;
};
