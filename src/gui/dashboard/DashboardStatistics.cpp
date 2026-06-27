#include "DashboardStatistics.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

DashboardStatistics::DashboardStatistics(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DashboardStatistics");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *titleLabel = new QLabel(tr("STATISTICS"), this);
    titleLabel->setStyleSheet(
        "font-size: 9px; font-weight: 700; color: #3a6090; letter-spacing: 2px; "
        "padding: 14px 20px 8px 20px; background-color: transparent;");
    outer->addWidget(titleLabel);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidgetResizable(true);

    auto *content = new QWidget();
    content->setStyleSheet("background-color: transparent;");

    m_grid = new QGridLayout(content);
    m_grid->setContentsMargins(20, 4, 20, 20);
    m_grid->setHorizontalSpacing(16);
    m_grid->setVerticalSpacing(5);
    m_grid->setColumnStretch(1, 1);

    scrollArea->setWidget(content);
    outer->addWidget(scrollArea, 1);
}

void DashboardStatistics::addSection(const QString &title)
{
    if (m_row > 0) {
        auto *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        sep->setStyleSheet("background-color: #1e2a3a; border: none;");
        m_grid->addWidget(sep, m_row++, 0, 1, 2);
    }

    auto *lbl = new QLabel(title, this);
    lbl->setStyleSheet(
        "font-size: 9px; font-weight: 700; color: #3a6090; letter-spacing: 2px; "
        "padding-top: 10px; padding-bottom: 2px; background-color: transparent;");
    m_grid->addWidget(lbl, m_row++, 0, 1, 2);
}

void DashboardStatistics::addStat(const QString &key, const QString &label)
{
    auto *keyLabel = new QLabel(label, this);
    keyLabel->setStyleSheet("font-size: 11px; color: #6a7a9a; background-color: transparent;");
    keyLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    auto *valLabel = new QLabel(QStringLiteral("—"), this);
    valLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #c0ccd8; "
                            "background-color: transparent;");
    valLabel->setWordWrap(true);
    valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_grid->addWidget(keyLabel, m_row,   0, Qt::AlignLeft | Qt::AlignVCenter);
    m_grid->addWidget(valLabel, m_row,   1, Qt::AlignRight | Qt::AlignVCenter);
    m_valueLabels[key] = valLabel;
    ++m_row;
}

void DashboardStatistics::setValue(const QString &key, const QString &value)
{
    if (auto *lbl = m_valueLabels.value(key, nullptr))
        lbl->setText(value.isEmpty() ? QStringLiteral("—") : value);
}
