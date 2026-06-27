#include "ContactsPanel.h"

#include "core/ContactStore.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QGuiApplication>
#include <QMessageBox>
#include <QClipboard>
#include <QPushButton>
#include <QVBoxLayout>

ContactsPanel::ContactsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ContactsPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(tr("Contact / Target / Quick Actions"), this);
    title->setStyleSheet("font-weight: bold; font-size: 13px;");
    layout->addWidget(title);

    auto *targetRow = new QHBoxLayout();
    targetRow->setSpacing(6);
    auto *targetLabel = new QLabel(tr("Target:"), this);
    targetLabel->setStyleSheet("color: #9aa8b8;");
    m_targetEdit = new QLineEdit(this);
    m_targetEdit->setObjectName("TargetUriEdit");
    m_targetEdit->setPlaceholderText(tr("sip:user@domain or user@domain"));
    m_targetEdit->setFixedHeight(28);
    targetRow->addWidget(targetLabel);
    targetRow->addWidget(m_targetEdit, 1);
    layout->addLayout(targetRow);

    auto *quickRow = new QHBoxLayout();
    quickRow->setSpacing(4);
    m_callBtn = new QPushButton(tr("Call Selected"), this);
    m_callBtn->setObjectName("CallSelectedBtn");
    m_callBtn->setFixedHeight(26);
    m_copyBtn = new QPushButton(tr("Copy SIP URI"), this);
    m_copyBtn->setObjectName("CopySipUriBtn");
    m_copyBtn->setFixedHeight(26);
    m_clearBtn = new QPushButton(tr("Clear Target"), this);
    m_clearBtn->setObjectName("ClearTargetBtn");
    m_clearBtn->setFixedHeight(26);
    quickRow->addWidget(m_callBtn);
    quickRow->addWidget(m_copyBtn);
    quickRow->addWidget(m_clearBtn);
    layout->addLayout(quickRow);

    m_emptyState = new QLabel(tr("Contacts not implemented yet"), this);
    m_emptyState->setObjectName("ContactsEmptyState");
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setWordWrap(true);
    m_emptyState->setStyleSheet("color: #8899aa; padding: 12px; border: 1px dashed #3b4d63;");

    m_list = new QListWidget(this);
    m_list->setObjectName("ContactList");
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_emptyState);
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

    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(m_addBtn,    &QPushButton::clicked,
            this, &ContactsPanel::onAddContact);
    connect(m_removeBtn, &QPushButton::clicked,
            this, &ContactsPanel::onRemoveContact);
    connect(m_callBtn,   &QPushButton::clicked, this, [this] {
        onCallSelected();
    });
    connect(m_copyBtn, &QPushButton::clicked,
            this, &ContactsPanel::onCopyTarget);
    connect(m_clearBtn, &QPushButton::clicked,
            this, &ContactsPanel::onClearTarget);
    connect(m_targetEdit, &QLineEdit::textChanged, this, [this] {
        syncActionButtons();
    });
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        const bool valid = (row >= 0);
        m_removeBtn->setEnabled(valid);
        if (valid)
            m_targetEdit->setText(selectedContactUri());
        syncActionButtons();
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        onCallSelected();
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
    else if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    const bool hasContacts = (m_list->count() > 0);
    m_emptyState->setVisible(!hasContacts);
    m_list->setVisible(hasContacts);

    const bool hasSelection = (m_list->currentRow() >= 0);
    m_removeBtn->setEnabled(hasSelection);
    syncActionButtons();
}

void ContactsPanel::setTargetUri(const QString &uri)
{
    if (m_targetEdit)
        m_targetEdit->setText(uri.trimmed());
    syncActionButtons();
}

QString ContactsPanel::targetUri() const
{
    return m_targetEdit ? m_targetEdit->text().trimmed() : QString{};
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

QString ContactsPanel::selectedContactUri() const
{
    const auto contacts = ContactStore::instance().contacts();
    const int row = m_list ? m_list->currentRow() : -1;
    if (row >= 0 && row < contacts.size())
        return contacts[row].uri;
    return targetUri();
}

void ContactsPanel::syncActionButtons()
{
    const bool hasTarget = !targetUri().isEmpty();
    const bool hasSelection = (m_list && m_list->currentRow() >= 0);
    m_callBtn->setEnabled(hasTarget || hasSelection);
    m_copyBtn->setEnabled(hasTarget || hasSelection);
    m_clearBtn->setEnabled(hasTarget);
}

void ContactsPanel::onCallSelected()
{
    const QString uri = selectedContactUri();
    if (uri.isEmpty()) {
        QMessageBox::information(this, tr("Call Selected"),
                                 tr("Select a contact or enter a target SIP URI first."));
        return;
    }
    emit dialRequested(uri);
}

void ContactsPanel::onCopyTarget()
{
    const QString uri = selectedContactUri();
    if (uri.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(uri);
}

void ContactsPanel::onClearTarget()
{
    if (m_targetEdit)
        m_targetEdit->clear();
    syncActionButtons();
}
