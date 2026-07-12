#include "NavRail.h"

#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QToolButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// SVG icon strings — modern, minimal, theme-aware line icons
// ---------------------------------------------------------------------------

static const QByteArray kIconDashboard = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="7" height="7" rx="1.5"/>
  <rect x="14" y="3" width="7" height="7" rx="1.5"/>
  <rect x="3" y="14" width="7" height="7" rx="1.5"/>
  <rect x="14" y="14" width="7" height="7" rx="1.5"/>
</svg>)";

static const QByteArray kIconClients = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <path d="M6.6 10.8c1-1.8 2.8-3 4.8-3s3.8 1.2 4.8 3"/>
  <path d="M3 17c0-2.8 2.2-5 5-5h8c2.8 0 5 2.2 5 5"/>
  <circle cx="12" cy="7" r="3"/>
</svg>)";

static const QByteArray kIconSipLadder = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <line x1="5" y1="3" x2="5" y2="21"/>
  <line x1="19" y1="3" x2="19" y2="21"/>
  <line x1="5" y1="8" x2="19" y2="8"/>
  <polyline points="15,5 19,8 15,11"/>
  <line x1="19" y1="16" x2="5" y2="16"/>
  <polyline points="9,13 5,16 9,19"/>
</svg>)";

static const QByteArray kIconMessaging = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <path d="M4 5h16v11H8l-4 4V5z"/>
  <line x1="8" y1="9" x2="16" y2="9"/>
  <line x1="8" y1="13" x2="13" y2="13"/>
</svg>)";

static const QByteArray kIconHistory = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="12" cy="13" r="8"/>
  <polyline points="12,9 12,13 15,15"/>
  <path d="M5 3 2 6"/>
  <path d="M19 3l3 3"/>
</svg>)";

static const QByteArray kIconLogs = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <rect x="4" y="3" width="16" height="18" rx="2"/>
  <line x1="8" y1="8" x2="16" y2="8"/>
  <line x1="8" y1="12" x2="16" y2="12"/>
  <line x1="8" y1="16" x2="12" y2="16"/>
</svg>)";

static const QByteArray kIconDiagnostics = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <path d="M3 12h4l2 7 4-14 2 7h6"/>
</svg>)";

static const QByteArray kIconSettings = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="12" cy="12" r="3"/>
  <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06
           a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09
           A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83
           l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09
           A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83
           l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09
           a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83
           l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09
           a1.65 1.65 0 0 0-1.51 1z"/>
</svg>)";

static const QByteArray kIconConfig = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/>
  <polyline points="7 10 12 15 17 10"/>
  <line x1="12" y1="15" x2="12" y2="3"/>
</svg>)";

static const QByteArray kIconHelp = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="12" cy="12" r="9"/>
  <path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"/>
  <line x1="12" y1="17" x2="12.01" y2="17"/>
</svg>)";

static const QByteArray kIconExit = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
  <path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/>
  <polyline points="16 17 21 12 16 7"/>
  <line x1="21" y1="12" x2="9" y2="12"/>
</svg>)";

// ---------------------------------------------------------------------------
// Icon rendering
// ---------------------------------------------------------------------------

QIcon NavRail::makeIconFromSvg(const QByteArray &svgData, int size)
{
    // Resolve currentColor to the navigation text color used in the QSS.
    // We inject a fixed neutral gray that is legible on both light and dark
    // NavRail backgrounds across all built-in themes.
    QByteArray patched = svgData;
    patched.replace("currentColor", "#8899aa");

    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QSvgRenderer renderer(patched);
    renderer.render(&p);
    p.end();

    // Also create a "selected" variant with a lighter/accent color.
    QByteArray patchedSelected = svgData;
    patchedSelected.replace("currentColor", "#5090d0");

    QPixmap pixSel(size, size);
    pixSel.fill(Qt::transparent);
    QPainter ps(&pixSel);
    ps.setRenderHint(QPainter::Antialiasing);
    QSvgRenderer rendererSel(patchedSelected);
    rendererSel.render(&ps);
    ps.end();

    QIcon icon;
    icon.addPixmap(pix,    QIcon::Normal,   QIcon::Off);
    icon.addPixmap(pixSel, QIcon::Normal,   QIcon::On);
    icon.addPixmap(pixSel, QIcon::Selected, QIcon::On);
    return icon;
}

// ---------------------------------------------------------------------------
// NavRail
// ---------------------------------------------------------------------------

