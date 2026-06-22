#pragma once
#include <QString>
#include <QDateTime>
#include <QUuid>

enum class SipTransport { UDP, TCP, TLS };

struct SipProfile
{
    QString      profileId;
    QString      displayName;
    QString      sipUsername;
    QString      sipDomain;
    QString      sipUri;
    QString      registrar;
    QString      proxy;
    QString      outboundProxy;
    SipTransport transport{SipTransport::UDP};
    QString      authUsername;
    QString      emergencyServiceUri;
    bool         enableRtt{false};
    bool         enableLmpe{false};
    bool         enableEtsiCompatibility{false};
    QDateTime    createdAt;
    QDateTime    updatedAt;

    // Returns sipUri if set; otherwise derives sip:<username>@<domain>
    QString effectiveSipUri() const
    {
        if (!sipUri.isEmpty())
            return sipUri;
        if (!sipUsername.isEmpty() && !sipDomain.isEmpty())
            return QStringLiteral("sip:") + sipUsername + QChar('@') + sipDomain;
        return {};
    }

    bool isNull() const { return profileId.isEmpty(); }

    static SipProfile createNew()
    {
        SipProfile p;
        p.profileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.createdAt = QDateTime::currentDateTimeUtc();
        p.updatedAt = p.createdAt;
        return p;
    }
};
