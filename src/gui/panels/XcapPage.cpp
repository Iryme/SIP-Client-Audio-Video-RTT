#include "XcapPage.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "security/CredentialStore.h"
#include "sip/XcapClient.h"
#include "sip/XcapDiagnosticsStore.h"

namespace {
constexpr int kColMethod = 0;
constexpr int kColUrl = 1;
constexpr int kColStatus = 2;
constexpr int kColContentType = 3;
constexpr int kColDuration = 4;
constexpr int kColEtag = 5;
constexpr int kColLastModified = 6;
constexpr int kColParseStatus = 7;
constexpr int kColTimestamp = 8;
constexpr int kColCount = 9;

const QString kXcapCredentialProfileId = QStringLiteral("xcap");
} // namespace

XcapPage::XcapPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    // ---- Server configuration ----
    auto *configBox = new QGroupBox(tr("XCAP Server"), this);
    auto *configLayout = new QVBoxLayout(configBox);

    auto *row1 = new QHBoxLayout;
    m_enableCheck = new QCheckBox(tr("Enable XCAP"), configBox);
    m_enableCheck->setChecked(AppSettings::enableXcap());
    row1->addWidget(m_enableCheck);
    row1->addWidget(new QLabel(tr("Root URI:"), configBox));
    m_rootUriEdit = new QLineEdit(configBox);
    m_rootUriEdit->setPlaceholderText(QStringLiteral("https://<xcap-host>/xcap-root"));
    m_rootUriEdit->setText(AppSettings::xcapRoot());
    row1->addWidget(m_rootUriEdit, 1);
    row1->addWidget(new QLabel(tr("XUI:"), configBox));
    m_xuiEdit = new QLineEdit(configBox);
    m_xuiEdit->setPlaceholderText(QStringLiteral("sip:<user>@<server-host>"));
    m_xuiEdit->setText(AppSettings::xcapXui());
    row1->addWidget(m_xuiEdit, 1);
    configLayout->addLayout(row1);

    auto *row2 = new QHBoxLayout;
    row2->addWidget(new QLabel(tr("Auth:"), configBox));
    m_authModeCombo = new QComboBox(configBox);
    m_authModeCombo->addItem(tr("None"), QStringLiteral("none"));
    m_authModeCombo->addItem(tr("Basic"), QStringLiteral("basic"));
    m_authModeCombo->addItem(tr("Digest"), QStringLiteral("digest"));
    m_authModeCombo->setCurrentIndex(m_authModeCombo->findData(AppSettings::xcapAuthentication()));
    row2->addWidget(m_authModeCombo);
    row2->addWidget(new QLabel(tr("Username:"), configBox));
    m_usernameEdit = new QLineEdit(configBox);
    m_usernameEdit->setText(AppSettings::xcapUsername());
    row2->addWidget(m_usernameEdit);
    row2->addWidget(new QLabel(tr("Password:"), configBox));
    m_passwordEdit = new QLineEdit(configBox);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    bool passwordFound = false;
    if (!AppSettings::xcapUsername().isEmpty()) {
        CredentialStore::instance().loadPassword(kXcapCredentialProfileId,
                                                  AppSettings::xcapUsername(), &passwordFound);
    }
    m_passwordEdit->setPlaceholderText(passwordFound ? tr("(stored — leave blank to keep)") : QString());
    row2->addWidget(m_passwordEdit);
    configLayout->addLayout(row2);

    auto *row3 = new QHBoxLayout;
    m_validateXmlCheck = new QCheckBox(tr("Validate XML before PUT"), configBox);
    m_validateXmlCheck->setChecked(AppSettings::validateXmlBeforePut());
    row3->addWidget(m_validateXmlCheck);
    row3->addWidget(new QLabel(tr("Timeout (s):"), configBox));
    m_timeoutSpin = new QSpinBox(configBox);
    m_timeoutSpin->setRange(1, 300);
    m_timeoutSpin->setValue(AppSettings::xcapTimeout());
    row3->addWidget(m_timeoutSpin);
    m_verifyTlsCheck = new QCheckBox(tr("Verify TLS"), configBox);
    m_verifyTlsCheck->setChecked(AppSettings::xcapVerifyTls());
    row3->addWidget(m_verifyTlsCheck);
    m_testConnectionBtn = new QPushButton(tr("Test Connection"), configBox);
    row3->addWidget(m_testConnectionBtn);
    row3->addStretch(1);
    configLayout->addLayout(row3);

    root->addWidget(configBox);

    connect(m_enableCheck, &QCheckBox::toggled, this, &XcapPage::onEnableToggled);
    connect(m_rootUriEdit, &QLineEdit::editingFinished, this, &XcapPage::onConfigFieldChanged);
    connect(m_xuiEdit, &QLineEdit::editingFinished, this, &XcapPage::onConfigFieldChanged);
    connect(m_usernameEdit, &QLineEdit::editingFinished, this, &XcapPage::onConfigFieldChanged);
    connect(m_passwordEdit, &QLineEdit::editingFinished, this, &XcapPage::savePasswordIfChanged);
    connect(m_authModeCombo, &QComboBox::currentIndexChanged, this, &XcapPage::onConfigFieldChanged);
    connect(m_validateXmlCheck, &QCheckBox::toggled, this,
            [](bool on) { AppSettings::setValidateXmlBeforePut(on); });
    connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [](int value) { AppSettings::setXcapTimeout(value); });
    connect(m_verifyTlsCheck, &QCheckBox::toggled, this,
            [](bool on) { AppSettings::setXcapVerifyTls(on); });
    connect(m_testConnectionBtn, &QPushButton::clicked, this, &XcapPage::onTestConnectionClicked);

    // ---- Document operation ----
    auto *opBox = new QGroupBox(tr("Document"), this);
    auto *opLayout = new QHBoxLayout(opBox);
    opLayout->addWidget(new QLabel(tr("AUID:"), opBox));
    m_auidCombo = new QComboBox(opBox);
    m_auidCombo->setEditable(true);
    m_auidCombo->addItem(QStringLiteral("resource-lists"));
    m_auidCombo->addItem(QStringLiteral("pres-rules"));
    m_auidCombo->addItem(QStringLiteral("rls-services"));
    m_auidCombo->addItem(QStringLiteral("xcap-caps"));
    opLayout->addWidget(m_auidCombo);
    opLayout->addWidget(new QLabel(tr("XUI override:"), opBox));
    m_docXuiEdit = new QLineEdit(opBox);
    m_docXuiEdit->setPlaceholderText(tr("(empty = use server XUI / global)"));
    opLayout->addWidget(m_docXuiEdit);
    opLayout->addWidget(new QLabel(tr("Document:"), opBox));
    m_documentNameEdit = new QLineEdit(opBox);
    m_documentNameEdit->setText(QStringLiteral("index"));
    opLayout->addWidget(m_documentNameEdit);
    opLayout->addWidget(new QLabel(tr("Node selector:"), opBox));
    m_nodeSelectorEdit = new QLineEdit(opBox);
    m_nodeSelectorEdit->setPlaceholderText(tr("(optional)"));
    opLayout->addWidget(m_nodeSelectorEdit);
    m_getBtn = new QPushButton(tr("GET"), opBox);
    m_putBtn = new QPushButton(tr("PUT"), opBox);
    m_deleteBtn = new QPushButton(tr("DELETE"), opBox);
    opLayout->addWidget(m_getBtn);
    opLayout->addWidget(m_putBtn);
    opLayout->addWidget(m_deleteBtn);
    root->addWidget(opBox);

    connect(m_getBtn, &QPushButton::clicked, this, &XcapPage::onGetClicked);
    connect(m_putBtn, &QPushButton::clicked, this, &XcapPage::onPutClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &XcapPage::onDeleteClicked);

    // ---- Result / document viewer + operation log ----
    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_documentEdit = new QPlainTextEdit(splitter);
    m_documentEdit->setPlaceholderText(tr("Document content (GET result / PUT body)…"));
    splitter->addWidget(m_documentEdit);

    auto *logContainer = new QWidget(splitter);
    auto *logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);
    auto *logHeaderLayout = new QHBoxLayout;
    m_resultLabel = new QLabel(this);
    m_clearLogBtn = new QPushButton(tr("Clear Log"), logContainer);
    logHeaderLayout->addWidget(m_resultLabel, 1);
    logHeaderLayout->addWidget(m_clearLogBtn);
    logLayout->addLayout(logHeaderLayout);

    m_logTable = new QTableWidget(0, kColCount, logContainer);
    m_logTable->setHorizontalHeaderLabels({
        tr("Method"), tr("URL (redacted)"), tr("Status"), tr("Content-Type"),
        tr("Duration (ms)"), tr("ETag"), tr("Last-Modified"), tr("Parse"), tr("Timestamp")
    });
    m_logTable->horizontalHeader()->setStretchLastSection(true);
    m_logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logLayout->addWidget(m_logTable, 1);
    splitter->addWidget(logContainer);

    root->addWidget(splitter, 1);

    connect(m_clearLogBtn, &QPushButton::clicked, this, &XcapPage::onClearLogClicked);
    connect(m_logTable, &QTableWidget::itemSelectionChanged, this, &XcapPage::onSelectionChanged);

    connect(&XcapClient::instance(), &XcapClient::operationCompleted,
            this, &XcapPage::onOperationCompleted);
    connect(&XcapDiagnosticsStore::instance(), &XcapDiagnosticsStore::cleared,
            this, [this]() { m_logRows.clear(); m_logTable->setRowCount(0); });

    // Ensures the diagnostics store starts collecting as soon as this page
    // is opened, rather than only on first export.
    XcapDiagnosticsStore::instance();

    rebuildLog();
    updateControlsEnabled();
}

