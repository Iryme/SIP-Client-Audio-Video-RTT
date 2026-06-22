#ifndef UNICODE
#  define UNICODE
#endif
#ifndef _UNICODE
#  define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>

#include "security/WindowsCredentialBackend.h"
#include <QString>

QString WindowsCredentialBackend::backendName() const
{
    return QStringLiteral("Windows Credential Manager");
}

bool WindowsCredentialBackend::isSecure() const
{
    return true;
}

bool WindowsCredentialBackend::store(const QString &key, const QString &username,
                                      const QString &password, QString *errorOut)
{
    const std::wstring wKey      = key.toStdWString();
    const std::wstring wUsername = username.toStdWString();
    const std::wstring wPassword = password.toStdWString();
    const DWORD blobSize = static_cast<DWORD>(wPassword.size() * sizeof(wchar_t));

    CREDENTIALW cred = {};
    cred.Type               = CRED_TYPE_GENERIC;
    cred.TargetName         = const_cast<LPWSTR>(wKey.c_str());
    cred.UserName           = const_cast<LPWSTR>(wUsername.c_str());
    cred.CredentialBlob     = reinterpret_cast<LPBYTE>(const_cast<wchar_t *>(wPassword.c_str()));
    cred.CredentialBlobSize = blobSize;
    cred.Persist            = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&cred, 0)) {
        const DWORD err = GetLastError();
        if (errorOut)
            *errorOut = QStringLiteral("CredWriteW failed: error %1").arg(err);
        return false;
    }
    return true;
}

QString WindowsCredentialBackend::load(const QString &key, const QString &username,
                                        bool *found, QString *errorOut)
{
    Q_UNUSED(username)
    if (found) *found = false;

    PCREDENTIALW pCred = nullptr;
    if (!CredReadW(key.toStdWString().c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        const DWORD err = GetLastError();
        if (err == ERROR_NOT_FOUND)
            return {};
        if (errorOut)
            *errorOut = QStringLiteral("CredReadW failed: error %1").arg(err);
        return {};
    }

    const QString password = QString::fromWCharArray(
        reinterpret_cast<const wchar_t *>(pCred->CredentialBlob),
        static_cast<int>(pCred->CredentialBlobSize / sizeof(wchar_t)));
    CredFree(pCred);

    if (found) *found = true;
    return password;
}

bool WindowsCredentialBackend::remove(const QString &key, const QString &username,
                                       QString *errorOut)
{
    Q_UNUSED(username)
    if (!CredDeleteW(key.toStdWString().c_str(), CRED_TYPE_GENERIC, 0)) {
        const DWORD err = GetLastError();
        if (err == ERROR_NOT_FOUND)
            return false;
        if (errorOut)
            *errorOut = QStringLiteral("CredDeleteW failed: error %1").arg(err);
        return false;
    }
    return true;
}

bool WindowsCredentialBackend::has(const QString &key, const QString &username)
{
    Q_UNUSED(username)
    PCREDENTIALW pCred = nullptr;
    if (!CredReadW(key.toStdWString().c_str(), CRED_TYPE_GENERIC, 0, &pCred))
        return false;
    CredFree(pCred);
    return true;
}
