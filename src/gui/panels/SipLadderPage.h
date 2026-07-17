#pragma once

#include <QWidget>
#include "sip/SipMessageTrace.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class SipLadderWidget;
class SipMessageDetailsDialog;

class SipLadderPage : public QWidget
{
    Q_OBJECT
public:
    explicit SipLadderPage(QWidget *parent = nullptr);

    // Task W113 (Call Workspace) deep link: sets the Call-ID filter and
    // re-applies filters immediately, so "Open in SIP Ladder" from a call
    // lands pre-filtered to that call's messages.
    void filterByCallId(const QString &callId);

private slots:
    void applyFilters();
    void onClear();
    void onExportText();
    void onExportJson();
    void onTraceActivated(const SipMessageTrace &trace);

private:
    SipLadderWidget *m_ladder{nullptr};
    QLineEdit       *m_callIdFilter{nullptr};
    QLineEdit       *m_methodFilter{nullptr};
    QComboBox       *m_directionFilter{nullptr};
    QPushButton     *m_clearBtn{nullptr};
    QPushButton     *m_exportTextBtn{nullptr};
    QPushButton     *m_exportJsonBtn{nullptr};
};
