#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

// Which subsystem an event originated from. "Warning"/"Error" are a catch-all
// bucket for entries whose source category doesn't map to one of the
// subsystem-specific values above (see DiagnosticsTimelineService.cpp for the
// exact mapping) — they are grouped by severity instead of by subsystem.
enum class TimelineCategory {
    Registration,
    Call,
    Sip,
    Media,
    Audio,
    Video,
    Rtt,
    Rtp,
    Camera,
    Settings,
    Diagnostics,
    Network,
    System,
    Warning,
    Error
};

enum class TimelineSeverity {
    Info,
    Warning,
    Error,
    Success
};

QString timelineCategoryName(TimelineCategory category);
TimelineCategory timelineCategoryFromName(const QString &name);
QString timelineSeverityName(TimelineSeverity severity);
TimelineSeverity timelineSeverityFromName(const QString &name);

// Foreground color for a severity — shared by the Timeline tab (per-row text
// color) and the Recent Activity widget.
QString timelineSeverityColor(TimelineSeverity severity);

// One row in the Diagnostics Timeline. Built exclusively by
// DiagnosticsTimelineService from already-existing signals (SipManager,
// AudioMediaManager, VideoMediaManager, CameraController, RttSession,
// Logger, ...) — never fabricated. See DiagnosticsTimelineService.cpp for
// exactly which signal produces which entry.
struct DiagnosticsTimelineEntry
{
    QString          id;
    QDateTime        timestamp;
    TimelineCategory category{TimelineCategory::System};
    TimelineSeverity severity{TimelineSeverity::Info};
    QString          title;
    QString          details;

    // Optional correlation fields — empty/0 when not applicable to this entry.
    QString          callId;
    QString          profileId;
    QString          remoteUri;
    int               sipCode{0};

    QString          colorHint;
    QString          iconHint;

    QJsonObject toJson() const;
    static DiagnosticsTimelineEntry fromJson(const QJsonObject &obj);
};

Q_DECLARE_METATYPE(DiagnosticsTimelineEntry)
