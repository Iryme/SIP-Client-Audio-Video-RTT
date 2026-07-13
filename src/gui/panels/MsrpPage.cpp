#include "MsrpPage.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "msrp/MsrpDiagnosticsStore.h"
#include "msrp/MsrpFileReceiver.h"
#include "msrp/MsrpRelayDiagnosticsStore.h"
#include "msrp/MsrpSession.h"
#include "msrp/MsrpSessionStore.h"
#include "sip/InteropTraceExporter.h"

namespace {
constexpr int kSessColKey = 0;
constexpr int kSessColCallId = 1;
constexpr int kSessColTransport = 2;
constexpr int kSessColRole = 3;
constexpr int kSessColState = 4;
// Task W102 Phase 9: negotiation state and peer-association result, made
// directly visible rather than only present in the JSON export — both
// read straight from MsrpSessionInfo (offerAnswerState / mediaIndex+
// sipHeaderCallId), never re-derived or guessed here.
constexpr int kSessColNegotiation = 5;
constexpr int kSessColPeerAssoc = 6;
constexpr int kSessColBytes = 7;
constexpr int kSessColFrames = 8;
constexpr int kSessColLastError = 9;
constexpr int kSessColCount = 10;

constexpr int kDiagColTime = 0;
constexpr int kDiagColDir = 1;
constexpr int kDiagColMethod = 2;
constexpr int kDiagColTid = 3;
constexpr int kDiagColMsgId = 4;
constexpr int kDiagColByteRange = 5;
constexpr int kDiagColContinuation = 6;
constexpr int kDiagColPreview = 7;
constexpr int kDiagColCount = 8;

constexpr int kRelayDiagColTime = 0;
constexpr int kRelayDiagColKind = 1;
constexpr int kRelayDiagColResponse = 2;
constexpr int kRelayDiagColAlgorithm = 3;
constexpr int kRelayDiagColAllocatedPath = 4;
constexpr int kRelayDiagColExpires = 5;
constexpr int kRelayDiagColError = 6;
constexpr int kRelayDiagColCount = 7;

QChar continuationChar(MsrpContinuation c) { return msrpContinuationToChar(c); }

QString relayEventKindLabel(MsrpRelayDiagnosticsEvent::Kind kind)
{
    switch (kind) {
    case MsrpRelayDiagnosticsEvent::Kind::Connect: return QStringLiteral("Connect");
    case MsrpRelayDiagnosticsEvent::Kind::ConnectFailed: return QStringLiteral("Connect Failed");
    case MsrpRelayDiagnosticsEvent::Kind::InitialAuthSent: return QStringLiteral("Initial AUTH Sent");
    case MsrpRelayDiagnosticsEvent::Kind::ChallengeReceived: return QStringLiteral("Challenge Received");
    case MsrpRelayDiagnosticsEvent::Kind::AuthenticatedAuthSent: return QStringLiteral("Authenticated AUTH Sent");
    case MsrpRelayDiagnosticsEvent::Kind::AllocationSuccess: return QStringLiteral("Allocation Success");
    case MsrpRelayDiagnosticsEvent::Kind::AllocationFailure: return QStringLiteral("Allocation Failure");
    case MsrpRelayDiagnosticsEvent::Kind::Refresh: return QStringLiteral("Refresh");
    case MsrpRelayDiagnosticsEvent::Kind::RefreshFailure: return QStringLiteral("Refresh Failure");
    case MsrpRelayDiagnosticsEvent::Kind::Reconnect: return QStringLiteral("Reconnect");
    case MsrpRelayDiagnosticsEvent::Kind::Reauthenticate: return QStringLiteral("Reauthenticate");
    case MsrpRelayDiagnosticsEvent::Kind::Expired: return QStringLiteral("Expired");
    case MsrpRelayDiagnosticsEvent::Kind::Closed: return QStringLiteral("Closed");
    case MsrpRelayDiagnosticsEvent::Kind::Error: return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}
} // namespace

MsrpPage::MsrpPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    // ---- Session configuration ----
    auto *configBox = new QGroupBox(tr("MSRP Configuration"), this);
    auto *configLayout = new QVBoxLayout(configBox);

