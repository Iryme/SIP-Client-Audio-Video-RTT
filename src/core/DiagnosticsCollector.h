#pragma once
#include <QObject>
#include <QTimer>

#include "core/DiagnosticsSnapshot.h"

// Builds DiagnosticsSnapshot instances from already-existing application
// singletons (SipManager, AudioMediaManager, VideoMediaManager, CameraController,
// MediaDeviceManager, VideoQualityManager, SipProfileManager, Logger) and
// re-emits them as a single event. DiagnosticsCenterPanel (and anything else
// that wants live diagnostics) reads only the emitted snapshot — it never
// queries SipManager or PJSIP directly, and this class never polls PJSIP
// synchronously on a UI repaint: rebuild() only reads cached getters that are
// already kept up to date by existing signals, and is triggered by a 1 Hz
// timer plus a handful of the existing state-change signals for responsiveness.
class DiagnosticsCollector : public QObject
{
    Q_OBJECT
public:
    static DiagnosticsCollector &instance();

    // Most recently built snapshot (built once immediately at construction).
    DiagnosticsSnapshot snapshot() const;

    // Forces an immediate rebuild + emit, bypassing the timer cadence.
    void refreshNow();

signals:
    void snapshotUpdated(const DiagnosticsSnapshot &snapshot);

private:
    DiagnosticsCollector();

    void rebuild();

    QTimer m_timer;
    DiagnosticsSnapshot m_snapshot;
};
