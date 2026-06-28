#pragma once
#include <QDialog>
#include "sip/CallStateMachine.h"

class QLabel;
class QPushButton;

// Non-blocking popup shown when the remote peer requests a media channel (video or RTT)
// during an active call. The user can Accept or Ignore the request.
// Dismisses automatically on call end or media becoming active.
class MediaRequestDialog : public QDialog
{
    Q_OBJECT
public:
    enum class MediaType { Video, Rtt };

    explicit MediaRequestDialog(QWidget *parent = nullptr);

    void showVideoRequest(const QString &remoteUri);
    void showRttRequest(const QString &remoteUri);

public slots:
    void onCallStateChanged(CallState state, const QString &statusText, int statusCode);
    void onVideoMediaConnected();
    void onRttMediaConnected();

private:
    void updateLayout(MediaType type, const QString &remoteUri);

    QLabel      *m_titleLabel{nullptr};
    QLabel      *m_nameLabel{nullptr};
    QLabel      *m_uriLabel{nullptr};
    QLabel      *m_typeLabel{nullptr};
    QPushButton *m_acceptBtn{nullptr};
    QPushButton *m_ignoreBtn{nullptr};

    MediaType m_currentType{MediaType::Video};
};