    auto *row1 = new QHBoxLayout;
    m_enableCheck = new QCheckBox(tr("Enable MSRP"), configBox);
    m_enableCheck->setChecked(AppSettings::enableMsrp());
    row1->addWidget(m_enableCheck);
    row1->addWidget(new QLabel(tr("Transport policy:"), configBox));
    m_transportModeCombo = new QComboBox(configBox);
    m_transportModeCombo->addItem(tr("Automatic"), QStringLiteral("automatic"));
    m_transportModeCombo->addItem(tr("SIP MESSAGE only"), QStringLiteral("sip-message-only"));
    m_transportModeCombo->addItem(tr("MSRP preferred"), QStringLiteral("msrp-preferred"));
    m_transportModeCombo->addItem(tr("MSRP required"), QStringLiteral("msrp-required"));
    m_transportModeCombo->setCurrentIndex(m_transportModeCombo->findData(AppSettings::messagingTransportMode()));
    row1->addWidget(m_transportModeCombo);
    m_enableTcpCheck = new QCheckBox(tr("TCP"), configBox);
    m_enableTcpCheck->setChecked(AppSettings::enableMsrpTcp());
    row1->addWidget(m_enableTcpCheck);
    m_enableTlsCheck = new QCheckBox(tr("TLS"), configBox);
    m_enableTlsCheck->setChecked(AppSettings::enableMsrpTls());
    row1->addWidget(m_enableTlsCheck);
    configLayout->addLayout(row1);

    auto *row2 = new QHBoxLayout;
    row2->addWidget(new QLabel(tr("Bind address:"), configBox));
    m_bindAddressEdit = new QLineEdit(configBox);
    m_bindAddressEdit->setPlaceholderText(QStringLiteral("(empty = any)"));
    m_bindAddressEdit->setText(AppSettings::msrpLocalBindAddress());
    row2->addWidget(m_bindAddressEdit);
    row2->addWidget(new QLabel(tr("Advertised host:"), configBox));
    m_advertisedHostEdit = new QLineEdit(configBox);
    m_advertisedHostEdit->setPlaceholderText(QStringLiteral("(required to advertise a=path)"));
    m_advertisedHostEdit->setText(AppSettings::msrpAdvertisedHost());
    row2->addWidget(m_advertisedHostEdit);
    row2->addWidget(new QLabel(tr("Port mode:"), configBox));
    m_portModeCombo = new QComboBox(configBox);
    m_portModeCombo->addItem(tr("Automatic"), QStringLiteral("automatic"));
    m_portModeCombo->addItem(tr("Fixed"), QStringLiteral("fixed"));
    m_portModeCombo->setCurrentIndex(m_portModeCombo->findData(AppSettings::msrpPortMode()));
    row2->addWidget(m_portModeCombo);
    m_fixedPortSpin = new QSpinBox(configBox);
    m_fixedPortSpin->setRange(0, 65535);
    m_fixedPortSpin->setValue(AppSettings::msrpFixedPort());
    row2->addWidget(m_fixedPortSpin);
    configLayout->addLayout(row2);

    auto *row3 = new QHBoxLayout;
    m_requestReportsCheck = new QCheckBox(tr("Request Success/Failure reports"), configBox);
    m_requestReportsCheck->setChecked(AppSettings::msrpRequestReports());
    row3->addWidget(m_requestReportsCheck);
    row3->addWidget(new QLabel(tr("Chunk size (bytes):"), configBox));
    m_chunkSizeSpin = new QSpinBox(configBox);
    m_chunkSizeSpin->setRange(256, 1048576);
    m_chunkSizeSpin->setValue(AppSettings::msrpChunkSizeBytes());
    row3->addWidget(m_chunkSizeSpin);
    row3->addWidget(new QLabel(tr("Connect timeout (ms):"), configBox));
    m_connectTimeoutSpin = new QSpinBox(configBox);
    m_connectTimeoutSpin->setRange(500, 120000);
    m_connectTimeoutSpin->setValue(AppSettings::msrpConnectionTimeoutMs());
    row3->addWidget(m_connectTimeoutSpin);
    row3->addStretch(1);
    configLayout->addLayout(row3);

