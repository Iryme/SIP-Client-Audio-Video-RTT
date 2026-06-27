#include "StatusCard.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

StatusCard::StatusCard(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("StatusCard"));
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(88);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 5, 8, 5);
    outer->setSpacing(2);

    // Top row: colored dot + title
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(4);
    topRow->setContentsMargins(0, 0, 0, 0);

    m_dotLabel = new QLabel(QStringLiteral("●"), this);  // ●
    m_dotLabel->setObjectName(QStringLiteral("StatusCardDot"));
    m_dotLabel->setFixedWidth(10);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("StatusCardTitle"));
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    topRow->addWidget(m_dotLabel);
    topRow->addWidget(titleLabel, 1);
    outer->addLayout(topRow);

    // Value row
    m_valueLabel = new QLabel(QStringLiteral("—"), this);  // —
    m_valueLabel->setObjectName(QStringLiteral("StatusCardValue"));
    m_valueLabel->setWordWrap(false);
    outer->addWidget(m_valueLabel);

    refreshColors();
}

void StatusCard::setValue(const QString &value)
{
    m_valueLabel->setText(value.isEmpty() ? QStringLiteral("—") : value);
}

void StatusCard::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    refreshColors();
}

void StatusCard::setTooltipText(const QString &tip)
{
    setToolTip(tip);
}

void StatusCard::refreshColors()
{
    QString dotColor;
    QString valColor;

    if (m_status == QLatin1String("ok")) {
        dotColor = QStringLiteral("#50c878");
        valColor = QStringLiteral("#50c878");
    } else if (m_status == QLatin1String("warn")) {
        dotColor = QStringLiteral("#e0b850");
        valColor = QStringLiteral("#e0b850");
    } else if (m_status == QLatin1String("err")) {
        dotColor = QStringLiteral("#e05050");
        valColor = QStringLiteral("#e05050");
    } else {
        dotColor = QStringLiteral("#3a4858");
        valColor = QStringLiteral("#7a8aaa");
    }

    m_dotLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 8px;").arg(dotColor));
    m_valueLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; font-weight: 500; font-family: monospace;").arg(valColor));
}
