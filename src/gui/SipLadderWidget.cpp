#include "SipLadderWidget.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QToolTip>
#include <QFontMetrics>

namespace {
static QString normalizedKey(const QString &value)
{
    return value.trimmed().toLower();
}

// Short badge shown under MESSAGE / messaging-related rows so the SIP Ladder
// surfaces CPIM / IMDN / is-composing / MSRP-SDP content at a glance without
// needing to open the details dialog. Header-string / body-substring based
// only — matches the lightweight detection style already used elsewhere for
// the ladder (no dependency on the Messaging Diagnostics store).
static QString contentTypeTag(const SipMessageTrace &t)
{
    const QString ct = t.contentType.trimmed().toLower();
    if (ct.startsWith(QStringLiteral("message/cpim")))
        return QStringLiteral("CPIM");
    if (ct.startsWith(QStringLiteral("message/imdn+xml")))
        return QStringLiteral("IMDN");
    if (ct.startsWith(QStringLiteral("application/im-iscomposing+xml")))
        return QStringLiteral("is-composing");
    if (t.rawSip.contains(QStringLiteral("m=message")))
        return QStringLiteral("MSRP-SDP");
    return QString();
}
}

SipLadderWidget::SipLadderWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(contentHeight());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

int SipLadderWidget::contentHeight() const
{
    return kHeaderH + visibleTraceIndices().size() * kRowHeight + kPadBottom;
}

QSize SipLadderWidget::sizeHint() const
{
    return QSize(420, contentHeight());
}

QSize SipLadderWidget::minimumSizeHint() const
{
    return QSize(260, contentHeight());
}

void SipLadderWidget::onMessageLogged(const SipMessageTrace &trace)
{
    m_traces.append(trace);
    setMinimumHeight(contentHeight());
    updateGeometry();
    update();
}

void SipLadderWidget::onCleared()
{
    m_traces.clear();
    m_rowRects.clear();
    m_hoverIndex = -1;
    QToolTip::hideText();
    setMinimumHeight(contentHeight());
    updateGeometry();
    update();
}

void SipLadderWidget::setCallIdFilter(const QString &callId)
{
    m_filterCallId = callId;
    setMinimumHeight(contentHeight());
    updateGeometry();
    update();
}

void SipLadderWidget::setMethodFilter(const QString &method)
{
    m_filterMethod = method;
    setMinimumHeight(contentHeight());
    updateGeometry();
    update();
}

void SipLadderWidget::setDirectionFilter(const QString &direction)
{
    m_filterDirection = direction;
    setMinimumHeight(contentHeight());
    updateGeometry();
    update();
}

QList<int> SipLadderWidget::visibleTraceIndices() const
{
    QList<int> indices;
    const QString callIdFilter = normalizedKey(m_filterCallId);
    const QString methodFilter = normalizedKey(m_filterMethod);
    const QString directionFilter = normalizedKey(m_filterDirection);

    for (int i = 0; i < m_traces.size(); ++i) {
        const SipMessageTrace &t = m_traces[i];

        if (!callIdFilter.isEmpty()
            && !normalizedKey(t.callId).contains(callIdFilter))
            continue;

        if (!methodFilter.isEmpty()) {
            const QString traceMethod = normalizedKey(t.statusCode > 0
                ? QStringLiteral("%1 %2").arg(t.statusCode).arg(t.statusText)
                : t.method);
            if (!traceMethod.contains(methodFilter))
                continue;
        }

        if (!directionFilter.isEmpty()) {
            const QString traceDirection = t.direction == SipMessageTrace::Direction::Outbound
                ? QStringLiteral("outbound") : QStringLiteral("inbound");
            if (!traceDirection.contains(directionFilter))
                continue;
        }

        indices.append(i);
    }
    return indices;
}

int SipLadderWidget::traceIndexAt(const QPoint &pos) const
{
    for (int i = 0; i < m_rowRects.size(); ++i) {
        if (m_rowRects[i].contains(pos))
            return i;
    }
    return -1;
}