    root->addWidget(configBox);

    connect(m_enableCheck, &QCheckBox::toggled, this, &MsrpPage::onEnableToggled);
    connect(m_transportModeCombo, &QComboBox::currentIndexChanged, this, &MsrpPage::onConfigFieldChanged);
    connect(m_enableTcpCheck, &QCheckBox::toggled, this, [](bool on) { AppSettings::setEnableMsrpTcp(on); });
    connect(m_enableTlsCheck, &QCheckBox::toggled, this, [](bool on) { AppSettings::setEnableMsrpTls(on); });
    connect(m_bindAddressEdit, &QLineEdit::editingFinished, this, &MsrpPage::onConfigFieldChanged);
    connect(m_advertisedHostEdit, &QLineEdit::editingFinished, this, &MsrpPage::onConfigFieldChanged);
    connect(m_portModeCombo, &QComboBox::currentIndexChanged, this, &MsrpPage::onConfigFieldChanged);
    connect(m_fixedPortSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [](int v) { AppSettings::setMsrpFixedPort(v); });
    connect(m_requestReportsCheck, &QCheckBox::toggled, this,
            [](bool on) { AppSettings::setMsrpRequestReports(on); });
    connect(m_chunkSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [](int v) { AppSettings::setMsrpChunkSizeBytes(v); });
    connect(m_connectTimeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [](int v) { AppSettings::setMsrpConnectionTimeoutMs(v); });

    // ---- Manual test session (experimental; independent of live calls — ----
    // ---- see docs/msrp-foundation.md for why live-call auto-wiring is  ----
    // ---- not implemented in this task) ----
    auto *testBox = new QGroupBox(tr("Manual Test Session (experimental)"), this);
    auto *testLayout = new QHBoxLayout(testBox);
    testLayout->addWidget(new QLabel(tr("Remote host:"), testBox));
    m_remoteHostEdit = new QLineEdit(testBox);
    m_remoteHostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
    testLayout->addWidget(m_remoteHostEdit);
    testLayout->addWidget(new QLabel(tr("Port:"), testBox));
    m_remotePortSpin = new QSpinBox(testBox);
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(2855);
    testLayout->addWidget(m_remotePortSpin);
    testLayout->addWidget(new QLabel(tr("Session-id:"), testBox));
    m_remoteSessionIdEdit = new QLineEdit(testBox);
    testLayout->addWidget(m_remoteSessionIdEdit);
    m_startActiveBtn = new QPushButton(tr("Connect (active)"), testBox);
    m_startPassiveBtn = new QPushButton(tr("Listen (passive)"), testBox);
    m_disconnectBtn = new QPushButton(tr("Disconnect"), testBox);
    testLayout->addWidget(m_startActiveBtn);
    testLayout->addWidget(m_startPassiveBtn);
    testLayout->addWidget(m_disconnectBtn);
    root->addWidget(testBox);

    connect(m_startActiveBtn, &QPushButton::clicked, this, &MsrpPage::onStartActiveClicked);
    connect(m_startPassiveBtn, &QPushButton::clicked, this, &MsrpPage::onStartPassiveClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MsrpPage::onDisconnectClicked);

