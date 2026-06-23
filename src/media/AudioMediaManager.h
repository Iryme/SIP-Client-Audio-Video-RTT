#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include "media/MediaDevice.h"

class SipCall;

// Application-layer audio media coordinator.
//
// Responsibilities:
//   - Track which SipCall is currently active for audio.
//   - Forward mute/unmute to the active call's PJSIP audio stream.
//   - Report input / output levels (0-100) from the active call.
//   - Persist and apply device selection changes via MediaDeviceSelectionModel.
//   - Emit diagnostic signals and log every audio lifecycle event.
//
// SipManager calls attachCall() when a new SipCall is created and detachCall()
// when it is destroyed. All other callers interact through the singleton and
// the public mute / device-selection API.
//
// PJSIP audio wiring lives entirely in SipCall.cpp (#ifdef HAVE_PJSIP).
// AudioMediaManager is Qt-only; it never includes pjsua2.hpp.
class AudioMediaManager : public QObject
{
    Q_OBJECT
public:
    static AudioMediaManager &instance();

    // Called by SipManager when a new active call is created.
    void attachCall(SipCall *call);

    // Called by SipManager when the active call is being destroyed.
    void detachCall();

    // Mute / unmute the local microphone.
    bool isMuted()      const;
    bool isMediaActive() const;

    // Current audio levels: updated ~10 Hz while media is active.
    int inputLevel()  const;
    int outputLevel() const;

    // Persist and apply a device selection. If a call is active the new device
    // is applied immediately in PJSIP mode (the PJSIP path is scaffolded in
    // SipCall.cpp; PJSIP integer device-index mapping is a known limitation).
    void setMicrophone(const QString &deviceId);
    void setSpeaker   (const QString &deviceId);

public slots:
    void setMuted(bool muted);

signals:
    void mutedChanged(bool muted);
    void inputLevelChanged(int level);
    void outputLevelChanged(int level);
    void mediaConnected();
    void mediaDisconnected();

private:
    AudioMediaManager();

    void onCallAudioMediaConnected();
    void onCallAudioMediaDisconnected();
    void onCallMuteChanged(bool muted);
    void onCallInputLevel(int level);
    void onCallOutputLevel(int level);
    void onCallConnected(const QString &remoteUri);
    void onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void onCallFailed(const QString &remoteUri, const QString &reason, int statusCode);

    QPointer<SipCall> m_call;
    bool m_muted{false};
    bool m_mediaActive{false};
    int  m_inputLevel{0};
    int  m_outputLevel{0};
};
