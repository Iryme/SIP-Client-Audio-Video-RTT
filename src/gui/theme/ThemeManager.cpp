#include "ThemeManager.h"
#include "core/AppSettings.h"

#include <QApplication>
#include <QStyleFactory>

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------

struct Pal {
    // Backgrounds (dark→light)
    const char *bg0;   // main window / panel bg
    const char *bg1;   // secondary panel
    const char *bg2;   // input / card / elevated widget bg
    const char *bg3;   // button / hover bg
    // Borders
    const char *b0;    // subtle dividers
    const char *b1;    // normal borders
    // Text
    const char *t0;    // primary
    const char *t1;    // secondary
    const char *t2;    // muted / disabled
    // Accent
    const char *ac;    // accent foreground
    const char *acBg;  // accent background (selections, badges)
    const char *acTx;  // text drawn on acBg
    // Status
    const char *ok;
    const char *wa;    // warning
    const char *er;    // error
    // Navigation rail
    const char *nav;   // nav rail background
    const char *navH;  // nav hover
    const char *navS;  // nav selected
    const char *nt;    // nav text inactive
    const char *nts;   // nav text selected
    // Status card
    const char *cbg;   // card background
    const char *cbrd;  // card border
    const char *ctit;  // card title color
    // Splitter
    const char *sph;
    // Hangup / Answer button background tints
    const char *hbg;
    const char *abg;
};

// ---------------------------------------------------------------------------
// Theme palettes
// ---------------------------------------------------------------------------

// Dark — matches the original dark_theme.qss
static constexpr Pal kDark = {
    "#13171f", "#111620", "#18202e", "#1e2a3a",
    "#1e2330", "#2a3448",
    "#d4d8e0", "#7090b0", "#404850",
    "#5090d0", "#1a3050", "#c0d8f0",
    "#50c878", "#e0b850", "#e05050",
    "#0c1018", "#1e2a3a", "#1a3050", "#8899aa", "#5090d0",
    "#18202e", "#2a3448", "#7090b0",
    "#1a2030",
    "#3a1010", "#103a10"
};

// Light
static constexpr Pal kLight = {
    "#f5f7fa", "#ffffff", "#eef1f6", "#e0e4ec",
    "#c8d0dc", "#b0bac8",
    "#1a2030", "#405070", "#8090a8",
    "#2060c0", "#dce8f8", "#ffffff",
    "#1a8040", "#a06010", "#c03030",
    "#e4e8f4", "#d4daf0", "#c0cef0", "#5070a0", "#1040c0",
    "#ffffff", "#c0c8d8", "#6080a0",
    "#c0c8d8",
    "#ffe0e0", "#e0ffe8"
};

// Fluent — Windows 11 Fluent Design inspired (light, blue)
static constexpr Pal kFluent = {
    "#f3f4f6", "#ffffff", "#eaedf2", "#dce1ea",
    "#c4c9d8", "#a8b0c0",
    "#202428", "#404c60", "#8090a0",
    "#0067b8", "#cce0f8", "#ffffff",
    "#107c10", "#9a4500", "#a40000",
    "#e8eaed", "#d8dce8", "#cce0f8", "#60708a", "#0067b8",
    "#ffffff", "#c4c9d8", "#607080",
    "#c4c9d8",
    "#ffe8e8", "#e8ffe8"
};

// Aqua — dark with cyan/teal accent
static constexpr Pal kAqua = {
    "#0a1520", "#0f1e28", "#152530", "#1a2e3c",
    "#1a3040", "#254858",
    "#c8e8f0", "#50a8c0", "#2a4858",
    "#20c8e0", "#0a3848", "#001820",
    "#20e080", "#e0c020", "#e04040",
    "#081018", "#152030", "#1a3848", "#3080a0", "#20c8e0",
    "#152530", "#1a3848", "#4090a8",
    "#1a2a38",
    "#3a0a0a", "#0a3a2a"
};

// Fusion Modern — medium dark, purple/violet accent
static constexpr Pal kFusionModern = {
    "#1a1d24", "#1f2330", "#252c3a", "#2c3448",
    "#2e3848", "#3a4860",
    "#dce0e8", "#8090a8", "#4c5868",
    "#6070e0", "#1a2070", "#e0e4ff",
    "#40c060", "#d0a030", "#d04040",
    "#141820", "#1e2538", "#1a2058", "#5868a0", "#8090e0",
    "#252c3a", "#3a4860", "#7080a0",
    "#1e2538",
    "#3a1020", "#102038"
};

