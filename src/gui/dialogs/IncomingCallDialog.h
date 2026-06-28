#pragma once
#include <QDialog>
#include "sip/CallStateMachine.h"

class QLabel;
class QPushButton;

// Non-blocking popup shown whenever an incoming call arrives.
// Disappears automatically on call state changes that end or answer the call.
class IncomingCallDialog : public QDialog
{
    Q_OBJECT
public:
    explicit IncomingCallDialog(QWidget *parent = nullptr);

    void setRemoteUri(const QString &uri);

public slots:
    void onCallStateChanged(CallState state, const QString &statusText, int statusCode);

private:
    QLabel      *m_nameLabel{nullptr};
    QLabel      *m_uriLabel{nullptr};
    QPushButton *m_answerBtn{nullptr};
    QPushButton *m_rejectBtn{nullptr};
};
