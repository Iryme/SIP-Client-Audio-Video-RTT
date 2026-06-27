#include "CallPanel.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "emergency/EmergencyCallAdapter.h"
#include "emergency/EmergencyCallController.h"
#include "emergency/EmergencyInviteBuilder.h"
#include "emergency/EmergencyLocation.h"
#include "emergency/PidfLoBuilder.h"
#include "emergency/StaticLocationProvider.h"
#include "media/AudioMediaManager.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoMediaManager.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"
#include "sip/SipUriNormalizer.h"

CallPanel::CallPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("CallPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);

    // --- Call info row -------------------------------------------------------
    auto *infoRow = new QHBoxLayout();

    m_remoteName = new QLabel(tr("No active call"), this);
    m_remoteName->setObjectName("RemoteName");
    m_remoteName->setStyleSheet("font-size: 16px; font-weight: bold;");

    m_callState = new QLabel(tr("Idle"), this);
    m_callState->setObjectName("CallState");
    m_callState->setStyleSheet("color: #aaaaaa; font-size: 12px;");

    m_duration = new QLabel(tr("00:00:00"), this);
    m_duration->setObjectName("Duration");
    m_duration->setStyleSheet("color: #aaaaaa; font-size: 12px; font-family: monospace;");

    infoRow->addWidget(m_remoteName);
    infoRow->addStretch();
    infoRow->addWidget(m_callState);
    infoRow->addSpacing(12);
    infoRow->addWidget(m_duration);

    m_remoteUri = new QLabel(tr("—"), this);
    m_remoteUri->setObjectName("RemoteUri");
    m_remoteUri->setStyleSheet("color: #888888; font-size: 11px;");

    layout->addLayout(infoRow);
    layout->addWidget(m_remoteUri);

    // --- Separator -----------------------------------------------------------
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep);

    // --- Audio level meters --------------------------------------------------
    auto *meterRow = new QHBoxLayout();
    meterRow->setSpacing(6);

    auto *micLabel = new QLabel(tr("Mic:"), this);
    micLabel->setStyleSheet("color: #888888; font-size: 10px;");
    micLabel->setFixedWidth(24);

    m_inputMeter = new QProgressBar(this);
    m_inputMeter->setObjectName("InputMeter");
    m_inputMeter->setRange(0, 100);
    m_inputMeter->setValue(0);
    m_inputMeter->setTextVisible(false);
    m_inputMeter->setFixedHeight(8);
    m_inputMeter->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; }"
        "QProgressBar::chunk { background: #50c878; border-radius: 2px; }");

    auto *spkLabel = new QLabel(tr("Spk:"), this);
    spkLabel->setStyleSheet("color: #888888; font-size: 10px;");
    spkLabel->setFixedWidth(24);

    m_outputMeter = new QProgressBar(this);
    m_outputMeter->setObjectName("OutputMeter");
    m_outputMeter->setRange(0, 100);
    m_outputMeter->setValue(0);
    m_outputMeter->setTextVisible(false);
    m_outputMeter->setFixedHeight(8);
    m_outputMeter->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; }"
        "QProgressBar::chunk { background: #5090e0; border-radius: 2px; }");

    meterRow->addWidget(micLabel);
    meterRow->addWidget(m_inputMeter);
    meterRow->addSpacing(8);
    meterRow->addWidget(spkLabel);
    meterRow->addWidget(m_outputMeter);

    layout->addLayout(meterRow);

    // --- Device selector row (hidden when idle) -------------------------------
    m_deviceRow = new QWidget(this);
    auto *devLayout = new QHBoxLayout(m_deviceRow);
    devLayout->setContentsMargins(0, 0, 0, 0);
    devLayout->setSpacing(8);

    auto *micDevLabel = new QLabel(tr("Microphone:"), m_deviceRow);
    micDevLabel->setStyleSheet("color: #888888; font-size: 10px;");

    m_micSelector = new QComboBox(m_deviceRow);
    m_micSelector->setObjectName("MicSelector");
    m_micSelector->setFixedHeight(24);

    auto *spkDevLabel = new QLabel(tr("Speaker:"), m_deviceRow);
    spkDevLabel->setStyleSheet("color: #888888; font-size: 10px;");

    m_spkSelector = new QComboBox(m_deviceRow);
    m_spkSelector->setObjectName("SpkSelector");
    m_spkSelector->setFixedHeight(24);

    devLayout->addWidget(micDevLabel);
    devLayout->addWidget(m_micSelector, 1);
    devLayout->addWidget(spkDevLabel);
    devLayout->addWidget(m_spkSelector, 1);

    m_deviceRow->setVisible(true);
    layout->addWidget(m_deviceRow);

    // --- Call control buttons ------------------------------------------------
    auto *ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(8);

    auto makeBtn = [&](const QString &label, bool checkable = false) -> QPushButton* {
        auto *btn = new QPushButton(label, this);
        btn->setObjectName("CallCtrlBtn");
        btn->setCheckable(checkable);
        btn->setFixedHeight(32);
        btn->setMinimumWidth(72);
        return btn;
    };

    m_btnMute   = makeBtn(tr("Mute"),    true);
    m_btnVideo  = makeBtn(tr("Video"),   true);
    m_btnShare  = makeBtn(tr("Share"),   false);
    m_btnHold   = makeBtn(tr("Hold"),    true);
    m_btnKeypad = makeBtn(tr("Keypad"),  true);
    m_btnRecord = makeBtn(tr("Record"),  true);
    m_btnAnswer = makeBtn(tr("Answer"),  false);
    m_btnReject = makeBtn(tr("Reject"),  false);
    m_btnHangup = makeBtn(tr("Hangup"),  false);

    m_btnAnswer->setObjectName("AnswerBtn");
    m_btnReject->setObjectName("RejectBtn");
    m_btnHangup->setObjectName("HangupBtn");

    // Share and Record are placeholders.
    m_btnShare->setEnabled(false);
    m_btnRecord->setEnabled(false);

    ctrlRow->addWidget(m_btnMute);
    ctrlRow->addWidget(m_btnVideo);
    ctrlRow->addWidget(m_btnShare);
    ctrlRow->addWidget(m_btnHold);
    ctrlRow->addWidget(m_btnKeypad);
    ctrlRow->addWidget(m_btnRecord);
    ctrlRow->addStretch();
    ctrlRow->addWidget(m_btnAnswer);
    ctrlRow->addWidget(m_btnReject);
    ctrlRow->addWidget(m_btnHangup);

    layout->addLayout(ctrlRow);

    // --- Dial row (visible only when Idle) -----------------------------------
    m_dialRow = new QWidget(this);
    m_dialRow->setObjectName("DialRow");
    m_dialRow->setStyleSheet(
        "QWidget#DialRow { background: #1e2a1e; border: 1px solid #3a5a3a; border-radius: 5px; }");

    auto *dialOuter = new QVBoxLayout(m_dialRow);
    dialOuter->setContentsMargins(10, 8, 10, 8);
    dialOuter->setSpacing(4);

    m_regStatusLabel = new QLabel(tr("Not registered — register a SIP profile first"), m_dialRow);
    m_regStatusLabel->setObjectName("DialRegStatus");
    m_regStatusLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
    dialOuter->addWidget(m_regStatusLabel);

    auto *dialLayout = new QHBoxLayout();
    dialLayout->setSpacing(6);

    m_dialInput = new QLineEdit(m_dialRow);
    m_dialInput->setObjectName("DialInput");
    m_dialInput->setPlaceholderText(tr("sip:user@domain  or  user@domain"));
    m_dialInput->setFixedHeight(30);

    m_btnCall = new QPushButton(tr("Call"), m_dialRow);
    m_btnCall->setObjectName("CallBtn");
    m_btnCall->setFixedHeight(30);
    m_btnCall->setMinimumWidth(64);

    dialLayout->addWidget(m_dialInput, 1);
    dialLayout->addWidget(m_btnCall);
    dialOuter->addLayout(dialLayout);
    layout->addWidget(m_dialRow);

    // --- Emergency test mode section (hidden by default) ---------------------
    // Visible only when emergency/testMode=true in SIPClient.ini.
    // Never used for real emergency calls — lab/test only.
    m_emergencyRow = new QWidget(this);
    m_emergencyRow->setObjectName("EmergencyRow");
    m_emergencyRow->setStyleSheet(
        "QWidget#EmergencyRow { background: #2a1200; border: 1px solid #8b3a00;"
        " border-radius: 5px; }");

    auto *emerOuter = new QVBoxLayout(m_emergencyRow);
    emerOuter->setContentsMargins(10, 8, 10, 8);
    emerOuter->setSpacing(4);

    auto *emerWarnLabel = new QLabel(
        tr("[TEST/LAB] Emergency Test Mode — NOT a real emergency service"), m_emergencyRow);
    emerWarnLabel->setObjectName("EmergencyWarnLabel");
    emerWarnLabel->setStyleSheet("color: #ff8c00; font-size: 10px; font-weight: bold;");
    emerWarnLabel->setWordWrap(true);
    emerOuter->addWidget(emerWarnLabel);

    // --- Manual location input section ---------------------------------------
    // No Windows Location API — coordinates entered manually by the user.
    auto *locHeaderLabel = new QLabel(
        tr("[TEST/LAB] Manual Location — enter WGS-84 coordinates below. "
           "No GPS, no Windows Location."), m_emergencyRow);
    locHeaderLabel->setStyleSheet("color: #cc7700; font-size: 9px;");
    locHeaderLabel->setWordWrap(true);
    emerOuter->addWidget(locHeaderLabel);

    auto *locInputRow = new QHBoxLayout();
    locInputRow->setSpacing(4);

    auto *latLabel = new QLabel(tr("Lat:"), m_emergencyRow);
    latLabel->setStyleSheet("color: #888888; font-size: 10px;");
    m_latInput = new QLineEdit(QStringLiteral("44.4268"), m_emergencyRow);
    m_latInput->setObjectName("LocLatInput");
    m_latInput->setFixedWidth(72);
    m_latInput->setFixedHeight(22);
    m_latInput->setToolTip(tr("-90 to 90 degrees (WGS-84)"));

    auto *lonLabel = new QLabel(tr("Lon:"), m_emergencyRow);
    lonLabel->setStyleSheet("color: #888888; font-size: 10px;");
    m_lonInput = new QLineEdit(QStringLiteral("26.1025"), m_emergencyRow);
    m_lonInput->setObjectName("LocLonInput");
    m_lonInput->setFixedWidth(72);
    m_lonInput->setFixedHeight(22);
    m_lonInput->setToolTip(tr("-180 to 180 degrees (WGS-84)"));

    auto *uncLabel = new QLabel(tr("Unc(m):"), m_emergencyRow);
    uncLabel->setStyleSheet("color: #888888; font-size: 10px;");
    m_uncertaintyInput = new QLineEdit(QStringLiteral("50.0"), m_emergencyRow);
    m_uncertaintyInput->setObjectName("LocUncInput");
    m_uncertaintyInput->setFixedWidth(56);
    m_uncertaintyInput->setFixedHeight(22);
    m_uncertaintyInput->setToolTip(tr("Accuracy radius in meters (>= 0)"));

    m_btnGeneratePidf = new QPushButton(tr("Generate PIDF-LO"), m_emergencyRow);
    m_btnGeneratePidf->setObjectName("GeneratePidfBtn");
    m_btnGeneratePidf->setFixedHeight(22);
    m_btnGeneratePidf->setStyleSheet("font-size: 10px;");

    locInputRow->addWidget(latLabel);
    locInputRow->addWidget(m_latInput);
    locInputRow->addWidget(lonLabel);
    locInputRow->addWidget(m_lonInput);
    locInputRow->addWidget(uncLabel);
    locInputRow->addWidget(m_uncertaintyInput);
    locInputRow->addWidget(m_btnGeneratePidf);
    locInputRow->addStretch();
    emerOuter->addLayout(locInputRow);

    m_locationStatusLabel = new QLabel(
        tr("Location: not generated — enter coordinates and click Generate PIDF-LO"),
        m_emergencyRow);
    m_locationStatusLabel->setObjectName("LocationStatusLabel");
    m_locationStatusLabel->setStyleSheet("color: #888888; font-size: 9px;");
    m_locationStatusLabel->setWordWrap(true);
    emerOuter->addWidget(m_locationStatusLabel);

    m_pidfPreview = new QPlainTextEdit(m_emergencyRow);
    m_pidfPreview->setObjectName("PidfPreview");
    m_pidfPreview->setReadOnly(true);
    m_pidfPreview->setMaximumHeight(56);
    m_pidfPreview->setPlaceholderText(tr("PIDF-LO XML appears here after Generate is clicked"));
    m_pidfPreview->setStyleSheet(
        "QPlainTextEdit { font-family: monospace; font-size: 8px;"
        " color: #999999; background: #1a1a1a; border: 1px solid #333; }");
    emerOuter->addWidget(m_pidfPreview);

    // --- Control row: state label + emergency call button + location update --
    auto *emerCtrlRow = new QHBoxLayout();
    emerCtrlRow->setSpacing(6);

    m_emergencyStateLabel = new QLabel(tr("State: Idle"), m_emergencyRow);
    m_emergencyStateLabel->setObjectName("EmergencyStateLabel");
    m_emergencyStateLabel->setStyleSheet("color: #aaaaaa; font-size: 10px;");
    emerCtrlRow->addWidget(m_emergencyStateLabel, 1);

    m_btnLocationUpdate = new QPushButton(tr("Send Location Update"), m_emergencyRow);
    m_btnLocationUpdate->setObjectName("LocationUpdateBtn");
    m_btnLocationUpdate->setFixedHeight(30);
    m_btnLocationUpdate->setEnabled(false);
    m_btnLocationUpdate->setStyleSheet(
        "QPushButton#LocationUpdateBtn { background: #1a3a4a; color: #5090c0;"
        " border-radius: 4px; font-size: 10px; }"
        "QPushButton#LocationUpdateBtn:hover { background: #2a4a5a; }"
        "QPushButton#LocationUpdateBtn:disabled { background: #1a2020; color: #444444; }");
    emerCtrlRow->addWidget(m_btnLocationUpdate);

    m_btnEmergency = new QPushButton(tr("112 Emergency (TEST)"), m_emergencyRow);
    m_btnEmergency->setObjectName("EmergencyBtn");
    m_btnEmergency->setFixedHeight(30);
    m_btnEmergency->setMinimumWidth(160);
    m_btnEmergency->setEnabled(false); // enabled only when registered
    m_btnEmergency->setStyleSheet(
        "QPushButton#EmergencyBtn { background: #8b0000; color: white;"
        " border-radius: 4px; font-weight: bold; }"
        "QPushButton#EmergencyBtn:hover { background: #b00000; }"
        "QPushButton#EmergencyBtn:disabled { background: #3a2020; color: #666666; }");
    emerCtrlRow->addWidget(m_btnEmergency);

    emerOuter->addLayout(emerCtrlRow);
    layout->addWidget(m_emergencyRow);

    // Hide unless test mode is explicitly enabled in settings.
    m_emergencyRow->setVisible(AppSettings::emergencyTestModeEnabled());

    // --- Emergency controller setup ------------------------------------------
    // DEMO static location: Bucharest, 44.4268°N 26.1025°E, uncertainty 50 m.
    // No Windows Location API — StaticLocationProvider only.
    QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!ts.endsWith('Z')) ts += 'Z';
    EmergencyLocation demoLoc = EmergencyLocation::makeStatic(44.4268, 26.1025, ts);
    demoLoc.uncertaintyMeters = 50.0;
    demoLoc.source = LocationSource::Static;

    m_staticLocationProvider = new StaticLocationProvider(demoLoc, this);
    m_emergencyController    = new EmergencyCallController(this);
    m_emergencyController->setLocationProvider(m_staticLocationProvider);

    const EmergencyCallProfile emProfile = EmergencyCallProfile::makeSos(
        AppSettings::emergencyTarget(), QStringLiteral("NG112-TEST"));
    m_emergencyController->setProfile(emProfile);

    // --- Internal signal wiring ----------------------------------------------
    connect(m_btnMute,   &QPushButton::toggled, this, &CallPanel::muteToggled);
    connect(m_btnVideo,  &QPushButton::toggled, this, &CallPanel::videoToggled);
    connect(m_btnHold,   &QPushButton::toggled, this, &CallPanel::holdToggled);
    connect(m_btnHangup, &QPushButton::clicked, this, &CallPanel::hangupRequested);
    connect(m_btnAnswer, &QPushButton::clicked, this, &CallPanel::answerRequested);
    connect(m_btnReject, &QPushButton::clicked, this, &CallPanel::rejectRequested);
    connect(m_btnKeypad, &QPushButton::toggled, this, &CallPanel::keypadToggled);

    // Mute button → AudioMediaManager
    connect(m_btnMute, &QPushButton::toggled,
            [](bool checked){ AudioMediaManager::instance().setMuted(checked); });

    // AudioMediaManager → mute button sync + level meters
    connect(&AudioMediaManager::instance(), &AudioMediaManager::mutedChanged,
            this, &CallPanel::onMuteChanged);
    connect(&AudioMediaManager::instance(), &AudioMediaManager::inputLevelChanged,
            this, &CallPanel::onInputLevelChanged);
    connect(&AudioMediaManager::instance(), &AudioMediaManager::outputLevelChanged,
            this, &CallPanel::onOutputLevelChanged);

    // Video button → VideoMediaManager (mute / unmute video stream)
    connect(m_btnVideo, &QPushButton::toggled,
            [](bool checked){ VideoMediaManager::instance().setVideoMuted(checked); });

    // VideoMediaManager → video button sync
    connect(&VideoMediaManager::instance(), &VideoMediaManager::videoMutedChanged,
            this, &CallPanel::onVideoMuteChanged);

    // Hold button → SipManager
    connect(this, &CallPanel::holdToggled, [](bool held) {
        if (held) SipManager::instance().holdCall();
        else      SipManager::instance().resumeCall();
    });

    // Answer / Reject / Hangup → SipManager
    connect(this, &CallPanel::answerRequested,
            []{ SipManager::instance().answerCall(); });
    connect(this, &CallPanel::rejectRequested,
            []{ SipManager::instance().rejectCall(); });
    connect(this, &CallPanel::hangupRequested,
            []{ SipManager::instance().hangupCall(); });

    // Device combos → AudioMediaManager (empty id = system default)
    connect(m_micSelector, &QComboBox::currentIndexChanged, this, [this](int idx) {
        AudioMediaManager::instance().setMicrophone(m_micSelector->itemData(idx).toString());
    });
    connect(m_spkSelector, &QComboBox::currentIndexChanged, this, [this](int idx) {
        AudioMediaManager::instance().setSpeaker(m_spkSelector->itemData(idx).toString());
    });

    // SipManager call signals
    connect(&SipManager::instance(), &SipManager::callStateChanged,
            this, &CallPanel::onCallStateChanged);
    connect(&SipManager::instance(), &SipManager::incomingCall,
            this, &CallPanel::onIncomingCall);
    connect(&SipManager::instance(), &SipManager::callConnected,
            this, &CallPanel::onCallConnected);
    connect(&SipManager::instance(), &SipManager::callDisconnected,
            this, &CallPanel::onCallDisconnected);
    connect(&SipManager::instance(), &SipManager::callFailed,
            this, &CallPanel::onCallFailed);

    // Dial row: Call button and Enter key both trigger makeCall.
    auto triggerCall = [this] {
        const QString raw = m_dialInput->text().trimmed();
        if (raw.isEmpty()) {
            m_regStatusLabel->setText(tr("Enter a SIP URI or extension to call"));
            m_regStatusLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
            return;
        }
        const QString fallbackDomain =
            SipProfileManager::instance().activeProfile().sipDomain;
        const SipUriNormalizer::Result result =
            SipUriNormalizer::normalize(raw, fallbackDomain);
        if (!result.isValid) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("Dial URI invalid: input=\"%1\" error=\"%2\"")
                    .arg(raw, result.error));
            m_regStatusLabel->setText(tr("Invalid URI: %1").arg(result.error));
            m_regStatusLabel->setStyleSheet("color: #e05050; font-size: 10px;");
            return;
        }
        if (result.uri != raw) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Dial URI normalized: \"%1\" → \"%2\"")
                    .arg(raw, result.uri));
            m_dialInput->setText(result.uri);
        }
        SipManager::instance().makeCall(result.uri);
    };
    connect(m_btnCall,  &QPushButton::clicked,  this, triggerCall);
    connect(m_dialInput, &QLineEdit::returnPressed, this, triggerCall);

    // Dial row: update registration status label and button enable.
    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            this, [this](RegistrationState state, const QString &, int) {
        const bool registered = (state == RegistrationState::Registered);
        m_btnCall->setEnabled(registered);
        if (registered) {
            m_regStatusLabel->setText(tr("Registered — enter a SIP URI and press Call"));
            m_regStatusLabel->setStyleSheet("color: #50c878; font-size: 10px;");
        } else if (state == RegistrationState::Registering) {
            m_regStatusLabel->setText(tr("Registering…"));
            m_regStatusLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
        } else {
            m_regStatusLabel->setText(tr("Not registered — register a SIP profile first"));
            m_regStatusLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
        }
        // Emergency button: requires registration + Idle/Failed SM state.
        if (m_btnEmergency) {
            const EmergencyCallState es = m_emergencyController->state();
            const bool canDial = (es == EmergencyCallState::Idle
                                  || es == EmergencyCallState::Failed
                                  || es == EmergencyCallState::Ended);
            m_btnEmergency->setEnabled(registered && canDial);
        }
    });

    // Emergency controller signals.
    connect(m_emergencyController, &EmergencyCallController::stateChanged,
            this, &CallPanel::onEmergencyStateChanged);
    connect(m_emergencyController, &EmergencyCallController::readyToDial,
            this, &CallPanel::onEmergencyReadyToDial);
    connect(m_emergencyController, &EmergencyCallController::preparationFailed,
            this, &CallPanel::onEmergencyFailed);

    // Emergency button click → confirm dialog → prepare().
    connect(m_btnEmergency, &QPushButton::clicked, this, &CallPanel::onEmergencyButtonClicked);

    // Manual location buttons.
    connect(m_btnGeneratePidf, &QPushButton::clicked, this, &CallPanel::onGeneratePidfClicked);
    connect(m_btnLocationUpdate, &QPushButton::clicked, this, &CallPanel::onLocationUpdateClicked);

    // SipManager call events → update emergency SM when an emergency call is active.
    connect(&SipManager::instance(), &SipManager::callConnected,
            this, [this](const QString &) {
        if (m_emergencyCallActive)
            m_emergencyController->stateMachine().transition(
                EmergencyCallState::Active, QStringLiteral("call connected"));
    });
    connect(&SipManager::instance(), &SipManager::callDisconnected,
            this, [this](const QString &, const QString &, int) {
        if (m_emergencyCallActive) {
            m_emergencyCallActive = false;
            if (m_btnLocationUpdate) m_btnLocationUpdate->setEnabled(false);
            m_emergencyController->stateMachine().transition(
                EmergencyCallState::Ended, QStringLiteral("call ended"));
            m_emergencyController->stateMachine().reset();
        }
    });
    connect(&SipManager::instance(), &SipManager::callFailed,
            this, [this](const QString &, const QString &reason, int) {
        if (m_emergencyCallActive) {
            m_emergencyCallActive = false;
            if (m_btnLocationUpdate) m_btnLocationUpdate->setEnabled(false);
            m_emergencyController->stateMachine().transition(
                EmergencyCallState::Failed,
                QStringLiteral("call failed: %1").arg(reason));
        }
    });

    // Populate device combos once at construction so they're always visible.
    populateDeviceCombos();

    // Initial idle state
    applyCallState(CallState::Idle);
}