// Material Dark — Material You dark palette (M3)
static constexpr Pal kMaterialDark = {
    "#121212", "#1e1e1e", "#242424", "#2c2c2c",
    "#303030", "#404040",
    "#e6e1e5", "#cac4d0", "#605d66",
    "#d0bcff", "#4a3f70", "#ffffff",
    "#03dac6", "#efb340", "#cf6679",
    "#0d0d0d", "#242424", "#2e2640", "#938f99", "#d0bcff",
    "#242424", "#404040", "#938f99",
    "#1e1e1e",
    "#3c1020", "#002a28"
};

// Material Light — Material You light palette (M3)
static constexpr Pal kMaterialLight = {
    "#fffbfe", "#ffffff", "#f4eff4", "#ece6f0",
    "#cac4d0", "#b0a8bc",
    "#1c1b1f", "#49454f", "#79747e",
    "#6750a4", "#e8def8", "#ffffff",
    "#006c4c", "#7d5700", "#b3261e",
    "#f4eff4", "#e8def8", "#d8d0ec", "#49454f", "#6750a4",
    "#ffffff", "#cac4d0", "#79747e",
    "#cac4d0",
    "#ffe8e8", "#e0fff4"
};

// ---------------------------------------------------------------------------
// QSS template
// ---------------------------------------------------------------------------

static const char kQssTemplate[] = R"(
/* SIP Client — dynamic theme */
QWidget {
    background-color: __BG0__;
    color: __T0__;
    font-family: "Segoe UI", "Helvetica Neue", Arial, sans-serif;
    font-size: 12px;
}
QMainWindow { background-color: __BG0__; }

/* --- Menu bar --- */
QMenuBar {
    background-color: __NAV__;
    color: __T0__;
    border-bottom: 1px solid __B0__;
    padding: 2px 0;
}
QMenuBar::item { background: transparent; padding: 4px 10px; border-radius: 4px; }
QMenuBar::item:selected { background-color: __BG2__; }
QMenu {
    background-color: __BG1__;
    border: 1px solid __B1__;
    padding: 4px 0;
}
QMenu::item { padding: 5px 24px; border-radius: 3px; }
QMenu::item:selected { background-color: __ACBG__; color: __ACTX__; }
QMenu::separator { height: 1px; background: __B1__; margin: 4px 0; }

/* --- Navigation rail --- */
#NavRail { background-color: __NAV__; border-right: 1px solid __B0__; }
#NavButton {
    background-color: transparent;
    color: __NT__;
    border: none;
    border-radius: 6px;
    font-size: 9px;
}
#NavButton:hover   { background-color: __NAVH__; color: __T0__; }
#NavButton:checked { background-color: __NAVS__; color: __NTS__; }

/* --- Sidebar --- */
#SidebarPanel { background-color: __BG1__; border-right: 1px solid __B0__; }
#AccountCard  { background-color: __BG2__; border: 1px solid __B1__; border-radius: 6px; }
#AccountName  { color: __T0__; }
#AccountUri   { color: __T1__; }
#RegStatus    { color: __ER__; }
#ContactList {
    background-color: __BG1__;
    border: 1px solid __B0__;
    border-radius: 4px;
    alternate-background-color: __BG0__;
}
#ContactList::item:selected { background-color: __ACBG__; color: __T0__; }
#SearchField {
    background-color: __BG2__;
    border: 1px solid __B1__;
    border-radius: 4px;
    padding: 4px 8px;
    color: __T0__;
}

/* --- Call panel --- */
#CallPanel { background-color: __BG1__; }
#CallCtrlBtn {
    background-color: __BG3__;
    color: __T0__;
    border: 1px solid __B1__;
    border-radius: 5px;
    padding: 4px 10px;
    font-size: 11px;
}
#CallCtrlBtn:hover    { background-color: __BG2__; border-color: __B1__; }
#CallCtrlBtn:checked  { background-color: __ACBG__; color: __AC__; border-color: __AC__; }
#CallCtrlBtn:disabled { color: __T2__; border-color: __B0__; }

