#include "SidebarPanel.h"

#include "core/Logger.h"
#include "gui/dialogs/SipProfileEditorDialog.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

SidebarPanel::SidebarPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SidebarPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(tr("SIP Profiles / Accounts"), this);
    title->setStyleSheet("font-weight: bold; font-size: 13px;");
    layout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Select the active profile, then register or switch accounts without blocking the UI."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet("color: #9aa8b8; font-size: 11px;");
    layout->addWidget(subtitle);

    m_profileSelector = new QComboBox(this);
    m_profileSelector->setObjectName("ProfileSelector");
    m_profileSelector->setToolTip(tr("Select active SIP profile"));
    layout->addWidget(m_profileSelector);

    auto *accountCard = new QFrame(this);
    accountCard->setObjectName("AccountCard");
    accountCard->setFrameShape(QFrame::StyledPanel);
    auto *cardLayout = new QVBoxLayout(accountCard);
    cardLayout->setContentsMargins(8, 8, 8, 8);
    cardLayout->setSpacing(2);

    m_accountName = new QLabel(tr("No Account"), accountCard);
    m_accountName->setObjectName("AccountName");
    m_accountName->setStyleSheet("font-weight: bold; font-size: 13px;");
    m_accountUri = new QLabel(QStringLiteral("-"), accountCard);
    m_accountUri->setObjectName("AccountUri");
    m_accountUri->setStyleSheet("color: #aaaaaa; font-size: 11px;");
    m_regStatus = new QLabel(tr("Unregistered"), accountCard);
    m_regStatus->setObjectName("RegStatus");

    cardLayout->addWidget(m_accountName);
    cardLayout->addWidget(m_accountUri);
    cardLayout->addWidget(m_regStatus);
    layout->addWidget(accountCard);

    m_registrationButton = new QPushButton(tr("Register"), this);
    m_registrationButton->setObjectName("RegistrationBtn");
    m_registrationButton->setEnabled(false);
    layout->addWidget(m_registrationButton);

    auto *profileBtnRow = new QHBoxLayout();
    profileBtnRow->setSpacing(4);
    m_addProfile = new QPushButton(tr("+ Profile"), this);
    m_editProfile = new QPushButton(tr("Edit"), this);
    m_deleteProfile = new QPushButton(tr("Delete"), this);
    m_addProfile->setObjectName("AddProfileBtn");
    m_editProfile->setObjectName("EditProfileBtn");
    m_deleteProfile->setObjectName("DeleteProfileBtn");
    m_addProfile->setToolTip(tr("Add SIP profile"));
    m_editProfile->setToolTip(tr("Edit selected profile"));
    m_deleteProfile->setToolTip(tr("Delete selected profile"));
    m_addProfile->setFixedHeight(24);
    m_editProfile->setFixedHeight(24);
    m_deleteProfile->setFixedHeight(24);
    m_editProfile->setEnabled(false);
    m_deleteProfile->setEnabled(false);
    profileBtnRow->addWidget(m_addProfile);
    profileBtnRow->addWidget(m_editProfile);
    profileBtnRow->addWidget(m_deleteProfile);
    layout->addLayout(profileBtnRow);

    connect(m_profileSelector, &QComboBox::currentIndexChanged,
            this, &SidebarPanel::onProfileSelectorChanged);
    connect(m_addProfile, &QPushButton::clicked, this, &SidebarPanel::onAddProfile);
    connect(m_editProfile, &QPushButton::clicked, this, &SidebarPanel::onEditProfile);
    connect(m_deleteProfile, &QPushButton::clicked, this, &SidebarPanel::onDeleteProfile);
    connect(m_registrationButton, &QPushButton::clicked,
            this, &SidebarPanel::onRegistrationClicked);

    auto &profiles = SipProfileManager::instance();
    connect(&profiles, &SipProfileManager::profileAdded,
            this, [this](const QString &) { refreshProfileSelector(); });
    connect(&profiles, &SipProfileManager::profileUpdated,
            this, [this](const QString &) { refreshProfileSelector(); });
    connect(&profiles, &SipProfileManager::profileRemoved,
            this, [this](const QString &) { refreshProfileSelector(); });
    connect(&profiles, &SipProfileManager::activeProfileChanged,
            this, &SidebarPanel::onActiveProfileChanged);
    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            this, &SidebarPanel::onRegistrationStateChanged);
    connect(&SipManager::instance(), &SipManager::profileSwitchStarted,
            this, &SidebarPanel::onProfileSwitchStarted);
    connect(&SipManager::instance(), &SipManager::profileSwitchCompleted,
            this, &SidebarPanel::onProfileSwitchCompleted);
    connect(&SipManager::instance(), &SipManager::profileSwitchFailed,
            this, &SidebarPanel::onProfileSwitchFailed);

    refreshProfileSelector();
}

