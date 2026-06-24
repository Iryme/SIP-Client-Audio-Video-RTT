#include "ContactStore.h"

#include <QSettings>

static constexpr char kGroup[] = "Contacts";
static constexpr char kKey[]   = "list";

ContactStore &ContactStore::instance()
{
    static ContactStore s;
    return s;
}

ContactStore::ContactStore() : QObject(nullptr)
{
    load();
}

QList<Contact> ContactStore::contacts() const
{
    return m_contacts;
}

void ContactStore::add(const Contact &c)
{
    if (c.uri.trimmed().isEmpty())
        return;
    m_contacts.append(c);
    save();
    emit contactsChanged();
}

void ContactStore::remove(int index)
{
    if (index < 0 || index >= m_contacts.size())
        return;
    m_contacts.removeAt(index);
    save();
    emit contactsChanged();
}

void ContactStore::load()
{
    m_contacts.clear();
    QSettings s;
    s.beginGroup(kGroup);
    const QStringList entries = s.value(kKey).toStringList();
    s.endGroup();

    for (const QString &entry : entries) {
        const int sep = entry.indexOf(QLatin1Char('|'));
        if (sep < 0)
            continue;
        Contact c;
        c.name = entry.left(sep);
        c.uri  = entry.mid(sep + 1);
        if (!c.uri.isEmpty())
            m_contacts.append(c);
    }
}

void ContactStore::save() const
{
    QStringList entries;
    entries.reserve(m_contacts.size());
    for (const Contact &c : m_contacts)
        entries.append(c.name + QLatin1Char('|') + c.uri);

    QSettings s;
    s.beginGroup(kGroup);
    s.setValue(kKey, entries);
    s.endGroup();
}