QPushButton[callRole="call"] {
    background-color: __ABG__;
    color: __OK__;
    border: 1px solid __OK__;
    border-radius: 5px;
    font-weight: bold;
}
QPushButton[callRole="call"]:hover { background-color: __OK__; color: #ffffff; }
QPushButton[callRole="call"]:disabled {
    background-color: __BG3__;
    color: __T2__;
    border-color: __B0__;
}

QPushButton[callRole="answer"] {
    background-color: __ABG__;
    color: __OK__;
    border: 1px solid __OK__;
    border-radius: 5px;
    font-weight: bold;
}
QPushButton[callRole="answer"]:hover { background-color: __OK__; color: #ffffff; }
QPushButton[callRole="answer"]:disabled {
    background-color: __BG3__;
    color: __T2__;
    border-color: __B0__;
}

#HangupBtn {
    background-color: __HBG__;
    color: __ER__;
    border: 1px solid __ER__;
    border-radius: 5px;
    font-weight: bold;
    min-width: 90px;
}
#HangupBtn:hover { background-color: __ER__; color: #ffffff; }

#AnswerBtn {
    background-color: __ABG__;
    color: __OK__;
    border: 1px solid __OK__;
    border-radius: 5px;
    font-weight: bold;
}
#AnswerBtn:hover { background-color: __OK__; color: #ffffff; }

#RejectBtn {
    background-color: __BG3__;
    color: __WA__;
    border: 1px solid __WA__;
    border-radius: 5px;
}
#RejectBtn:hover { color: #ffffff; background-color: __WA__; }

QPushButton[callRole="reject"],
QPushButton[callRole="hangup"] {
    background-color: __HBG__;
    color: __ER__;
    border: 1px solid __ER__;
    border-radius: 5px;
    font-weight: bold;
}
QPushButton[callRole="reject"]:hover,
QPushButton[callRole="hangup"]:hover { background-color: __ER__; color: #ffffff; }
QPushButton[callRole="reject"]:disabled,
QPushButton[callRole="hangup"]:disabled {
    background-color: __BG3__;
    color: __T2__;
    border-color: __B0__;
}

QPushButton[callRole="hold"] {
    background-color: __BG3__;
    color: __T0__;
    border: 1px solid __B1__;
    border-radius: 5px;
    padding: 4px 10px;
    font-size: 11px;
}
QPushButton[callRole="hold"]:hover { background-color: __BG2__; border-color: __B1__; }
QPushButton[callRole="hold"]:checked {
    background-color: __WA__;
    color: #ffffff;
    border-color: __WA__;
}
QPushButton[callRole="hold"]:disabled {
    background-color: __BG3__;
    color: __T2__;
    border-color: __B0__;
}

QPushButton[callRole="requestVideo"],
QPushButton[callRole="requestRtt"] {
    background-color: __BG3__;
    color: __T0__;
    border: 1px solid __B1__;
    border-radius: 5px;
    padding: 4px 10px;
    font-size: 11px;
}
QPushButton[callRole="requestVideo"]:hover,
QPushButton[callRole="requestRtt"]:hover { background-color: __BG2__; border-color: __B1__; }
QPushButton[callRole="requestVideo"]:checked,
QPushButton[callRole="requestRtt"]:checked {
    background-color: __ACBG__;
    color: __AC__;
    border-color: __AC__;
}
QPushButton[callRole="requestVideo"]:disabled,
QPushButton[callRole="requestRtt"]:disabled {
    background-color: __BG3__;
    color: __T2__;
    border-color: __B0__;
}

#StartVideoBtn, #StopVideoBtn {
    background-color: __BG3__;
    color: __T1__;
    border: 1px solid __B1__;
    border-radius: 5px;
    padding: 4px 8px;
    font-size: 11px;
}
#StartVideoBtn:hover, #StopVideoBtn:hover { background-color: __BG2__; }

/* --- Status cards --- */
#StatusCard {
    background-color: __CBG__;
    border: 1px solid __CBRD__;
    border-radius: 5px;
}
#StatusCardDot {
    color: __T2__;
    font-size: 8px;
}
#StatusCardTitle {
    color: __CTIT__;
    font-size: 9px;
}
#StatusCardValue {
    color: __T1__;
    font-size: 11px;
    font-weight: 500;
    font-family: monospace;
}

/* --- Video panel --- */
#VideoPanel { background-color: __BG0__; }
#RemoteLabel {
    background-color: rgba(0,0,0,0.55);
    color: #e0e8f0;
    border-radius: 4px;
    padding: 3px 8px;
}
#SignalIndicator {
    background-color: rgba(0,0,0,0.55);
    color: __OK__;
    border-radius: 4px;
    padding: 3px 8px;
}
#LocalPreview {
    background-color: __BG2__;
    color: __T1__;
    border: 1px solid __B1__;
    border-radius: 4px;
}