    auto *msgLayout = new QHBoxLayout;
    m_testMessageEdit = new QPlainTextEdit(testBox);
    m_testMessageEdit->setPlaceholderText(tr("Test message body (text/plain)…"));
    m_testMessageEdit->setMaximumHeight(60);
    msgLayout->addWidget(m_testMessageEdit, 1);
    m_sendTestBtn = new QPushButton(tr("Send"), testBox);
    msgLayout->addWidget(m_sendTestBtn);
    m_sendFileBtn = new QPushButton(tr("Send File…"), testBox);
    msgLayout->addWidget(m_sendFileBtn);
    root->addLayout(msgLayout);
    connect(m_sendTestBtn, &QPushButton::clicked, this, &MsrpPage::onSendTestMessageClicked);
    connect(m_sendFileBtn, &QPushButton::clicked, this, &MsrpPage::onSendFileClicked);

    m_statusLabel = new QLabel(this);
    root->addWidget(m_statusLabel);

    // ---- Active sessions + Diagnostics ----
    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_sessionsTable = new QTableWidget(0, kSessColCount, splitter);
    m_sessionsTable->setHorizontalHeaderLabels({
        tr("Session"), tr("SIP Call-ID"), tr("Transport"), tr("Role"),
        tr("State"), tr("Negotiation"), tr("Peer Association"),
        tr("Bytes S/R"), tr("Frames S/R"), tr("Last Error")
    });
    m_sessionsTable->horizontalHeader()->setStretchLastSection(true);
    m_sessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    splitter->addWidget(m_sessionsTable);

    auto *diagContainer = new QWidget(splitter);
    auto *diagLayout = new QVBoxLayout(diagContainer);
    diagLayout->setContentsMargins(0, 0, 0, 0);
    auto *diagHeaderLayout = new QHBoxLayout;
    diagHeaderLayout->addWidget(new QLabel(tr("Diagnostics"), diagContainer), 1);
    m_clearDiagBtn = new QPushButton(tr("Clear Diagnostics"), diagContainer);
    m_exportJsonBtn = new QPushButton(tr("Export JSON"), diagContainer);
    m_exportTxtBtn = new QPushButton(tr("Export TXT"), diagContainer);
    diagHeaderLayout->addWidget(m_clearDiagBtn);
    diagHeaderLayout->addWidget(m_exportJsonBtn);
    diagHeaderLayout->addWidget(m_exportTxtBtn);
    diagLayout->addLayout(diagHeaderLayout);

    m_diagnosticsTable = new QTableWidget(0, kDiagColCount, diagContainer);
    m_diagnosticsTable->setHorizontalHeaderLabels({
        tr("Time"), tr("Dir"), tr("Method"), tr("Transaction-ID"),
        tr("Message-ID"), tr("Byte-Range"), tr("Cont."), tr("Preview")
    });
    m_diagnosticsTable->horizontalHeader()->setStretchLastSection(true);
    m_diagnosticsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_diagnosticsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    diagLayout->addWidget(m_diagnosticsTable, 1);
    splitter->addWidget(diagContainer);

    root->addWidget(splitter, 1);

    // ---- MSRP Relay diagnostics (Task W107, RFC 4976 — experimental) ----
    // Relay support itself is Disabled by default (MsrpRelayConfig::mode);
    // this table only ever shows rows when a relay is explicitly configured
    // and used, and every field is already redacted at the source (see
    // MsrpRelayDiagnosticsEvent) — no nonce/digest/credential ever reaches
    // this UI.
    auto *relayBox = new QGroupBox(tr("MSRP Relay Diagnostics (RFC 4976, Experimental)"), this);
    auto *relayLayout = new QVBoxLayout(relayBox);
    auto *relayHeaderLayout = new QHBoxLayout;
    relayHeaderLayout->addWidget(new QLabel(
        tr("Disabled by default. Populated only when a relay is explicitly configured."), relayBox), 1);
    m_clearRelayDiagBtn = new QPushButton(tr("Clear"), relayBox);
    relayHeaderLayout->addWidget(m_clearRelayDiagBtn);
    relayLayout->addLayout(relayHeaderLayout);

