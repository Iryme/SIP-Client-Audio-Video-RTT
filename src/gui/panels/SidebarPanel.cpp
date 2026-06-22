#include "SidebarPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QFrame>

SidebarPanel::SidebarPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SidebarPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Account card
    auto *accountCard = new QFrame(this);
    accountCard->setObjectName("AccountCard");
    accountCard->setFrameShape(QFrame::StyledPanel);
    auto *cardLayout = new QVBoxLayout(accountCard);
    cardLayout->setContentsMargins(8, 8, 8, 8);
    cardLayout->setSpacing(2);

    m_accountName = new QLabel(tr("No Account"), accountCard);
    m_accountName->setObjectName("AccountName");
    m_accountName->setStyleSheet("font-weight: bold; font-size: 13px;");

    m_accountUri = new QLabel(tr("sip:user@server"), accountCard);
    m_accountUri->setObjectName("AccountUri");
    m_accountUri->setStyleSheet("color: #aaaaaa; font-size: 11px;");

    m_regStatus = new QLabel(tr("● Not registered"), accountCard);
    m_regStatus->setObjectName("RegStatus");
    m_regStatus->setStyleSheet("color: #e05050; font-size: 11px;");

    cardLayout->addWidget(m_accountName);
    cardLayout->addWidget(m_accountUri);
    cardLayout->addWidget(m_regStatus);
    layout->addWidget(accountCard);

    // Search
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search contacts..."));
    m_search->setObjectName("SearchField");
    layout->addWidget(m_search);

    // Contact list
    m_contactList = new QListWidget(this);
    m_contactList->setObjectName("ContactList");
    // Placeholder entries
    m_contactList->addItem(tr("Alice — sip:alice@example.com"));
    m_contactList->addItem(tr("Bob — sip:bob@example.com"));
    m_contactList->addItem(tr("Emergency — sip:emergency@psap.example"));
    layout->addWidget(m_contactList, 1);

    // Buttons row
    auto *btnRow = new QHBoxLayout();
    m_addContact = new QPushButton(tr("+ Contact"), this);
    m_addAccount = new QPushButton(tr("+ Account"), this);
    btnRow->addWidget(m_addContact);
    btnRow->addWidget(m_addAccount);
    layout->addLayout(btnRow);
}