/* --- RTT panel --- */
#RttPanel { background-color: __BG1__; border-left: 1px solid __B0__; }
#RttTabs::pane { border: none; background-color: __BG1__; }
QTabWidget::tab-bar { alignment: left; }
QTabBar::tab {
    background-color: __BG2__;
    color: __T1__;
    border: 1px solid __B1__;
    border-bottom: none;
    padding: 5px 16px;
    border-radius: 4px 4px 0 0;
}
QTabBar::tab:selected {
    background-color: __BG1__;
    color: __T0__;
    border-bottom-color: __BG1__;
}
QTabBar::tab:hover:!selected { background-color: __BG3__; }
#RttState, #LmpeState { color: __T2__; }
#RttRemoteLive {
    background-color: __BG2__;
    border: 1px solid __B0__;
    border-radius: 4px;
    color: __T1__;
    font-style: italic;
}
#RttTranscript, #LmpeList {
    background-color: __BG0__;
    border: 1px solid __B0__;
    border-radius: 4px;
}
#RttInput, #LmpeInput {
    background-color: __BG2__;
    border: 1px solid __B1__;
    border-radius: 4px;
    padding: 4px 8px;
    color: __T0__;
}
#SendBtn {
    background-color: __ACBG__;
    color: __AC__;
    border: 1px solid __AC__;
    border-radius: 4px;
    padding: 4px 16px;
    font-weight: bold;
}
#SendBtn:hover { background-color: __BG3__; }

/* --- Diagnostics panel --- */
#DiagnosticsPanel { background-color: __BG0__; border-top: 1px solid __B0__; }
#LogLevelBtn {
    background-color: __BG2__;
    color: __T1__;
    border: 1px solid __B0__;
    border-radius: 3px;
    padding: 2px 8px;
    font-size: 10px;
    font-weight: bold;
}
#LogLevelBtn:checked { background-color: __ACBG__; color: __AC__; border-color: __AC__; }
#LogLevelBtn:hover   { background-color: __BG3__; }

/* --- Tables --- */
QTableWidget {
    background-color: __BG0__;
    alternate-background-color: __BG1__;
    gridline-color: __B0__;
    border: 1px solid __B0__;
    selection-background-color: __ACBG__;
    selection-color: __T0__;
    font-size: 11px;
}
QHeaderView::section {
    background-color: __BG2__;
    color: __T1__;
    border: none;
    border-right: 1px solid __B0__;
    border-bottom: 1px solid __B0__;
    padding: 3px 6px;
    font-size: 11px;
}

/* --- Status bar --- */
#AppStatusBar { background-color: __NAV__; border-top: 1px solid __B0__; }

/* --- Splitter --- */
QSplitter::handle { background-color: __SPH__; }
QSplitter::handle:horizontal { width: 4px; }
QSplitter::handle:vertical   { height: 4px; }
QSplitter::handle:hover      { background-color: __ACBG__; }

/* --- Scrollbars --- */
QScrollBar:vertical   { background: __BG1__; width: 8px; border: none; }
QScrollBar:horizontal { background: __BG1__; height: 8px; border: none; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: __B1__; border-radius: 4px;
}
QScrollBar::handle:vertical   { min-height: 24px; }
QScrollBar::handle:horizontal { min-width: 24px; }
QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background: __AC__; }
QScrollBar::add-line:vertical,  QScrollBar::sub-line:vertical  { height: 0; }
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{ width: 0; }

/* --- Buttons --- */
QPushButton {
    background-color: __BG3__;
    color: __T0__;
    border: 1px solid __B1__;
    border-radius: 4px;
    padding: 4px 12px;
}
QPushButton:hover    { background-color: __BG2__; }
QPushButton:pressed  { background-color: __BG1__; }
QPushButton:disabled { color: __T2__; border-color: __B0__; }

/* --- Inputs --- */
QLineEdit {
    background-color: __BG2__;
    color: __T0__;
    border: 1px solid __B1__;
    border-radius: 4px;
    padding: 4px 8px;
    selection-background-color: __ACBG__;
}
QLineEdit:focus { border-color: __AC__; }

QTextEdit {
    background-color: __BG1__;
    color: __T0__;
    border: 1px solid __B0__;
    border-radius: 4px;
    selection-background-color: __ACBG__;
}

QPlainTextEdit {
    background-color: __BG1__;
    color: __T0__;
    border: 1px solid __B0__;
    border-radius: 4px;
}