// ---------------------------------------------------------------------------

void CallPanel::setRemoteName(const QString &name)  { m_remoteName->setText(name); }
void CallPanel::setRemoteUri(const QString &uri)    { m_remoteUri->setText(uri); }
void CallPanel::setCallState(const QString &state)  { m_callState->setText(state); }
void CallPanel::setDuration(const QString &dur)     { m_duration->setText(dur); }

void CallPanel::populateDeviceCombos()
{
    // Preserve current selection
    const QString curMic = m_micSelector->currentData().toString();
    const QString curSpk = m_spkSelector->currentData().toString();

    // Resolve persisted defaults
    MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    const QString defaultMicId = sel.selectedMicrophone().id;
    const QString defaultSpkId = sel.selectedSpeaker().id;

    m_micSelector->blockSignals(true);
    m_spkSelector->blockSignals(true);

    m_micSelector->clear();
    m_micSelector->addItem(tr("Default (system)"), QString{});
    for (const MediaDevice &d : MediaDeviceManager::instance().listMicrophones())
        m_micSelector->addItem(d.displayName, d.id);

    m_spkSelector->clear();
    m_spkSelector->addItem(tr("Default (system)"), QString{});
    for (const MediaDevice &d : MediaDeviceManager::instance().listSpeakers())
        m_spkSelector->addItem(d.displayName, d.id);

    // Restore selection: prefer the previous call-panel selection, then the persisted default.
    auto selectById = [](QComboBox *combo, const QString &preferred, const QString &fallback) {
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == preferred) { combo->setCurrentIndex(i); return; }
        }
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == fallback)  { combo->setCurrentIndex(i); return; }
        }
    };
    selectById(m_micSelector, curMic, defaultMicId);
    selectById(m_spkSelector, curSpk, defaultSpkId);

    m_micSelector->blockSignals(false);
    m_spkSelector->blockSignals(false);
}

