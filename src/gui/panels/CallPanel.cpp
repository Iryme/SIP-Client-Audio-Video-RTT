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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QLabel *infoKeyLabel(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet("color: #7a8aaa; font-size: 10px;");
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return lbl;
}

static QLabel *infoValLabel(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet("color: #d0dae8; font-size: 10px; font-family: monospace;");
    lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return lbl;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

CallPanel::CallPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("CallPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    // ── Dial row: URI input + call type + Call button ─────────────────────
    {
        auto *dialOuter = new QWidget(this);
        dialOuter->setObjectName("DialRow");
        dialOuter->setStyleSheet(
            "QWidget#DialRow { background: #1e2a1e; border: 1px solid #3a5a3a; border-radius: 5px; }");
        auto *dialV = new QVBoxLayout(dialOuter);
        dialV->setContentsMargins(10, 8, 10, 8);
        dialV->setSpacing(4);

        m_regStatusLabel = new QLabel(tr("Not registered — register a SIP profile first"), dialOuter);
        m_regStatusLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
        dialV->addWidget(m_regStatusLabel);

        auto *dialH = new QHBoxLayout();
        dialH->setSpacing(6);

        m_dialInput = new QLineEdit(dialOuter);
        m_dialInput->setObjectName("DialInput");
        m_dialInput->setPlaceholderText(tr("sip:user@domain  or  user@domain"));
        m_dialInput->setFixedHeight(30);

        m_callTypeCombo = new QComboBox(dialOuter);
        m_callTypeCombo->setFixedHeight(30);
        m_callTypeCombo->addItem(callTypeName(CallType::AudioOnly),     static_cast<int>(CallType::AudioOnly));
        m_callTypeCombo->addItem(callTypeName(CallType::AudioVideo),    static_cast<int>(CallType::AudioVideo));
        m_callTypeCombo->addItem(callTypeName(CallType::AudioRtt),      static_cast<int>(CallType::AudioRtt));
        m_callTypeCombo->addItem(callTypeName(CallType::AudioVideoRtt), static_cast<int>(CallType::AudioVideoRtt));
        m_callTypeCombo->addItem(callTypeName(CallType::RttOnly),       static_cast<int>(CallType::RttOnly));

        m_btnCall = new QPushButton(tr("Call"), dialOuter);
        m_btnCall->setObjectName("CallBtn");
        m_btnCall->setFixedHeight(30);
        m_btnCall->setMinimumWidth(64);
        m_btnCall->setEnabled(false);

        dialH->addWidget(m_dialInput, 1);
        dialH->addWidget(m_callTypeCombo);
        dialH->addWidget(m_btnCall);
        dialV->addLayout(dialH);

        layout->addWidget(dialOuter);
    }

    // ── Separator ─────────────────────────────────────────────────────────
    {
        auto *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        layout->addWidget(sep);
    }

    // ── Level meters + device selectors ───────────────────────────────────
    {
        auto *meterRow = new QHBoxLayout();
        meterRow->setSpacing(6);

        auto *micLabel = new QLabel(tr("Mic:"), this);
        micLabel->setStyleSheet("color: #888; font-size: 10px;");
        micLabel->setFixedWidth(24);

        m_inputMeter = new QProgressBar(this);
        m_inputMeter->setRange(0, 100);
        m_inputMeter->setValue(0);
        m_inputMeter->setTextVisible(false);
        m_inputMeter->setFixedHeight(8);
        m_inputMeter->setStyleSheet(
            "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; }"
            "QProgressBar::chunk { background: #50c878; border-radius: 2px; }");

        auto *spkLabel = new QLabel(tr("Spk:"), this);
        spkLabel->setStyleSheet("color: #888; font-size: 10px;");
        spkLabel->setFixedWidth(24);

        m_outputMeter = new QProgressBar(this);
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

        auto *devRow = new QHBoxLayout();
        devRow->setSpacing(6);

        auto *micDevLabel = new QLabel(tr("Microphone:"), this);
        micDevLabel->setStyleSheet("color: #888; font-size: 10px;");
        m_micSelector = new QComboBox(this);
        m_micSelector->setFixedHeight(24);

        auto *spkDevLabel = new QLabel(tr("Speaker:"), this);
        spkDevLabel->setStyleSheet("color: #888; font-size: 10px;");
        m_spkSelector = new QComboBox(this);
        m_spkSelector->setFixedHeight(24);

        devRow->addWidget(micDevLabel);
        devRow->addWidget(m_micSelector, 1);
        devRow->addWidget(spkDevLabel);
        devRow->addWidget(m_spkSelector, 1);

        layout->addLayout(devRow);
    }

    // ── Call control buttons ───────────────────────────────────────────────
    {
        auto *ctrlRow = new QHBoxLayout();
        ctrlRow->setSpacing(6);

        auto makeBtn = [&](const QString &label, bool checkable = false) -> QPushButton* {
            auto *btn = new QPushButton(label, this);
            btn->setObjectName("CallCtrlBtn");
            btn->setCheckable(checkable);
            btn->setFixedHeight(32);
            btn->setMinimumWidth(72);
            return btn;
        };

        m_btnMute       = makeBtn(tr("Mute"),          true);
        m_btnHold       = makeBtn(tr("Hold"),          true);
        m_btnStartVideo = makeBtn(tr("Start Video"),   false);
        m_btnStopVideo  = makeBtn(tr("Stop Video"),    false);
        m_btnAnswer     = makeBtn(tr("Answer"),        false);
        m_btnReject     = makeBtn(tr("Reject"),        false);
        m_btnHangup     = makeBtn(tr("Hangup"),        false);

        m_btnAnswer->setObjectName("AnswerBtn");
        m_btnReject->setObjectName("RejectBtn");
        m_btnHangup->setObjectName("HangupBtn");
        m_btnStartVideo->setObjectName("StartVideoBtn");
        m_btnStopVideo->setObjectName("StopVideoBtn");

        ctrlRow->addWidget(m_btnMute);
        ctrlRow->addWidget(m_btnHold);
        ctrlRow->addWidget(m_btnStartVideo);
        ctrlRow->addWidget(m_btnStopVideo);
        ctrlRow->addStretch();
        ctrlRow->addWidget(m_btnAnswer);
        ctrlRow->addWidget(m_btnReject);
        ctrlRow->addWidget(m_btnHangup);

        layout->addLayout(ctrlRow);
    }

    // ── Info grid ─────────────────────────────────────────────────────────
    {
        auto *grid = new QGridLayout();
        grid->setSpacing(2);
        grid->setColumnStretch(0, 0);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(2, 0);
        grid->setColumnStretch(3, 1);

        int row = 0;

        m_infoState    = infoValLabel(tr("Idle"), this);
        m_infoDuration = infoValLabel(tr("00:00:00"), this);
        grid->addWidget(infoKeyLabel(tr("State:"), this),    row, 0);
        grid->addWidget(m_infoState,                         row, 1);
        grid->addWidget(infoKeyLabel(tr("Duration:"), this), row, 2);
        grid->addWidget(m_infoDuration,                      row, 3);
        ++row;

        m_infoRemoteUri = infoValLabel(tr("—"), this);
        m_infoLocalUri  = infoValLabel(tr("—"), this);
        grid->addWidget(infoKeyLabel(tr("Remote:"), this), row, 0);
        grid->addWidget(m_infoRemoteUri,                   row, 1);
        grid->addWidget(infoKeyLabel(tr("Local:"), this),  row, 2);
        grid->addWidget(m_infoLocalUri,                    row, 3);
        ++row;

        m_infoCallType   = infoValLabel(tr("—"), this);
        m_infoNegotiated = infoValLabel(tr("—"), this);
        grid->addWidget(infoKeyLabel(tr("Call type:"), this),  row, 0);
        grid->addWidget(m_infoCallType,                        row, 1);
        grid->addWidget(infoKeyLabel(tr("Negotiated:"), this), row, 2);
        grid->addWidget(m_infoNegotiated,                      row, 3);
        ++row;

        m_infoAudioConn = infoValLabel(tr("—"), this);
        m_infoVideoConn = infoValLabel(tr("—"), this);
        grid->addWidget(infoKeyLabel(tr("Audio:"), this), row, 0);
        grid->addWidget(m_infoAudioConn,                  row, 1);
        grid->addWidget(infoKeyLabel(tr("Video:"), this), row, 2);
        grid->addWidget(m_infoVideoConn,                  row, 3);
        ++row;

        m_infoRttConn = infoValLabel(tr("—"), this);
        grid->addWidget(infoKeyLabel(tr("RTT:"), this), row, 0);
        grid->addWidget(m_infoRttConn,                  row, 1);
        ++row;

        layout->addLayout(grid);
    }

    // ── Separator ─────────────────────────────────────────────────────────
    {
        auto *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        layout->addWidget(sep);
    }

    // ── Emergency test mode section (hidden by default) ────────────────────
    m_emergencyRow = new QWidget(this);
    m_emergencyRow->setObjectName("EmergencyRow");
    m_emergencyRow->setStyleSheet(
        "QWidget#EmergencyRow { background: #2a1200; border: 1px solid #8b3a00;"
        " border-radius: 5px; }");

    {
        auto *emerOuter = new QVBoxLayout(m_emergencyRow);
        emerOuter->setContentsMargins(10, 8, 10, 8);
        emerOuter->setSpacing(4);

        auto *emerWarnLabel = new QLabel(
            tr("[TEST/LAB] Emergency Test Mode — NOT a real emergency service"), m_emergencyRow);
        emerWarnLabel->setStyleSheet("color: #ff8c00; font-size: 10px; font-weight: bold;");
        emerWarnLabel->setWordWrap(true);
        emerOuter->addWidget(emerWarnLabel);

        auto *locHeaderLabel = new QLabel(
            tr("[TEST/LAB] Manual Location — enter WGS-84 coordinates below. "
               "No GPS, no Windows Location."), m_emergencyRow);
        locHeaderLabel->setStyleSheet("color: #cc7700; font-size: 9px;");
        locHeaderLabel->setWordWrap(true);
        emerOuter->addWidget(locHeaderLabel);

        auto *locInputRow = new QHBoxLayout();
        locInputRow->setSpacing(4);

        auto *latLabel = new QLabel(tr("Lat:"), m_emergencyRow);
        latLabel->setStyleSheet("color: #888; font-size: 10px;");
        m_latInput = new QLineEdit(QStringLiteral("44.4268"), m_emergencyRow);
        m_latInput->setFixedWidth(72);
        m_latInput->setFixedHeight(22);
        m_latInput->setToolTip(tr("-90 to 90 degrees (WGS-84)"));

        auto *lonLabel = new QLabel(tr("Lon:"), m_emergencyRow);
        lonLabel->setStyleSheet("color: #888; font-size: 10px;");
        m_lonInput = new QLineEdit(QStringLiteral("26.1025"), m_emergencyRow);
        m_lonInput->setFixedWidth(72);
        m_lonInput->setFixedHeight(22);
        m_lonInput->setToolTip(tr("-180 to 180 degrees (WGS-84)"));

        auto *uncLabel = new QLabel(tr("Unc(m):"), m_emergencyRow);
        uncLabel->setStyleSheet("color: #888; font-size: 10px;");
        m_uncertaintyInput = new QLineEdit(QStringLiteral("50.0"), m_emergencyRow);
        m_uncertaintyInput->setFixedWidth(56);
        m_uncertaintyInput->setFixedHeight(22);
        m_uncertaintyInput->setToolTip(tr("Accuracy radius in meters (>= 0)"));

        m_btnGeneratePidf = new QPushButton(tr("Generate PIDF-LO"), m_emergencyRow);
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
        m_locationStatusLabel->setStyleSheet("color: #888; font-size: 9px;");
        m_locationStatusLabel->setWordWrap(true);
        emerOuter->addWidget(m_locationStatusLabel);

        m_pidfPreview = new QPlainTextEdit(m_emergencyRow);
        m_pidfPreview->setReadOnly(true);
        m_pidfPreview->setMaximumHeight(56);
        m_pidfPreview->setPlaceholderText(tr("PIDF-LO XML appears here after Generate is clicked"));
        m_pidfPreview->setStyleSheet(
            "QPlainTextEdit { font-family: monospace; font-size: 8px;"
            " color: #999; background: #1a1a1a; border: 1px solid #333; }");
        emerOuter->addWidget(m_pidfPreview);

        auto *emerCtrlRow = new QHBoxLayout();
        emerCtrlRow->setSpacing(6);

        m_emergencyStateLabel = new QLabel(tr("State: Idle"), m_emergencyRow);
        m_emergencyStateLabel->setStyleSheet("color: #aaa; font-size: 10px;");
        emerCtrlRow->addWidget(m_emergencyStateLabel, 1);

        m_btnLocationUpdate = new QPushButton(tr("Send Location Update"), m_emergencyRow);
        m_btnLocationUpdate->setFixedHeight(30);
        m_btnLocationUpdate->setEnabled(false);
        m_btnLocationUpdate->setStyleSheet(
            "QPushButton { background: #1a3a4a; color: #5090c0;"
            " border-radius: 4px; font-size: 10px; }"
            "QPushButton:hover { background: #2a4a5a; }"
            "QPushButton:disabled { background: #1a2020; color: #444; }");
        emerCtrlRow->addWidget(m_btnLocationUpdate);

        m_btnEmergency = new QPushButton(tr("112 Emergency (TEST)"), m_emergencyRow);
        m_btnEmergency->setObjectName("EmergencyBtn");
        m_btnEmergency->setFixedHeight(30);
        m_btnEmergency->setMinimumWidth(160);
        m_btnEmergency->setEnabled(false);
        m_btnEmergency->setStyleSheet(
            "QPushButton#EmergencyBtn { background: #8b0000; color: white;"
            " border-radius: 4px; font-weight: bold; }"
            "QPushButton#EmergencyBtn:hover { background: #b00000; }"
            "QPushButton#EmergencyBtn:disabled { background: #3a2020; color: #666; }");
        emerCtrlRow->addWidget(m_btnEmergency);

        emerOuter->addLayout(emerCtrlRow);
    }

    layout->addWidget(m_emergencyRow);
    layout->addStretch(1);

    m_emergencyRow->setVisible(AppSettings::emergencyTestModeEnabled());

    // ── Emergency controller setup ─────────────────────────────────────────
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

    // ── Signal wiring ──────────────────────────────────────────────────────

    // Mute → AudioMediaManager
    connect(m_btnMute, &QPushButton::toggled,
            [](bool checked){ AudioMediaManager::instance().setMuted(checked); });
    connect(&AudioMediaManager::instance(), &AudioMediaManager::mutedChanged,
            this, &CallPanel::onMuteChanged);
    connect(&AudioMediaManager::instance(), &AudioMediaManager::inputLevelChanged,
            this, &CallPanel::onInputLevelChanged);
    connect(&AudioMediaManager::instance(), &AudioMediaManager::outputLevelChanged,
            this, &CallPanel::onOutputLevelChanged);

    // Hold → SipManager
    connect(m_btnHold, &QPushButton::toggled, [](bool held) {
        if (held) SipManager::instance().holdCall();
        else      SipManager::instance().resumeCall();
    });

    // Answer / Reject / Hangup → SipManager
    connect(m_btnAnswer, &QPushButton::clicked, []{ SipManager::instance().answerCall(); });
    connect(m_btnReject, &QPushButton::clicked, []{ SipManager::instance().rejectCall(); });
    connect(m_btnHangup, &QPushButton::clicked, []{ SipManager::instance().hangupCall(); });

    // Start/Stop Video → signals for MainWindow to wire to VideoPanel
    connect(m_btnStartVideo, &QPushButton::clicked, this, &CallPanel::startLocalVideoRequested);
    connect(m_btnStopVideo,  &QPushButton::clicked, this, &CallPanel::stopLocalVideoRequested);

    // Device combos → AudioMediaManager
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

    // Media connected/disconnected
    connect(&SipManager::instance(), &SipManager::audioMediaConnected,
            this, &CallPanel::onAudioMediaConnected);
    connect(&SipManager::instance(), &SipManager::audioMediaDisconnected,
            this, &CallPanel::onAudioMediaDisconnected);
    connect(&SipManager::instance(), &SipManager::videoMediaConnected,
            this, &CallPanel::onVideoMediaConnected);
    connect(&SipManager::instance(), &SipManager::videoMediaDisconnected,
            this, &CallPanel::onVideoMediaDisconnected);
    connect(&SipManager::instance(), &SipManager::rttMediaConnected,
            this, &CallPanel::onRttMediaConnected);
    connect(&SipManager::instance(), &SipManager::rttMediaDisconnected,
            this, &CallPanel::onRttMediaDisconnected);

    // Dial row: Call button / Enter
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
        const int typeIdx = m_callTypeCombo->currentIndex();
        const CallType ct = static_cast<CallType>(m_callTypeCombo->itemData(typeIdx).toInt());
        m_activeCallType = ct;
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Placing call: uri=%1 type=%2")
                .arg(result.uri, callTypeName(ct)));
        SipManager::instance().makeCall(result.uri, CallMediaOptions::fromType(ct));
    };
    connect(m_btnCall,   &QPushButton::clicked,       this, triggerCall);
    connect(m_dialInput, &QLineEdit::returnPressed,   this, triggerCall);

    // Registration state → update status label + Call button enable
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
        if (m_btnEmergency) {
            const EmergencyCallState es = m_emergencyController->state();
            const bool canDial = (es == EmergencyCallState::Idle
                                  || es == EmergencyCallState::Failed
                                  || es == EmergencyCallState::Ended);
            m_btnEmergency->setEnabled(registered && canDial);
        }
    });

    // Emergency controller
    connect(m_emergencyController, &EmergencyCallController::stateChanged,
            this, &CallPanel::onEmergencyStateChanged);
    connect(m_emergencyController, &EmergencyCallController::readyToDial,
            this, &CallPanel::onEmergencyReadyToDial);
    connect(m_emergencyController, &EmergencyCallController::preparationFailed,
            this, &CallPanel::onEmergencyFailed);
    connect(m_btnEmergency,    &QPushButton::clicked, this, &CallPanel::onEmergencyButtonClicked);
    connect(m_btnGeneratePidf, &QPushButton::clicked, this, &CallPanel::onGeneratePidfClicked);
    connect(m_btnLocationUpdate, &QPushButton::clicked, this, &CallPanel::onLocationUpdateClicked);

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

    // Duration timer
    m_durationTimer.setInterval(1000);
    connect(&m_durationTimer, &QTimer::timeout, this, &CallPanel::onDurationTick);

    populateDeviceCombos();
    applyCallState(CallState::Idle);
}

