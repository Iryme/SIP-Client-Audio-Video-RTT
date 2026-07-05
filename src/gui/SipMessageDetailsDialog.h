#pragma once

#include <QDialog>

#include "sip/SipMessageTrace.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;

class SipMessageDetailsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SipMessageDetailsDialog(QWidget *parent = nullptr);

    void setTrace(const SipMessageTrace &trace);

private slots:
    void onCopyRawClicked();

private:
    static QString formatDirection(SipMessageTrace::Direction direction);
    static QString formatHeaders(const SipMessageTrace &trace);
    static QString extractBody(const QString &rawSip);

    QLabel         *m_title{nullptr};
    QLabel         *m_summary{nullptr};
    QLabel         *m_timestamp{nullptr};
    QLabel         *m_direction{nullptr};
    QLabel         *m_method{nullptr};
    QLabel         *m_from{nullptr};
    QLabel         *m_to{nullptr};
    QLabel         *m_callId{nullptr};
    QLabel         *m_cseq{nullptr};
    QLabel         *m_contentType{nullptr};
    QPlainTextEdit  *m_headers{nullptr};
    QPlainTextEdit  *m_body{nullptr};
    QPlainTextEdit  *m_raw{nullptr};
    QPushButton     *m_copyRaw{nullptr};
    SipMessageTrace  m_trace;
};
