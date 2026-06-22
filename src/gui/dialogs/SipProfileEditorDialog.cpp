#include "gui/dialogs/SipProfileEditorDialog.h"
#include "sip/SipProfileManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFrame>

// ---- construction -------------------------------------------------------

SipProfileEditorDialog::SipProfileEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add SIP Profile"));
    buildUi();
}

SipProfileEditorDialog::SipProfileEditorDialog(const SipProfile &profile, QWidget *parent)
    : QDialog(parent)
    , m_editMode(true)
    , m_profileId(profile.profileId)
{
    setWindowTitle(tr("Edit SIP Profile"));
    buildUi();
    populateFrom(profile);
}

// ---- public API ---------------------------------------------------------

SipProfile SipProfileEditorDialog::profile() const
{
    SipProfile p;
    if (m_editMode) {
        p.profileId = m_profileId;
    } else {
        p = SipProfile::createNew();
    }

    p.displayName    = m_displayName->text().trimmed();
    p.sipUsername    = m_sipUsername->text().trimmed();
    p.sipDomain      = m_sipDomain->text().trimmed();
    p.sipUri         = m_sipUri->text().trimmed();
    p.authUsername   = m_authUsername->text().trimmed();
    p.registrar      = m_registrar->text().trimmed();
    p.proxy          = m_proxy->text().trimmed();
    p.outboundProxy  = m_outboundProxy->text().trimmed();
    p.emergencyServiceUri = m_emergencyUri->text().trimmed();
    p.enableRtt              = m_enableRtt->isChecked();
    p.enableLmpe             = m_enableLmpe->isChecked();
    p.enableEtsiCompatibility = m_enableEtsi->isChecked();

    if (m_udp->isChecked())       p.transport = SipTransport::UDP;
    else if (m_tcp->isChecked())  p.transport = SipTransport::TCP;
    else                          p.transport = SipTransport::TLS;

    return p;
}

QString SipProfileEditorDialog::password() const
{
    return m_password->text();
}

bool SipProfileEditorDialog::passwordChanged() const
{
    return m_passwordChanged;
}

// ---- private slots ------------------------------------------------------

void SipProfileEditorDialog::onSipFieldChanged()
{
    if (!m_sipUriManuallyEdited) {
        const QString user   = m_sipUsername->text().trimmed();
        const QString domain = m_sipDomain->text().trimmed();
        if (!user.isEmpty() && !domain.isEmpty())
            m_sipUri->setText(QStringLiteral("sip:") + user + QChar('@') + domain);
        else
            m_sipUri->clear();
    }
}

void SipProfileEditorDialog::onSipUriManuallyEdited()
{
    m_sipUriManuallyEdited = true;
}

void SipProfileEditorDialog::onPasswordTextChanged()
{
    m_passwordChanged = !m_password->text().isEmpty();

    const int len = m_password->text().length();
    if (len == 0)
        m_pwStrength->setText(tr("—"));
    else if (len < 8)
        m_pwStrength->setText(QStringLiteral("<span style='color:#e05050'>Weak</span>"));
    else if (len < 12)
        m_pwStrength->setText(QStringLiteral("<span style='color:#e0b040'>Fair</span>"));
    else
        m_pwStrength->setText(QStringLiteral("<span style='color:#50c050'>Strong</span>"));
}

void SipProfileEditorDialog::onTogglePasswordVisibility()
{
    const bool hidden = (m_password->echoMode() == QLineEdit::Password);
    m_password->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
    m_confirmPassword->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
    m_showHideBtn->setText(hidden ? tr("Hide") : tr("Show"));
}

void SipProfileEditorDialog::onToggleAdvanced()
{
    const bool visible = !m_advancedContents->isVisible();
    m_advancedContents->setVisible(visible);
    m_advancedToggle->setText(visible ? tr("▼ Advanced") : tr("▶ Advanced"));
    adjustSize();
}

void SipProfileEditorDialog::onAccept()
{
    QStringList errors;
    if (m_displayName->text().trimmed().isEmpty())
        errors << tr("Display Name is required");
    if (m_sipUsername->text().trimmed().isEmpty())
        errors << tr("SIP Username is required");
    if (m_sipDomain->text().trimmed().isEmpty())
        errors << tr("SIP Domain is required");
    if (m_registrar->text().trimmed().isEmpty())
        errors << tr("Registrar is required");
    if (m_passwordChanged && m_password->text() != m_confirmPassword->text())
        errors << tr("Passwords do not match");

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), errors.join(QChar('\n')));
        return;
    }

    accept();
}