// ---------------------------------------------------------------------------

void CallPanel::placeCall(const QString &uri)
{
    if (m_dialInput && !uri.trimmed().isEmpty())
        m_dialInput->setText(uri.trimmed());

    const QString raw = m_dialInput ? m_dialInput->text().trimmed() : QString{};
    if (raw.isEmpty()) {
        if (m_regStatusLabel) {
            m_regStatusLabel->setText(tr("Enter a SIP URI or extension to call"));
            m_regStatusLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
        }
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
        if (m_regStatusLabel) {
            m_regStatusLabel->setText(tr("Invalid URI: %1").arg(result.error));
            m_regStatusLabel->setStyleSheet("color: #e05050; font-size: 10px;");
        }
        return;
    }
    if (result.uri != raw && m_dialInput)
        m_dialInput->setText(result.uri);

    const int typeIdx = m_callTypeCombo ? m_callTypeCombo->currentIndex() : 0;
    const CallType ct = m_callTypeCombo
        ? static_cast<CallType>(m_callTypeCombo->itemData(typeIdx).toInt())
        : CallType::AudioOnly;
    m_activeCallType = ct;
    SipManager::instance().makeCall(result.uri, CallMediaOptions::fromType(ct));
}

void CallPanel::populateDeviceCombos()
{
    const QString curMic = m_micSelector->currentData().toString();
    const QString curSpk = m_spkSelector->currentData().toString();

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

    // Dial row visible when no active call
    const bool showDial = isIdle || isFailed;
    if (m_dialInput) m_dialInput->parentWidget()->setVisible(showDial);

    if (showDial) {
        const bool registered =
            (SipManager::instance().registrationState() == RegistrationState::Registered);
        m_btnCall->setEnabled(registered);
        if (isIdle) {
            if (registered) {
                m_regStatusLabel->setText(tr("Registered — enter a SIP URI and press Call"));
                m_regStatusLabel->setStyleSheet("color: #50c878; font-size: 10px;");
            } else {
                m_regStatusLabel->setText(tr("Not registered — register a SIP profile first"));
                m_regStatusLabel->setStyleSheet("color: #e0b850; font-size: 10px;");
            }
        }
    }

    // Info grid state label
    QString stateText = callStateDisplayText(state);
    QString stateColor = QStringLiteral("#aaaaaa");
    if (state == CallState::Active)
        stateColor = QStringLiteral("#50c878");
    else if (state == CallState::IncomingRinging || state == CallState::Ringing
             || state == CallState::OutgoingInit || state == CallState::Connecting)
        stateColor = QStringLiteral("#e0b850");
    else if (state == CallState::Failed)
        stateColor = QStringLiteral("#e05050");
    else if (state == CallState::Held)
        stateColor = QStringLiteral("#5090e0");

    m_infoState->setText(stateText);
    m_infoState->setStyleSheet(
        QStringLiteral("color: %1; font-size: 10px; font-family: monospace;").arg(stateColor));

    if (isIdle) {
        m_durationTimer.stop();
        m_durationSeconds = 0;
        resetInfoGrid();
    }
}

