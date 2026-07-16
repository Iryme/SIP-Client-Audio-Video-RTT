#include <QtTest/QtTest>
#include <QCoreApplication>
#include <memory>

#include "media/MediaDevice.h"
#include "media/IMediaDeviceBackend.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "core/AppSettings.h"

static constexpr const char *kTestOrg = "IrymeTest";
static constexpr const char *kTestApp = "SIPClientTest_MediaDevice";

// ---------------------------------------------------------------------------
// Stub backend — injects a fixed device list into MediaDeviceManager
// ---------------------------------------------------------------------------
class StubMediaDeviceBackend : public IMediaDeviceBackend
{
public:
    QList<MediaDevice> m_mics;
    QList<MediaDevice> m_speakers;
    QList<MediaDevice> m_cameras;

    QList<MediaDevice> microphones() const override { return m_mics;     }
    QList<MediaDevice> speakers()    const override { return m_speakers; }
    QList<MediaDevice> cameras()     const override { return m_cameras;  }

    static MediaDevice makeDevice(const QString &id, const QString &name,
                                  MediaDeviceType type, bool isDefault = false)
    {
        MediaDevice d;
        d.id          = id;
        d.displayName = name;
        d.type        = type;
        d.isDefault   = isDefault;
        d.isAvailable = true;
        return d;
    }
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class TestMediaDevice : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void fallbackToDefaultWhenSelectedMissing();
    void selectedDevicePersistence();
    void emptyDeviceListHandling();
    void deviceTypeFiltering();
    void remoteAudioExcludedFromPhysicalDeviceLists();
    void defaultSpeakerAndMicrophoneNeverRemoteAudio();
    void allowRedirectedAudioDevicesOptInSurfacesRemoteAudio();
};

static void purgeTestSettings()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kTestApp);
    s.remove("media/device/microphone");
    s.remove("media/device/speaker");
    s.remove("media/device/camera");
    s.sync();
}

static void injectStub(StubMediaDeviceBackend *stub)
{
    MediaDeviceManager::instance().setBackend(
        std::unique_ptr<IMediaDeviceBackend>(stub));
}

void TestMediaDevice::initTestCase()
{
    QCoreApplication::setOrganizationName(kTestOrg);
    QCoreApplication::setApplicationName(kTestApp);
    purgeTestSettings();
}

void TestMediaDevice::cleanupTestCase()
{
    purgeTestSettings();
}

void TestMediaDevice::init()
{
    purgeTestSettings();
    // Reset manager to empty state before each test
    auto *stub = new StubMediaDeviceBackend;
    injectStub(stub);
}

// ---------------------------------------------------------------------------
// 1. fallbackToDefaultWhenSelectedMissing
//    Persist id "mic-2" but only supply "mic-1" in the stub.
//    selectedMicrophone() must return "mic-1" (the default) and warn.
// ---------------------------------------------------------------------------
void TestMediaDevice::fallbackToDefaultWhenSelectedMissing()
{
    auto *stub = new StubMediaDeviceBackend;
    stub->m_mics = {
        StubMediaDeviceBackend::makeDevice("mic-1", "Built-in Mic",
                                           MediaDeviceType::Microphone, true)
    };
    injectStub(stub);

    // Persist an id that is NOT in the device list
    AppSettings::saveSelectedMicrophone("mic-2");

    MediaDeviceSelectionModel model(&MediaDeviceManager::instance());
    const MediaDevice resolved = model.selectedMicrophone();

    QCOMPARE(resolved.id, QStringLiteral("mic-1"));
}

// ---------------------------------------------------------------------------
// 2. selectedDevicePersistence
//    Select "mic-2"; verify AppSettings stores "mic-2" and re-resolve returns it.
// ---------------------------------------------------------------------------
void TestMediaDevice::selectedDevicePersistence()
{
    auto *stub = new StubMediaDeviceBackend;
    stub->m_mics = {
        StubMediaDeviceBackend::makeDevice("mic-1", "Built-in Mic",
                                           MediaDeviceType::Microphone, true),
        StubMediaDeviceBackend::makeDevice("mic-2", "USB Headset",
                                           MediaDeviceType::Microphone)
    };
    injectStub(stub);

    MediaDeviceSelectionModel model(&MediaDeviceManager::instance());
    model.selectMicrophone("mic-2");

    // Persisted id must match
    QCOMPARE(AppSettings::loadSelectedMicrophone(), QStringLiteral("mic-2"));

    // Fresh resolve also returns mic-2
    const MediaDevice resolved = model.selectedMicrophone();
    QCOMPARE(resolved.id, QStringLiteral("mic-2"));
}

// ---------------------------------------------------------------------------
// 3. emptyDeviceListHandling
//    If no microphones/cameras exist, the selection model returns null devices.
// ---------------------------------------------------------------------------
void TestMediaDevice::emptyDeviceListHandling()
{
    auto *stub = new StubMediaDeviceBackend;
    // all lists remain empty
    injectStub(stub);

    MediaDeviceSelectionModel model(&MediaDeviceManager::instance());

    QVERIFY(model.selectedMicrophone().isNull());
    QVERIFY(model.selectedSpeaker().isNull());
    QVERIFY(model.selectedCamera().isNull());
}

