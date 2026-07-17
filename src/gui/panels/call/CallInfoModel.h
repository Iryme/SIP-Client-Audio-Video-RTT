#pragma once
#include <QObject>
#include <QString>

#include "core/CodecInfo.h"
#include "media/RtpStats.h"
#include "sip/CallMediaOptions.h"
#include "sip/CallStateMachine.h"

// Task W113 (Call Workspace). Single source of truth for the active call's
// display state, consumed by CallWorkspacePanel.
//
// Deliberately dependency-free: it never reaches into SipManager itself, it
// is only fed via the setters below (same pattern as ConversationListModel /
// CallHistoryListModel — a "dumb" aggregator, not a duplicate owner of the
// data). MainWindow's existing SipManager-signal connections call these
// setters; CallWorkspacePanel binds to callInfoChanged() and re-reads the
// getters. This means the panel never has to infer active media from a
// button's checked-state — it always reads back from here.
//
// reset() (called by MainWindow on every transition to Idle/Failed) clears
// every field. This is the concrete mechanism behind this task's "call
// isolation" requirement at the level SipManager's single-active-call
// architecture actually supports: no stale identity/media/stats from a
// finished call can leak into the display of the next one. Real concurrent
// multi-call support is out of scope (see docs/call-state-and-media-model.md).
class CallInfoModel : public QObject
{
    Q_OBJECT
public:
    explicit CallInfoModel(QObject *parent = nullptr);

    CallState state() const { return m_state; }
    QString statusText() const { return m_statusText; }

    QString remoteUri() const { return m_remoteUri; }
    QString displayName() const { return m_displayName; }
    QString presenceText() const { return m_presenceText; }

    int durationSeconds() const { return m_durationSeconds; }

    bool muted() const { return m_muted; }
    bool held() const { return m_held; }

    bool videoConnected() const { return m_videoConnected; }
    bool videoRequested() const { return m_videoRequested; }
    bool rttConnected() const { return m_rttConnected; }
    bool rttRequested() const { return m_rttRequested; }

    // What was actually asked for when the call was launched (captured by
    // MainWindow::placeCall() at call-start time — SipCall itself only ever
    // knows the *negotiated* result, never the original request).
    CallMediaOptions selectedMedia() const { return m_selectedMedia; }

    // What the SDP negotiation actually produced. Invalid/default-constructed
    // AudioCodecInfo/VideoCodecInfo means "nothing negotiated yet" (see
    // CodecInfo.h's own isValid() contract).
    AudioCodecInfo negotiatedAudio() const { return m_negotiatedAudio; }
    VideoCodecInfo negotiatedVideo() const { return m_negotiatedVideo; }

    // Actual, currently-measured media (as opposed to what was selected or
    // negotiated): live RTP/RTCP stats and the video pipeline's own fps/drop
    // counters. Kept as two distinct fields on purpose — see
    // docs/call-state-and-media-model.md for the packet-loss/drop-count
    // conflation bug this separation fixes.
    RtpStatsSnapshot rtpStats() const { return m_rtpStats; }
    float videoFps() const { return m_videoFps; }
    int videoDropsPerSecond() const { return m_videoDropsPerSecond; }

    QString micDeviceName() const { return m_micDeviceName; }
    QString speakerDeviceName() const { return m_speakerDeviceName; }
    QString cameraDeviceName() const { return m_cameraDeviceName; }

public slots:
    void setState(CallState state, const QString &statusText);
    void setRemoteUri(const QString &uri);
    void setDisplayName(const QString &name);
    void setPresenceText(const QString &text);
    void setDurationSeconds(int seconds);
    void setMuted(bool muted);
    void setHeld(bool held);
    void setVideoConnected(bool connected);
    void setVideoRequested(bool requested);
    void setRttConnected(bool connected);
    void setRttRequested(bool requested);
    void setSelectedMedia(const CallMediaOptions &options);
    void setNegotiatedAudio(const AudioCodecInfo &info);
    void setNegotiatedVideo(const VideoCodecInfo &info);
    void setRtpStats(const RtpStatsSnapshot &stats);
    void setVideoStats(float fps, int dropsPerSecond);
    void setDeviceNames(const QString &mic, const QString &speaker, const QString &camera);

    // Clears every field back to its default. Called on every transition to
    // Idle/Failed so the next call starts from a clean slate.
    void reset();

signals:
    void callInfoChanged();

private:
    CallState m_state{CallState::Idle};
    QString m_statusText;

    QString m_remoteUri;
    QString m_displayName;
    QString m_presenceText;

    int m_durationSeconds{0};

    bool m_muted{false};
    bool m_held{false};

    bool m_videoConnected{false};
    bool m_videoRequested{false};
    bool m_rttConnected{false};
    bool m_rttRequested{false};

    CallMediaOptions m_selectedMedia;
    AudioCodecInfo m_negotiatedAudio;
    VideoCodecInfo m_negotiatedVideo;

    RtpStatsSnapshot m_rtpStats;
    float m_videoFps{0.0f};
    int m_videoDropsPerSecond{0};

    QString m_micDeviceName;
    QString m_speakerDeviceName;
    QString m_cameraDeviceName;
};
