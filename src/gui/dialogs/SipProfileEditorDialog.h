#pragma once
#include <QDialog>
#include "sip/SipProfile.h"

class QLineEdit;
class QRadioButton;
class QCheckBox;
class QPushButton;
class QLabel;
class QWidget;

class SipProfileEditorDialog : public QDialog
{
    Q_OBJECT
public:
    // Add mode — empty dialog
    explicit SipProfileEditorDialog(QWidget *parent = nullptr);
    // Edit mode — pre-populated; password field is always empty (never displayed)
    explicit SipProfileEditorDialog(const SipProfile &profile, QWidget *parent = nullptr);

    SipProfile profile() const;
    QString    password() const;
    bool       passwordChanged() const;

private slots:
    void onSipFieldChanged();
    void onSipUriManuallyEdited();
    void onPasswordTextChanged();
    void onTogglePasswordVisibility();
    void onToggleAdvanced();
    void onAccept();

private:
    void buildUi();
    void populateFrom(const SipProfile &profile);

    // General
    QLineEdit *m_displayName{nullptr};
    // SIP
    QLineEdit *m_sipUsername{nullptr};
    QLineEdit *m_sipDomain{nullptr};
    QLineEdit *m_sipUri{nullptr};
    QLineEdit *m_authUsername{nullptr};
    // Network
    QLineEdit *m_registrar{nullptr};
    QLineEdit *m_proxy{nullptr};
    QLineEdit *m_outboundProxy{nullptr};
    // Transport
    QRadioButton *m_udp{nullptr};
    QRadioButton *m_tcp{nullptr};
    QRadioButton *m_tls{nullptr};
    // Extensions
    QCheckBox *m_enableRtt{nullptr};
    QCheckBox *m_enableLmpe{nullptr};
    QCheckBox *m_enableEtsi{nullptr};
    // Security
    QLineEdit   *m_password{nullptr};
    QLineEdit   *m_confirmPassword{nullptr};
    QPushButton *m_showHideBtn{nullptr};
    QLabel      *m_pwStrength{nullptr};
    // Advanced
    QPushButton *m_advancedToggle{nullptr};
    QWidget     *m_advancedContents{nullptr};
    QLineEdit   *m_emergencyUri{nullptr};

    bool    m_editMode{false};
    QString m_profileId;
    bool    m_sipUriManuallyEdited{false};
    bool    m_passwordChanged{false};
};
