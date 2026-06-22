#include "SidebarPanel.h"
#include "gui/dialogs/SipProfileEditorDialog.h"
#include "sip/SipProfileManager.h"
#include "core/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QFrame>

SidebarPanel::SidebarPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SidebarPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Profile selector
    m_profileSelector = new QComboBox(this);
    m_profileSelector->setObjectName("ProfileSelector");
    m_profileSelector->setToolTip(tr("Select active SIP profile"));
    layout->addWidget(m_profileSelector);

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

    m_accountUri = new QLabel(tr("—"), accountCard);
    m_accountUri->setObjectName("AccountUri");
    m_accountUri->setStyleSheet("color: #aaaaaa; font-size: 11px;");

    m_regStatus = new QLabel(tr("● Unregistered"), accountCard);
    m_regStatus->setObjectName("RegStatus");
    m_regStatus->setStyleSheet("color: #e05050; font-size: 11px;");

    cardLayout->addWidget(m_accountName);
    cardLayout->addWidget(m_accountUri);
    cardLayout->addWidget(m_regStatus);
    layout->addWidget(accountCard);

    // Profile action buttons row
    auto *profileBtnRow = new QHBoxLayout();
    profileBtnRow->setSpacing(4);

    m_addProfile = new QPushButton(tr("+ Profile"), this);
    m_addProfile->setObjectName("AddProfileBtn");
    m_addProfile->setToolTip(tr("Add SIP profile"));
    m_addProfile->setFixedHeight(24);

    m_editProfile = new QPushButton(tr("Edit"), this);
    m_editProfile->setObjectName("EditProfileBtn");
    m_editProfile->setToolTip(tr("Edit selected profile"));
    m_editProfile->setFixedHeight(24);
    m_editProfile->setEnabled(false);

    m_deleteProfile = new QPushButton(tr("Delete"), this);
    m_deleteProfile->setObjectName("DeleteProfileBtn");
    m_deleteProfile->setToolTip(tr("Delete selected profile"));
    m_deleteProfile->setFixedHeight(24);
    m_deleteProfile->setEnabled(false);

    profileBtnRow->addWidget(m_addProfile);
    profileBtnRow->addWidget(m_editProfile);
    profileBtnRow->addWidget(m_deleteProfile);
    layout->addLayout(profileBtnRow);

    // Search
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search contacts..."));
    m_search->setObjectName("SearchField");
    layout->addWidget(m_search);

    // Contact list
    m_contactList = new QListWidget(this);
    m_contactList->setObjectName("ContactList");
    m_contactList->addItem(tr("Alice — sip:alice@example.com"));
    m_contactList->addItem(tr("Bob — sip:bob@example.com"));
    m_contactList->addItem(tr("Emergency — sip:emergency@psap.example"));
    layout->addWidget(m_contactList, 1);

    // Wire signals
    connect(m_profileSelector, &QComboBox::currentIndexChanged,
            this, &SidebarPanel::onProfileSelectorChanged);
    connect(m_addProfile,    &QPushButton::clicked, this, &SidebarPanel::onAddProfile);
    connect(m_editProfile,   &QPushButton::clicked, this, &SidebarPanel::onEditProfile);
    connect(m_deleteProfile, &QPushButton::clicked, this, &SidebarPanel::onDeleteProfile);

    auto &mgr = SipProfileManager::instance();
    connect(&mgr, &SipProfileManager::profileAdded,
            this, [this](const QString &) { refreshProfileSelector(); });
    connect(&mgr, &SipProfileManager::profileUpdated,
            this, [this](const QString &) { refreshProfileSelector(); });
    connect(&mgr, &SipProfileManager::profileRemoved,
            this, [this](const QString &) { refreshProfileSelector(); });
    connect(&mgr, &SipProfileManager::activeProfileChanged,
            this, &SidebarPanel::onActiveProfileChanged);

    refreshProfileSelector();
}

