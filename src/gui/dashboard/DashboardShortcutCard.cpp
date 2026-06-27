#include "DashboardShortcutCard.h"

#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

DashboardShortcutCard::DashboardShortcutCard(const QString &icon,
                                             const QString &title,
                                             const QString &description,
                                             const QString &page,
                                             QWidget *parent)
    : QFrame(parent), m_page(page)
{
    setObjectName("ShortcutCard");
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(130);
    setFixedHeight(128);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet(kStyleNormal);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 14, 16, 12);
    lay->setSpacing(3);

    auto *iconLabel = new QLabel(icon, this);
    iconLabel->setStyleSheet("font-size: 24px; background-color: transparent; border: none;");
    iconLabel->setAlignment(Qt::AlignLeft);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #d4d8e0; "
                              "background-color: transparent; border: none;");

    auto *descLabel = new QLabel(description, this);
    descLabel->setStyleSheet("font-size: 10px; color: #6a7a9a; "
                             "background-color: transparent; border: none;");
    descLabel->setWordWrap(true);

    m_statusLabel = new QLabel(QStringLiteral("—"), this);
    m_statusLabel->setStyleSheet("font-size: 10px; color: #6a7a9a; "
                                 "background-color: transparent; border: none;");

    lay->addWidget(iconLabel);
    lay->addWidget(titleLabel);
    lay->addWidget(descLabel, 1);
    lay->addWidget(m_statusLabel);
}

void DashboardShortcutCard::setStatus(const QString &text, const QString &color)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(
        QStringLiteral("font-size: 10px; color: %1; background-color: transparent; border: none;")
            .arg(color));
}

void DashboardShortcutCard::mousePressEvent(QMouseEvent *event)
{
    QFrame::mousePressEvent(event);
    if (event->button() == Qt::LeftButton)
        emit navigateTo(m_page);
}

void DashboardShortcutCard::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
    setStyleSheet(kStyleHover);
}

void DashboardShortcutCard::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    setStyleSheet(kStyleNormal);
}