void CallPanel::updateInfoGrid()
{
    m_infoRemoteUri->setText(m_remoteUri.isEmpty() ? tr("—") : m_remoteUri);

    const SipProfile p = SipProfileManager::instance().activeProfile();
    m_infoLocalUri->setText(p.isNull() ? tr("—") : p.effectiveSipUri());

    m_infoCallType->setText(callTypeName(m_activeCallType));

    QStringList negotiated;
    if (m_audioConnected) negotiated << QStringLiteral("Audio");
    if (m_videoConnected) negotiated << QStringLiteral("Video");
    if (m_rttConnected)   negotiated << QStringLiteral("RTT");
    m_infoNegotiated->setText(negotiated.isEmpty() ? tr("—") : negotiated.join(QStringLiteral(" + ")));

    auto connected = [](bool v) -> QString {
        return v ? QStringLiteral("Connected") : QStringLiteral("—");
    };
    m_infoAudioConn->setText(connected(m_audioConnected));
    m_infoVideoConn->setText(connected(m_videoConnected));
    m_infoRttConn->setText(connected(m_rttConnected));
}

void CallPanel::resetInfoGrid()
{
    m_remoteUri.clear();
    m_audioConnected = false;
    m_videoConnected = false;
    m_rttConnected   = false;

    m_infoDuration->setText(tr("00:00:00"));
    m_infoRemoteUri->setText(tr("—"));
    m_infoLocalUri->setText(tr("—"));
    m_infoCallType->setText(tr("—"));
    m_infoNegotiated->setText(tr("—"));
    m_infoAudioConn->setText(tr("—"));
    m_infoVideoConn->setText(tr("—"));
    m_infoRttConn->setText(tr("—"));
    m_inputMeter->setValue(0);
    m_outputMeter->setValue(0);
}