// ---- UI construction ----------------------------------------------------

static QFormLayout *attachForm(QGroupBox *box)
{
    auto *form = new QFormLayout(box);
    form->setContentsMargins(8, 4, 8, 8);
    form->setSpacing(6);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return form;
}

static QLineEdit *addField(QFormLayout *form, const QString &label,
                            const QString &placeholder = {})
{
    auto *edit = new QLineEdit;
    if (!placeholder.isEmpty())
        edit->setPlaceholderText(placeholder);
    form->addRow(label, edit);
    return edit;
}

void SipProfileEditorDialog::buildUi()
{
    setMinimumWidth(480);
    resize(500, 640);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 8);
    outerLayout->setSpacing(0);

    // Scrollable field area
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget;
    auto *layout    = new QVBoxLayout(container);
    layout->setContentsMargins(12, 12, 12, 4);
    layout->setSpacing(10);

    // --- General ---
    auto *genGroup = new QGroupBox(tr("General"), container);
    auto *genForm  = attachForm(genGroup);
    m_displayName = addField(genForm, tr("Display Name:"), tr("e.g. Alice Smith"));
    layout->addWidget(genGroup);

    // --- SIP ---
    auto *sipGroup = new QGroupBox(tr("SIP"), container);
    auto *sipForm  = attachForm(sipGroup);
    m_sipUsername  = addField(sipForm, tr("SIP Username:"),  tr("e.g. alice"));
    m_sipDomain    = addField(sipForm, tr("SIP Domain:"),    tr("e.g. example.com"));
    m_sipUri       = addField(sipForm, tr("SIP URI:"),       tr("Auto-generated"));
    m_authUsername = addField(sipForm, tr("Auth Username:"), tr("Leave blank to use SIP username"));
    layout->addWidget(sipGroup);

    // --- Network ---
    auto *netGroup   = new QGroupBox(tr("Network"), container);
    auto *netForm    = attachForm(netGroup);
    m_registrar      = addField(netForm, tr("Registrar:"),       tr("e.g. registrar.example.com"));
    m_proxy          = addField(netForm, tr("Proxy:"),           tr("Optional"));
    m_outboundProxy  = addField(netForm, tr("Outbound Proxy:"),  tr("Optional"));
    layout->addWidget(netGroup);

    // --- Transport ---
    auto *transGroup  = new QGroupBox(tr("Transport"), container);
    auto *transLayout = new QHBoxLayout(transGroup);
    transLayout->setContentsMargins(8, 4, 8, 8);
    transLayout->setSpacing(16);
    m_udp = new QRadioButton(tr("UDP"), transGroup);
    m_tcp = new QRadioButton(tr("TCP"), transGroup);
    m_tls = new QRadioButton(tr("TLS"), transGroup);
    m_udp->setChecked(true);
    transLayout->addWidget(m_udp);
    transLayout->addWidget(m_tcp);
    transLayout->addWidget(m_tls);
    transLayout->addStretch();
    layout->addWidget(transGroup);

    // --- Extensions ---
    auto *extGroup  = new QGroupBox(tr("Extensions"), container);
    auto *extLayout = new QVBoxLayout(extGroup);
    extLayout->setContentsMargins(8, 4, 8, 8);
    extLayout->setSpacing(4);
    m_enableRtt  = new QCheckBox(tr("Enable RTT (Real-Time Text)"), extGroup);
    m_enableLmpe = new QCheckBox(tr("Enable LMPE"), extGroup);
    m_enableEtsi = new QCheckBox(tr("Enable ETSI Compatibility"), extGroup);
    extLayout->addWidget(m_enableRtt);
    extLayout->addWidget(m_enableLmpe);
    extLayout->addWidget(m_enableEtsi);
    layout->addWidget(extGroup);

    // --- Security ---
    auto *secGroup = new QGroupBox(tr("Security"), container);
    auto *secForm  = new QFormLayout(secGroup);
    secForm->setContentsMargins(8, 4, 8, 8);
    secForm->setSpacing(6);
    secForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_password = new QLineEdit(secGroup);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(
        m_editMode ? tr("Leave blank to keep existing password") : tr("Enter password"));
    secForm->addRow(tr("Password:"), m_password);

    m_confirmPassword = new QLineEdit(secGroup);
    m_confirmPassword->setEchoMode(QLineEdit::Password);
    m_confirmPassword->setPlaceholderText(tr("Confirm password"));
    secForm->addRow(tr("Confirm:"), m_confirmPassword);

    auto *pwCtrlRow = new QHBoxLayout;
    m_showHideBtn = new QPushButton(tr("Show"), secGroup);
    m_showHideBtn->setFixedWidth(60);
    auto *strengthLabel = new QLabel(tr("Strength:"), secGroup);
    m_pwStrength = new QLabel(tr("\xe2\x80\x94"), secGroup);  // em dash
    m_pwStrength->setTextFormat(Qt::RichText);
    pwCtrlRow->addWidget(m_showHideBtn);
    pwCtrlRow->addSpacing(8);
    pwCtrlRow->addWidget(strengthLabel);
    pwCtrlRow->addWidget(m_pwStrength);
    pwCtrlRow->addStretch();
    secForm->addRow(QString{}, pwCtrlRow);

    layout->addWidget(secGroup);

    // --- Advanced toggle ---
    m_advancedToggle = new QPushButton(tr("▶ Advanced"), container);
    m_advancedToggle->setFlat(true);
    m_advancedToggle->setStyleSheet(
        QStringLiteral("text-align: left; font-weight: bold; padding: 2px 0;"));
    layout->addWidget(m_advancedToggle);

    // Advanced section (hidden by default)
    m_advancedContents = new QWidget(container);
    auto *advLayout = new QVBoxLayout(m_advancedContents);
    advLayout->setContentsMargins(0, 0, 0, 0);
    advLayout->setSpacing(10);

    auto *advGroup = new QGroupBox(tr("Advanced"), m_advancedContents);
    auto *advForm  = attachForm(advGroup);
    m_emergencyUri = addField(advForm, tr("Emergency URI:"), tr("e.g. sip:112@psap.example"));

    auto *futureGroup = new QGroupBox(tr("Custom SIP Headers (future)"), m_advancedContents);
    auto *futureLayout = new QVBoxLayout(futureGroup);
    futureLayout->setContentsMargins(8, 4, 8, 8);
    futureLayout->addWidget(new QLabel(tr("Custom header configuration not yet implemented."),
                                       futureGroup));

    advLayout->addWidget(advGroup);
    advLayout->addWidget(futureGroup);

    m_advancedContents->setVisible(false);
    layout->addWidget(m_advancedContents);
    layout->addStretch();

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea, 1);

    // Dialog button box
    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outerLayout->addWidget(btnBox);

    // Wire signals
    connect(m_sipUsername, &QLineEdit::textChanged,
            this, &SipProfileEditorDialog::onSipFieldChanged);
    connect(m_sipDomain, &QLineEdit::textChanged,
            this, &SipProfileEditorDialog::onSipFieldChanged);
    connect(m_sipUri, &QLineEdit::textEdited,
            this, &SipProfileEditorDialog::onSipUriManuallyEdited);
    connect(m_password, &QLineEdit::textChanged,
            this, &SipProfileEditorDialog::onPasswordTextChanged);
    connect(m_showHideBtn, &QPushButton::clicked,
            this, &SipProfileEditorDialog::onTogglePasswordVisibility);
    connect(m_advancedToggle, &QPushButton::clicked,
            this, &SipProfileEditorDialog::onToggleAdvanced);
    connect(btnBox, &QDialogButtonBox::accepted,
            this, &SipProfileEditorDialog::onAccept);
    connect(btnBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

void SipProfileEditorDialog::populateFrom(const SipProfile &profile)
{
    m_displayName->setText(profile.displayName);
    m_sipUsername->setText(profile.sipUsername);
    m_sipDomain->setText(profile.sipDomain);

    const QString derivedUri = profile.effectiveSipUri();
    const QString storedUri  = profile.sipUri;
    if (!storedUri.isEmpty() && storedUri != derivedUri)
        m_sipUriManuallyEdited = true;
    m_sipUri->setText(storedUri.isEmpty() ? derivedUri : storedUri);

    m_authUsername->setText(profile.authUsername);
    m_registrar->setText(profile.registrar);
    m_proxy->setText(profile.proxy);
    m_outboundProxy->setText(profile.outboundProxy);
    m_emergencyUri->setText(profile.emergencyServiceUri);

    m_enableRtt->setChecked(profile.enableRtt);
    m_enableLmpe->setChecked(profile.enableLmpe);
    m_enableEtsi->setChecked(profile.enableEtsiCompatibility);

    switch (profile.transport) {
    case SipTransport::UDP: m_udp->setChecked(true); break;
    case SipTransport::TCP: m_tcp->setChecked(true); break;
    case SipTransport::TLS: m_tls->setChecked(true); break;
    }
}
