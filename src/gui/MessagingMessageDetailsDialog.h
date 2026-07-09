#pragma once

#include <QDialog>

#include "sip/MessagingTraceEntry.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;

// Read-only detail view for a single Messaging Diagnostics entry: base SIP
// fields plus whichever structured CPIM / IMDN / is-composing / SDP-MSRP
// sections were detected for it.
class MessagingMessageDetailsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MessagingMessageDetailsDialog(QWidget *parent = nullptr);

    void setEntry(const MessagingTraceEntry &entry);

private slots:
    void onCopyRawClicked();

private:
    static QString formatDirection(SipMessageTrace::Direction direction);
    static QString formatStructuredSections(const MessagingTraceEntry &entry);

    QLabel         *m_title{nullptr};
    QLabel         *m_timestamp{nullptr};
    QLabel         *m_direction{nullptr};
    QLabel         *m_method{nullptr};
    QLabel         *m_from{nullptr};
    QLabel         *m_to{nullptr};
    QLabel         *m_callId{nullptr};
    QLabel         *m_cseq{nullptr};
    QLabel         *m_contentType{nullptr};
    QLabel         *m_contentKind{nullptr};
    QPlainTextEdit  *m_structured{nullptr};
    QPlainTextEdit  *m_body{nullptr};
    QPlainTextEdit  *m_raw{nullptr};
    QPushButton     *m_copyRaw{nullptr};
    MessagingTraceEntry m_entry;
};
