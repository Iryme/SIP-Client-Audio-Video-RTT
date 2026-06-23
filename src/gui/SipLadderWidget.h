#pragma once
#include <QList>
#include <QWidget>

#include "sip/SipMessageTrace.h"

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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static QColor colorForTrace(const SipMessageTrace &t);
    int contentHeight() const;

    QList<SipMessageTrace> m_traces;

    static constexpr int kRowHeight  = 46;
    static constexpr int kHeaderH    = 56;  // entity box area + top gap
    static constexpr int kPadBottom  = 16;
};
