#include "FileTransferModel.h"

#include "sip/SipManager.h"

FileTransferModel::FileTransferModel(QObject *parent)
    : QObject(parent)
{
}

void FileTransferModel::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

bool FileTransferModel::sendFile(const QString &filePath, const QString &contentType, QString &error)
{
    setState(State::Sending);
    const SipCall::MsrpFileSendResult result = SipManager::instance().sendMsrpFile(filePath, contentType);
    error = result.error;
    setState(result.ok ? State::Sent : State::Failed);
    emit sendResult(result.ok, result.error, result.messageId, result.fileSize, result.sha1Hex);
    return result.ok;
}
