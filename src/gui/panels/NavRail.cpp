#include "NavRail.h"
#include <QVBoxLayout>
#include <QToolButton>
#include <QSizePolicy>

NavRail::NavRail(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("NavRail");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignTop);

    addNavButton(layout, "Accounts",  "accounts");
    addNavButton(layout, "Contacts",  "contacts");
    addNavButton(layout, "Dialpad",   "dialpad");
    addNavButton(layout, "Settings",  "settings");

    // Non-functional in this release — disabled so users know they are not active.
    addNavButton(layout, "History",   "history");
    addNavButton(layout, "Messages",  "messages");
    setPageEnabled("history",  false);
    setPageEnabled("messages", false);

    layout->addStretch(1);
}

void NavRail::setPageEnabled(const QString &page, bool enabled)
{
    if (auto *btn = m_buttons.value(page, nullptr))
        btn->setEnabled(enabled);
}

QToolButton *NavRail::addNavButton(QVBoxLayout *layout,
                                    const QString &label,
                                    const QString &page)
{
    auto *btn = new QToolButton(this);
    btn->setText(label);
    btn->setToolTip(label);
    btn->setCheckable(true);
    btn->setAutoExclusive(true);
    btn->setFixedSize(56, 56);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setObjectName("NavButton");

    connect(btn, &QToolButton::clicked, this, [this, page]() {
        emit pageRequested(page);
    });

    layout->addWidget(btn, 0, Qt::AlignHCenter);
    m_buttons.insert(page, btn);
    return btn;
}
