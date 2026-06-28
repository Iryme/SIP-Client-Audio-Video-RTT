#include "SidebarPanel.h"

#include "core/Logger.h"
#include "gui/dialogs/SipProfileEditorDialog.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
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
        tr("Select a profile to make it active. Double-click to select and register."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet("color: #9aa8b8; font-size: 11px;");
    layout->addWidget(subtitle);
    m_profileList = new QListWidget(this);
    m_profileList->setObjectName("ProfileList");
    m_profileList->setAlternatingRowColors(true);
    m_profileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileList->setMinimumHeight(120);
    m_profileList->setToolTip(tr("Single-click: set active profile. Double-click: set active + register."));
    layout->addWidget(m_profileList, 1);
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
    m_accountUri->setWordWrap(true);
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
    m_addProfile       = new QPushButton(tr("+ Add"),    this);
    m_editProfile      = new QPushButton(tr("Edit"),     this);
    m_duplicateProfile = new QPushButton(tr("Duplicate"),this);
    m_deleteProfile    = new QPushButton(tr("Delete"),   this);
    m_addProfile->setObjectName("AddProfileBtn");
    m_editProfile->setObjectName("EditProfileBtn");
    m_duplicateProfile->setObjectName("DuplicateProfileBtn");
    m_deleteProfile->setObjectName("DeleteProfileBtn");
    m_addProfile->setToolTip(tr("Add new SIP profile"));
    m_editProfile->setToolTip(tr("Edit selected profile"));
    m_duplicateProfile->setToolTip(tr("Duplicate selected profile"));
    m_deleteProfile->setToolTip(tr("Delete selected profile"));
    m_addProfile->setFixedHeight(24);
    m_editProfile->setFixedHeight(24);
    m_duplicateProfile->setFixedHeight(24);
    m_deleteProfile->setFixedHeight(24);
    m_editProfile->setEnabled(false);
    m_duplicateProfile->setEnabled(false);
    m_deleteProfile->setEnabled(false);
    profileBtnRow->addWidget(m_addProfile);
    profileBtnRow->addWidget(m_editProfile);
    profileBtnRow->addWidget(m_duplicateProfile);
    profileBtnRow->addWidget(m_deleteProfile);
    layout->addLayout(profileBtnRow);

    connect(m_profileList, &QListWidget::itemClicked,
            this, &SidebarPanel::onProfileListItemClicked);
    connect(m_profileList, &QListWidget::itemDoubleClicked,
            this, &SidebarPanel::onProfileListDoubleClicked);
    connect(m_addProfile,       &QPushButton::clicked, this, &SidebarPanel::onAddProfile);
    connect(m_editProfile,      &QPushButton::clicked, this, &SidebarPanel::onEditProfile);
    connect(m_duplicateProfile, &QPushButton::clicked, this, &SidebarPanel::onDuplicateProfile);
    connect(m_deleteProfile,    &QPushButton::clicked, this, &SidebarPanel::onDeleteProfile);
    connect(m_registrationButton, &QPushButton::clicked,
            this, &SidebarPanel::onRegistrationClicked);

    auto &profiles = SipProfileManager::instance();
    connect(&profiles, &SipProfileManager::profileAdded,
            this, [this](const QString &) { refreshProfileList(); });
    connect(&profiles, &SipProfileManager::profileUpdated,
            this, [this](const QString &) { refreshProfileList(); });
    connect(&profiles, &SipProfileManager::profileRemoved,
            this, [this](const QString &) { refreshProfileList(); });
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
    refreshProfileList();
}

// ---------------------------------------------------------------------------

void SidebarPanel::refreshProfileList()
{
    if (m_refreshing)
        return;
    m_refreshing = true;
    auto &mgr = SipProfileManager::instance();
    const QString activeId     = mgr.activeProfileId();
    const QString registeredId = SipManager::instance().registeredProfileId();
    const RegistrationState regState = SipManager::instance().registrationState();

    QSignalBlocker blocker(m_profileList);
    m_profileList->clear();

    for (const SipProfile &p : mgr.profiles()) {
        const bool isActive     = (p.profileId == activeId);
        const bool isRegistered = (p.profileId == registeredId);

        QString statusStr;
        if (isRegistered) {
            switch (regState) {
            case RegistrationState::Registered:        statusStr = tr("Registered");    break;
            case RegistrationState::Registering:       statusStr = tr("Registeringâ€¦");  break;
            case RegistrationState::Unregistering:     statusStr = tr("Unregisteringâ€¦");break;
            case RegistrationState::RegistrationFailed:statusStr = tr("Failed");        break;
            default:                                   statusStr = tr("Unregistered");  break;
            }
        } else {
            statusStr = tr("Unregistered");
        }

        const QString uri = p.effectiveSipUri();
        const QString text = QStringLiteral("%1\n%2\n%3")
            .arg(p.displayName,
                 uri.isEmpty() ? QStringLiteral("(no URI)") : uri,
                 (isActive ? QStringLiteral("â˜… ") : QStringLiteral("  ")) + statusStr);

        auto *item = new QListWidgetItem(text, m_profileList);
        item->setData(Qt::UserRole, p.profileId);
        item->setToolTip(QStringLiteral("%1\n%2").arg(p.displayName, uri));

        // Bold = active profile
        if (isActive) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            m_profileList->setCurrentItem(item);
        }

        // Color-code by registration status
        if (isRegistered && regState == RegistrationState::Registered)
            item->setForeground(QColor(QStringLiteral("#50c878")));
        else if (isRegistered && regState == RegistrationState::RegistrationFailed)
            item->setForeground(QColor(QStringLiteral("#e05050")));
        else if (isRegistered)
            item->setForeground(QColor(QStringLiteral("#e0b850")));
    }

    const bool hasActive = !activeId.isEmpty();
    m_editProfile->setEnabled(hasActive);
    m_duplicateProfile->setEnabled(hasActive);
    m_deleteProfile->setEnabled(hasActive);
    updateAccountCard(activeId);
    m_refreshing = false;
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

