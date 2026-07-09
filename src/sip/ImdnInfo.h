#pragma once
#include <QString>

// RFC 5438 IMDN (Instant Message Disposition Notification) diagnostics info.
struct ImdnInfo
{
    enum class Disposition { None, Delivered, Displayed, Failed, Error };

    bool        present{false};
    Disposition disposition{Disposition::None};
    QString     messageId;
    QString     originalRecipient;
    QString     finalRecipient;

    static QString dispositionToString(Disposition d)
    {
        switch (d) {
        case Disposition::Delivered: return QStringLiteral("delivered");
        case Disposition::Displayed: return QStringLiteral("displayed");
        case Disposition::Failed:    return QStringLiteral("failed");
        case Disposition::Error:     return QStringLiteral("error");
        case Disposition::None:      break;
        }
        return QStringLiteral("none");
    }
};
