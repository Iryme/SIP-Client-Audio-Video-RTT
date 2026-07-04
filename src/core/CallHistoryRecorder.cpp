#include "CallHistoryRecorder.h"
#include "core/CallHistoryStore.h"
#include "core/ContactStore.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

#include <QDateTime>

CallHistoryRecorder &CallHistoryRecorder::instance()
{
    static CallHistoryRecorder s;
    return s;
}

CallHistoryRecorder::CallHistoryRecorder() : QObject(nullptr)
{
    SipManager &sip = SipManager::instance();
    connect(&sip, &SipManager::incomingCall,       this, &CallHistoryRecorder::onIncomingCall);
    connect(&sip, &SipManager::callStateChanged,   this, &CallHistoryRecorder::onCallStateChanged);
    connect(&sip, &SipManager::callDisconnected,   this, &CallHistoryRecorder::onCallDisconnected);
    connect(&sip, &SipManager::callFailed,         this, &CallHistoryRecorder::onCallFailed);
    connect(&sip, &SipManager::audioMediaConnected, this, &CallHistoryRecorder::onAudioMediaConnected);
    connect(&sip, &SipManager::videoMediaConnected, this, &CallHistoryRecorder::onVideoMediaConnected);
    connect(&sip, &SipManager::rttMediaConnected,   this, &CallHistoryRecorder::onRttMediaConnected);
}

QString CallHistoryRecorder::lookupDisplayName(const QString &remoteUri)
{
    const QString needle = remoteUri.trimmed().toLower();
    if (needle.isEmpty())
        return {};

    for (const Contact &c : ContactStore::instance().contacts()) {
        const QString uri = c.uri.trimmed().toLower();
        if (uri == needle || uri.contains(needle) || needle.contains(uri))
            return c.name;
    }
    return {};
}

void CallHistoryRecorder::beginEntry(CallDirection direction, const QString &remoteUri)
{
    if (!m_currentEntryId.isEmpty())
        return;

    CallHistoryEntry e;
    e.direction   = direction;
    e.remoteUri   = remoteUri;
    e.displayName = lookupDisplayName(remoteUri);
    const SipProfile p = SipProfileManager::instance().activeProfile();
    e.profileId   = p.profileId;
    e.profileName = p.displayName;
    e.startTime   = QDateTime::currentDateTimeUtc();
    e.result      = CallResult::Pending;

    m_hadAudio = m_hadVideo = m_hadRtt = false;
    m_currentEntryId = CallHistoryStore::instance().addEntry(e);
}

void CallHistoryRecorder::markAnswered()
{
    if (m_currentEntryId.isEmpty())
        return;
    CallHistoryStore::instance().updateEntry(m_currentEntryId, [](CallHistoryEntry &e) {
        if (e.answerTime.isNull())
            e.answerTime = QDateTime::currentDateTimeUtc();
    });
}

void CallHistoryRecorder::finalize(const QString &reason, int statusCode, bool failed)
{
    if (m_currentEntryId.isEmpty())
        return;

    const QString id = m_currentEntryId;
    const bool hadAudio = m_hadAudio;
    const bool hadVideo = m_hadVideo;
    const bool hadRtt   = m_hadRtt;

    CallHistoryStore::instance().updateEntry(id, [&](CallHistoryEntry &e) {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        e.endTime      = now;
        e.lastSipCode  = statusCode;
        e.reason       = reason;
        e.hadAudio     = hadAudio;
        e.hadVideo     = hadVideo;
        e.hadRtt       = hadRtt;

        if (failed) {
            e.result = CallResult::Failed;
        } else if (!e.answerTime.isNull()) {
            e.result = CallResult::Completed;
        } else if (e.direction == CallDirection::Incoming) {
            // Local reject sends 486 Busy Here (see SipCall::reject()); any
            // other unanswered-incoming termination is treated as missed.
            e.result = (statusCode == 486) ? CallResult::Rejected : CallResult::Missed;
        } else {
            e.result = CallResult::Cancelled;
        }

        e.durationSec = e.answerTime.isNull() ? 0 : static_cast<int>(e.answerTime.secsTo(now));
    });

    m_currentEntryId.clear();
    m_hadAudio = m_hadVideo = m_hadRtt = false;
}

void CallHistoryRecorder::onIncomingCall(const QString &remoteUri)
{
    beginEntry(CallDirection::Incoming, remoteUri);
}

void CallHistoryRecorder::onCallStateChanged(CallState state, const QString &statusText, int statusCode)
{
    Q_UNUSED(statusText);
    Q_UNUSED(statusCode);

    if (state == CallState::OutgoingInit) {
        beginEntry(CallDirection::Outgoing, SipManager::instance().activeCallRemoteUri());
    } else if (state == CallState::Active) {
        markAnswered();
    }
}

void CallHistoryRecorder::onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode)
{
    Q_UNUSED(remoteUri);
    finalize(reason, statusCode, /*failed=*/false);
}

void CallHistoryRecorder::onCallFailed(const QString &remoteUri, const QString &reason, int statusCode)
{
    Q_UNUSED(remoteUri);
    finalize(reason, statusCode, /*failed=*/true);
}

void CallHistoryRecorder::onAudioMediaConnected() { m_hadAudio = true; }
void CallHistoryRecorder::onVideoMediaConnected() { m_hadVideo = true; }
void CallHistoryRecorder::onRttMediaConnected()   { m_hadRtt = true; }
