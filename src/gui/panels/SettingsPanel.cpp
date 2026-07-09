#include "SettingsPanel.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "gui/panels/MediaSettingsPanel.h"
#include "gui/panels/SidebarPanel.h"
#include "gui/panels/VideoSettingsPanel.h"
#include "gui/theme/ThemeManager.h"
#include "sip/MessagingEventStore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFont>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SettingsPanel");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);
    auto *title = new QLabel(tr("Settings"), this);
    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    root->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Connection, SIP and appearance preferences are stored locally and restored on restart."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet("color: #b7c4d6;");
    root->addWidget(subtitle);
    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);
    auto *leftWrap = new QWidget(split);
    auto *leftLayout = new QVBoxLayout(leftWrap);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    m_accountsPanel = new SidebarPanel(leftWrap);
    leftLayout->addWidget(m_accountsPanel);
    split->addWidget(leftWrap);
    auto *rightWrap = new QWidget(split);
    auto *rightLayout = new QVBoxLayout(rightWrap);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    m_tabs = new QTabWidget(rightWrap);

    // ── Connection tab ─────────────────────────────────────────────────────
    {
        auto *group = new QGroupBox(tr("Connection"), m_tabs);
        auto *form = new QFormLayout(group);
        form->setLabelAlignment(Qt::AlignLeft);
        form->setFormAlignment(Qt::AlignTop);

        m_serverIp = new QLineEdit(group);
        m_serverIp->setPlaceholderText(tr("10.0.0.1"));
        form->addRow(tr("IP server"), m_serverIp);

        m_sipDomain = new QLineEdit(group);
        m_sipDomain->setPlaceholderText(tr("example.com"));
        form->addRow(tr("SIP domain"), m_sipDomain);

        m_sipPort = new QLineEdit(group);
        m_sipPort->setPlaceholderText(tr("5060"));
        form->addRow(tr("SIP port"), m_sipPort);

        m_wsUrl = new QLineEdit(group);
        m_wsUrl->setPlaceholderText(tr("ws://localhost:8080"));
        form->addRow(tr("WebSocket URL"), m_wsUrl);

        m_debugSIP = new QCheckBox(tr("Enable SIP debug log"), group);
        form->addRow(QString(), m_debugSIP);

        m_rawSIP = new QCheckBox(tr("Enable raw SIP capture"), group);
        form->addRow(QString(), m_rawSIP);

        m_persistMedia = new QCheckBox(tr("Persist media selection locally"), group);
        m_persistMedia->setChecked(true);
        form->addRow(QString(), m_persistMedia);

        m_tabs->addTab(group, tr("Connection"));
    }

    // ── Video tab ──────────────────────────────────────────────────────────
    m_videoSettings = new VideoSettingsPanel(m_tabs);
    m_tabs->addTab(m_videoSettings, tr("Video"));

    // ── Media (audio) tab ──────────────────────────────────────────────────
    m_mediaSettings = new MediaSettingsPanel(m_tabs);
    m_tabs->addTab(m_mediaSettings, tr("Media"));

    // ── Appearance tab ─────────────────────────────────────────────────────
    {
        auto *appearWidget = new QWidget(m_tabs);
        auto *appearLayout = new QVBoxLayout(appearWidget);
        appearLayout->setContentsMargins(12, 12, 12, 12);
        appearLayout->setSpacing(10);

        auto *appearGroup = new QGroupBox(tr("Theme"), appearWidget);
        auto *appearForm = new QFormLayout(appearGroup);
        appearForm->setLabelAlignment(Qt::AlignLeft);

        m_themeCombo = new QComboBox(appearGroup);
        const AppTheme themes[] = {
            AppTheme::Auto, AppTheme::Dark, AppTheme::Light,
            AppTheme::Fluent, AppTheme::Aqua, AppTheme::FusionModern,
            AppTheme::MaterialDark, AppTheme::MaterialLight
        };
        for (AppTheme t : themes)
            m_themeCombo->addItem(appThemeName(t), static_cast<int>(t));

        // Select the current theme
        const int curIdx = static_cast<int>(ThemeManager::instance().currentTheme());
        for (int i = 0; i < m_themeCombo->count(); ++i) {
            if (m_themeCombo->itemData(i).toInt() == curIdx) {
                m_themeCombo->setCurrentIndex(i);
                break;
            }
        }

        appearForm->addRow(tr("Application theme:"), m_themeCombo);

        auto *themeDesc = new QLabel(
            tr("Theme is applied immediately and persisted across restarts.\n"
               "Auto defaults to Dark on all platforms."), appearGroup);
        themeDesc->setWordWrap(true);
        themeDesc->setStyleSheet("color: #8899aa; font-size: 11px;");
        appearForm->addRow(themeDesc);

        appearLayout->addWidget(appearGroup);
        appearLayout->addStretch();

        m_tabs->addTab(appearWidget, tr("Appearance"));
    }

    // â"€â"€ Text / Accessibility tab â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€â"€
    {
        auto *textWidget = new QWidget(m_tabs);
        auto *textLayout = new QVBoxLayout(textWidget);
        textLayout->setContentsMargins(12, 12, 12, 12);
        textLayout->setSpacing(10);
        const QStringList families = QFontDatabase::families();

        auto *rttGroup = new QGroupBox(tr("RTT"), textWidget);
        auto *rttForm = new QFormLayout(rttGroup);
        m_rttFontFamily = new QComboBox(rttGroup);
        m_rttFontFamily->addItems(families);
        m_rttFontSize = new QSpinBox(rttGroup);
        m_rttFontSize->setRange(8, 24);
        m_rttBold = new QCheckBox(tr("Bold"), rttGroup);
        m_rttHighContrast = new QCheckBox(tr("High contrast"), rttGroup);
        rttForm->addRow(tr("Font family:"), m_rttFontFamily);
        rttForm->addRow(tr("Font size:"), m_rttFontSize);
        rttForm->addRow(QString(), m_rttBold);
        rttForm->addRow(QString(), m_rttHighContrast);
        auto *lmpeGroup = new QGroupBox(tr("LMPE"), textWidget);
        auto *lmpeForm = new QFormLayout(lmpeGroup);
        m_lmpeFontFamily = new QComboBox(lmpeGroup);
        m_lmpeFontFamily->addItems(families);
        m_lmpeFontSize = new QSpinBox(lmpeGroup);
        m_lmpeFontSize->setRange(8, 24);
        m_lmpeBold = new QCheckBox(tr("Bold"), lmpeGroup);
        m_lmpeHighContrast = new QCheckBox(tr("High contrast"), lmpeGroup);
        lmpeForm->addRow(tr("Font family:"), m_lmpeFontFamily);
        lmpeForm->addRow(tr("Font size:"), m_lmpeFontSize);
        lmpeForm->addRow(QString(), m_lmpeBold);
        lmpeForm->addRow(QString(), m_lmpeHighContrast);

        textLayout->addWidget(rttGroup);
        textLayout->addWidget(lmpeGroup);

        auto *messagingGroup = new QGroupBox(tr("Messaging Diagnostics"), textWidget);
        auto *messagingForm = new QFormLayout(messagingGroup);
        m_messagingMaxEvents = new QSpinBox(messagingGroup);
        m_messagingMaxEvents->setRange(50, 20000);
        m_messagingMaxEvents->setSingleStep(50);
        messagingForm->addRow(tr("Max events retained:"), m_messagingMaxEvents);
        auto *messagingDesc = new QLabel(
            tr("Diagnostic-only limit for how many Messaging Diagnostics rows are kept in "
               "memory; oldest rows are dropped first once the limit is exceeded."),
            messagingGroup);
        messagingDesc->setWordWrap(true);
        messagingDesc->setStyleSheet("color: #8899aa; font-size: 11px;");
        messagingForm->addRow(messagingDesc);
        textLayout->addWidget(messagingGroup);

        textLayout->addStretch();

        m_tabs->addTab(textWidget, tr("Text / Accessibility"));
    }
    rightLayout->addWidget(m_tabs, 1);
    split->addWidget(rightWrap);

    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    split->setSizes(QList<int>{360, 700});

    root->addWidget(split, 1);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    m_reset = new QPushButton(tr("Reset"), this);
    m_save = new QPushButton(tr("Save"), this);
    buttons->addWidget(m_reset);
    buttons->addWidget(m_save);
    root->addLayout(buttons);

    connect(m_reset, &QPushButton::clicked, this, &SettingsPanel::onLoadDefaults);
    connect(m_save, &QPushButton::clicked, this, &SettingsPanel::onSave);
    connect(m_debugSIP, &QCheckBox::toggled, this, &SettingsPanel::applyDebugToggle);
    connect(m_rawSIP, &QCheckBox::toggled, this, &SettingsPanel::applyDebugToggle);

    // Theme combo: apply immediately on change (no Save required)
    connect(m_themeCombo, &QComboBox::currentIndexChanged,
            this, &SettingsPanel::onThemeChanged);
    load();
}