XcapServerConfig XcapPage::currentConfig() const
{
    XcapServerConfig config;
    config.rootUri = m_rootUriEdit->text().trimmed();
    config.xui = m_xuiEdit->text().trimmed();
    config.username = m_usernameEdit->text().trimmed();
    config.authMode = xcapAuthModeFromString(m_authModeCombo->currentData().toString());
    config.validateXmlBeforePut = m_validateXmlCheck->isChecked();
    config.timeoutSeconds = m_timeoutSpin->value();
    config.verifyTls = m_verifyTlsCheck->isChecked();
    return config;
}

XcapDocument XcapPage::currentDocument() const
{
    XcapDocument doc;
    doc.auid = m_auidCombo->currentText().trimmed();
    doc.xui = m_docXuiEdit->text().trimmed();
    doc.documentName = m_documentNameEdit->text().trimmed().isEmpty()
        ? QStringLiteral("index") : m_documentNameEdit->text().trimmed();
    doc.nodeSelector = m_nodeSelectorEdit->text().trimmed();
    return doc;
}

void XcapPage::updateControlsEnabled()
{
    const bool on = AppSettings::enableXcap();
    m_testConnectionBtn->setEnabled(on);
    m_getBtn->setEnabled(on);
    m_putBtn->setEnabled(on);
    m_deleteBtn->setEnabled(on);
}

