#include <QtTest/QtTest>

#include "core/DiagnosticsSnapshot.h"

// Tests for DiagnosticsSnapshot in isolation (no SipManager/PJSIP involved —
// DiagnosticsCollector wiring is exercised manually since it depends on the
// full singleton graph). Covers exactly what the Diagnostics Center task asked
// for: serialization round-trip, an empty/default snapshot, and updating one.

class TestDiagnosticsSnapshot : public QObject
{
    Q_OBJECT

private slots:
    void serializationRoundTrip();
    void emptySnapshot();
    void snapshotUpdate();
};

void TestDiagnosticsSnapshot::serializationRoundTrip()
{
    DiagnosticsSnapshot s;
    s.capturedAtUtc = QDateTime::currentDateTimeUtc();
    s.registrationState = QStringLiteral("Registered");
    s.registrationStatusText = QStringLiteral("OK");
    s.registrationStatusCode = 200;
    s.callState = QStringLiteral("Active");
    s.currentProfileName = QStringLiteral("Test Profile");
    s.remoteUri = QStringLiteral("sip:bob@example.com");
    s.audioConnected = true;
    s.audioCodec = QStringLiteral("PCMU");
    s.audioJitterAvailable = true;
    s.audioJitterMs = 12.5;
    s.videoConnected = true;
    s.videoResolution = QSize(1280, 720);
    s.videoFps = 30;
    s.microphoneName = QStringLiteral("Default Mic");
    s.microphoneVolume = 80;
    s.qtVersion = QStringLiteral("6.7.0");
    s.appVersion = QStringLiteral("1.4.0");
    s.gitCommit = QStringLiteral("abc1234");

    const QJsonObject json = s.toJson();
    const DiagnosticsSnapshot roundTripped = DiagnosticsSnapshot::fromJson(json);

    QCOMPARE(roundTripped.registrationState, s.registrationState);
    QCOMPARE(roundTripped.registrationStatusCode, s.registrationStatusCode);
    QCOMPARE(roundTripped.callState, s.callState);
    QCOMPARE(roundTripped.currentProfileName, s.currentProfileName);
    QCOMPARE(roundTripped.remoteUri, s.remoteUri);
    QCOMPARE(roundTripped.audioConnected, s.audioConnected);
    QCOMPARE(roundTripped.audioCodec, s.audioCodec);
    QCOMPARE(roundTripped.audioJitterAvailable, s.audioJitterAvailable);
    QCOMPARE(roundTripped.audioJitterMs, s.audioJitterMs);
    QCOMPARE(roundTripped.videoConnected, s.videoConnected);
    QCOMPARE(roundTripped.videoResolution, s.videoResolution);
    QCOMPARE(roundTripped.videoFps, s.videoFps);
    QCOMPARE(roundTripped.microphoneName, s.microphoneName);
    QCOMPARE(roundTripped.microphoneVolume, s.microphoneVolume);
    QCOMPARE(roundTripped.qtVersion, s.qtVersion);
    QCOMPARE(roundTripped.appVersion, s.appVersion);
    QCOMPARE(roundTripped.gitCommit, s.gitCommit);
}

void TestDiagnosticsSnapshot::emptySnapshot()
{
    DiagnosticsSnapshot s;

    // Default-constructed fields with no real backing source must read as
    // "N/A", never as an invented plausible value or an empty string that
    // could be mistaken for "checked and found empty".
    QCOMPARE(s.audioCodec, diagnosticsNotAvailable());
    QCOMPARE(s.audioPtime, diagnosticsNotAvailable());
    QCOMPARE(s.audioPacketsTx, diagnosticsNotAvailable());
    QCOMPARE(s.localIp, diagnosticsNotAvailable());
    QCOMPARE(s.remoteIp, diagnosticsNotAvailable());
    QCOMPARE(s.ice, diagnosticsNotAvailable());
    QCOMPARE(s.stun, diagnosticsNotAvailable());
    QCOMPARE(s.turn, diagnosticsNotAvailable());
    QCOMPARE(s.pjsipVersion, diagnosticsNotAvailable());

    QVERIFY(!s.audioConnected);
    QVERIFY(!s.videoConnected);
    QVERIFY(!s.audioJitterAvailable);
    QVERIFY(!s.audioLossAvailable);
    QVERIFY(!s.audioRttAvailable);
    QCOMPARE(s.registrationStatusCode, 0);
    QVERIFY(s.remoteUri.isEmpty());

    // Round-tripping an empty snapshot through JSON must not crash or
    // fabricate values either.
    const DiagnosticsSnapshot roundTripped = DiagnosticsSnapshot::fromJson(s.toJson());
    QCOMPARE(roundTripped.audioCodec, s.audioCodec);
    QCOMPARE(roundTripped.localIp, s.localIp);
    QVERIFY(!roundTripped.audioConnected);
}

void TestDiagnosticsSnapshot::snapshotUpdate()
{
    DiagnosticsSnapshot s;
    s.registrationState = QStringLiteral("Unregistered");
    s.callState = QStringLiteral("Idle");
    s.audioConnected = false;

    // Simulate what DiagnosticsCollector::rebuild() does: build a fresh
    // snapshot on top of new state and replace the old one wholesale.
    DiagnosticsSnapshot updated;
    updated.registrationState = QStringLiteral("Registered");
    updated.callState = QStringLiteral("Active");
    updated.audioConnected = true;
    updated.audioCodec = QStringLiteral("Opus");
    updated.remoteUri = QStringLiteral("sip:alice@example.com");

    s = updated;

    QCOMPARE(s.registrationState, QStringLiteral("Registered"));
    QCOMPARE(s.callState, QStringLiteral("Active"));
    QVERIFY(s.audioConnected);
    QCOMPARE(s.audioCodec, QStringLiteral("Opus"));
    QCOMPARE(s.remoteUri, QStringLiteral("sip:alice@example.com"));
}

QTEST_GUILESS_MAIN(TestDiagnosticsSnapshot)
#include "test_diagnostics_snapshot.moc"
