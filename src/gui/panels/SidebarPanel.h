#pragma once
#include <QWidget>
#include "sip/SipAccount.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QComboBox;

class SidebarPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SidebarPanel(QWidget *parent = nullptr);

private slots:
    void onProfileSelectorChanged(int index);
    void onAddProfile();
    void onEditProfile();
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
    void refreshProfileSelector();
    void updateAccountCard(const QString &profileId);

    QComboBox    *m_profileSelector{nullptr};
    QLabel       *m_accountName{nullptr};
    QLabel       *m_accountUri{nullptr};
    QLabel       *m_regStatus{nullptr};
    QLineEdit    *m_search{nullptr};
    QListWidget  *m_contactList{nullptr};
    QPushButton  *m_addProfile{nullptr};
    QPushButton  *m_editProfile{nullptr};
    QPushButton  *m_deleteProfile{nullptr};
    QPushButton  *m_registrationButton{nullptr};
};
