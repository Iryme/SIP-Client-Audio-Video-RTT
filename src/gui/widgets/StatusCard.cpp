#include "StatusCard.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
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

    // Top row: colored dot + title.
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(4);
    topRow->setContentsMargins(0, 0, 0, 0);

    m_dotLabel = new QLabel(QStringLiteral("\u2022"), this);
    m_dotLabel->setObjectName(QStringLiteral("StatusCardDot"));
    m_dotLabel->setFixedWidth(10);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("StatusCardTitle"));
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    topRow->addWidget(m_dotLabel);
    topRow->addWidget(titleLabel, 1);
    outer->addLayout(topRow);

    // Value row.
    m_valueLabel = new QLabel(QStringLiteral("\u2014"), this);
    m_valueLabel->setObjectName(QStringLiteral("StatusCardValue"));
    m_valueLabel->setWordWrap(false);
    m_valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_valueLabel->setMinimumWidth(0);
    outer->addWidget(m_valueLabel);

    m_valueText = QStringLiteral("\u2014");
    m_valueLabel->setToolTip(m_valueText);
    refreshColors();
    updateElidedValue();
}

void StatusCard::setValue(const QString &value)
{
    m_valueText = value.isEmpty() ? QStringLiteral("\u2014") : value;
    m_valueLabel->setToolTip(m_valueText);
    updateElidedValue();
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

    if (m_status.isEmpty()) {
        m_dotLabel->setStyleSheet({});
        m_valueLabel->setStyleSheet({});
    } else {
        m_dotLabel->setStyleSheet(QStringLiteral("color: %1;").arg(dotColor));
        m_valueLabel->setStyleSheet(QStringLiteral("color: %1;").arg(valColor));
    }

    updateElidedValue();
}

void StatusCard::updateElidedValue()
{
    if (!m_valueLabel)
        return;

    const QString text = m_valueText.isEmpty() ? QStringLiteral("\u2014") : m_valueText;
    const int available = qMax(0, m_valueLabel->contentsRect().width());
    const QString shown = QFontMetrics(m_valueLabel->font()).elidedText(
        text, Qt::ElideRight, available);
    m_valueLabel->setText(shown);
}

void StatusCard::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateElidedValue();
}
