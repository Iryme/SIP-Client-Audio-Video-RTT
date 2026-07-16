#pragma once
#include <QObject>
#include <QString>

// Task W111 Phase 6 (experimental): thin state wrapper around
// SipManager::sendMsrpFile() / SipCall::sendMsrpFile() for the Client
// Messaging View's "Send File" action. MsrpSession::sendFile() (Task W104)
// reads a file fully into memory and sends it as one MSRP message — the
// same bounded-in-memory model as any other MSRP message, with no
// chunk-level progress signal and no mid-transfer cancel hook (there is
// nothing "in flight" to cancel: the call is synchronous). This model
// therefore reports only discrete states, never a byte progress bar, and
// intentionally offers no cancel action while sending.
//
// File transfer is marked Experimental in the UI and stays off the "actual
// transport" fallback path entirely: a file send either goes out over MSRP
// or fails outright — it never silently falls back to SIP MESSAGE.
class FileTransferModel : public QObject
{
    Q_OBJECT
public:
    enum class State { Idle, Sending, Sent, Failed };

    explicit FileTransferModel(QObject *parent = nullptr);

    State state() const { return m_state; }

    // Sends filePath via the active call's established MSRP session.
    // Returns false immediately (state -> Failed) if no MSRP session is
    // established, the file cannot be opened, or it exceeds the configured
    // max message size.
    bool sendFile(const QString &filePath, const QString &contentType, QString &error);

signals:
    void stateChanged(State state);
    void sendResult(bool ok, const QString &error, const QString &messageId,
                    qint64 fileSize, const QString &sha1Hex);

private:
    void setState(State s);

    State m_state{State::Idle};
};
