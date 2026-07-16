#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <qwindowdefs.h>

#include "media/MediaDevice.h"

class SipCall;

// Application-layer video media coordinator.
//
// Responsibilities:
//   - Track which SipCall is currently active for video.
//   - Forward video mute/unmute to the active call's PJSIP video stream.
//   - Reflect local/remote video availability state.
//   - Persist camera selection via MediaDeviceSelectionModel.
//   - Emit diagnostic signals and log every video lifecycle event.
//
// SipManager calls attachCall() when a new SipCall is created and detachCall()
// when it is destroyed.  All other callers interact through the singleton.
//
// PJSIP video wiring lives entirely in SipCall.cpp (#ifdef HAVE_PJSIP).
// VideoMediaManager is Qt-only; it never includes pjsua2.hpp.
class VideoMediaManager : public QObject
{
    Q_OBJECT
public:
    static VideoMediaManager &instance();

    // Called by SipManager when a new active call is created.
    void attachCall(SipCall *call);

    // Called by SipManager when the active call is being destroyed. Idempotent.
    void detachCall();

    bool isVideoMuted()           const;
    bool isVideoActive()          const;
    bool isLocalVideoAvailable()  const;
    bool isRemoteVideoAvailable() const;

    // Persist and apply a camera selection. Logs a hot-swap diagnostic if a
    // call is active (PJSIP video device re-initialisation is scaffolded; takes
    // effect on next call in the current stub implementation).
    void setCamera(const QString &deviceId);

    // Attach PJSIP video windows to the given Qt widget native handles.
    // Forwards to the active SipCall::attachVideoWindows(). No-op when no
    // active call or on non-Windows / stub builds.
    // Returns true when the attach completed and needs no retry.
    bool attachVideoToWidgets(WId remoteWidget, WId localPreviewWidget);

    // Pauses/resumes PJSIP's rendering into the attached video window(s)
    // without dropping the attachment — call from the hosting widget's
    // hideEvent()/showEvent() so a page switch that hides the video widget
    // (e.g. QStackedWidget::setCurrentIndex) doesn't race PJSIP's renderer
    // against Qt's own widget hide/paint machinery. No-op when no active
    // call or on non-Windows/stub builds.
    bool setVideoWindowVisible(bool visible);

public slots:
    void setVideoMuted(bool muted);

signals:
    void videoMutedChanged(bool muted);
    void videoMediaConnected();
    void videoMediaDisconnected();
    void localVideoStarted();
    void localVideoStopped();
    void remoteVideoStarted();
    void remoteVideoStopped();
    void cameraChanged(const QString &deviceId);

private:
    VideoMediaManager();

    void onCallVideoMediaConnected();
    void onCallVideoMediaDisconnected();
    void onCallLocalVideoStarted();
    void onCallLocalVideoStopped();
    void onCallRemoteVideoStarted();
    void onCallRemoteVideoStopped();
    void onCallVideoMuteChanged(bool muted);
    void onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void onCallFailed(const QString &remoteUri, const QString &reason, int statusCode);

    QPointer<SipCall> m_call;
    bool m_videoMuted{false};
    bool m_videoActive{false};
    bool m_localVideoAvailable{false};
    bool m_remoteVideoAvailable{false};
};
