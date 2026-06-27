#include "DashboardRecentEvents.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>

DashboardRecentEvents::DashboardRecentEvents(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DashboardRecentEvents");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *titleLabel = new QLabel(tr("RECENT ACTIVITY"), this);
    titleLabel->setStyleSheet(
        "font-size: 9px; font-weight: 700; color: #3a6090; letter-spacing: 2px; "
        "padding: 14px 16px 8px 16px; background-color: transparent;");
    outer->addWidget(titleLabel);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidgetResizable(true);

    auto *content = new QWidget();
    content->setStyleSheet("background-color: transparent;");
    m_list = new QVBoxLayout(content);
    m_list->setContentsMargins(12, 4, 12, 12);
    m_list->setSpacing(5);
    m_list->addStretch(1);

    scrollArea->setWidget(content);
    outer->addWidget(scrollArea, 1);

    connect(&Logger::instance(), &Logger::entryAdded,
            this, &DashboardRecentEvents::onEntryAdded,
            Qt::UniqueConnection);
}

bool DashboardRecentEvents::isRelevant(const LogEntry &entry)
{
    if (entry.category == LogCategory::Perf ||
        entry.category == LogCategory::Sdp)
        return false;

    const QString &msg = entry.message;

    if (entry.category == LogCategory::App)
        return msg.contains(QLatin1String("starting"), Qt::CaseInsensitive)
            || msg.contains(QLatin1String("initialized"), Qt::CaseInsensitive);

    if (entry.category == LogCategory::Sip) {
        if (msg.startsWith(QLatin1String("TX "))   ||
            msg.startsWith(QLatin1String("RX "))   ||
            msg.startsWith(QLatin1String("pjsip")) ||
            msg.contains(QLatin1String("suppressed"), Qt::CaseInsensitive) ||
            msg.contains(QLatin1String("keepalive"), Qt::CaseInsensitive))
            return false;
        return true;
    }

    if (entry.category == LogCategory::Media)
        return !msg.contains(QLatin1String("retry"), Qt::CaseInsensitive)
            && !msg.contains(QLatin1String("frame"), Qt::CaseInsensitive)
            && !msg.startsWith(QLatin1String("[PERF]"));

    if (entry.category == LogCategory::Rtt)
        return !msg.contains(QLatin1String("empty"), Qt::CaseInsensitive)
            && !msg.contains(QLatin1String("keepalive"), Qt::CaseInsensitive);

    return false;
}

QString DashboardRecentEvents::colorFor(const LogEntry &entry)
{
    if (entry.level == LogLevel::Error) return QStringLiteral("#ef5350");
    if (entry.level == LogLevel::Warn)  return QStringLiteral("#ffa726");
    switch (entry.category) {
    case LogCategory::Sip:   return QStringLiteral("#4fc3f7");
    case LogCategory::Media: return QStringLiteral("#66bb6a");
    case LogCategory::Rtt:   return QStringLiteral("#ba68c8");
    default:                 return QStringLiteral("#546e7a");
    }
}

void DashboardRecentEvents::onEntryAdded(const LogEntry &entry)
{
    if (!isRelevant(entry))
        return;

    // Remove oldest when at capacity (index 0 is newest, stretch is last)
    if (m_count >= kMaxEvents) {
        // stretch is at m_list->count()-1; oldest event is at m_list->count()-2
        const int oldestIdx = m_list->count() - 2;
        if (oldestIdx >= 0) {
            QLayoutItem *item = m_list->takeAt(oldestIdx);
            if (item) {
                delete item->widget();
                delete item;
            }
        }
    } else {
        ++m_count;
    }

    insertEventRow(entry.timestamp.toString(QStringLiteral("hh:mm:ss")),
                   entry.message,
                   colorFor(entry));
}

void DashboardRecentEvents::insertEventRow(const QString &time,
                                           const QString &message,
                                           const QString &color)
{
    auto *row = new QWidget(this);
    row->setStyleSheet(QStringLiteral(
        "QWidget { background-color: #161c28; border-radius: 6px; border: 1px solid #1e2a3a; }"));
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(10, 7, 10, 7);
    lay->setSpacing(9);

    auto *dot = new QLabel(row);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet(QStringLiteral(
        "background-color: %1; border-radius: 4px; border: none;").arg(color));

    auto *timeLabel = new QLabel(time, row);
    timeLabel->setFixedWidth(54);
    timeLabel->setStyleSheet("font-size: 10px; color: #506070; "
                             "background-color: transparent; border: none;");

    auto *msgLabel = new QLabel(message, row);
    msgLabel->setStyleSheet("font-size: 11px; color: #b0bfd0; "
                            "background-color: transparent; border: none;");
    msgLabel->setWordWrap(true);

    lay->addWidget(dot);
    lay->addWidget(timeLabel);
    lay->addWidget(msgLabel, 1);

    // Insert at position 0 (newest at top), before stretch at end
    m_list->insertWidget(0, row);
}