void CallPanel::applyCallState(CallState state)
{
    const bool isIdle     = (state == CallState::Idle);
    const bool isFailed   = (state == CallState::Failed);
    const bool isIncoming = (state == CallState::IncomingRinging);
    const bool isActive   = (state == CallState::Active || state == CallState::Held);
    const bool hasCall    = !isIdle && !isFailed;

    m_btnAnswer->setVisible(isIncoming);
    m_btnReject->setVisible(isIncoming);
    m_btnHangup->setVisible(!isIncoming && hasCall);
    m_btnHold->setEnabled(isActive);
    m_btnMute->setEnabled(state == CallState::Active);
    m_btnVideo->setEnabled(state == CallState::Active);
    m_btnKeypad->setEnabled(state == CallState::Active);

    // Dial row: visible when Idle or Failed so the user can retry after a failed call.
    m_dialRow->setVisible(isIdle || isFailed);
    if (isIdle || isFailed) {
        const bool registered =
            (SipManager::instance().registrationState() == RegistrationState::Registered);
        m_btnCall->setEnabled(registered);
        if (isIdle) {
            // Reset status label to registration hint when returning to Idle normally.
            if (registered) {
                m_regStatusLabel->setText(tr("Registered — enter a SIP URI and press Call"));
                m_regStatusLabel->setStyleSheet("color: #50c878; font-size: 10px;");
            } else {
                m_regStatusLabel->setText(tr("Not registered — register a SIP profile first"));
                m_regStatusLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
            }
        }
        // For Failed: leave the label for onCallFailed to fill with the specific reason.
    }

    // Device selectors always visible (Jitsi-style).
    m_deviceRow->setVisible(true);

    m_callState->setText(callStateDisplayText(state));

    // Color-code the state label.
    QString color = QStringLiteral("#aaaaaa"); // Idle / default
    if (state == CallState::Active)
        color = QStringLiteral("#50c878"); // green
    else if (state == CallState::IncomingRinging || state == CallState::Ringing
             || state == CallState::OutgoingInit || state == CallState::Connecting)
        color = QStringLiteral("#e0b850"); // amber
    else if (state == CallState::Failed)
        color = QStringLiteral("#e05050"); // red
    else if (state == CallState::Held)
        color = QStringLiteral("#5090e0"); // blue

    m_callState->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(color));

    if (isIdle) {
        m_remoteName->setText(tr("No active call"));
        m_remoteUri->setText(tr("—"));
        m_duration->setText(tr("00:00:00"));
        m_inputMeter->setValue(0);
        m_outputMeter->setValue(0);
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CallPanel::onCallStateChanged(CallState state, const QString &statusText, int statusCode)
{
    Q_UNUSED(statusText)
    Q_UNUSED(statusCode)
    applyCallState(state);
}

void CallPanel::onIncomingCall(const QString &remoteUri)
{
    m_remoteName->setText(tr("Incoming Call"));
    m_remoteUri->setText(remoteUri);
    applyCallState(CallState::IncomingRinging);
}

void CallPanel::onCallConnected(const QString &remoteUri)
{
    Q_UNUSED(remoteUri)
    applyCallState(CallState::Active);
}

void CallPanel::onCallDisconnected(const QString &remoteUri, const QString &reason, int statusCode)
{
    Q_UNUSED(remoteUri)
    Q_UNUSED(reason)
    Q_UNUSED(statusCode)
    applyCallState(CallState::Idle);
}

void CallPanel::onCallFailed(const QString &remoteUri, const QString &reason, int statusCode)
{
    Q_UNUSED(remoteUri)
    Q_UNUSED(statusCode)
    // applyCallState(Failed) has already run (callStateChanged fires before callFailed).
    // Override the status label with the specific failure reason so the user can correct it.
    m_regStatusLabel->setText(
        tr("Call failed: %1 — correct the URI and try again").arg(reason));
    m_regStatusLabel->setStyleSheet("color: #e05050; font-size: 10px;");
    Logger::instance().info(LogCategory::App,
        QStringLiteral("Call failed — dial row visible; user can retry without restarting"));
}

void CallPanel::onInputLevelChanged(int level)
{
    m_inputMeter->setValue(level);
}

void CallPanel::onOutputLevelChanged(int level)
{
    m_outputMeter->setValue(level);
}

void CallPanel::onMuteChanged(bool muted)
{
    QSignalBlocker blocker(m_btnMute);
    m_btnMute->setChecked(muted);
    m_btnMute->setText(muted ? tr("Unmute") : tr("Mute"));
}

void CallPanel::onVideoMuteChanged(bool muted)
{
    QSignalBlocker blocker(m_btnVideo);
    m_btnVideo->setChecked(muted);
    m_btnVideo->setText(muted ? tr("Show Video") : tr("Video"));
}

void CallPanel::setDialTarget(const QString &uri)
{
    if (m_dialInput) {
        m_dialInput->setText(uri);
        focusDialInput();
    }
}

void CallPanel::focusDialInput()
{
    if (m_dialInput) {
        m_dialInput->setFocus();
        m_dialInput->selectAll();
    }
}

// ---------------------------------------------------------------------------
// Emergency call slots
// ---------------------------------------------------------------------------

void CallPanel::onEmergencyButtonClicked()
{
    const QString target = AppSettings::emergencyTarget();
    const QString msg = tr(
        "EMERGENCY TEST CALL\n\n"
        "This will send a SIP INVITE to the configured lab PSAP target:\n"
        "  %1\n\n"
        "THIS IS A TEST/LAB CALL ONLY.\n"
        "Do NOT use for real emergencies — this is not connected to emergency services.\n\n"
        "Proceed with the test call?").arg(target);

    const int ret = QMessageBox::warning(
        this,
        tr("Confirm Emergency Test Call — TEST/LAB Only"),
        msg,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);  // default: No (prevents accidental confirmation)

    if (ret != QMessageBox::Yes) {
        Logger::instance().info(LogCategory::App,
            QStringLiteral("Emergency test call: user cancelled confirm dialog"));
        return;
    }

    Logger::instance().info(LogCategory::App,
        QStringLiteral("[EMERGENCY TEST/DEMO] Emergency call initiated — target=%1").arg(target));

    m_btnEmergency->setEnabled(false);

    // Update the controller profile with the current target (may have changed in settings).
    const EmergencyCallProfile emProfile = EmergencyCallProfile::makeSos(
        target, QStringLiteral("NG112-TEST"));
    m_emergencyController->setProfile(emProfile);

    m_emergencyController->prepare();
}

void CallPanel::onEmergencyReadyToDial(const EmergencyCallProfile &profile)
{
    const bool hasLocation = !profile.pidfLo.isEmpty();
    const QString contentId = EmergencyCallAdapter::generateContentId();

    const EmergencyInvite invite = EmergencyInviteBuilder(profile)
        .setLocationAvailable(hasLocation)
        .setLocationRequired(false)
        .setContentId(contentId)
        .build();

    const EmergencyInviteValidationResult vr = EmergencyInviteBuilder::validate(invite);
    if (!vr.isValid()) {
        const QString reason = vr.errors.join(QStringLiteral("; "));
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("[EMERGENCY] Invite invalid, call blocked: %1").arg(reason));
        m_emergencyController->stateMachine().transition(
            EmergencyCallState::Failed, reason);
        return;
    }

    const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(invite);
    m_emergencyCallActive = true;

    m_emergencyController->stateMachine().transition(
        EmergencyCallState::Dialing, QStringLiteral("makeEmergencyCall"));

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("[EMERGENCY TEST/DEMO] makeEmergencyCall → %1, contentId=%2")
            .arg(profile.routingTarget, contentId));

    if (!SipManager::instance().makeEmergencyCall(profile.routingTarget, opts)) {
        m_emergencyCallActive = false;
        m_emergencyController->stateMachine().transition(
            EmergencyCallState::Failed, QStringLiteral("makeEmergencyCall returned false"));
    }
}