void SidebarPanel::refreshProfileSelector()
{
    auto &mgr = SipProfileManager::instance();
    const QString currentId = mgr.activeProfileId();

    // Block signals to prevent triggering onProfileSelectorChanged while rebuilding
    const bool blocked = m_profileSelector->blockSignals(true);
    m_profileSelector->clear();
    m_profileSelector->addItem(tr("(no profile)"), QString{});

    int activeIdx = 0;
    const auto profiles = mgr.profiles();
    for (int i = 0; i < profiles.size(); ++i) {
        const SipProfile &p = profiles[i];
        m_profileSelector->addItem(p.displayName, p.profileId);
        if (p.profileId == currentId)
            activeIdx = i + 1; // +1 because "(no profile)" is index 0
    }
    m_profileSelector->setCurrentIndex(activeIdx);
    m_profileSelector->blockSignals(blocked);

    const bool hasProfiles = !profiles.isEmpty();
    const bool hasActive   = mgr.hasActiveProfile();
    m_editProfile->setEnabled(hasActive);
    m_deleteProfile->setEnabled(hasActive);

    updateAccountCard(currentId);
}

void SidebarPanel::updateAccountCard(const QString &profileId)
{
    if (profileId.isEmpty()) {
        m_accountName->setText(tr("No Account"));
        m_accountUri->setText(tr("—"));
        m_regStatus->setText(tr("● Unregistered"));
        return;
    }

    const SipProfile p = SipProfileManager::instance().profile(profileId);
    if (p.isNull()) {
        m_accountName->setText(tr("No Account"));
        m_accountUri->setText(tr("—"));
    } else {
        m_accountName->setText(p.displayName);
        m_accountUri->setText(p.effectiveSipUri());
    }
    m_regStatus->setText(tr("● Unregistered"));
}

void SidebarPanel::onProfileSelectorChanged(int index)
{
    const QString id = m_profileSelector->itemData(index).toString();
    SipProfileManager::instance().setActiveProfileId(id);
}

void SidebarPanel::onActiveProfileChanged(const QString &profileId)
{
    updateAccountCard(profileId);
    m_editProfile->setEnabled(!profileId.isEmpty());
    m_deleteProfile->setEnabled(!profileId.isEmpty());

    // Sync combo selection if changed externally
    const int comboIdx = m_profileSelector->findData(profileId);
    if (comboIdx >= 0 && m_profileSelector->currentIndex() != comboIdx) {
        const bool blocked = m_profileSelector->blockSignals(true);
        m_profileSelector->setCurrentIndex(comboIdx);
        m_profileSelector->blockSignals(blocked);
    }
}

void SidebarPanel::onAddProfile()
{
    SipProfileEditorDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    SipProfile p = dlg.profile();
    const QString id = SipProfileManager::instance().add(p);
    if (id.isEmpty()) {
        Logger::instance().warn(LogCategory::App,
            QStringLiteral("Add profile: validation failed"));
        return;
    }

    if (dlg.passwordChanged() && !dlg.password().isEmpty())
        SipProfileManager::instance().setProfilePassword(id, dlg.password());

    SipProfileManager::instance().setActiveProfileId(id);
}

void SidebarPanel::onEditProfile()
{
    const QString id = SipProfileManager::instance().activeProfileId();
    if (id.isEmpty()) return;

    const SipProfile current = SipProfileManager::instance().profile(id);
    if (current.isNull()) return;

    SipProfileEditorDialog dlg(current, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    SipProfile updated  = dlg.profile();
    updated.createdAt   = current.createdAt; // preserve original creation timestamp
    if (!SipProfileManager::instance().update(updated)) {
        Logger::instance().warn(LogCategory::App,
            QStringLiteral("Edit profile: update failed for %1").arg(id));
        return;
    }

    if (dlg.passwordChanged() && !dlg.password().isEmpty())
        SipProfileManager::instance().setProfilePassword(id, dlg.password());
}

void SidebarPanel::onDeleteProfile()
{
    const QString id = SipProfileManager::instance().activeProfileId();
    if (id.isEmpty()) return;

    const SipProfile p  = SipProfileManager::instance().profile(id);
    const QString name  = p.isNull() ? id : p.displayName;

    const auto btn = QMessageBox::question(
        this, tr("Delete Profile"),
        tr("Delete profile \"%1\"?\n\nThe stored password will also be removed. "
           "This cannot be undone.").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (btn != QMessageBox::Yes)
        return;

    SipProfileManager::instance().remove(id);
}