QString CallPanel::formatDuration(int seconds) const
{
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

// ---------------------------------------------------------------------------
// Call state slots
// ---------------------------------------------------------------------------

void CallPanel::onCallStateChanged(CallState state, const QString &, int)
{
    applyCallState(state);
}

void CallPanel::onIncomingCall(const QString &remoteUri)
{
    m_remoteUri = remoteUri;
    m_activeCallType = CallType::AudioOnly; // will be refined by media signals
    updateInfoGrid();
    applyCallState(CallState::IncomingRinging);
}

void CallPanel::onCallConnected(const QString &remoteUri)
{
    m_remoteUri = remoteUri;
    m_durationSeconds = 0;
    m_durationTimer.start();
    updateInfoGrid();
    applyCallState(CallState::Active);
}

void CallPanel::onCallDisconnected(const QString &, const QString &, int)
{
    m_durationTimer.stop();
    applyCallState(CallState::Idle);
}

void CallPanel::onCallFailed(const QString &, const QString &reason, int)
{
    m_durationTimer.stop();
    if (m_regStatusLabel) {
        m_regStatusLabel->setText(
            tr("Call failed: %1 — correct the URI and try again").arg(reason));
        m_regStatusLabel->setStyleSheet("color: #e05050; font-size: 10px;");
    }
    Logger::instance().info(LogCategory::App,
        QStringLiteral("Call failed — dial row visible; user can retry"));
}

void CallPanel::onInputLevelChanged(int level)  { m_inputMeter->setValue(level); }
void CallPanel::onOutputLevelChanged(int level) { m_outputMeter->setValue(level); }

void CallPanel::onMuteChanged(bool muted)
{
    QSignalBlocker b(m_btnMute);
    m_btnMute->setChecked(muted);
    m_btnMute->setText(muted ? tr("Unmute") : tr("Mute"));
}

void CallPanel::onAudioMediaConnected()
{
    m_audioConnected = true;
    updateInfoGrid();
}

void CallPanel::onAudioMediaDisconnected()
{
    m_audioConnected = false;
    updateInfoGrid();
}

void CallPanel::onVideoMediaConnected()
{
    m_videoConnected = true;
    updateInfoGrid();
}

void CallPanel::onVideoMediaDisconnected()
{
    m_videoConnected = false;
    updateInfoGrid();
}

void CallPanel::onRttMediaConnected()
{
    m_rttConnected = true;
    updateInfoGrid();
}

void CallPanel::onRttMediaDisconnected()
{
    m_rttConnected = false;
    updateInfoGrid();
}

void CallPanel::onDurationTick()
{
    ++m_durationSeconds;
    m_infoDuration->setText(formatDuration(m_durationSeconds));
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
        QMessageBox::No);

    if (ret != QMessageBox::Yes) {
        Logger::instance().info(LogCategory::App,
            QStringLiteral("Emergency test call: user cancelled confirm dialog"));
        return;
    }

    Logger::instance().info(LogCategory::App,
        QStringLiteral("[EMERGENCY TEST/DEMO] Emergency call initiated — target=%1").arg(target));

    m_btnEmergency->setEnabled(false);

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

    QString color = QStringLiteral("#aaa");
    switch (state) {
    case EmergencyCallState::Active:
        color = QStringLiteral("#50c878"); break;
    case EmergencyCallState::Dialing:
    case EmergencyCallState::Preparing:
    case EmergencyCallState::LocationPending:
    case EmergencyCallState::ReadyToDial:
        color = QStringLiteral("#e0b850"); break;
    case EmergencyCallState::Failed:
        color = QStringLiteral("#e05050"); break;
    case EmergencyCallState::Ended:
        color = QStringLiteral("#5090e0"); break;
    default: break;
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