void CallPanel::onEmergencyStateChanged(EmergencyCallState state)
{
    const QString name = emergencyCallStateName(state);

    QString color = QStringLiteral("#aaaaaa"); // Idle / default
    switch (state) {
    case EmergencyCallState::Active:
        color = QStringLiteral("#50c878"); break;          // green
    case EmergencyCallState::Dialing:
    case EmergencyCallState::Preparing:
    case EmergencyCallState::LocationPending:
    case EmergencyCallState::ReadyToDial:
        color = QStringLiteral("#e0b850"); break;          // amber
    case EmergencyCallState::Failed:
        color = QStringLiteral("#e05050"); break;          // red
    case EmergencyCallState::Ended:
        color = QStringLiteral("#5090e0"); break;          // blue
    default:
        break;
    }

    m_emergencyStateLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 10px;").arg(color));
    m_emergencyStateLabel->setText(tr("State: %1").arg(name));

    const bool registered =
        (SipManager::instance().registrationState() == RegistrationState::Registered);
    const bool canDial = (state == EmergencyCallState::Idle
                          || state == EmergencyCallState::Failed
                          || state == EmergencyCallState::Ended);
    if (m_btnEmergency)
        m_btnEmergency->setEnabled(registered && canDial);

    // Location update: only allowed when emergency call is active and location was generated.
    if (m_btnLocationUpdate)
        m_btnLocationUpdate->setEnabled(
            state == EmergencyCallState::Active && m_manualLocationValid);
}