    m_relayDiagnosticsTable = new QTableWidget(0, kRelayDiagColCount, relayBox);
    m_relayDiagnosticsTable->setHorizontalHeaderLabels({
        tr("Time"), tr("Kind"), tr("Response"), tr("Algorithm"),
        tr("Allocated Path"), tr("Expires"), tr("Error")
    });
    m_relayDiagnosticsTable->horizontalHeader()->setStretchLastSection(true);
    m_relayDiagnosticsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_relayDiagnosticsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_relayDiagnosticsTable->setMaximumHeight(160);
    relayLayout->addWidget(m_relayDiagnosticsTable);
    root->addWidget(relayBox);

    connect(m_clearDiagBtn, &QPushButton::clicked, this, &MsrpPage::onClearDiagnosticsClicked);
    connect(m_exportJsonBtn, &QPushButton::clicked, this, &MsrpPage::onExportJsonClicked);
    connect(m_exportTxtBtn, &QPushButton::clicked, this, &MsrpPage::onExportTxtClicked);
    connect(m_clearRelayDiagBtn, &QPushButton::clicked, this, [this]() {
        MsrpRelayDiagnosticsStore::instance().clear();
    });

    connect(&MsrpSessionStore::instance(), &MsrpSessionStore::sessionUpdated, this, &MsrpPage::onSessionUpdated);
    connect(&MsrpSessionStore::instance(), &MsrpSessionStore::sessionRemoved, this, &MsrpPage::onSessionRemoved);
    connect(&MsrpSessionStore::instance(), &MsrpSessionStore::cleared, this, [this]() {
        m_sessionRows.clear(); m_sessionsTable->setRowCount(0);
    });
    connect(&MsrpDiagnosticsStore::instance(), &MsrpDiagnosticsStore::entryLogged, this, &MsrpPage::onDiagnosticEvent);
    connect(&MsrpDiagnosticsStore::instance(), &MsrpDiagnosticsStore::cleared, this, [this]() {
        m_diagRows.clear(); m_diagnosticsTable->setRowCount(0);
    });
    connect(&MsrpRelayDiagnosticsStore::instance(), &MsrpRelayDiagnosticsStore::entryLogged,
            this, &MsrpPage::onRelayDiagnosticEvent);
    connect(&MsrpRelayDiagnosticsStore::instance(), &MsrpRelayDiagnosticsStore::cleared, this, [this]() {
        m_relayDiagRows.clear(); m_relayDiagnosticsTable->setRowCount(0);
    });

    rebuildSessionTable();
    rebuildDiagnosticsTable();
    rebuildRelayDiagnosticsTable();
    updateControlsEnabled();
}

MsrpPage::~MsrpPage()
{
    delete m_testSession;
}

void MsrpPage::updateControlsEnabled()
{
    const bool on = AppSettings::enableMsrp();
    m_startActiveBtn->setEnabled(on);
    m_startPassiveBtn->setEnabled(on);
}

void MsrpPage::onEnableToggled(bool on)
{
    AppSettings::setEnableMsrp(on);
    updateControlsEnabled();
}

void MsrpPage::onConfigFieldChanged()
{
    AppSettings::setMessagingTransportMode(m_transportModeCombo->currentData().toString());
    AppSettings::setMsrpLocalBindAddress(m_bindAddressEdit->text().trimmed());
    AppSettings::setMsrpAdvertisedHost(m_advertisedHostEdit->text().trimmed());
    AppSettings::setMsrpPortMode(m_portModeCombo->currentData().toString());
}

void MsrpPage::wireTestSessionSignals()
{
    connect(m_testSession, &MsrpSession::payloadReceived, this,
            [this](const QString &, const QString &, const QString &contentType, const QByteArray &body) {
        m_statusLabel->setText(tr("Received %1 bytes (%2)").arg(body.size()).arg(contentType));
    });
    // Task W104: fires in addition to payloadReceived above (never instead
    // of it) for inbound messages whose Content-Disposition marks them as a
    // file transfer (RFC 5547). The save location is always the user's own
    // choice via this dialog — the peer-supplied name is only ever used as
    // a suggested default (already sanitized by MsrpSession).
    connect(m_testSession, &MsrpSession::fileTransferReceived, this,
            [this](const QString &, const QString &, const QString &, const QString &suggestedFileName,
                   const QByteArray &body) {
        const QString path = QFileDialog::getSaveFileName(this, tr("Save received file"), suggestedFileName);
        if (path.isEmpty()) {
            m_statusLabel->setText(tr("Received file (%1 bytes) — save skipped").arg(body.size()));
            return;
        }
        const auto result = MsrpFileReceiver::saveToPath(body, path);
        if (result.ok)
            m_statusLabel->setText(tr("Saved received file: %1 (%2 bytes)").arg(path).arg(result.bytesWritten));
        else
            QMessageBox::warning(this, tr("Save failed"), result.error);
    });
}