// ---------------------------------------------------------------------------
// 4. deviceTypeFiltering
//    findDevice() with the wrong type must not match even if id matches.
// ---------------------------------------------------------------------------
void TestMediaDevice::deviceTypeFiltering()
{
    auto *stub = new StubMediaDeviceBackend;
    stub->m_mics = {
        StubMediaDeviceBackend::makeDevice("dev-1", "Built-in Mic",
                                           MediaDeviceType::Microphone, true)
    };
    stub->m_speakers = {
        StubMediaDeviceBackend::makeDevice("dev-2", "Built-in Speaker",
                                           MediaDeviceType::Speaker, true)
    };
    stub->m_cameras = {
        StubMediaDeviceBackend::makeDevice("dev-3", "Front Camera",
                                           MediaDeviceType::Camera, true)
    };
    injectStub(stub);

    auto &mgr = MediaDeviceManager::instance();

    // Each id found only for its own type
    QVERIFY(!mgr.findDevice(MediaDeviceType::Microphone, "dev-1").isNull());
    QVERIFY( mgr.findDevice(MediaDeviceType::Speaker,    "dev-1").isNull()); // wrong type
    QVERIFY(!mgr.findDevice(MediaDeviceType::Speaker,    "dev-2").isNull());
    QVERIFY( mgr.findDevice(MediaDeviceType::Camera,     "dev-2").isNull()); // wrong type
    QVERIFY(!mgr.findDevice(MediaDeviceType::Camera,     "dev-3").isNull());
    QVERIFY( mgr.findDevice(MediaDeviceType::Microphone, "dev-3").isNull()); // wrong type
}

// ---------------------------------------------------------------------------
// 5. remoteAudioExcludedFromPhysicalDeviceLists
//    A backend that reports a virtual "Remote Audio" endpoint (e.g. Windows
//    Remote Desktop's redirected audio device) must never surface it as a
//    selectable physical Speaker/Microphone.
// ---------------------------------------------------------------------------
void TestMediaDevice::remoteAudioExcludedFromPhysicalDeviceLists()
{
    auto *stub = new StubMediaDeviceBackend;
    stub->m_mics = {
        StubMediaDeviceBackend::makeDevice("mic-1", "Built-in Mic",
                                           MediaDeviceType::Microphone, true),
        StubMediaDeviceBackend::makeDevice("mic-2", "Remote Audio",
                                           MediaDeviceType::Microphone)
    };
    stub->m_speakers = {
        StubMediaDeviceBackend::makeDevice("spk-1", "Built-in Speaker",
                                           MediaDeviceType::Speaker, true),
        StubMediaDeviceBackend::makeDevice("spk-2", "Remote Audio",
                                           MediaDeviceType::Speaker)
    };
    injectStub(stub);

    auto &mgr = MediaDeviceManager::instance();

    QCOMPARE(mgr.listMicrophones().size(), 1);
    QCOMPARE(mgr.listSpeakers().size(), 1);
    QVERIFY(mgr.findDevice(MediaDeviceType::Microphone, "mic-2").isNull());
    QVERIFY(mgr.findDevice(MediaDeviceType::Speaker, "spk-2").isNull());
}

// ---------------------------------------------------------------------------
// 6. defaultSpeakerAndMicrophoneNeverRemoteAudio
//    Even if the backend marks the "Remote Audio" endpoint as the OS default,
//    it must be filtered out before default resolution runs, so the fallback
//    default is a real physical device (or null if none exist).
// ---------------------------------------------------------------------------
void TestMediaDevice::defaultSpeakerAndMicrophoneNeverRemoteAudio()
{
    auto *stub = new StubMediaDeviceBackend;
    stub->m_mics = {
        StubMediaDeviceBackend::makeDevice("mic-1", "Remote Audio",
                                           MediaDeviceType::Microphone, /*isDefault=*/true),
        StubMediaDeviceBackend::makeDevice("mic-2", "USB Headset",
                                           MediaDeviceType::Microphone)
    };
    stub->m_speakers = {
        StubMediaDeviceBackend::makeDevice("spk-1", "Remote Audio",
                                           MediaDeviceType::Speaker, /*isDefault=*/true)
    };
    injectStub(stub);

    auto &mgr = MediaDeviceManager::instance();

    QCOMPARE(mgr.defaultMicrophone().id, QStringLiteral("mic-2"));
    QVERIFY(mgr.defaultSpeaker().isNull());
}

// ---------------------------------------------------------------------------
// 7. allowRedirectedAudioDevicesOptInSurfacesRemoteAudio
//    AppSettings::allowRedirectedAudioDevices() is an explicit opt-in (off by
//    default, see tests 5/6 above) for machines with no physical audio
//    hardware at all -- when enabled, "Remote Audio" must appear and be
//    selectable/default like any other device.
// ---------------------------------------------------------------------------
void TestMediaDevice::allowRedirectedAudioDevicesOptInSurfacesRemoteAudio()
{
    AppSettings::setAllowRedirectedAudioDevices(true);

    auto *stub = new StubMediaDeviceBackend;
    stub->m_mics = {
        StubMediaDeviceBackend::makeDevice("mic-1", "Remote Audio",
                                           MediaDeviceType::Microphone, /*isDefault=*/true)
    };
    stub->m_speakers = {
        StubMediaDeviceBackend::makeDevice("spk-1", "Remote Audio",
                                           MediaDeviceType::Speaker, /*isDefault=*/true)
    };
    injectStub(stub);

    auto &mgr = MediaDeviceManager::instance();

    QCOMPARE(mgr.listMicrophones().size(), 1);
    QCOMPARE(mgr.listSpeakers().size(), 1);
    QCOMPARE(mgr.defaultMicrophone().id, QStringLiteral("mic-1"));
    QCOMPARE(mgr.defaultSpeaker().id, QStringLiteral("spk-1"));

    AppSettings::setAllowRedirectedAudioDevices(false);
}

QTEST_GUILESS_MAIN(TestMediaDevice)
#include "test_media_device.moc"