QString SipLadderWidget::tooltipForTrace(const SipMessageTrace &trace)
{
    const QString direction = trace.direction == SipMessageTrace::Direction::Outbound
        ? QObject::tr("Outbound")
        : QObject::tr("Inbound");
    const QString method = trace.statusCode > 0
        ? QStringLiteral("%1 %2").arg(trace.statusCode).arg(trace.statusText)
        : trace.method;
    const QString timestamp = trace.timestamp.isValid()
        ? trace.timestamp.toString(Qt::ISODateWithMs)
        : QObject::tr("Unknown");

    QStringList parts;
    parts << QStringLiteral("Direction: %1").arg(direction);
    parts << QStringLiteral("Method/Status: %1").arg(method);
    parts << QStringLiteral("From: %1").arg(trace.fromUri.isEmpty() ? QObject::tr("Not available") : trace.fromUri);
    parts << QStringLiteral("To: %1").arg(trace.toUri.isEmpty() ? QObject::tr("Not available") : trace.toUri);
    parts << QStringLiteral("Call-ID: %1").arg(trace.callId.isEmpty() ? QObject::tr("Not available") : trace.callId);
    parts << QStringLiteral("CSeq: %1").arg(trace.cSeq.isEmpty() ? QObject::tr("Not available") : trace.cSeq);
    parts << QStringLiteral("Timestamp: %1").arg(timestamp);
    return parts.join('\n');
}

void SipLadderWidget::updateHoverTip(const QPoint &pos)
{
    const int index = traceIndexAt(pos);
    if (index < 0 || index >= m_rowRects.size()) {
        if (m_hoverIndex != -1) {
            m_hoverIndex = -1;
            QToolTip::hideText();
        }
        return;
    }

    if (m_hoverIndex == index)
        return;

    m_hoverIndex = index;
    const QList<int> visible = visibleTraceIndices();
    if (index < 0 || index >= visible.size())
        return;

    const SipMessageTrace &trace = m_traces[visible[index]];
    QToolTip::showText(mapToGlobal(pos), tooltipForTrace(trace), this);
}

void SipLadderWidget::mouseMoveEvent(QMouseEvent *event)
{
    updateHoverTip(event->pos());
    QWidget::mouseMoveEvent(event);
}

void SipLadderWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hoverIndex = -1;
    QToolTip::hideText();
}

void SipLadderWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const int index = traceIndexAt(event->pos());
    if (index < 0)
        return;

    const QList<int> visible = visibleTraceIndices();
    if (index >= visible.size())
        return;

    emit traceActivated(m_traces[visible[index]]);
}

void SipLadderWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const int W = width();
    const QList<int> visible = visibleTraceIndices();
    m_rowRects.clear();
    m_rowRects.reserve(visible.size());

    // Entity column centres
    const int x1 = W * 27 / 100;   // Local UA
    const int x2 = W * 73 / 100;   // Remote (registrar / peer)

    // Background
    p.fillRect(rect(), QColor("#1e1e1e"));

    // Entity header boxes
    QFont boldFont = p.font();
    boldFont.setBold(true);
    const qreal basePt = boldFont.pointSizeF();
    if (basePt > 0)
        boldFont.setPointSizeF(basePt * 0.88);
    p.setFont(boldFont);

    auto drawEntity = [&](int cx, const QString &label) {
        const int bw = 100, bh = 26;
        const QRect box(cx - bw / 2, 12, bw, bh);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#2a5c8a"));
        p.drawRoundedRect(box, 5, 5);
        p.setPen(QColor("#c8e0ff"));
        p.drawText(box, Qt::AlignCenter, label);
    };
    drawEntity(x1, tr("Local (UA)"));
    drawEntity(x2, tr("Remote"));

    // Vertical timeline dashed lines
    const int lineTop = 38 + 12;
    const int lineBot = kHeaderH + visible.size() * kRowHeight;
    QPen dashPen(QColor("#2e2e2e"), 1, Qt::DashLine);
    p.setPen(dashPen);
    p.drawLine(x1, lineTop, x1, lineBot);
    p.drawLine(x2, lineTop, x2, lineBot);

    if (visible.isEmpty()) {
        p.setPen(QColor("#555555"));
        QFont ph = p.font();
        ph.setBold(false);
        p.setFont(ph);
        p.drawText(QRect(0, kHeaderH, W, 80), Qt::AlignCenter,
                   tr("No SIP messages captured.\n"
                      "Messages appear here when SIP activity occurs."));
        return;
    }

    // Per-row font
    QFont rowFont = p.font();
    rowFont.setBold(false);
    const qreal rowPt = rowFont.pointSizeF();
    if (rowPt > 0)
        rowFont.setPointSizeF(rowPt * 0.86);
    p.setFont(rowFont);
    const QFontMetrics fm(rowFont);

    QFont annFont = rowFont;
    const qreal annPt = annFont.pointSizeF();
    if (annPt > 0)
        annFont.setPointSizeF(annPt * 0.82);
    const QFontMetrics afm(annFont);

    for (int row = 0; row < visible.size(); ++row) {
        const SipMessageTrace &t = m_traces[visible[row]];
        const int rowMid = kHeaderH + row * kRowHeight + kRowHeight / 2;
        const QRect rowRect(0, kHeaderH + row * kRowHeight, W, kRowHeight);
        m_rowRects.append(rowRect);

        const bool outbound = (t.direction == SipMessageTrace::Direction::Outbound);
        const int srcX = outbound ? x1 : x2;
        const int dstX = outbound ? x2 : x1;

        const QColor clr = colorForTrace(t);

        // Timestamp (left margin)
        p.setFont(annFont);
        p.setPen(QColor("#686868"));
        const QString ts = t.timestamp.toString(QStringLiteral("hh:mm:ss.zzz"));
        p.drawText(4, rowMid - 10, ts);
        p.setFont(rowFont);

        // Arrow shaft
        const int arrowY = rowMid + 6;
        p.setPen(QPen(clr, 1.5));
        p.drawLine(srcX, arrowY, dstX, arrowY);

        // Arrowhead triangle
        const int dir = outbound ? 1 : -1;
        const int tipX = dstX;
        QPainterPath head;
        head.moveTo(tipX, arrowY);
        head.lineTo(tipX - dir * 10, arrowY - 4);
        head.lineTo(tipX - dir * 10, arrowY + 4);
        head.closeSubpath();
        p.setPen(Qt::NoPen);
        p.fillPath(head, clr);

        // Method / status label above the shaft
        const QString label = t.summary();
        const int labelW = fm.horizontalAdvance(label);
        const int midX   = (srcX + dstX) / 2;
        p.setPen(clr);
        p.setFont(rowFont);
        p.drawText(midX - labelW / 2, rowMid + 3, label);

        // CSeq / Call-ID annotation below shaft (smaller, muted)
        const QString tag = contentTypeTag(t);
        if (!t.cSeq.isEmpty() || !t.callId.isEmpty() || !tag.isEmpty()) {
            QString ann;
            if (!tag.isEmpty())
                ann = QStringLiteral("[%1]").arg(tag);
            if (!t.cSeq.isEmpty()) {
                if (!ann.isEmpty()) ann += QStringLiteral("  ");
                ann += QStringLiteral("CSeq ") + t.cSeq;
            }
            if (!t.callId.isEmpty()) {
                if (!ann.isEmpty()) ann += QStringLiteral("  ");
                ann += t.callId.left(16);
            }
            p.setFont(annFont);
            p.setPen(QColor("#505050"));
            const int aw = afm.horizontalAdvance(ann);
            p.drawText(midX - aw / 2, rowMid + 18, ann);
        }
    }
}

QColor SipLadderWidget::colorForTrace(const SipMessageTrace &t)
{
    if (t.statusCode > 0) {
        if (t.statusCode < 200)  return QColor("#AAAAAA");   // provisional
        if (t.statusCode < 300)  return QColor("#50D890");   // 2xx success
        if (t.statusCode < 400)  return QColor("#A0D0FF");   // 3xx redirect
        if (t.statusCode < 500)  return QColor("#FF9944");   // 4xx client error
        return QColor("#FF5555");                             // 5xx/6xx server error
    }
    const QString &m = t.method;
    if (m == QStringLiteral("REGISTER") || m == QStringLiteral("UNREGISTER"))
        return QColor("#5B8CFF");
    if (m == QStringLiteral("INVITE"))    return QColor("#5EBF6C");
    if (m == QStringLiteral("BYE"))       return QColor("#FF6B6B");
    if (m == QStringLiteral("CANCEL"))    return QColor("#FF9944");
    if (m == QStringLiteral("ACK"))       return QColor("#CCCCAA");
    if (m == QStringLiteral("MESSAGE"))   return QColor("#B084F5");
    return QColor("#CCCCCC");
}
