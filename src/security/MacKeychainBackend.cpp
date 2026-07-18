#ifdef __APPLE__

#include "security/MacKeychainBackend.h"

#include <Security/Security.h>

namespace {

CFStringRef cfString(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return CFStringCreateWithBytes(kCFAllocatorDefault,
                                   reinterpret_cast<const UInt8 *>(bytes.constData()),
                                   bytes.size(), kCFStringEncodingUTF8, false);
}

QString statusMessage(OSStatus status)
{
    CFStringRef text = SecCopyErrorMessageString(status, nullptr);
    if (!text)
        return QStringLiteral("Keychain error %1").arg(status);
    const CFIndex length = CFStringGetLength(text);
    const CFIndex maximum = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    QByteArray buffer(static_cast<int>(maximum), '\0');
    const bool converted = CFStringGetCString(text, buffer.data(), maximum, kCFStringEncodingUTF8);
    CFRelease(text);
    return converted ? QString::fromUtf8(buffer.constData())
                     : QStringLiteral("Keychain error %1").arg(status);
}

CFMutableDictionaryRef queryFor(const QString &key, const QString &username)
{
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    const CFStringRef service = cfString(key);
    const CFStringRef account = cfString(username);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, account);
    CFRelease(service);
    CFRelease(account);
    return query;
}

} // namespace

QString MacKeychainBackend::backendName() const
{
    return QStringLiteral("macOS Keychain");
}

bool MacKeychainBackend::isSecure() const
{
    return true;
}

bool MacKeychainBackend::store(const QString &key, const QString &username,
                               const QString &password, QString *errorOut)
{
    const QByteArray secret = password.toUtf8();
    CFMutableDictionaryRef query = queryFor(key, username);
    const CFDataRef value = CFDataCreate(
        kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(secret.constData()),
        secret.size());
    CFDictionarySetValue(query, kSecValueData, value);
    OSStatus status = SecItemAdd(query, nullptr);
    if (status == errSecDuplicateItem) {
        CFDictionaryRemoveValue(query, kSecValueData);
        const void *keys[] = {kSecValueData};
        const void *values[] = {value};
        const CFDictionaryRef updates = CFDictionaryCreate(
            kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        status = SecItemUpdate(query, updates);
        CFRelease(updates);
    }
    CFRelease(value);
    CFRelease(query);
    if (status != errSecSuccess) {
        if (errorOut)
            *errorOut = statusMessage(status);
        return false;
    }
    return true;
}

QString MacKeychainBackend::load(const QString &key, const QString &username,
                                 bool *found, QString *errorOut)
{
    if (found)
        *found = false;
    CFMutableDictionaryRef query = queryFor(key, username);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status == errSecItemNotFound)
        return {};
    if (status != errSecSuccess) {
        if (errorOut)
            *errorOut = statusMessage(status);
        return {};
    }
    const auto data = static_cast<CFDataRef>(result);
    const QString password = QString::fromUtf8(
        reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
        static_cast<int>(CFDataGetLength(data)));
    CFRelease(result);
    if (found)
        *found = true;
    return password;
}

bool MacKeychainBackend::remove(const QString &key, const QString &username,
                                QString *errorOut)
{
    CFMutableDictionaryRef query = queryFor(key, username);
    const OSStatus status = SecItemDelete(query);
    CFRelease(query);
    if (status == errSecItemNotFound)
        return false;
    if (status != errSecSuccess) {
        if (errorOut)
            *errorOut = statusMessage(status);
        return false;
    }
    return true;
}

bool MacKeychainBackend::has(const QString &key, const QString &username)
{
    CFMutableDictionaryRef query = queryFor(key, username);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    const OSStatus status = SecItemCopyMatching(query, nullptr);
    CFRelease(query);
    return status == errSecSuccess;
}

#endif