void SidebarPanel::refreshProfileSelector()
{
    auto &mgr = SipProfileManager::instance();
    const QString currentId = mgr.activeProfileId();
    const bool blocked = m_profileSelector->blockSignals(true);
    m_profileSelector->clear();
    m_profileSelector->addItem(tr("(no profile)"), QString{});

    int activeIndex = 0;
    const auto profiles = mgr.profiles();
    for (int i = 0; i < profiles.size(); ++i) {
        m_profileSelector->addItem(profiles[i].displayName, profiles[i].profileId);
        if (profiles[i].profileId == currentId)
            activeIndex = i + 1;
    }
    m_profileSelector->setCurrentIndex(activeIndex);
    m_profileSelector->blockSignals(blocked);

    const bool hasActive = mgr.hasActiveProfile();
    m_editProfile->setEnabled(hasActive);
    m_deleteProfile->setEnabled(hasActive);
    updateAccountCard(currentId);
}

void SidebarPanel::updateAccountCard(const QString &profileId)
{
    const SipProfile profile = SipProfileManager::instance().profile(profileId);
    if (profileId.isEmpty() || profile.isNull()) {
        m_accountName->setText(tr("No Account"));
        m_accountUri->setText(QStringLiteral("-"));
        m_regStatus->setText(tr("Unregistered"));
        m_regStatus->setStyleSheet("color: #e05050; font-size: 11px;");
        m_registrationButton->setText(tr("Register"));
        m_registrationButton->setEnabled(false);
        return;
    }

    m_accountName->setText(profile.displayName);
    m_accountUri->setText(profile.effectiveSipUri());

    auto &sip = SipManager::instance();
    if (sip.registeredProfileId().isEmpty() || sip.registeredProfileId() == profileId) {
        onRegistrationStateChanged(sip.registrationState(),
                                   sip.registrationStatusText(),
                                   sip.registrationStatusCode());
    } else {
        m_regStatus->setText(tr("Unregistered"));
        m_regStatus->setStyleSheet("color: #e05050; font-size: 11px;");
        m_registrationButton->setText(tr("Register"));
        m_registrationButton->setEnabled(true);
    }
}

void SidebarPanel::onProfileSelectorChanged(int index)
{
    const QString newId = m_profileSelector->itemData(index).toString();
    if (!SipManager::instance().switchActiveProfile(newId)) {
        // Switch rejected (one already pending) — revert combo to current active profile.
        const QString current = SipProfileManager::instance().activeProfileId();
        const int revertIndex = m_profileSelector->findData(current);
        if (revertIndex >= 0 && m_profileSelector->currentIndex() != revertIndex) {
            const bool blocked = m_profileSelector->blockSignals(true);
            m_profileSelector->setCurrentIndex(revertIndex);
            m_profileSelector->blockSignals(blocked);
        }
    }
}

void SidebarPanel::onActiveProfileChanged(const QString &profileId)
{
    updateAccountCard(profileId);
    const bool hasActive = !profileId.isEmpty();
    m_editProfile->setEnabled(hasActive);
    m_deleteProfile->setEnabled(hasActive);

    const int comboIndex = m_profileSelector->findData(profileId);
    if (comboIndex >= 0 && m_profileSelector->currentIndex() != comboIndex) {
        const bool blocked = m_profileSelector->blockSignals(true);
        m_profileSelector->setCurrentIndex(comboIndex);
        m_profileSelector->blockSignals(blocked);
    }
}

void SidebarPanel::onAddProfile()
{
    SipProfileEditorDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString id = SipProfileManager::instance().add(dialog.profile());
    if (id.isEmpty()) {
        Logger::instance().warn(LogCategory::App,
                                QStringLiteral("Add profile: validation failed"));
        return;
    }
    if (dialog.passwordChanged() && !dialog.password().isEmpty())
        SipProfileManager::instance().setProfilePassword(id, dialog.password());
    SipProfileManager::instance().setActiveProfileId(id);
}