void MsrpPage::onStartActiveClicked()
{
    delete m_testSession;
    m_testSession = new MsrpSession(QStringLiteral("manual-test"), this);
    wireTestSessionSignals();

    const QString host = m_remoteHostEdit->text().trimmed().isEmpty()
        ? QStringLiteral("127.0.0.1") : m_remoteHostEdit->text().trimmed();
    const QString sessionId = m_remoteSessionIdEdit->text().trimmed().isEmpty()
        ? MsrpPath::generateSessionId() : m_remoteSessionIdEdit->text().trimmed();

    m_testSession->setLocalUri(MsrpPath::buildUri(m_enableTlsCheck->isChecked(),
        m_advertisedHostEdit->text().trimmed().isEmpty() ? QStringLiteral("0.0.0.0") : m_advertisedHostEdit->text().trimmed(),
        0, MsrpPath::generateSessionId()));
    m_testSession->setRemotePath({MsrpPath::buildUri(m_enableTlsCheck->isChecked(), host,
                                                     m_remotePortSpin->value(), sessionId)});
    m_testSession->setChunkSizeBytes(m_chunkSizeSpin->value());
    m_testSession->setRequestReports(m_requestReportsCheck->isChecked());
    m_testSession->connectAsActive(m_connectTimeoutSpin->value());
    m_statusLabel->setText(tr("Connecting (active) to %1:%2…").arg(host).arg(m_remotePortSpin->value()));
}

void MsrpPage::onStartPassiveClicked()
{
    delete m_testSession;
    m_testSession = new MsrpSession(QStringLiteral("manual-test"), this);
    wireTestSessionSignals();

    const int port = AppSettings::msrpPortMode() == QStringLiteral("fixed") ? m_fixedPortSpin->value() : 0;
    m_testSession->setLocalUri(MsrpPath::buildUri(m_enableTlsCheck->isChecked(),
        QStringLiteral("0.0.0.0"), port, MsrpPath::generateSessionId()));
    m_testSession->setChunkSizeBytes(m_chunkSizeSpin->value());
    m_testSession->setRequestReports(m_requestReportsCheck->isChecked());
    m_testSession->listenAsPassive(m_bindAddressEdit->text().trimmed(), m_connectTimeoutSpin->value());
    m_statusLabel->setText(tr("Listening (passive) on port %1…").arg(m_testSession->transportLocalPort()));
}

void MsrpPage::onSendTestMessageClicked()
{
    if (!m_testSession) {
        m_statusLabel->setText(tr("Start a test session first."));
        return;
    }
    const QString msgId = m_testSession->sendMessage(QStringLiteral("text/plain"),
                                                     m_testMessageEdit->toPlainText().toUtf8());
    m_statusLabel->setText(tr("Sent message %1").arg(msgId));
}

void MsrpPage::onSendFileClicked()
{
    if (!m_testSession) {
        m_statusLabel->setText(tr("Start a test session first."));
        return;
    }
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Select file to send"));
    if (filePath.isEmpty())
        return;

    const auto result = m_testSession->sendFile(filePath, QStringLiteral("application/octet-stream"));
    if (result.ok)
        m_statusLabel->setText(tr("Sending file %1 (%2 bytes, message %3)")
            .arg(QFileInfo(filePath).fileName()).arg(result.fileSize).arg(result.messageId));
    else
        QMessageBox::warning(this, tr("Send file failed"), result.error);
}