QComboBox {
    background-color: __BG2__;
    color: __T0__;
    border: 1px solid __B1__;
    border-radius: 4px;
    padding: 2px 8px;
}
QComboBox:hover { border-color: __AC__; }
QComboBox::drop-down { border: none; }
QComboBox QAbstractItemView {
    background-color: __BG2__;
    color: __T0__;
    border: 1px solid __B1__;
    selection-background-color: __ACBG__;
}

QCheckBox { color: __T0__; spacing: 6px; }
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid __B1__; border-radius: 3px; background: __BG2__;
}
QCheckBox::indicator:checked { background: __AC__; border-color: __AC__; }

/* --- Group box --- */
QGroupBox {
    color: __T0__;
    border: 1px solid __B1__;
    border-radius: 4px;
    margin-top: 8px;
    padding-top: 8px;
}
QGroupBox::title { subcontrol-origin: margin; left: 8px; color: __T1__; }

/* --- Misc frames --- */
QFrame[frameShape="4"],
QFrame[frameShape="5"] { color: __B0__; }

/* --- Progress bars (audio meters) --- */
QProgressBar {
    border: 1px solid __B1__;
    border-radius: 3px;
    background: __BG2__;
    text-align: center;
    color: __T0__;
}
QProgressBar::chunk { background: __AC__; border-radius: 2px; }

/* --- Tooltips --- */
QToolTip {
    background-color: __BG3__;
    color: __T0__;
    border: 1px solid __B1__;
    padding: 4px;
}
)";

// ---------------------------------------------------------------------------
// Build QSS from palette
// ---------------------------------------------------------------------------

static QString buildQssFromPal(const Pal &p)
{
    QString s = QLatin1String(kQssTemplate);

    auto repl = [&](const char *token, const char *value) {
        s.replace(QLatin1String(token), QLatin1String(value));
    };

    repl("__BG0__",  p.bg0);
    repl("__BG1__",  p.bg1);
    repl("__BG2__",  p.bg2);
    repl("__BG3__",  p.bg3);
    repl("__B0__",   p.b0);
    repl("__B1__",   p.b1);
    repl("__T0__",   p.t0);
    repl("__T1__",   p.t1);
    repl("__T2__",   p.t2);
    repl("__AC__",   p.ac);
    repl("__ACBG__", p.acBg);
    repl("__ACTX__", p.acTx);
    repl("__OK__",   p.ok);
    repl("__WA__",   p.wa);
    repl("__ER__",   p.er);
    repl("__NAV__",  p.nav);
    repl("__NAVH__", p.navH);
    repl("__NAVS__", p.navS);
    repl("__NT__",   p.nt);
    repl("__NTS__",  p.nts);
    repl("__CBG__",  p.cbg);
    repl("__CBRD__", p.cbrd);
    repl("__CTIT__", p.ctit);
    repl("__SPH__",  p.sph);
    repl("__HBG__",  p.hbg);
    repl("__ABG__",  p.abg);

    return s;
}

// ---------------------------------------------------------------------------
// ThemeManager
// ---------------------------------------------------------------------------

ThemeManager &ThemeManager::instance()
{
    static ThemeManager s_instance;
    return s_instance;
}

ThemeManager::ThemeManager() = default;

QString ThemeManager::buildQss(AppTheme theme)
{
    switch (theme) {
    case AppTheme::Auto:          return buildQssFromPal(kDark);
    case AppTheme::Dark:          return buildQssFromPal(kDark);
    case AppTheme::Light:         return buildQssFromPal(kLight);
    case AppTheme::Fluent:        return buildQssFromPal(kFluent);
    case AppTheme::Aqua:          return buildQssFromPal(kAqua);
    case AppTheme::FusionModern:  return buildQssFromPal(kFusionModern);
    case AppTheme::MaterialDark:  return buildQssFromPal(kMaterialDark);
    case AppTheme::MaterialLight: return buildQssFromPal(kMaterialLight);
    }
    return buildQssFromPal(kDark);
}

void ThemeManager::applyFromSettings()
{
    const int idx = AppSettings::savedThemeIndex();
    AppTheme theme = AppTheme::Auto;
    if (idx > 0 && idx <= static_cast<int>(AppTheme::MaterialLight))
        theme = static_cast<AppTheme>(idx);
    apply(theme);
}

void ThemeManager::apply(AppTheme theme)
{
    m_current = theme;
    AppSettings::saveThemeIndex(static_cast<int>(theme));

    // Switch to Fusion style for consistent cross-platform rendering.
    // Safe at runtime — Fusion does not rebuild any widget tree.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    qApp->setStyleSheet(buildQss(theme));

    emit themeChanged(theme);
}