void SidebarPanel::onEditProfile()
{
    const QString id = SipProfileManager::instance().activeProfileId();
    const SipProfile current = SipProfileManager::instance().profile(id);
    if (current.isNull())
        return;

    SipProfileEditorDialog dialog(current, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    SipProfile updated = dialog.profile();
    updated.createdAt = current.createdAt;
    if (!SipProfileManager::instance().update(updated)) {
        Logger::instance().warn(LogCategory::App,
            QStringLiteral("Edit profile: update failed for %1").arg(id));
        return;
    }
    if (dialog.passwordChanged() && !dialog.password().isEmpty())
        SipProfileManager::instance().setProfilePassword(id, dialog.password());
}

void SidebarPanel::onDeleteProfile()
{
    const QString id = SipProfileManager::instance().activeProfileId();
    if (id.isEmpty())
        return;

    const SipProfile profile = SipProfileManager::instance().profile(id);
    const QString name = profile.isNull() ? id : profile.displayName;
    const auto answer = QMessageBox::question(
        this, tr("Delete Profile"),
        tr("Delete profile \"%1\"?\n\nThe stored password will also be removed. "
           "This cannot be undone.").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        SipProfileManager::instance().remove(id);
}

void SidebarPanel::onRegistrationClicked()
{
    auto &sip = SipManager::instance();
    const RegistrationState state = sip.registrationState();
    if (state == RegistrationState::Registered) {
        sip.unregisterActiveProfile();
    } else if (state == RegistrationState::Unregistered
               || state == RegistrationState::RegistrationFailed) {
        sip.registerActiveProfile();
    }
    // Registering and Unregistering: button should already be disabled by
    // onRegistrationStateChanged; ignore stray clicks defensively.
}

void SidebarPanel::onProfileSwitchStarted(const QString &newProfileId)
{
    Q_UNUSED(newProfileId)
    m_profileSelector->setEnabled(false);
    m_addProfile->setEnabled(false);
    m_editProfile->setEnabled(false);
    m_deleteProfile->setEnabled(false);
    m_registrationButton->setEnabled(false);
    m_regStatus->setText(tr("Switching SIP profile..."));
    m_regStatus->setStyleSheet(QStringLiteral("color: #e0b850; font-size: 11px;"));
}

void SidebarPanel::onProfileSwitchCompleted(const QString &newProfileId)
{
    Q_UNUSED(newProfileId)
    m_profileSelector->setEnabled(true);
    m_addProfile->setEnabled(true);
    // Edit/delete/register button states are updated by the subsequent registrationStateChanged signal.
}

void SidebarPanel::onProfileSwitchFailed(const QString &newProfileId, const QString &reason)
{
    Q_UNUSED(newProfileId)
    Q_UNUSED(reason)
    m_profileSelector->setEnabled(true);
    m_addProfile->setEnabled(true);
    const QString activeId = SipProfileManager::instance().activeProfileId();
    const bool hasActive = !activeId.isEmpty();
    m_editProfile->setEnabled(hasActive);
    m_deleteProfile->setEnabled(hasActive);
    m_registrationButton->setEnabled(hasActive);
}

void SidebarPanel::onRegistrationStateChanged(RegistrationState state,
                                              const QString &statusText,
                                              int statusCode)
{
    Q_UNUSED(statusCode)
    const QString activeId = SipProfileManager::instance().activeProfileId();
    const QString registeredId = SipManager::instance().registeredProfileId();
    if (!registeredId.isEmpty() && registeredId != activeId)
        return;

    const bool transient = (state == RegistrationState::Registering
                            || state == RegistrationState::Unregistering);

    QString color = QStringLiteral("#e05050");
    switch (state) {
    case RegistrationState::Unregistered:
        m_regStatus->setText(tr("Unregistered"));
        m_registrationButton->setText(tr("Register"));
        break;
    case RegistrationState::Registering:
        color = QStringLiteral("#e0b850");
        m_regStatus->setText(tr("Registering"));
        break;
    case RegistrationState::Registered:
        color = QStringLiteral("#50c878");
        m_regStatus->setText(tr("Registered"));
        m_registrationButton->setText(tr("Unregister"));
        break;
    case RegistrationState::Unregistering:
        color = QStringLiteral("#e0b850");
        m_regStatus->setText(tr("Unregistering"));
        break;
    case RegistrationState::RegistrationFailed:
        m_regStatus->setText(tr("Registration failed"));
        m_registrationButton->setText(tr("Retry registration"));
        break;
    }

    m_regStatus->setToolTip(statusText);
    m_regStatus->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px;").arg(color));

    // Register button: enabled only when action can be taken.
    const bool canRegister = !activeId.isEmpty()
        && (state == RegistrationState::Unregistered
            || state == RegistrationState::Registered
            || state == RegistrationState::RegistrationFailed);
    m_registrationButton->setEnabled(canRegister);

    // Profile editing must not be available while a transient operation is in progress.
    const bool hasActive = !activeId.isEmpty();
    m_editProfile->setEnabled(hasActive && !transient);
    m_deleteProfile->setEnabled(hasActive && !transient);
}