void MsrpPage::onDisconnectClicked()
{
    if (m_testSession)
        m_testSession->closeSession();
}

void MsrpPage::onClearDiagnosticsClicked()
{
    MsrpDiagnosticsStore::instance().clear();
}

void MsrpPage::onExportJsonClicked()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export MSRP JSON"), QString(), QStringLiteral("*.json"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(InteropTraceExporter::exportToJson().toUtf8());
        f.close();
    } else {
        QMessageBox::warning(this, tr("Export failed"), f.errorString());
    }
}

void MsrpPage::onExportTxtClicked()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export MSRP TXT"), QString(), QStringLiteral("*.txt"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Export failed"), f.errorString());
        return;
    }
    QTextStream ts(&f);
    for (const auto &ev : MsrpDiagnosticsStore::instance().entries()) {
        ts << ev.timestamp.toString(Qt::ISODateWithMs)
           << (ev.direction == MsrpDiagnosticsEvent::Direction::Outbound ? " >>> " : " <<< ")
           << ev.method << " tid=" << ev.transactionId << " msg=" << ev.messageId
           << " " << ev.byteRangeText << " " << continuationChar(ev.continuation)
           << "\n  " << ev.toPathRedacted << " <- " << ev.fromPathRedacted
           << "\n  preview: " << ev.bodyPreview << "\n";
    }
}

void MsrpPage::onSessionUpdated(const MsrpSessionInfo &info)
{
    addOrUpdateSessionRow(info);
}

void MsrpPage::onSessionRemoved(const QString &sessionKey)
{
    for (int i = 0; i < m_sessionRows.size(); ++i) {
        if (m_sessionRows.at(i).sessionKey == sessionKey) {
            m_sessionRows.removeAt(i);
            m_sessionsTable->removeRow(i);
            break;
        }
    }
}

void MsrpPage::onDiagnosticEvent(const MsrpDiagnosticsEvent &event)
{
    m_diagRows.append(event);
    addDiagnosticRow(event);
}

void MsrpPage::onRelayDiagnosticEvent(const MsrpRelayDiagnosticsEvent &event)
{
    m_relayDiagRows.append(event);
    addRelayDiagnosticRow(event);
}

void MsrpPage::rebuildRelayDiagnosticsTable()
{
    m_relayDiagRows = MsrpRelayDiagnosticsStore::instance().entries();
    m_relayDiagnosticsTable->setRowCount(0);
    for (const auto &ev : m_relayDiagRows)
        addRelayDiagnosticRow(ev);
}

void MsrpPage::addRelayDiagnosticRow(const MsrpRelayDiagnosticsEvent &event)
{
    const int row = m_relayDiagnosticsTable->rowCount();
    m_relayDiagnosticsTable->insertRow(row);
    for (int col = 0; col < kRelayDiagColCount; ++col)
        m_relayDiagnosticsTable->setItem(row, col, new QTableWidgetItem());

    m_relayDiagnosticsTable->item(row, kRelayDiagColTime)->setText(event.timestamp.toString(Qt::ISODateWithMs));
    m_relayDiagnosticsTable->item(row, kRelayDiagColKind)->setText(relayEventKindLabel(event.kind));
    m_relayDiagnosticsTable->item(row, kRelayDiagColResponse)->setText(
        event.responseCode > 0 ? QStringLiteral("%1 %2").arg(event.responseCode).arg(event.responseComment)
                               : QString());
    m_relayDiagnosticsTable->item(row, kRelayDiagColAlgorithm)->setText(event.algorithm);
    m_relayDiagnosticsTable->item(row, kRelayDiagColAllocatedPath)->setText(event.allocatedPathRedacted);
    m_relayDiagnosticsTable->item(row, kRelayDiagColExpires)->setText(
        event.expiresAt.isValid() ? event.expiresAt.toString(Qt::ISODateWithMs) : QString());
    m_relayDiagnosticsTable->item(row, kRelayDiagColError)->setText(event.error);
}

