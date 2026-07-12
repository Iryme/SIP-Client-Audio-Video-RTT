#include "MsrpSdpNegotiator.h"

namespace MsrpSdpNegotiator {

namespace {

QStringList splitLines(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return normalized.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

MsrpDirection directionFromAttrName(const QString &name)
{
    if (name == QLatin1String("sendrecv")) return MsrpDirection::SendRecv;
    if (name == QLatin1String("sendonly")) return MsrpDirection::SendOnly;
    if (name == QLatin1String("recvonly")) return MsrpDirection::RecvOnly;
    if (name == QLatin1String("inactive")) return MsrpDirection::Inactive;
    return MsrpDirection::Unknown;
}

MsrpTransportProtocol transportFromToken(const QString &token)
{
    if (token.compare(QStringLiteral("TCP/TLS/MSRP"), Qt::CaseInsensitive) == 0)
        return MsrpTransportProtocol::Tls;
    if (token.compare(QStringLiteral("TCP/MSRP"), Qt::CaseInsensitive) == 0)
        return MsrpTransportProtocol::Tcp;
    return MsrpTransportProtocol::Unknown;
}

} // namespace

QList<MediaBlock> parseMessageBlocks(const QString &sdpText)
{
    QList<MediaBlock> blocks;
    const QStringList lines = splitLines(sdpText);

    // Split into media sections starting at each "m=" line; the "session
    // level" prefix (before the first m=) is not relevant here.
    QList<QStringList> sections;
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("m="))) {
            sections.append(QStringList{line});
        } else if (!sections.isEmpty()) {
            sections.last().append(line);
        }
    }

    for (const QStringList &section : sections) {
        if (section.isEmpty() || !section.first().startsWith(QStringLiteral("m=message")))
            continue;

        MediaBlock block;
        const QString mLine = section.first();
        // m=<media> <port> <proto> <fmt...> — media is the literal word
        // "message" for MSRP; skip it, mTokens[0] is the port.
        QStringList mTokens = mLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!mTokens.isEmpty())
            mTokens.removeFirst(); // drop "m=message"
        if (mTokens.size() < 2) {
            block.parseStatus = MsrpParseStatus::Error;
            block.warnings << QStringLiteral("malformed m=message line");
            blocks.append(block);
            continue;
        }
        bool portOk = false;
        block.port = mTokens.at(0).toInt(&portOk);
        if (!portOk) {
            block.parseStatus = MsrpParseStatus::Error;
            block.warnings << QStringLiteral("invalid port in m=message line");
        }
        block.rejected = portOk && block.port == 0;
        block.transport = transportFromToken(mTokens.at(1));
        if (block.transport == MsrpTransportProtocol::Unknown && !block.rejected) {
            block.parseStatus = MsrpParseStatus::Partial;
            block.warnings << QStringLiteral("unrecognized transport protocol: %1").arg(mTokens.at(1));
        }

        for (int i = 1; i < section.size(); ++i) {
            const QString &attrLine = section.at(i);
            if (!attrLine.startsWith(QStringLiteral("a=")))
                continue;
            const QString attr = attrLine.mid(2);
            const int colonIdx = attr.indexOf(QLatin1Char(':'));
            const QString name = (colonIdx >= 0 ? attr.left(colonIdx) : attr).trimmed();
            const QString value = (colonIdx >= 0 ? attr.mid(colonIdx + 1) : QString()).trimmed();

            if (name == QLatin1String("path")) {
                block.path = MsrpPath::parsePath(value);
                for (const MsrpUri &u : block.path) {
                    if (!u.ok) {
                        block.parseStatus = MsrpParseStatus::Partial;
                        block.warnings << QStringLiteral("invalid path entry: %1").arg(u.errorMessage);
                    }
                }
            } else if (name == QLatin1String("accept-types")) {
                block.acceptTypes = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            } else if (name == QLatin1String("accept-wrapped-types")) {
                block.acceptWrappedTypes = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            } else if (name == QLatin1String("setup")) {
                block.setup = msrpSetupFromString(value);
            } else if (name == QLatin1String("connection")) {
                block.connection = value;
            } else if (name == QLatin1String("file-selector")) {
                block.fileSelector = value;
            } else if (name == QLatin1String("file-disposition")) {
                block.fileDisposition = value;
            } else if (name == QLatin1String("file-transfer-id")) {
                block.fileTransferId = value;
            } else {
                const MsrpDirection dir = directionFromAttrName(name);
                if (dir != MsrpDirection::Unknown)
                    block.direction = dir;
                // Unknown attributes are tolerated (diagnostic-only,
                // never fatal) — matches PidfParser/CpimParser tolerance.
            }
        }

        if (!block.rejected && block.path.isEmpty()) {
            block.parseStatus = MsrpParseStatus::Partial;
            block.warnings << QStringLiteral("missing a=path");
        }

        blocks.append(block);
    }

    return blocks;
}

NegotiationResult negotiateRole(MsrpSetup localSetup, MsrpSetup remoteSetup)
{
    NegotiationResult result;

    if (localSetup == MsrpSetup::HoldConn || remoteSetup == MsrpSetup::HoldConn) {
        result.valid = true;
        result.role = MsrpRole::HoldConn;
        return result;
    }
    if (localSetup == MsrpSetup::ActPass || remoteSetup == MsrpSetup::ActPass) {
        result.errorMessage = QStringLiteral("setup not yet resolved (actpass present after offer/answer)");
        return result;
    }
    if (localSetup == MsrpSetup::Active && remoteSetup == MsrpSetup::Passive) {
        result.valid = true;
        result.role = MsrpRole::ActiveConnector;
        return result;
    }
    if (localSetup == MsrpSetup::Passive && remoteSetup == MsrpSetup::Active) {
        result.valid = true;
        result.role = MsrpRole::PassiveListener;
        return result;
    }
    if (localSetup == MsrpSetup::Active && remoteSetup == MsrpSetup::Active) {
        result.errorMessage = QStringLiteral("invalid offer/answer: both endpoints chose active");
        return result;
    }
    if (localSetup == MsrpSetup::Passive && remoteSetup == MsrpSetup::Passive) {
        result.errorMessage = QStringLiteral("invalid offer/answer: both endpoints chose passive");
        return result;
    }

    result.errorMessage = QStringLiteral("unknown/unset setup value");
    return result;
}

QString buildOfferBlock(const MsrpUri &localUri, MsrpSetup setup,
                        const QStringList &acceptTypes,
                        const QStringList &acceptWrappedTypes)
{
    QString out;
    const QString proto = localUri.transportProtocol() == MsrpTransportProtocol::Tls
        ? QStringLiteral("TCP/TLS/MSRP") : QStringLiteral("TCP/MSRP");

    out += QStringLiteral("m=message %1 %2 *\r\n").arg(localUri.port).arg(proto);
    out += QStringLiteral("a=path:%1\r\n").arg(localUri.toString());
    out += QStringLiteral("a=setup:%1\r\n").arg(msrpSetupToString(setup));
    out += QStringLiteral("a=accept-types:%1\r\n").arg(acceptTypes.join(QLatin1Char(' ')));
    if (!acceptWrappedTypes.isEmpty())
        out += QStringLiteral("a=accept-wrapped-types:%1\r\n").arg(acceptWrappedTypes.join(QLatin1Char(' ')));

    return out;
}

} // namespace MsrpSdpNegotiator
