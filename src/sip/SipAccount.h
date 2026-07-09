#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QMetaType>

#include "sip/SipProfile.h"

enum class RegistrationState {
    Unregistered,
    Registering,
    Registered,
    Unregistering,
    RegistrationFailed
};

Q_DECLARE_METATYPE(RegistrationState)

QString registrationStateName(RegistrationState state);

// Wraps one pjsua2 Account when PJSIP is available. PJSIP details are hidden
// behind Impl so stub builds never include pjsua2 headers.
class SipAccount : public QObject
{
    Q_OBJECT
public:
    explicit SipAccount(const QString &profileId, QObject *parent = nullptr);
    ~SipAccount() override;

    QString profileId() const;

    bool startRegistration(const SipProfile &profile, const QString &password,
                           int transportId = -1);
    bool startUnregistration();
    bool refreshRegistration();
    bool applyVideoSettings();

    // Returns the internal pj::Account as a void* for use by SipCall in PJSIP mode.
    // Returns nullptr in stub mode. Cast to pj::Account* inside HAVE_PJSIP guards.
    void *pjAccountHandle() const;

    // Sends a SIP MESSAGE out-of-dialog via a transient, non-subscribing
    // pjsua2 Buddy (no presence/dialog-event side effects). Fire-and-forget:
    // returns as soon as the request is submitted to the transaction layer;
    // it does not wait for a response. Never touches MSRP. Returns false
    // (with `error` set) if PJSIP is unavailable or no account is active.
    // correlationId is echoed back (unchanged) on instantMessageStatusReceived
    // once/if a final SIP response arrives, so the caller can correlate it
    // with e.g. a MessageHistoryStore entry without any lookup table here.
    bool sendMessage(const QString &toUri, const QString &contentType, const QString &body,
                     const QList<QPair<QString, QString>> &extraHeaders,
                     qint64 correlationId, QString &error);

    // Removes and returns the pre-created pj::Call* registered in onIncomingCall
    // to prevent pjsua2's auto-reject. Caller takes ownership and must delete it
    // AFTER deleting the real PjCall for that callId. Returns nullptr if not found.
    void *takeEarlyPjCall(int callId);

signals:
    void registrationStateChanged(RegistrationState state,
                                  const QString &statusText,
                                  int statusCode);
    void registrationExpiryReceived(int seconds);

    // Emitted when a new incoming call arrives (PJSIP onIncomingCall callback).
    // In stub mode this signal is never emitted.
    void incomingCallReceived(const QString &remoteUri);

    // PJSIP-only detail for binding an incoming INVITE to SipCall without
    // exposing pjsua2 headers outside SipAccount/SipCall/SipManager.
    void incomingPjsipCallReceived(const QString &remoteUri, int callId);

    // Emitted from the dedicated pjsua2 onInstantMessage callback (Task
    // W093) for an incoming SIP MESSAGE. contactUri is empty when the
    // request had no Contact header. profileId is this account's own
    // profileId (the association is unambiguous — this signal is only ever
    // emitted by the account that received the message). In stub mode this
    // signal is never emitted.
    void instantMessageReceived(const QString &fromUri, const QString &toUri,
                                const QString &contactUri, const QString &contentType,
                                const QString &body, const QString &callId,
                                const QString &profileId);

    // Emitted once/if a final SIP response arrives for a message previously
    // submitted via sendMessage(). success is true for 2xx. In stub mode
    // this signal is never emitted (sendMessage always fails synchronously).
    void instantMessageStatusReceived(qint64 correlationId, bool success,
                                      int statusCode, const QString &reason);

private:
    struct Impl;

    void postRegistrationResult(RegistrationState state,
                                const QString &statusText,
                                int statusCode,
                                int expirySeconds = 0);

    QString m_profileId;
    Impl   *m_impl{nullptr};
};