void CallPanel::onEmergencyFailed(const QString &reason)
{
    Logger::instance().warn(LogCategory::App,
        QStringLiteral("[EMERGENCY] Preparation failed: %1").arg(reason));
    m_emergencyStateLabel->setStyleSheet("color: #e05050; font-size: 10px;");
    m_emergencyStateLabel->setText(tr("State: Failed — %1").arg(reason));
    if (m_btnEmergency) {
        const bool registered =
            (SipManager::instance().registrationState() == RegistrationState::Registered);
        m_btnEmergency->setEnabled(registered);
    }
    if (m_btnLocationUpdate) m_btnLocationUpdate->setEnabled(false);
}

// ---------------------------------------------------------------------------
// Manual PIDF-LO generation
// ---------------------------------------------------------------------------

void CallPanel::onGeneratePidfClicked()
{
    bool latOk = false, lonOk = false, uncOk = false;
    const double lat = m_latInput->text().trimmed().toDouble(&latOk);
    const double lon = m_lonInput->text().trimmed().toDouble(&lonOk);
    const double unc = m_uncertaintyInput->text().trimmed().toDouble(&uncOk);

    QStringList errors;
    if (!latOk || lat < -90.0 || lat > 90.0)
        errors << tr("Latitude must be -90..90");
    if (!lonOk || lon < -180.0 || lon > 180.0)
        errors << tr("Longitude must be -180..180");
    if (!uncOk || unc < 0.0)
        errors << tr("Uncertainty must be >= 0");

    if (!errors.isEmpty()) {
        m_locationStatusLabel->setText(tr("Invalid: %1").arg(errors.join(QStringLiteral("; "))));
        m_locationStatusLabel->setStyleSheet("color: #e05050; font-size: 9px;");
        m_pidfPreview->clear();
        m_manualLocationValid = false;
        if (m_btnLocationUpdate) m_btnLocationUpdate->setEnabled(false);
        return;
    }

    QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!ts.endsWith('Z')) ts += 'Z';

    EmergencyLocation loc = EmergencyLocation::makeStatic(lat, lon, ts);
    loc.uncertaintyMeters = unc;
    loc.source = LocationSource::Manual;

    if (!loc.isValid()) {
        const QStringList ve = loc.validationErrors();
        m_locationStatusLabel->setText(tr("Location invalid: %1").arg(ve.join(QStringLiteral("; "))));
        m_locationStatusLabel->setStyleSheet("color: #e05050; font-size: 9px;");
        m_pidfPreview->clear();
        m_manualLocationValid = false;
        if (m_btnLocationUpdate) m_btnLocationUpdate->setEnabled(false);
        return;
    }

    const QString cid = EmergencyCallAdapter::generateContentId();
    const PidfLoResult r = PidfLoBuilder(loc).setContentId(cid).build();

    if (!r.success) {
        m_locationStatusLabel->setText(tr("PIDF-LO build failed: %1").arg(r.error));
        m_locationStatusLabel->setStyleSheet("color: #e05050; font-size: 9px;");
        m_pidfPreview->clear();
        m_manualLocationValid = false;
        if (m_btnLocationUpdate) m_btnLocationUpdate->setEnabled(false);
        return;
    }

    // Update the static provider with the newly entered location.
    m_staticLocationProvider->setLocation(loc);

    m_pidfPreview->setPlainText(r.xml);
    m_locationStatusLabel->setText(
        tr("[TEST/LAB] Valid — lat=%1 lon=%2 unc=%3m — PIDF-LO ready")
            .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6).arg(unc, 0, 'f', 1));
    m_locationStatusLabel->setStyleSheet("color: #50c878; font-size: 9px;");
    m_manualLocationValid = true;

    const bool emergencyActive =
        (m_emergencyCallActive
         && m_emergencyController->state() == EmergencyCallState::Active);
    if (m_btnLocationUpdate) m_btnLocationUpdate->setEnabled(emergencyActive);

    Logger::instance().info(LogCategory::App,
        QStringLiteral("[TEST/LAB] Manual PIDF-LO generated: lat=%1 lon=%2 unc=%3m")
            .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6).arg(unc, 0, 'f', 1));
}

