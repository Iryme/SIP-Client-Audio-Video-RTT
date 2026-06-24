#include "ContactsPanel.h"

#include "core/ContactStore.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ContactsPanel::ContactsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ContactsPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(tr("Contacts"), this);
    title->setStyleSheet("font-weight: bold; font-size: 13px;");
    layout->addWidget(title);

    m_list = new QListWidget(this);
    m_list->setObjectName("ContactList");
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

    // Button row
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(4);

    m_addBtn = new QPushButton(tr("+ Add"), this);
    m_addBtn->setObjectName("AddContactBtn");
    m_addBtn->setFixedHeight(24);

    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_removeBtn->setObjectName("RemoveContactBtn");
    m_removeBtn->setFixedHeight(24);
    m_removeBtn->setEnabled(false);

    m_callBtn = new QPushButton(tr("Call"), this);
    m_callBtn->setObjectName("CallContactBtn");
    m_callBtn->setFixedHeight(24);
    m_callBtn->setEnabled(false);

    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_callBtn);
    layout->addLayout(btnRow);

    connect(m_addBtn,    &QPushButton::clicked,
            this, &ContactsPanel::onAddContact);
    connect(m_removeBtn, &QPushButton::clicked,
            this, &ContactsPanel::onRemoveContact);
    connect(m_callBtn,   &QPushButton::clicked, this, [this] {
        onContactActivated(m_list->currentRow());
    });
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        const bool valid = (row >= 0);
        m_removeBtn->setEnabled(valid);
        m_callBtn->setEnabled(valid);
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        onContactActivated(m_list->currentRow());
    });

    connect(&ContactStore::instance(), &ContactStore::contactsChanged,
            this, &ContactsPanel::refresh);

    refresh();
}

void ContactsPanel::refresh()
{
    const int prevRow = m_list->currentRow();
    m_list->clear();

    const auto contacts = ContactStore::instance().contacts();
    for (const Contact &c : contacts) {
        const QString label = c.name.isEmpty()
            ? c.uri
            : QStringLiteral("%1  <%2>").arg(c.name, c.uri);
        m_list->addItem(label);
    }

    if (prevRow >= 0 && prevRow < m_list->count())
        m_list->setCurrentRow(prevRow);

    const bool hasSelection = (m_list->currentRow() >= 0);
    m_removeBtn->setEnabled(hasSelection);
    m_callBtn->setEnabled(hasSelection);
}

void ContactsPanel::onAddContact()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add Contact"));
    dlg.setMinimumWidth(320);

    auto *form = new QFormLayout(&dlg);
    auto *nameEdit = new QLineEdit(&dlg);
    auto *uriEdit  = new QLineEdit(&dlg);
    uriEdit->setPlaceholderText(tr("sip:user@domain"));

    form->addRow(tr("Name (optional):"), nameEdit);
    form->addRow(tr("SIP URI:"),         uriEdit);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(btns);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString uri = uriEdit->text().trimmed();
    if (uri.isEmpty()) {
        QMessageBox::warning(this, tr("Add Contact"), tr("SIP URI must not be empty."));
        return;
    }

    Contact c;
    c.name = nameEdit->text().trimmed();
    c.uri  = uri.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive) ? uri
             : QStringLiteral("sip:%1").arg(uri);
    ContactStore::instance().add(c);
}

void ContactsPanel::onRemoveContact()
{
    const int row = m_list->currentRow();
    if (row < 0)
        return;
    ContactStore::instance().remove(row);
}

void ContactsPanel::onContactActivated(int row)
{
    const auto contacts = ContactStore::instance().contacts();
    if (row < 0 || row >= contacts.size())
        return;
    emit dialRequested(contacts[row].uri);
}
