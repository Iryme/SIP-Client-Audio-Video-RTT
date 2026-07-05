#include "AudioLevelMeter.h"

#include <QDateTime>
#include <QLinearGradient>
#include <QPainter>

AudioLevelMeter::AudioLevelMeter(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(10);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void AudioLevelMeter::setLevel(int level)
{
    const int clamped = qBound(0, level, 100);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (clamped >= m_peak || now - m_peakTs > 1200) {
        m_peak = clamped;
        m_peakTs = now;
    }
    if (clamped == m_level)
        return;
    m_level = clamped;
    update();
}

void AudioLevelMeter::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF groove = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QColor(0x44, 0x44, 0x44));
    p.setBrush(QColor(0x22, 0x22, 0x22));
    p.drawRoundedRect(groove, 3, 3);

    if (m_level > 0) {
        // Gradient spans the full groove; the fill only reveals it up to the
        // current level, so color encodes loudness (green → yellow → red).
        QLinearGradient grad(groove.left(), 0, groove.right(), 0);
        grad.setColorAt(0.0,  QColor(0x2e, 0xb8, 0x5c)); // green
        grad.setColorAt(0.6,  QColor(0xd8, 0xc8, 0x30)); // yellow
        grad.setColorAt(0.85, QColor(0xe0, 0x7a, 0x28)); // orange
        grad.setColorAt(1.0,  QColor(0xe0, 0x40, 0x40)); // red

        QRectF fill = groove.adjusted(1, 1, 0, -1);
        fill.setWidth(qMax(2.0, groove.width() * m_level / 100.0));
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawRoundedRect(fill, 2, 2);
    }

    // Peak marker: thin vertical line at the recent maximum.
    if (m_peak > 2) {
        const qreal x = groove.left() + groove.width() * m_peak / 100.0;
        p.setPen(QPen(QColor(0xdd, 0xdd, 0xdd, 160), 1));
        p.drawLine(QPointF(x, groove.top() + 1), QPointF(x, groove.bottom() - 1));
    }
}