void XcapPage::onEnableToggled(bool on)
{
    AppSettings::setEnableXcap(on);
    updateControlsEnabled();
}

void XcapPage::onConfigFieldChanged()
{
    AppSettings::setXcapRoot(m_rootUriEdit->text().trimmed());
    AppSettings::setXcapXui(m_xuiEdit->text().trimmed());
    AppSettings::setXcapUsername(m_usernameEdit->text().trimmed());
    AppSettings::setXcapAuthentication(m_authModeCombo->currentData().toString());
}

void XcapPage::savePasswordIfChanged()
{
    const QString password = m_passwordEdit->text();
    if (password.isEmpty())
        return; // blank means "keep existing stored password"
    const QString username = m_usernameEdit->text().trimmed();
    if (username.isEmpty())
        return;
    CredentialStore::instance().storePassword(kXcapCredentialProfileId, username, password);
    m_passwordEdit->clear();
    m_passwordEdit->setPlaceholderText(tr("(stored — leave blank to keep)"));
}

void XcapPage::onTestConnectionClicked()
{
    onConfigFieldChanged();
    XcapDocument caps;
    caps.auid = QStringLiteral("xcap-caps");
    caps.documentName = QStringLiteral("index");
    m_resultLabel->setText(tr("Testing connection (GET xcap-caps)…"));
    XcapClient::instance().get(currentConfig(), caps);
}