// ---------------------------------------------------------------------------
// In-dialog location update (SIP UPDATE)
// ---------------------------------------------------------------------------

void CallPanel::onLocationUpdateClicked()
{
    if (!m_emergencyCallActive || !m_manualLocationValid) {
        Logger::instance().warn(LogCategory::App,
            QStringLiteral("[EMERGENCY] Location update ignored: active=%1 locationValid=%2")
                .arg(m_emergencyCallActive).arg(m_manualLocationValid));
        return;
    }

    const QString target = AppSettings::emergencyTarget();
    const int ret = QMessageBox::warning(
        this,
        tr("Confirm Location Update — TEST/LAB Only"),
        tr("Send a SIP UPDATE with updated location to the active emergency test call?\n\n"
           "Target: %1\n\n"
           "THIS IS A TEST/LAB OPERATION ONLY.\n"
           "Manual coordinates (not GPS) will be sent in a new PIDF-LO body.").arg(target),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (ret != QMessageBox::Yes) {
        Logger::instance().info(LogCategory::App,
            QStringLiteral("[EMERGENCY] Location update: user cancelled"));
        return;
    }

    const QString contentId = EmergencyCallAdapter::generateContentId();
    const SipCallOptions opts = EmergencyCallAdapter::toLocationUpdateOptions(
        m_staticLocationProvider->pidfLo(), contentId);

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("[EMERGENCY TEST/DEMO] sendEmergencyLocationUpdate: contentId=%1")
            .arg(contentId));

    const bool ok = SipManager::instance().sendEmergencyLocationUpdate(opts);
    if (ok) {
        m_locationStatusLabel->setText(
            tr("[TEST/LAB] Location UPDATE sent — contentId=%1").arg(contentId));
        m_locationStatusLabel->setStyleSheet("color: #50c878; font-size: 9px;");
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("[EMERGENCY TEST/DEMO] Location UPDATE accepted by PJSIP: contentId=%1")
                .arg(contentId));
    } else {
        m_locationStatusLabel->setText(tr("Location UPDATE failed — see log (stub or not Active)"));
        m_locationStatusLabel->setStyleSheet("color: #e05050; font-size: 9px;");
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("[EMERGENCY] sendEmergencyLocationUpdate returned false"));
    }
}