// ---------------------------------------------------------------------------
// Profile list slots
// ---------------------------------------------------------------------------

void SidebarPanel::onProfileListItemClicked(QListWidgetItem *item)
{
    if (!item)
        return;
    const QString newId    = item->data(Qt::UserRole).toString();
    const QString currentId = SipProfileManager::instance().activeProfileId();
    if (newId == currentId)
        return;

    if (!SipManager::instance().switchActiveProfile(newId)) {
        // Switch rejected (one already pending) â€” revert list display
        refreshProfileList();
    }
}

void SidebarPanel::onProfileListDoubleClicked(QListWidgetItem *item)
{
    if (!item)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    if (id == SipProfileManager::instance().activeProfileId()) {
        // Profile is already active â€” just trigger register/unregister
        onRegistrationClicked();
        return;
    }
    // Profile switch was already triggered by the preceding itemClicked signal.
    // Flag to register once the switch completes.
    m_registerAfterSwitch = true;
}

// ---------------------------------------------------------------------------
// Profile management slots
// ---------------------------------------------------------------------------

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

void SidebarPanel::onDuplicateProfile()
{
    const QString id = SipProfileManager::instance().activeProfileId();
    const SipProfile current = SipProfileManager::instance().profile(id);
    if (current.isNull())
        return;

    // Pre-populate editor with a copy of the active profile; clear identity fields.
    SipProfile copy = current;
    copy.profileId   = {};
    copy.displayName = copy.displayName + tr(" (copy)");
    copy.createdAt   = {};
    copy.updatedAt   = {};

    SipProfileEditorDialog dialog(copy, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString newId = SipProfileManager::instance().add(dialog.profile());
    if (newId.isEmpty()) {
        Logger::instance().warn(LogCategory::App,
            QStringLiteral("Duplicate profile: validation failed"));
        return;
    }
    if (dialog.passwordChanged() && !dialog.password().isEmpty())
        SipProfileManager::instance().setProfilePassword(newId, dialog.password());
    SipProfileManager::instance().setActiveProfileId(newId);
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

// ---------------------------------------------------------------------------
// Active profile changed
// ---------------------------------------------------------------------------

void SidebarPanel::onActiveProfileChanged(const QString &profileId)
{
    updateAccountCard(profileId);
    const bool hasActive = !profileId.isEmpty();
    m_editProfile->setEnabled(hasActive);
    m_duplicateProfile->setEnabled(hasActive);
    m_deleteProfile->setEnabled(hasActive);
    refreshProfileList();
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

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
}

void SidebarPanel::onRegistrationStateChanged(RegistrationState state,
                                              const QString &statusText,
                                              int statusCode)
{
    Q_UNUSED(statusCode)
    const QString activeId     = SipProfileManager::instance().activeProfileId();
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

    const bool canRegister = !activeId.isEmpty()
        && (state == RegistrationState::Unregistered
            || state == RegistrationState::Registered
            || state == RegistrationState::RegistrationFailed);
    m_registrationButton->setEnabled(canRegister);

    const bool hasActive = !activeId.isEmpty();
    m_editProfile->setEnabled(hasActive && !transient);
    m_duplicateProfile->setEnabled(hasActive && !transient);
    m_deleteProfile->setEnabled(hasActive && !transient);

    // Refresh list to update per-profile status colors
    refreshProfileList();
}

// ---------------------------------------------------------------------------
// Profile switch
// ---------------------------------------------------------------------------

void SidebarPanel::onProfileSwitchStarted(const QString &newProfileId)
{
    Q_UNUSED(newProfileId)
    m_profileList->setEnabled(false);
    m_addProfile->setEnabled(false);
    m_editProfile->setEnabled(false);
    m_duplicateProfile->setEnabled(false);
    m_deleteProfile->setEnabled(false);
    m_registrationButton->setEnabled(false);
    m_regStatus->setText(tr("Switching SIP profile..."));
    m_regStatus->setStyleSheet(QStringLiteral("color: #e0b850; font-size: 11px;"));
}

void SidebarPanel::onProfileSwitchCompleted(const QString &newProfileId)
{
    Q_UNUSED(newProfileId)
    m_profileList->setEnabled(true);
    m_addProfile->setEnabled(true);
    // Edit/delete/register button states are updated by the subsequent registrationStateChanged.

    if (m_registerAfterSwitch) {
        m_registerAfterSwitch = false;
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Double-click triggered register for switched profile"));
        SipManager::instance().registerActiveProfile();
    }
}

void SidebarPanel::onProfileSwitchFailed(const QString &newProfileId, const QString &reason)
{
    Q_UNUSED(newProfileId)
    Q_UNUSED(reason)
    m_registerAfterSwitch = false;
    m_profileList->setEnabled(true);
    m_addProfile->setEnabled(true);
    const QString activeId = SipProfileManager::instance().activeProfileId();
    const bool hasActive = !activeId.isEmpty();
    m_editProfile->setEnabled(hasActive);
    m_duplicateProfile->setEnabled(hasActive);
    m_deleteProfile->setEnabled(hasActive);
    m_registrationButton->setEnabled(hasActive);
    refreshProfileList();
}

