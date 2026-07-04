#include "DiagnosticsTimelineEntry.h"

QString timelineCategoryName(TimelineCategory category)
{
    switch (category) {
    case TimelineCategory::Registration: return QStringLiteral("Registration");
    case TimelineCategory::Call:         return QStringLiteral("Call");
    case TimelineCategory::Sip:          return QStringLiteral("SIP");
    case TimelineCategory::Media:        return QStringLiteral("Media");
    case TimelineCategory::Audio:        return QStringLiteral("Audio");
    case TimelineCategory::Video:        return QStringLiteral("Video");
    case TimelineCategory::Rtt:          return QStringLiteral("RTT");
    case TimelineCategory::Rtp:          return QStringLiteral("RTP");
    case TimelineCategory::Camera:       return QStringLiteral("Camera");
    case TimelineCategory::Settings:     return QStringLiteral("Settings");
    case TimelineCategory::Diagnostics:  return QStringLiteral("Diagnostics");
    case TimelineCategory::Network:      return QStringLiteral("Network");
    case TimelineCategory::System:       return QStringLiteral("System");
    case TimelineCategory::Warning:      return QStringLiteral("Warning");
    case TimelineCategory::Error:        return QStringLiteral("Error");
    }
    return QStringLiteral("System");
}

TimelineCategory timelineCategoryFromName(const QString &name)
{
    if (name == QLatin1String("Registration")) return TimelineCategory::Registration;
    if (name == QLatin1String("Call"))         return TimelineCategory::Call;
    if (name == QLatin1String("SIP"))          return TimelineCategory::Sip;
    if (name == QLatin1String("Media"))        return TimelineCategory::Media;
    if (name == QLatin1String("Audio"))        return TimelineCategory::Audio;
    if (name == QLatin1String("Video"))        return TimelineCategory::Video;
    if (name == QLatin1String("RTT"))          return TimelineCategory::Rtt;
    if (name == QLatin1String("RTP"))          return TimelineCategory::Rtp;
    if (name == QLatin1String("Camera"))       return TimelineCategory::Camera;
    if (name == QLatin1String("Settings"))     return TimelineCategory::Settings;
    if (name == QLatin1String("Diagnostics"))  return TimelineCategory::Diagnostics;
    if (name == QLatin1String("Network"))      return TimelineCategory::Network;
    if (name == QLatin1String("Warning"))      return TimelineCategory::Warning;
    if (name == QLatin1String("Error"))        return TimelineCategory::Error;
    return TimelineCategory::System;
}

QString timelineSeverityName(TimelineSeverity severity)
{
    switch (severity) {
    case TimelineSeverity::Info:    return QStringLiteral("Info");
    case TimelineSeverity::Warning: return QStringLiteral("Warning");
    case TimelineSeverity::Error:   return QStringLiteral("Error");
    case TimelineSeverity::Success: return QStringLiteral("Success");
    }
    return QStringLiteral("Info");
}

TimelineSeverity timelineSeverityFromName(const QString &name)
{
    if (name == QLatin1String("Warning")) return TimelineSeverity::Warning;
    if (name == QLatin1String("Error"))   return TimelineSeverity::Error;
    if (name == QLatin1String("Success")) return TimelineSeverity::Success;
    return TimelineSeverity::Info;
}

QString timelineSeverityColor(TimelineSeverity severity)
{
    switch (severity) {
    case TimelineSeverity::Info:    return QStringLiteral("#4fc3f7");
    case TimelineSeverity::Warning: return QStringLiteral("#ffa726");
    case TimelineSeverity::Error:   return QStringLiteral("#ef5350");
    case TimelineSeverity::Success: return QStringLiteral("#66bb6a");
    }
    return QStringLiteral("#b0bfd0");
}

QJsonObject DiagnosticsTimelineEntry::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), id);
    o.insert(QStringLiteral("timestamp"), timestamp.toString(Qt::ISODateWithMs));
    o.insert(QStringLiteral("category"), timelineCategoryName(category));
    o.insert(QStringLiteral("severity"), timelineSeverityName(severity));
    o.insert(QStringLiteral("title"), title);
    o.insert(QStringLiteral("details"), details);
    o.insert(QStringLiteral("callId"), callId);
    o.insert(QStringLiteral("profileId"), profileId);
    o.insert(QStringLiteral("remoteUri"), remoteUri);
    o.insert(QStringLiteral("sipCode"), sipCode);
    o.insert(QStringLiteral("colorHint"), colorHint);
    o.insert(QStringLiteral("iconHint"), iconHint);
    return o;
}

DiagnosticsTimelineEntry DiagnosticsTimelineEntry::fromJson(const QJsonObject &o)
{
    DiagnosticsTimelineEntry e;
    e.id = o.value(QStringLiteral("id")).toString();
    e.timestamp = QDateTime::fromString(o.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    e.category = timelineCategoryFromName(o.value(QStringLiteral("category")).toString());
    e.severity = timelineSeverityFromName(o.value(QStringLiteral("severity")).toString());
    e.title = o.value(QStringLiteral("title")).toString();
    e.details = o.value(QStringLiteral("details")).toString();
    e.callId = o.value(QStringLiteral("callId")).toString();
    e.profileId = o.value(QStringLiteral("profileId")).toString();
    e.remoteUri = o.value(QStringLiteral("remoteUri")).toString();
    e.sipCode = o.value(QStringLiteral("sipCode")).toInt();
    e.colorHint = o.value(QStringLiteral("colorHint")).toString();
    e.iconHint = o.value(QStringLiteral("iconHint")).toString();
    return e;
}