void XcapPage::onGetClicked()
{
    onConfigFieldChanged();
    if (currentConfig().rootUri.isEmpty()) {
        m_resultLabel->setText(tr("Configure a Root URI first."));
        return;
    }
    m_resultLabel->setText(tr("GET requested…"));
    XcapClient::instance().get(currentConfig(), currentDocument());
}

void XcapPage::onPutClicked()
{
    onConfigFieldChanged();
    if (currentConfig().rootUri.isEmpty()) {
        m_resultLabel->setText(tr("Configure a Root URI first."));
        return;
    }
    m_resultLabel->setText(tr("PUT requested…"));
    XcapClient::instance().put(currentConfig(), currentDocument(), m_documentEdit->toPlainText());
}

void XcapPage::onDeleteClicked()
{
    onConfigFieldChanged();
    if (currentConfig().rootUri.isEmpty()) {
        m_resultLabel->setText(tr("Configure a Root URI first."));
        return;
    }
    m_resultLabel->setText(tr("DELETE requested…"));
    XcapClient::instance().del(currentConfig(), currentDocument());
}

void XcapPage::onClearLogClicked()
{
    XcapDiagnosticsStore::instance().clear();
}

void XcapPage::onOperationCompleted(const XcapResult &result)
{
    m_logRows.append(result);
    addLogRow(result);

    if (result.method == XcapHttpMethod::Get && !result.networkError && result.ok())
        m_documentEdit->setPlainText(result.bodyPreview);

    if (result.networkError) {
        m_resultLabel->setText(tr("%1 failed: %2")
            .arg(xcapHttpMethodToString(result.method), result.errorString));
    } else {
        m_resultLabel->setText(tr("%1 -> HTTP %2 %3 (%4 ms)")
            .arg(xcapHttpMethodToString(result.method))
            .arg(result.httpStatus)
            .arg(result.httpReason, QString::number(result.durationMs)));
    }
}

void XcapPage::onSelectionChanged()
{
    const auto selected = m_logTable->selectionModel() ? m_logTable->selectionModel()->selectedRows() : QModelIndexList();
    if (selected.isEmpty())
        return;
    const int row = selected.first().row();
    if (row < 0 || row >= m_logRows.size())
        return;
    const XcapResult &result = m_logRows.at(row);
    if (result.method == XcapHttpMethod::Get)
        m_documentEdit->setPlainText(result.bodyPreview);
}

void XcapPage::rebuildLog()
{
    m_logRows = XcapDiagnosticsStore::instance().entries();
    m_logTable->setRowCount(0);
    for (const XcapResult &result : m_logRows)
        addLogRow(result);
}

void XcapPage::addLogRow(const XcapResult &result)
{
    const int row = m_logTable->rowCount();
    m_logTable->insertRow(row);
    for (int col = 0; col < kColCount; ++col)
        m_logTable->setItem(row, col, new QTableWidgetItem());

    m_logTable->item(row, kColMethod)->setText(xcapHttpMethodToString(result.method));
    m_logTable->item(row, kColUrl)->setText(result.urlRedacted);
    m_logTable->item(row, kColStatus)->setText(
        result.networkError ? tr("error") : QString::number(result.httpStatus));
    m_logTable->item(row, kColContentType)->setText(result.contentType);
    m_logTable->item(row, kColDuration)->setText(QString::number(result.durationMs));
    m_logTable->item(row, kColEtag)->setText(result.etag);
    m_logTable->item(row, kColLastModified)->setText(result.lastModified);
    m_logTable->item(row, kColParseStatus)->setText(xcapParseStatusToString(result.parseStatus));
    m_logTable->item(row, kColTimestamp)->setText(
        result.timestamp.isValid() ? result.timestamp.toString(Qt::ISODateWithMs) : QString());
}
