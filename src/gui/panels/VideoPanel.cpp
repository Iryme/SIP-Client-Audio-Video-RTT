#include "VideoPanel.h"
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>

VideoPanel::VideoPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("VideoPanel");
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Remote participant label (bottom-left)
    m_remoteLabel = new QLabel(tr("Remote Video"), this);
    m_remoteLabel->setObjectName("RemoteLabel");
    m_remoteLabel->setStyleSheet(
        "background: rgba(0,0,0,140); color: #ffffff; padding: 4px 8px; border-radius: 4px;");
    m_remoteLabel->adjustSize();

    // Signal indicator (top-right)
    m_signalIndicator = new QLabel(tr("● AUDIO"), this);
    m_signalIndicator->setObjectName("SignalIndicator");
    m_signalIndicator->setStyleSheet(
        "background: rgba(0,0,0,140); color: #50e050; padding: 4px 8px; border-radius: 4px;");
    m_signalIndicator->adjustSize();

    // Local preview (bottom-right, PiP)
    m_localPreview = new QLabel(this);
    m_localPreview->setObjectName("LocalPreview");
    m_localPreview->setStyleSheet(
        "background: #1a2030; border: 1px solid #3a4060; border-radius: 4px;");
    m_localPreview->setAlignment(Qt::AlignCenter);
    m_localPreview->setText(tr("Local\nPreview"));
    m_localPreview->setWordWrap(true);
    m_localPreview->setFixedSize(160, 90);
}

void VideoPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionPreview();

    // Remote label: 8px from bottom-left
    if (m_remoteLabel)
        m_remoteLabel->move(8, height() - m_remoteLabel->height() - 8);

    // Signal indicator: 8px from top-right
    if (m_signalIndicator)
        m_signalIndicator->move(width() - m_signalIndicator->width() - 8, 8);
}

void VideoPanel::repositionPreview()
{
    if (!m_localPreview)
        return;
    // Anchor PiP to bottom-right, 8px margin
    const int margin = 8;
    m_localPreview->move(
        width()  - m_localPreview->width()  - margin,
        height() - m_localPreview->height() - margin);
}

void VideoPanel::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0d, 0x11, 0x1a));

    // Placeholder crosshair when no video
    p.setPen(QColor(0x2a, 0x35, 0x50));
    p.drawLine(width() / 2, 0, width() / 2, height());
    p.drawLine(0, height() / 2, width(), height() / 2);

    p.setPen(QColor(0x3a, 0x45, 0x70));
    const QString placeholder = tr("No video — waiting for call");
    p.setFont(QFont("Sans", 12));
    p.drawText(rect(), Qt::AlignCenter, placeholder);
}
