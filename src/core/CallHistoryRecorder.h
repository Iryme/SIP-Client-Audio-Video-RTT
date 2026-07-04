#pragma once
#include <QObject>
#include <QString>

#include "core/CallHistoryEntry.h"
#include "sip/CallStateMachine.h"

// Listens to SipManager's call lifecycle signals and records each call into
// CallHistoryStore. Kept separate from SipManager/MainWindow so it can be
// unit tested by driving CallHistoryStore directly (see test_call_history)
// without needing a live SipManager/PJSIP backend.
//
// One call is tracked at a time (SipManager itself only ever has one active
// call), identified by m_currentEntryId.
class CallHistoryRecorder : public QObject
{
    Q_OBJECT
public:
    // Wires this recorder to SipManager::instance()'s signals. Safe to call
    // once at application startup; touching instance() is enough to start
    // recording.
    static CallHistoryRecorder &instance();

private slots:
    void onIncomingCall(const QString &remoteUri);
    void onCallStateChanged(CallState state, const QString &statusText, int statusCode);
    void onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode);
    void onCallFailed(const QString &remoteUri, const QString &reason, int statusCode);
    void onAudioMediaConnected();
    void onVideoMediaConnected();
    void onRttMediaConnected();

private:
    CallHistoryRecorder();

    void beginEntry(CallDirection direction, const QString &remoteUri);
    void markAnswered();
    void finalize(const QString &reason, int statusCode, bool failed);
    static QString lookupDisplayName(const QString &remoteUri);

    QString m_currentEntryId;
    bool    m_hadAudio{false};
    bool    m_hadVideo{false};
    bool    m_hadRtt{false};
};
