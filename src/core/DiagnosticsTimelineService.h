#pragma once
#include <QObject>
#include <QTimer>

#include "core/DiagnosticsTimelineModel.h"
#include "core/Logger.h"
#include "sip/CallStateMachine.h"
#include "sip/RegistrationStateMachine.h"
#include "rtt/RttSession.h"

struct RtpStatsSnapshot;

// Listens to the signals already emitted by SipManager, AudioMediaManager,
// VideoMediaManager, CameraController, RttSession and Logger, and turns each
// relevant one into a DiagnosticsTimelineEntry appended to its model. Never
// invents an event and never duplicates the same real-world occurrence into
// two categories (see the wiring comments in the .cpp for exactly which
// signal maps to which entry).
//
// Owns persistence: loads the last session's timeline from timeline.json in
// AppData at construction, and (coalesced onto a short timer, same pattern as
// CallHistoryStore) writes it back after new entries arrive.
class DiagnosticsTimelineService : public QObject
{
    Q_OBJECT
public:
    static DiagnosticsTimelineService &instance();

    DiagnosticsTimelineModel *model();

    static QString persistedFilePath();

    // Loads timeline.json (if present) into the model. Public so it can be
    // re-invoked manually; already called once from the constructor.
    void loadPersisted();

    // Writes the current model contents to timeline.json. Normally invoked
    // automatically (coalesced) after append(); public so callers can force
    // a save (e.g. before shutdown).
    void savePersisted();

private slots:
    void onRegistrationStateChanged(RegistrationState state, const QString &statusText, int statusCode);
    void onCallStateChanged(CallState state, const QString &statusText, int statusCode);
    void onIncomingCall(const QString &remoteUri);
    void onCallConnected(const QString &remoteUri);
    void onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void onCallFailed(const QString &remoteUri, const QString &reason, int statusCode);
    void onProfileSwitchStarted(const QString &profileId);
    void onProfileSwitchCompleted(const QString &profileId);
    void onProfileSwitchFailed(const QString &profileId, const QString &reason);
    void onAudioMediaConnected();
    void onAudioMediaDisconnected();
    void onVideoMediaConnected();
    void onVideoMediaDisconnected();
    void onAudioMuted(bool muted);
    void onAudioDeviceSelectionChanged();
    void onVideoMuted(bool muted);
    void onLocalVideoStarted();
    void onLocalVideoStopped();
    void onRemoteVideoStarted();
    void onRemoteVideoStopped();
    void onCameraChanged(const QString &deviceId);
    void onCameraEnabledChanged(bool enabled);
    void onRttStateChanged(RttState state);
    void onRtpStatsChanged(const RtpStatsSnapshot &stats);
    void onLogEntryAdded(const LogEntry &entry);
    void onSipInitialized();
    void onSipInitializationFailed(const QString &reason);

private:
    DiagnosticsTimelineService();

    void record(TimelineCategory category, TimelineSeverity severity,
                const QString &title, const QString &details = {},
                const QString &remoteUri = {}, int sipCode = 0,
                const QString &profileId = {});

    DiagnosticsTimelineModel m_model;
    QTimer                   m_saveTimer;
    bool                     m_rtpWarned{false};
};
