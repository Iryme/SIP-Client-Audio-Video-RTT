#pragma once
#include <QWidget>
#include "sip/SipAccount.h"

class QLabel;
class QPushButton;
class QListWidget;
class QListWidgetItem;

class SidebarPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SidebarPanel(QWidget *parent = nullptr);

private slots:
    void onProfileListItemClicked(QListWidgetItem *item);
    void onProfileListDoubleClicked(QListWidgetItem *item);
    void onAddProfile();
    void onEditProfile();
    void onDuplicateProfile();
    void onDeleteProfile();
    void onActiveProfileChanged(const QString &profileId);
    void onRegistrationClicked();
    void onRegistrationStateChanged(RegistrationState state,
                                    const QString &statusText,
                                    int statusCode);
    void onProfileSwitchStarted(const QString &newProfileId);
    void onProfileSwitchCompleted(const QString &newProfileId);
    void onProfileSwitchFailed(const QString &newProfileId, const QString &reason);

private:
    void refreshProfileList();
    void updateAccountCard(const QString &profileId);

    QListWidget  *m_profileList{nullptr};
    QLabel       *m_accountName{nullptr};
    QLabel       *m_accountUri{nullptr};
    QLabel       *m_regStatus{nullptr};
    QPushButton  *m_addProfile{nullptr};
    QPushButton  *m_editProfile{nullptr};
    QPushButton  *m_duplicateProfile{nullptr};
    QPushButton  *m_deleteProfile{nullptr};
    QPushButton  *m_registrationButton{nullptr};

    bool m_registerAfterSwitch{false};
    bool m_refreshing{false};
};