void MsrpPage::rebuildSessionTable()
{
    m_sessionRows = MsrpSessionStore::instance().snapshot();
    m_sessionsTable->setRowCount(0);
    for (const auto &info : m_sessionRows)
        addOrUpdateSessionRow(info);
}

void MsrpPage::rebuildDiagnosticsTable()
{
    m_diagRows = MsrpDiagnosticsStore::instance().entries();
    m_diagnosticsTable->setRowCount(0);
    for (const auto &ev : m_diagRows)
        addDiagnosticRow(ev);
}

void MsrpPage::addOrUpdateSessionRow(const MsrpSessionInfo &info)
{
    int row = -1;
    for (int i = 0; i < m_sessionRows.size(); ++i) {
        if (m_sessionRows.at(i).sessionKey == info.sessionKey) { row = i; break; }
    }
    if (row < 0) {
        row = m_sessionRows.size();
        m_sessionRows.append(info);
        m_sessionsTable->insertRow(row);
        for (int col = 0; col < kSessColCount; ++col)
            m_sessionsTable->setItem(row, col, new QTableWidgetItem());
    } else {
        m_sessionRows[row] = info;
    }

    m_sessionsTable->item(row, kSessColKey)->setText(info.sessionKey);
    m_sessionsTable->item(row, kSessColCallId)->setText(info.sipCallId);
    m_sessionsTable->item(row, kSessColTransport)->setText(msrpTransportProtocolToString(info.localTransport));
    m_sessionsTable->item(row, kSessColRole)->setText(msrpRoleToString(info.role));
    m_sessionsTable->item(row, kSessColState)->setText(msrpSessionStateToString(info.state));
    m_sessionsTable->item(row, kSessColNegotiation)->setText(
        info.offerAnswerState.isEmpty() ? tr("(unknown)") : info.offerAnswerState);
    {
        const bool exact = info.mediaIndex >= 0 && !info.sipHeaderCallId.isEmpty();
        m_sessionsTable->item(row, kSessColPeerAssoc)->setText(
            exact ? tr("exact (media #%1)").arg(info.mediaIndex) : tr("unknown"));
    }
    m_sessionsTable->item(row, kSessColBytes)->setText(
        QStringLiteral("%1 / %2").arg(info.bytesSent).arg(info.bytesReceived));
    m_sessionsTable->item(row, kSessColFrames)->setText(
        QStringLiteral("%1 / %2").arg(info.framesSent).arg(info.framesReceived));
    m_sessionsTable->item(row, kSessColLastError)->setText(info.lastError);
}

void MsrpPage::addDiagnosticRow(const MsrpDiagnosticsEvent &event)
{
    const int row = m_diagnosticsTable->rowCount();
    m_diagnosticsTable->insertRow(row);
    for (int col = 0; col < kDiagColCount; ++col)
        m_diagnosticsTable->setItem(row, col, new QTableWidgetItem());

    m_diagnosticsTable->item(row, kDiagColTime)->setText(event.timestamp.toString(Qt::ISODateWithMs));
    m_diagnosticsTable->item(row, kDiagColDir)->setText(
        event.direction == MsrpDiagnosticsEvent::Direction::Outbound ? QStringLiteral(">>>") : QStringLiteral("<<<"));
    m_diagnosticsTable->item(row, kDiagColMethod)->setText(event.method);
    m_diagnosticsTable->item(row, kDiagColTid)->setText(event.transactionId);
    m_diagnosticsTable->item(row, kDiagColMsgId)->setText(event.messageId);
    m_diagnosticsTable->item(row, kDiagColByteRange)->setText(event.byteRangeText);
    m_diagnosticsTable->item(row, kDiagColContinuation)->setText(QString(continuationChar(event.continuation)));
    m_diagnosticsTable->item(row, kDiagColPreview)->setText(event.bodyPreview);
}
