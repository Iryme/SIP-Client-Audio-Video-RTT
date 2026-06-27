#include "NavRail.h"

#include <QApplication>
#include <QStyle>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

NavRail::NavRail(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("NavRail");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignTop);

    auto *style = qApp ? qApp->style() : nullptr;
    addNavButton(layout, style ? style->standardIcon(QStyle::SP_DesktopIcon)
                               : QIcon(), "Dashboard", "dashboard");
    addNavButton(layout, style ? style->standardIcon(QStyle::SP_DirHomeIcon)
                               : QIcon(), "Clients", "clients");
    addNavButton(layout, style ? style->standardIcon(QStyle::SP_FileDialogDetailedView)
                               : QIcon(), "SIP Ladder", "sipladder");
    addNavButton(layout, style ? style->standardIcon(QStyle::SP_MessageBoxInformation)
                               : QIcon(), "Logs", "logs");
    addNavButton(layout, style ? style->standardIcon(QStyle::SP_FileDialogContentsView)
                               : QIcon(), "Settings", "settings");

    layout->addStretch(1);
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
                                    const QIcon &icon,
                                    const QString &label,
                                    const QString &page)
{
    auto *btn = new QToolButton(this);
    btn->setText(label);
    btn->setIcon(icon);
    btn->setToolTip(label);
    btn->setCheckable(true);
    btn->setAutoExclusive(true);
    btn->setFixedSize(64, 58);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setObjectName("NavButton");

    connect(btn, &QToolButton::clicked, this, [this, page]() {
        emit pageRequested(page);
    });

    layout->addWidget(btn, 0, Qt::AlignHCenter);
    m_buttons.insert(page, btn);
    return btn;
}