NavRail::NavRail(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("NavRail");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(2);
    layout->setAlignment(Qt::AlignTop);

    addNavButton(layout, QString{}, "Dashboard", "dashboard");
    addNavButton(layout, QString{}, "Clients",   "clients");
    addNavButton(layout, QString{}, "SIP Ladder","sipladder");
    addNavButton(layout, QString{}, "Messaging", "messaging");
    addNavButton(layout, QString{}, "Presence",  "presence");
    addNavButton(layout, QString{}, "XCAP",      "xcap");
    addNavButton(layout, QString{}, "MSRP",      "msrp");
    addNavButton(layout, QString{}, "History",   "callhistory");
    addNavButton(layout, QString{}, "Logs",      "logs");
    addNavButton(layout, QString{}, "Diagnostics", "diagnostics");
    addNavButton(layout, QString{}, "Settings",  "settings");

    // Push bottom actions to the bottom
    layout->addStretch(1);

    // --- Bottom actions: Import/Export Config, Help, Exit ---
    auto *configBtn = addActionButton(layout, {}, "Config", "NavActionConfig");
    connect(configBtn, &QToolButton::clicked, this, [this, configBtn]() {
        auto *menu = new QMenu(configBtn);
        menu->addAction(tr("Import configuration…"), this, &NavRail::importConfigRequested);
        menu->addAction(tr("Export configuration…"), this, &NavRail::exportConfigRequested);
        menu->exec(configBtn->mapToGlobal(configBtn->rect().topRight()));
    });

    auto *helpBtn = addActionButton(layout, {}, "Help", "NavActionHelp");
    connect(helpBtn, &QToolButton::clicked, this, [this, helpBtn]() {
        auto *menu = new QMenu(helpBtn);
        menu->addAction(tr("About…"),            this, &NavRail::aboutRequested);
        menu->addAction(tr("Diagnostics info…"), this, &NavRail::helpRequested);
        menu->exec(helpBtn->mapToGlobal(helpBtn->rect().topRight()));
    });

    auto *exitBtn = addActionButton(layout, {}, "Exit", "NavActionExit");
    exitBtn->setObjectName("NavActionExit");
    connect(exitBtn, &QToolButton::clicked, this, &NavRail::exitRequested);
}

void NavRail::setPageEnabled(const QString &page, bool enabled)
{
    if (auto *btn = m_buttons.value(page, nullptr))
        btn->setEnabled(enabled);
}

void NavRail::setPageActive(const QString &page)
{
    m_activePage = page;
    for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it)
        it.value()->setChecked(it.key() == page);
}

QToolButton *NavRail::addNavButton(QVBoxLayout *layout,
                                    const QString & /*svgPath*/,
                                    const QString &label,
                                    const QString &page)
{
    // Choose icon by page id
    QByteArray svgData;
    if (page == QLatin1String("dashboard"))  svgData = kIconDashboard;
    else if (page == QLatin1String("clients")) svgData = kIconClients;
    else if (page == QLatin1String("sipladder")) svgData = kIconSipLadder;
    else if (page == QLatin1String("messaging")) svgData = kIconMessaging;
    else if (page == QLatin1String("presence")) svgData = kIconDiagnostics; // reused icon; no dedicated Presence glyph yet
    else if (page == QLatin1String("xcap")) svgData = kIconConfig; // reused icon; no dedicated XCAP glyph yet
    else if (page == QLatin1String("msrp")) svgData = kIconMessaging; // reused icon; no dedicated MSRP glyph yet
    else if (page == QLatin1String("callhistory")) svgData = kIconHistory;
    else if (page == QLatin1String("logs"))   svgData = kIconLogs;
    else if (page == QLatin1String("diagnostics")) svgData = kIconDiagnostics;
    else if (page == QLatin1String("settings")) svgData = kIconSettings;

    auto *btn = new QToolButton(this);
    btn->setText(label);
    btn->setIcon(makeIconFromSvg(svgData, 28));
    btn->setToolTip(label);
    btn->setCheckable(true);
    btn->setAutoExclusive(true);
    btn->setFixedSize(64, 58);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setObjectName("NavButton");
    btn->setIconSize(QSize(28, 28));

    connect(btn, &QToolButton::clicked, this, [this, page]() {
        emit pageRequested(page);
    });

    layout->addWidget(btn, 0, Qt::AlignHCenter);
    m_buttons.insert(page, btn);
    return btn;
}

QToolButton *NavRail::addActionButton(QVBoxLayout *layout,
                                       const QString & /*svgPath*/,
                                       const QString &label,
                                       const QString &objectName)
{
    QByteArray svgData;
    if (objectName == QLatin1String("NavActionConfig")) svgData = kIconConfig;
    else if (objectName == QLatin1String("NavActionHelp")) svgData = kIconHelp;
    else if (objectName == QLatin1String("NavActionExit")) svgData = kIconExit;

    auto *btn = new QToolButton(this);
    btn->setText(label);
    btn->setIcon(makeIconFromSvg(svgData, 22));
    btn->setToolTip(label);
    btn->setFixedSize(64, 50);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setObjectName(objectName);
    btn->setIconSize(QSize(22, 22));

    layout->addWidget(btn, 0, Qt::AlignHCenter);
    return btn;
}