void SettingsPanel::reload()
{
    load();
}

void SettingsPanel::load()
{
    auto &s = AppSettings::settings();
    m_serverIp->setText(s.value(QStringLiteral("connection/serverIp"), QStringLiteral("")).toString());
    m_sipDomain->setText(s.value(QStringLiteral("connection/sipDomain"), QStringLiteral("")).toString());
    m_sipPort->setText(s.value(QStringLiteral("connection/sipPort"), QStringLiteral("5060")).toString());
    m_wsUrl->setText(s.value(QStringLiteral("connection/wsUrl"), QStringLiteral("")).toString());
    m_debugSIP->setChecked(s.value(QStringLiteral("connection/debugSip"), false).toBool());
    m_rawSIP->setChecked(s.value(QStringLiteral("connection/rawSip"), false).toBool());
    m_persistMedia->setChecked(s.value(QStringLiteral("connection/persistMedia"), true).toBool());
    applyDebugToggle(m_debugSIP->isChecked());
    loadTextAppearance();
    if (m_messagingMaxEvents)
        m_messagingMaxEvents->setValue(AppSettings::loadMaxMessagingEventsRetained());

    // Sync theme combo to whatever is currently active
    if (m_themeCombo) {
        const int curIdx = static_cast<int>(ThemeManager::instance().currentTheme());
        QSignalBlocker blk(m_themeCombo);
        for (int i = 0; i < m_themeCombo->count(); ++i) {
            if (m_themeCombo->itemData(i).toInt() == curIdx) {
                m_themeCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void SettingsPanel::loadTextAppearance()
{
    auto &s = AppSettings::settings();
    const auto load = [&s](const QString &prefix, QComboBox *family, QSpinBox *size,
                           QCheckBox *bold, QCheckBox *contrast) {
        if (family) {
            const QString saved = s.value(prefix + QStringLiteral("/fontFamily"),
                                          family->currentText()).toString();
            const int idx = family->findText(saved);
            family->setCurrentIndex(idx >= 0 ? idx : 0);
        }
        if (size)
            size->setValue(s.value(prefix + QStringLiteral("/fontSize"), 12).toInt());
        if (bold)
            bold->setChecked(s.value(prefix + QStringLiteral("/bold"), false).toBool());
        if (contrast)
            contrast->setChecked(s.value(prefix + QStringLiteral("/highContrast"), false).toBool());
    };

    load(QStringLiteral("text/rtt"), m_rttFontFamily, m_rttFontSize, m_rttBold, m_rttHighContrast);
    load(QStringLiteral("text/lmpe"), m_lmpeFontFamily, m_lmpeFontSize, m_lmpeBold, m_lmpeHighContrast);
}

void SettingsPanel::applyDebugToggle(bool enabled)
{
    Logger::instance().setLevelEnabled(LogLevel::Raw, m_rawSIP->isChecked() || enabled);
}

void SettingsPanel::onLoadDefaults()
{
    m_serverIp->clear();
    m_sipDomain->clear();
    m_sipPort->setText(QStringLiteral("5060"));
    m_wsUrl->clear();
    m_debugSIP->setChecked(false);
    m_rawSIP->setChecked(false);
    m_persistMedia->setChecked(true);
    if (m_rttFontFamily) {
        const int idx = m_rttFontFamily->findText(QStringLiteral("Segoe UI"));
        m_rttFontFamily->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (m_rttFontSize) m_rttFontSize->setValue(12);
    if (m_rttBold) m_rttBold->setChecked(false);
    if (m_rttHighContrast) m_rttHighContrast->setChecked(false);
    if (m_lmpeFontFamily) {
        const int idx = m_lmpeFontFamily->findText(QStringLiteral("Segoe UI"));
        m_lmpeFontFamily->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (m_lmpeFontSize) m_lmpeFontSize->setValue(12);
    if (m_lmpeBold) m_lmpeBold->setChecked(false);
    if (m_lmpeHighContrast) m_lmpeHighContrast->setChecked(false);
    if (m_messagingMaxEvents) m_messagingMaxEvents->setValue(1000);
    applyDebugToggle(false);
}

void SettingsPanel::onSave()
{
    auto &s = AppSettings::settings();
    s.setValue(QStringLiteral("connection/serverIp"), m_serverIp->text().trimmed());
    s.setValue(QStringLiteral("connection/sipDomain"), m_sipDomain->text().trimmed());
    s.setValue(QStringLiteral("connection/sipPort"), m_sipPort->text().trimmed());
    s.setValue(QStringLiteral("connection/wsUrl"), m_wsUrl->text().trimmed());
    s.setValue(QStringLiteral("connection/debugSip"), m_debugSIP->isChecked());
    s.setValue(QStringLiteral("connection/rawSip"), m_rawSIP->isChecked());
    s.setValue(QStringLiteral("connection/persistMedia"), m_persistMedia->isChecked());
    saveTextAppearance();
    if (m_messagingMaxEvents) {
        AppSettings::saveMaxMessagingEventsRetained(m_messagingMaxEvents->value());
        MessagingEventStore::instance().setMaxEventsRetained(m_messagingMaxEvents->value());
    }
    s.sync();
    applyDebugToggle(m_debugSIP->isChecked());
}

void SettingsPanel::saveTextAppearance()
{
    auto &s = AppSettings::settings();
    const auto save = [&s](const QString &prefix, QComboBox *family, QSpinBox *size,
                           QCheckBox *bold, QCheckBox *contrast) {
        if (family)
            s.setValue(prefix + QStringLiteral("/fontFamily"), family->currentText());
        if (size)
            s.setValue(prefix + QStringLiteral("/fontSize"), size->value());
        if (bold)
            s.setValue(prefix + QStringLiteral("/bold"), bold->isChecked());
        if (contrast)
            s.setValue(prefix + QStringLiteral("/highContrast"), contrast->isChecked());
    };

    save(QStringLiteral("text/rtt"), m_rttFontFamily, m_rttFontSize, m_rttBold, m_rttHighContrast);
    save(QStringLiteral("text/lmpe"), m_lmpeFontFamily, m_lmpeFontSize, m_lmpeBold, m_lmpeHighContrast);
}

void SettingsPanel::focusVideoTab()
{
    if (!m_tabs)
        return;
    m_tabs->setCurrentWidget(m_videoSettings);
}

void SettingsPanel::focusMediaTab()
{
    if (!m_tabs)
        return;
    m_tabs->setCurrentWidget(m_mediaSettings);
}

void SettingsPanel::onThemeChanged(int comboIndex)
{
    const int themeIdx = m_themeCombo->itemData(comboIndex).toInt();
    const AppTheme theme = static_cast<AppTheme>(themeIdx);
    // apply() saves to QSettings + calls qApp->setStyleSheet() — no widget rebuilt.
    ThemeManager::instance().apply(theme);
}

