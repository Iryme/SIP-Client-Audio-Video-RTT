#pragma once
#include <QList>
#include <QRect>
#include <QVector>
#include <QWidget>

#include "sip/SipMessageTrace.h"

class QEvent;
class QMouseEvent;
class QPoint;

// Draws a SIP message sequence (ladder) diagram. Placed inside a QScrollArea
// in DiagnosticsPanel.  Connect SipTraceLogger::messageLogged to
// onMessageLogged() and SipTraceLogger::cleared to onCleared().
class SipLadderWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SipLadderWidget(QWidget *parent = nullptr);

    QSize sizeHint()        const override;
    QSize minimumSizeHint() const override;

public slots:
    void onMessageLogged(const SipMessageTrace &trace);
    void onCleared();

    void setCallIdFilter(const QString &callId);
    void setMethodFilter(const QString &method);
    void setDirectionFilter(const QString &direction);

signals:
    void traceActivated(const SipMessageTrace &trace);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    static QColor colorForTrace(const SipMessageTrace &t);
    static QString tooltipForTrace(const SipMessageTrace &trace);
    int contentHeight() const;
    QList<int> visibleTraceIndices() const;
    int traceIndexAt(const QPoint &pos) const;
    void updateHoverTip(const QPoint &pos);

    QList<SipMessageTrace> m_traces;
    QVector<QRect>         m_rowRects;
    int                    m_hoverIndex{-1};
    QString                m_filterCallId;
    QString                m_filterMethod;
    QString                m_filterDirection;

    static constexpr int kRowHeight  = 46;
    static constexpr int kHeaderH    = 56;  // entity box area + top gap
    static constexpr int kPadBottom  = 16;
};
