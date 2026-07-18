#include "CallWorkspacePanel.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "core/ContactStore.h"
#include "core/Logger.h"
#include "gui/CameraController.h"
#include "gui/panels/call/CallInfoModel.h"
#include "emergency/EmergencyCallAdapter.h"
#include "emergency/EmergencyCallController.h"
#include "emergency/EmergencyInviteBuilder.h"
#include "emergency/EmergencyLocation.h"
#include "emergency/PidfLoBuilder.h"
#include "emergency/StaticLocationProvider.h"
#include "gui/widgets/AudioLevelMeter.h"
#include "gui/widgets/FlowLayout.h"
#include "gui/widgets/StatusCard.h"
#include "media/AudioMediaManager.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoMediaManager.h"
#include "media/VideoQualityManager.h"
#include "media/VideoStatistics.h"
#include "sip/PresenceInfo.h"
#include "sip/PresenceStore.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"
#include "sip/SipUriNormalizer.h"

namespace {
QString displayNameFor(const QString &uri)
{
    if (uri.isEmpty())
        return QString();
    for (const Contact &c : ContactStore::instance().contacts()) {
        if (c.uri.compare(uri, Qt::CaseInsensitive) == 0)
            return c.name;
    }
    return QString();
}
} // namespace

CallWorkspacePanel::CallWorkspacePanel(QLineEdit *targetInput, QWidget *parent)
    : QWidget(parent)
    , m_targetInput(targetInput)
{
    setObjectName(QStringLiteral("CallWorkspacePanel"));
    m_callInfoModel = new CallInfoModel(this);

    connect(&m_videoRequestBlinker, &RequestBlinker::toggled, this, &CallWorkspacePanel::refreshRequestVideoButton);
    connect(&m_rttRequestBlinker, &RequestBlinker::toggled, this, &CallWorkspacePanel::refreshRequestRttButton);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // ------------------------------------------------------------------
    // Call control buttons
    // ------------------------------------------------------------------
    auto makeActionButton = [this](const QString &text, bool checkable = false) {
        auto *btn = new QPushButton(text, this);
        btn->setObjectName(QStringLiteral("CallCtrlBtn"));
        btn->setCheckable(checkable);
        btn->setMinimumHeight(40);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return btn;
    };

    auto *btnGrid = new QGridLayout();
    btnGrid->setHorizontalSpacing(6);
    btnGrid->setVerticalSpacing(6);
    for (int c = 0; c < 4; ++c)
        btnGrid->setColumnStretch(c, 1);

    m_btnCall = makeActionButton(tr("Call"));
    m_btnAnswer = makeActionButton(tr("Answer"));
    m_btnReject = makeActionButton(tr("Reject"));
    m_btnHangup = makeActionButton(tr("Hangup"));
    m_btnMute = makeActionButton(tr("Mute"), true);
    m_btnHold = makeActionButton(tr("Pause"), true);
    m_btnRequestVideo = makeActionButton(tr("Request Video"), true);
    m_btnRequestRtt = makeActionButton(tr("Request RTT"), true);

    m_btnCall->setProperty("callRole", QStringLiteral("call"));
    m_btnAnswer->setProperty("callRole", QStringLiteral("answer"));
    m_btnReject->setProperty("callRole", QStringLiteral("reject"));
    m_btnHangup->setProperty("callRole", QStringLiteral("hangup"));
    m_btnMute->setProperty("callRole", QStringLiteral("mute"));
    m_btnHold->setProperty("callRole", QStringLiteral("pause"));
    m_btnRequestVideo->setProperty("callRole", QStringLiteral("requestVideo"));
    m_btnRequestRtt->setProperty("callRole", QStringLiteral("requestRtt"));
    m_btnAnswer->setVisible(false);
    m_btnReject->setVisible(false);
    m_btnHangup->setVisible(false);

    btnGrid->addWidget(m_btnCall, 0, 0);
    btnGrid->addWidget(m_btnAnswer, 0, 1);
    btnGrid->addWidget(m_btnReject, 0, 2);
    btnGrid->addWidget(m_btnHangup, 0, 3);
    btnGrid->addWidget(m_btnMute, 1, 0);
    btnGrid->addWidget(m_btnHold, 1, 1);
    btnGrid->addWidget(m_btnRequestVideo, 1, 2);
    btnGrid->addWidget(m_btnRequestRtt, 1, 3);
    root->addLayout(btnGrid);

    // ------------------------------------------------------------------
    // Status cards
    // ------------------------------------------------------------------
    auto makeStatusCard = [this](const QString &title, const QString &tip) {
        auto *card = new StatusCard(title, this);
        card->setTooltipText(tip);
        return card;
    };

    auto *cardsArea = new QScrollArea(this);
    cardsArea->setWidgetResizable(true);
    cardsArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cardsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cardsArea->setFrameShape(QFrame::NoFrame);
    cardsArea->setMinimumHeight(120);

    auto *cardsHost = new QWidget(cardsArea);
    auto *cardsHostLayout = new QVBoxLayout(cardsHost);
    cardsHostLayout->setContentsMargins(0, 0, 0, 0);
    cardsHostLayout->setSpacing(4);

    // Essential — always visible, at-a-glance call status.
    auto *essentialHost = new QWidget(cardsHost);
    auto *essentialFlow = new FlowLayout(essentialHost, 4, 5, 5);
    essentialHost->setLayout(essentialFlow);

    m_cardState = makeStatusCard(tr("Call State"), tr("Current SIP call state"));
    m_cardDuration = makeStatusCard(tr("Duration"), tr("Elapsed call duration"));
    m_cardRemoteUri = makeStatusCard(tr("Remote URI"), tr("Remote SIP URI / display name"));
    m_cardPresence = makeStatusCard(tr("Presence"), tr("Remote party's presence status"));
    m_cardAudio = makeStatusCard(tr("Audio"), tr("Audio media state"));
    m_cardRemoteVideo = makeStatusCard(tr("Remote Video"), tr("Remote video stream state"));
    m_cardRtt = makeStatusCard(tr("RTT"), tr("RTT request / media state"));
    m_cardCamera = makeStatusCard(tr("Camera"), tr("Hardware camera state"));

    StatusCard *essentialCards[] = {
        m_cardState, m_cardDuration, m_cardRemoteUri, m_cardPresence,
        m_cardAudio, m_cardRemoteVideo, m_cardRtt, m_cardCamera
    };
    for (StatusCard *card : essentialCards)
        essentialFlow->addWidget(card);

    m_cardCamera->setValue(tr("On"));
    m_cardCamera->setStatus(QStringLiteral("ok"));

    cardsHostLayout->addWidget(essentialHost);

    // Task W113a layout pass: everything else (codecs, bitrate/resolution/
    // fps, selected media, local account, packet loss/video drops/jitter/
    // latency, LMPE) is secondary diagnostic detail, not needed at a glance
    // during a normal call — tucked behind a disclosure, collapsed by
    // default, so the panel doesn't force ~21 cards into view at once.
    m_advancedToggle = new QToolButton(cardsHost);
    m_advancedToggle->setCheckable(true);
    m_advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_advancedToggle->setArrowType(Qt::RightArrow);
    m_advancedToggle->setText(tr("Advanced diagnostics"));
    m_advancedToggle->setAutoRaise(true);
    cardsHostLayout->addWidget(m_advancedToggle);

    m_advancedHost = new QWidget(cardsHost);
    auto *advancedFlow = new FlowLayout(m_advancedHost, 4, 5, 5);
    m_advancedHost->setLayout(advancedFlow);

    m_cardAudioCodec = makeStatusCard(tr("Audio Codec"), tr("Negotiated audio codec"));
    m_cardLocalVideo = makeStatusCard(tr("Local Video"), tr("Local camera preview state"));
    m_cardLmpe = makeStatusCard(tr("LMPE"), tr(
        "LMPE is disabled because the required interoperable format is not available."));
    m_cardVideoCodec = makeStatusCard(tr("Video Codec"), tr("Negotiated video codec"));
    m_cardBitrate = makeStatusCard(tr("Bitrate"), tr("Negotiated/configured video bitrate"));
    m_cardResolution = makeStatusCard(tr("Resolution"), tr("Negotiated/configured video resolution"));
    m_cardFps = makeStatusCard(tr("FPS"), tr("Local preview frame rate"));
    m_cardInitialOffer = makeStatusCard(tr("Selected Media"), tr("Media requested when the call was placed"));
    m_cardLocalAccount = makeStatusCard(tr("Local Account"), tr("Active SIP account URI"));
    m_cardPacketLoss = makeStatusCard(tr("Packet Loss"), tr("RTP/RTCP packet loss percentage"));
    m_cardVideoDrops = makeStatusCard(tr("Video Drops"), tr("Local video pipeline frame drops this second"));
    m_cardJitter = makeStatusCard(tr("Jitter"), tr("RTP jitter"));
    m_cardLatency = makeStatusCard(tr("Latency"), tr("Round-trip latency"));

    StatusCard *advancedCards[] = {
        m_cardAudioCodec, m_cardLocalVideo, m_cardLmpe, m_cardVideoCodec, m_cardBitrate,
        m_cardResolution, m_cardFps, m_cardInitialOffer, m_cardLocalAccount,
        m_cardPacketLoss, m_cardVideoDrops, m_cardJitter, m_cardLatency
    };
    for (StatusCard *card : advancedCards)
        advancedFlow->addWidget(card);

    cardsHostLayout->addWidget(m_advancedHost);

    const bool advancedExpanded = AppSettings::callWorkspaceAdvancedDiagnosticsExpanded();
    m_advancedToggle->setChecked(advancedExpanded);
    m_advancedToggle->setArrowType(advancedExpanded ? Qt::DownArrow : Qt::RightArrow);
    m_advancedHost->setVisible(advancedExpanded);
    connect(m_advancedToggle, &QToolButton::toggled, this, &CallWorkspacePanel::onAdvancedDiagnosticsToggled);

    cardsArea->setWidget(cardsHost);
    root->addWidget(cardsArea, 1);

    // ------------------------------------------------------------------
    // Audio level meters
    // ------------------------------------------------------------------
    {
        auto *meterGrid = new QGridLayout();
        meterGrid->setHorizontalSpacing(6);
        meterGrid->setVerticalSpacing(4);

        auto *micMeterLabel = new QLabel(tr("Mic level"), this);
        micMeterLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 10px;"));
        m_inputMeter = new AudioLevelMeter(this);
        m_inputMeter->setToolTip(tr("Microphone input level of the active call"));

        auto *spkMeterLabel = new QLabel(tr("Speaker level"), this);
        spkMeterLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 10px;"));
        m_outputMeter = new AudioLevelMeter(this);
        m_outputMeter->setToolTip(tr("Speaker output level of the active call"));

        meterGrid->addWidget(micMeterLabel, 0, 0);
        meterGrid->addWidget(m_inputMeter, 0, 1);
        meterGrid->addWidget(spkMeterLabel, 1, 0);
        meterGrid->addWidget(m_outputMeter, 1, 1);
        meterGrid->setColumnStretch(1, 1);
        root->addLayout(meterGrid);

        connect(&AudioMediaManager::instance(), &AudioMediaManager::inputLevelChanged,
                m_inputMeter, &AudioLevelMeter::setLevel);
        connect(&AudioMediaManager::instance(), &AudioMediaManager::outputLevelChanged,
                m_outputMeter, &AudioLevelMeter::setLevel);
    }

    // ------------------------------------------------------------------
    // Device selectors + camera/video-mute controls
    // ------------------------------------------------------------------
    {
        auto *deviceRow = new QHBoxLayout();
        deviceRow->setSpacing(6);
        auto *micLabel = new QLabel(tr("Microphone"), this);
        auto *speakerLabel = new QLabel(tr("Speaker"), this);
        auto *cameraLabel = new QLabel(tr("Camera"), this);
        m_micSelector = new QComboBox(this);
        m_spkSelector = new QComboBox(this);
        m_cameraSelector = new QComboBox(this);
        m_micSelector->setMinimumHeight(28);
        m_spkSelector->setMinimumHeight(28);
        m_cameraSelector->setMinimumHeight(28);
        deviceRow->addWidget(micLabel);
        deviceRow->addWidget(m_micSelector, 1);
        deviceRow->addWidget(speakerLabel);
        deviceRow->addWidget(m_spkSelector, 1);
        deviceRow->addWidget(cameraLabel);
        deviceRow->addWidget(m_cameraSelector, 1);
        root->addLayout(deviceRow);

        auto *videoCtrlRow = new QHBoxLayout();
        videoCtrlRow->setSpacing(6);
        m_btnCameraToggle = new QPushButton(tr("Camera Off"), this);
        m_btnCameraToggle->setObjectName(QStringLiteral("CameraOnOffBtn"));
        m_btnCameraToggle->setCheckable(true);
        m_btnCameraToggle->setChecked(false); // "camera is on, click to turn off"
        m_btnCameraToggle->setMinimumHeight(32);
        m_btnCameraToggle->setProperty("callRole", QStringLiteral("cameraOff"));

        m_btnVideoMute = new QPushButton(tr("Mute Video"), this);
        m_btnVideoMute->setObjectName(QStringLiteral("VideoMuteBtn"));
        m_btnVideoMute->setCheckable(true);
        m_btnVideoMute->setMinimumHeight(32);
        m_btnVideoMute->setEnabled(false);
        m_btnVideoMute->setToolTip(tr("No active video stream"));

        videoCtrlRow->addWidget(new QLabel(tr("Hardware Camera:"), this));
        videoCtrlRow->addWidget(m_btnCameraToggle);
        videoCtrlRow->addWidget(m_btnVideoMute);
        videoCtrlRow->addStretch(1);
        root->addLayout(videoCtrlRow);
    }

    // ------------------------------------------------------------------
    // SIP Ladder deep link
    // ------------------------------------------------------------------
    {
        auto *diagRow = new QHBoxLayout();
        m_btnOpenSipLadder = new QPushButton(tr("Open in SIP Ladder"), this);
        m_btnOpenSipLadder->setToolTip(
            tr("Open Tools → SIP Ladder filtered to this call's Call-ID"));
        diagRow->addWidget(m_btnOpenSipLadder);
        diagRow->addStretch(1);
        root->addLayout(diagRow);

        connect(m_btnOpenSipLadder, &QPushButton::clicked, this, [this]() {
            emit openSipLadderRequested(SipManager::instance().activeCallSipId());
        });
    }

    // ------------------------------------------------------------------
    // Emergency test mode section (hidden unless AppSettings::emergencyTestModeEnabled())
    // Ported from the previously-orphaned gui/panels/CallPanel — this is the
    // only GUI entrypoint for emergency calling, which was unreachable
    // before this task because CallPanel was never instantiated anywhere.
    // ------------------------------------------------------------------
    m_emergencyRow = new QWidget(this);
    m_emergencyRow->setObjectName(QStringLiteral("EmergencyRow"));
    m_emergencyRow->setStyleSheet(
        QStringLiteral("QWidget#EmergencyRow { background: #2a1200; border: 1px solid #8b3a00;"
                       " border-radius: 5px; }"));
    {
        auto *emerOuter = new QVBoxLayout(m_emergencyRow);
        emerOuter->setContentsMargins(10, 8, 10, 8);
        emerOuter->setSpacing(4);

        auto *emerWarnLabel = new QLabel(
            tr("[TEST/LAB] Emergency Test Mode — NOT a real emergency service"), m_emergencyRow);
        emerWarnLabel->setStyleSheet(QStringLiteral("color: #ff8c00; font-size: 10px; font-weight: bold;"));
        emerWarnLabel->setWordWrap(true);
        emerOuter->addWidget(emerWarnLabel);

        auto *locHeaderLabel = new QLabel(
            tr("[TEST/LAB] Manual Location — enter WGS-84 coordinates below. "
               "No GPS, no Windows Location."), m_emergencyRow);
        locHeaderLabel->setStyleSheet(QStringLiteral("color: #cc7700; font-size: 9px;"));
        locHeaderLabel->setWordWrap(true);
        emerOuter->addWidget(locHeaderLabel);

        auto *locInputRow = new QHBoxLayout();
        locInputRow->setSpacing(4);

        auto *latLabel = new QLabel(tr("Lat:"), m_emergencyRow);
        latLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 10px;"));
        m_latInput = new QLineEdit(QStringLiteral("44.4268"), m_emergencyRow);
        m_latInput->setFixedWidth(72);
        m_latInput->setFixedHeight(22);
        m_latInput->setToolTip(tr("-90 to 90 degrees (WGS-84)"));

        auto *lonLabel = new QLabel(tr("Lon:"), m_emergencyRow);
        lonLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 10px;"));
        m_lonInput = new QLineEdit(QStringLiteral("26.1025"), m_emergencyRow);
        m_lonInput->setFixedWidth(72);
        m_lonInput->setFixedHeight(22);
        m_lonInput->setToolTip(tr("-180 to 180 degrees (WGS-84)"));

        auto *uncLabel = new QLabel(tr("Unc(m):"), m_emergencyRow);
        uncLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 10px;"));
        m_uncertaintyInput = new QLineEdit(QStringLiteral("50.0"), m_emergencyRow);
        m_uncertaintyInput->setFixedWidth(56);
        m_uncertaintyInput->setFixedHeight(22);
        m_uncertaintyInput->setToolTip(tr("Accuracy radius in meters (>= 0)"));

        m_btnGeneratePidf = new QPushButton(tr("Generate PIDF-LO"), m_emergencyRow);
        m_btnGeneratePidf->setFixedHeight(22);
        m_btnGeneratePidf->setStyleSheet(QStringLiteral("font-size: 10px;"));

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
        m_locationStatusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 9px;"));
        m_locationStatusLabel->setWordWrap(true);
        emerOuter->addWidget(m_locationStatusLabel);

        m_pidfPreview = new QPlainTextEdit(m_emergencyRow);
        m_pidfPreview->setReadOnly(true);
        m_pidfPreview->setMaximumHeight(56);
        m_pidfPreview->setPlaceholderText(tr("PIDF-LO XML appears here after Generate is clicked"));
        m_pidfPreview->setStyleSheet(
            QStringLiteral("QPlainTextEdit { font-family: monospace; font-size: 8px;"
                           " color: #999; background: #1a1a1a; border: 1px solid #333; }"));
        emerOuter->addWidget(m_pidfPreview);

        auto *emerCtrlRow = new QHBoxLayout();
        emerCtrlRow->setSpacing(6);

        m_emergencyStateLabel = new QLabel(tr("State: Idle"), m_emergencyRow);
        m_emergencyStateLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
        emerCtrlRow->addWidget(m_emergencyStateLabel, 1);

        m_btnLocationUpdate = new QPushButton(tr("Send Location Update"), m_emergencyRow);
        m_btnLocationUpdate->setFixedHeight(30);
        m_btnLocationUpdate->setEnabled(false);
        m_btnLocationUpdate->setStyleSheet(
            QStringLiteral("QPushButton { background: #1a3a4a; color: #5090c0;"
                           " border-radius: 4px; font-size: 10px; }"
                           "QPushButton:hover { background: #2a4a5a; }"
                           "QPushButton:disabled { background: #1a2020; color: #444; }"));
        emerCtrlRow->addWidget(m_btnLocationUpdate);

        m_btnEmergency = new QPushButton(tr("112 Emergency (TEST)"), m_emergencyRow);
        m_btnEmergency->setObjectName(QStringLiteral("EmergencyBtn"));
        m_btnEmergency->setFixedHeight(30);
        m_btnEmergency->setMinimumWidth(160);
        m_btnEmergency->setEnabled(false);
        m_btnEmergency->setStyleSheet(
            QStringLiteral("QPushButton#EmergencyBtn { background: #8b0000; color: white;"
                           " border-radius: 4px; font-weight: bold; }"
                           "QPushButton#EmergencyBtn:hover { background: #b00000; }"
                           "QPushButton#EmergencyBtn:disabled { background: #3a2020; color: #666; }"));
        emerCtrlRow->addWidget(m_btnEmergency);

        emerOuter->addLayout(emerCtrlRow);
    }
    root->addWidget(m_emergencyRow);

    m_emergencyRow->setVisible(AppSettings::emergencyTestModeEnabled());

    QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!ts.endsWith(QLatin1Char('Z')))
        ts += QLatin1Char('Z');
    EmergencyLocation demoLoc = EmergencyLocation::makeStatic(44.4268, 26.1025, ts);
    demoLoc.uncertaintyMeters = 50.0;
    demoLoc.source = LocationSource::Static;

    m_staticLocationProvider = new StaticLocationProvider(demoLoc, this);
    m_emergencyController = new EmergencyCallController(this);
    m_emergencyController->setLocationProvider(m_staticLocationProvider);

    const EmergencyCallProfile emProfile = EmergencyCallProfile::makeSos(
        AppSettings::emergencyTarget(), QStringLiteral("NG112-TEST"));
    m_emergencyController->setProfile(emProfile);

    // ------------------------------------------------------------------
    // Wiring
    // ------------------------------------------------------------------
    connect(m_btnCall, &QPushButton::clicked, this, [this]() {
        placeCall(m_targetInput ? m_targetInput->text() : QString());
    });
    connect(m_btnAnswer, &QPushButton::clicked, this, []() { SipManager::instance().answerCall(); });
    connect(m_btnReject, &QPushButton::clicked, this, []() { SipManager::instance().rejectCall(); });
    connect(m_btnHangup, &QPushButton::clicked, this, []() { SipManager::instance().hangupCall(); });
    connect(m_btnMute, &QPushButton::toggled, this, [](bool on) {
        SipManager::instance().setCallMuted(on);
    });

    connect(m_btnHold, &QPushButton::toggled, this, [this](bool on) {
        Logger::instance().info(LogCategory::Sip,
            on ? QStringLiteral("Pause requested") : QStringLiteral("Resume requested"));
        m_btnHold->setText(on ? tr("Pausing...") : tr("Resuming..."));
        m_holdConfirmTimer.start();
        const bool ok = on ? SipManager::instance().holdCall() : SipManager::instance().resumeCall();
        if (!ok) {
            m_holdConfirmTimer.stop();
            Logger::instance().warn(LogCategory::Sip, QStringLiteral("Pause/resume failed"));
            refreshHoldButton();
        }
    });
    connect(&m_holdConfirmTimer, &QTimer::timeout, this, [this]() {
        const CallState state = SipManager::instance().callState();
        const bool desiredHold = m_btnHold->isChecked();
        const bool confirmed = desiredHold ? (state == CallState::Held) : (state == CallState::Active);
        if (confirmed) {
            Logger::instance().info(LogCategory::Sip,
                desiredHold ? QStringLiteral("Pause active") : QStringLiteral("Resume active"));
        } else {
            Logger::instance().warn(LogCategory::Sip, QStringLiteral("Pause/resume failed"));
            QSignalBlocker blocker(m_btnHold);
            m_btnHold->setChecked(state == CallState::Held);
        }
        refreshHoldButton();
    });

    connect(m_btnRequestVideo, &QPushButton::toggled, this, [this](bool enabled) {
        m_videoRequestFailed = false;
        const bool acceptMode = m_videoRequested && !VideoMediaManager::instance().isVideoActive();
        if (SipManager::instance().callState() == CallState::Idle
            || SipManager::instance().callState() == CallState::Failed) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Request Video %1 will apply on next call")
                    .arg(enabled ? QStringLiteral("ON") : QStringLiteral("OFF")));
            refreshCards();
            return;
        }
        const bool sendEnabled = acceptMode ? true : enabled;
        if (acceptMode) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Accepting pending incoming video request"));
        }
        if (!SipManager::instance().requestCallVideo(sendEnabled)) {
            m_videoRequestFailed = true;
            if (acceptMode) {
                m_videoRequested = true;
                m_videoRequestBlinker.start();
            }
        } else if (acceptMode) {
            m_videoRequested = false;
            m_videoRequestBlinker.stop();
        }
        refreshRequestVideoButton();
        refreshCards();
    });

    connect(m_btnRequestRtt, &QPushButton::toggled, this, [this](bool checked) {
        m_rttRequestFailed = false;
        const bool acceptMode = m_rttRequested && !SipManager::instance().rttSession()->isActive();
        if (acceptMode) {
            Logger::instance().info(LogCategory::Sip, QStringLiteral("Accepting RTT request"));
            if (!SipManager::instance().acceptIncomingRtt()) {
                m_rttRequestFailed = true;
                m_rttRequested = true;
                m_rttRequestBlinker.start();
            } else {
                m_rttRequested = false;
                m_rttRequestBlinker.stop();
            }
            refreshRequestRttButton();
            refreshCards();
            return;
        }
        if (SipManager::instance().callState() == CallState::Idle
            || SipManager::instance().callState() == CallState::Failed) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Requesting RTT (will apply on next call)"));
            refreshRequestRttButton();
            refreshCards();
            return;
        }
        if (!SipManager::instance().requestCallRtt(checked))
            m_rttRequestFailed = true;
        refreshRequestRttButton();
        refreshCards();
    });

    connect(m_micSelector, &QComboBox::currentIndexChanged, this, [this](int idx) {
        AudioMediaManager::instance().setMicrophone(m_micSelector->itemData(idx).toString());
    });
    connect(m_spkSelector, &QComboBox::currentIndexChanged, this, [this](int idx) {
        AudioMediaManager::instance().setSpeaker(m_spkSelector->itemData(idx).toString());
    });
    connect(m_cameraSelector, &QComboBox::currentIndexChanged, this, [this](int idx) {
        VideoMediaManager::instance().setCamera(m_cameraSelector->itemData(idx).toString());
    });
    connect(&MediaDeviceManager::instance(), &MediaDeviceManager::devicesChanged,
            this, [this]() { populateDeviceCombos(); refreshCards(); });

    connect(m_btnCameraToggle, &QPushButton::clicked, this, [this](bool checked) {
        // checked = camera OFF requested (button shows "Camera On" when off)
        CameraController::instance().setEnabled(!checked, QStringLiteral("CallWorkspacePanel"));
    });
    connect(&CameraController::instance(), &CameraController::enabledChanged,
            this, &CallWorkspacePanel::onCameraEnabledChanged);

    connect(m_btnVideoMute, &QPushButton::toggled, this, [](bool checked) {
        VideoMediaManager::instance().setVideoMuted(checked);
    });
    connect(&VideoMediaManager::instance(), &VideoMediaManager::videoMutedChanged,
            this, &CallWorkspacePanel::onVideoMutedChanged);

    connect(&SipManager::instance(), &SipManager::callStateChanged,
            this, &CallWorkspacePanel::onCallStateChanged);
    connect(&SipManager::instance(), &SipManager::callConnected,
            this, &CallWorkspacePanel::onCallConnected);
    connect(&SipManager::instance(), &SipManager::callDisconnected,
            this, &CallWorkspacePanel::onCallDisconnected);
    connect(&SipManager::instance(), &SipManager::callFailed,
            this, &CallWorkspacePanel::onCallFailed);
    connect(&SipManager::instance(), &SipManager::audioMediaConnected,
            this, &CallWorkspacePanel::onAudioMediaConnected);
    connect(&SipManager::instance(), &SipManager::audioMediaDisconnected,
            this, &CallWorkspacePanel::onAudioMediaDisconnected);
    connect(&SipManager::instance(), &SipManager::videoMediaConnected,
            this, &CallWorkspacePanel::onVideoMediaConnected);
    connect(&SipManager::instance(), &SipManager::videoMediaDisconnected,
            this, &CallWorkspacePanel::onVideoMediaDisconnected);
    connect(&SipManager::instance(), &SipManager::videoRequested,
            this, &CallWorkspacePanel::onVideoRequested);
    connect(&SipManager::instance(), &SipManager::rttMediaConnected,
            this, &CallWorkspacePanel::onRttMediaConnected);
    connect(&SipManager::instance(), &SipManager::rttMediaDisconnected,
            this, &CallWorkspacePanel::onRttMediaDisconnected);
    connect(&SipManager::instance(), &SipManager::rttRequested,
            this, &CallWorkspacePanel::onRttRequested);
    // RTT has explicit reject/withdraw/negotiation-failed signals that video
    // has no equivalent for (see docs/incoming-media-request-alerts.md) --
    // wired here so the alert can't keep flashing for a request that's no
    // longer live (peer cancelled, user rejected via the MediaRequestDialog
    // popup, or the re-INVITE itself failed at the transport/SDP level).
    connect(&SipManager::instance(), &SipManager::rttRequestRejected, this, [this]() {
        m_rttRequested = false;
        m_rttRequestBlinker.stop();
        refreshRequestRttButton();
        refreshCards();
    });
    connect(&SipManager::instance(), &SipManager::rttRequestWithdrawn, this, [this]() {
        m_rttRequested = false;
        m_rttRequestBlinker.stop();
        refreshRequestRttButton();
        refreshCards();
    });
    connect(&SipManager::instance(), &SipManager::rttNegotiationFailed, this, [this](const QString &) {
        m_rttRequested = false;
        m_rttRequestBlinker.stop();
        refreshRequestRttButton();
        refreshCards();
    });
    connect(&SipManager::instance(), &SipManager::rtpStatsChanged,
            this, [this](const RtpStatsSnapshot &stats) {
        m_callInfoModel->setRtpStats(stats);
        refreshCards();
    });
    connect(&VideoMediaManager::instance(), &VideoMediaManager::localVideoStarted,
            this, &CallWorkspacePanel::onLocalVideoStarted);
    connect(&VideoMediaManager::instance(), &VideoMediaManager::localVideoStopped,
            this, &CallWorkspacePanel::onLocalVideoStopped);
    connect(&VideoMediaManager::instance(), &VideoMediaManager::remoteVideoStarted,
            this, &CallWorkspacePanel::onRemoteVideoStarted);
    connect(&VideoMediaManager::instance(), &VideoMediaManager::remoteVideoStopped,
            this, &CallWorkspacePanel::onRemoteVideoStopped);
    connect(&VideoStatistics::instance(), &VideoStatistics::statsUpdated,
            this, &CallWorkspacePanel::onVideoStatsUpdated);
    connect(&PresenceStore::instance(), &PresenceStore::presenceUpdated,
            this, [this](const PresenceInfo &info) { onPresenceUpdated(info.entityUri); });

    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            this, [this](RegistrationState state, const QString &, int) {
        m_btnCall->setEnabled(state == RegistrationState::Registered
                              && (SipManager::instance().callState() == CallState::Idle
                                  || SipManager::instance().callState() == CallState::Failed));
    });

    connect(&m_durationTimer, &QTimer::timeout, this, &CallWorkspacePanel::onDurationTick);
    m_holdConfirmTimer.setSingleShot(true);
    m_holdConfirmTimer.setInterval(1200);
    m_durationTimer.setInterval(1000);

    connect(m_emergencyController, &EmergencyCallController::stateChanged,
            this, &CallWorkspacePanel::onEmergencyStateChanged);
    connect(m_emergencyController, &EmergencyCallController::readyToDial,
            this, &CallWorkspacePanel::onEmergencyReadyToDial);
    connect(m_emergencyController, &EmergencyCallController::preparationFailed,
            this, &CallWorkspacePanel::onEmergencyFailed);
    connect(m_btnEmergency, &QPushButton::clicked, this, &CallWorkspacePanel::onEmergencyButtonClicked);
    connect(m_btnGeneratePidf, &QPushButton::clicked, this, &CallWorkspacePanel::onGeneratePidfClicked);
    connect(m_btnLocationUpdate, &QPushButton::clicked, this, &CallWorkspacePanel::onLocationUpdateClicked);
    connect(&SipManager::instance(), &SipManager::callConnected, this, [this](const QString &) {
        if (m_emergencyCallActive)
            m_emergencyController->stateMachine().transition(
                EmergencyCallState::Active, QStringLiteral("call connected"));
    });
    connect(&SipManager::instance(), &SipManager::callDisconnected, this,
            [this](const QString &, const QString &, int) {
        if (m_emergencyCallActive) {
            m_emergencyCallActive = false;
            m_btnLocationUpdate->setEnabled(false);
            m_emergencyController->stateMachine().transition(
                EmergencyCallState::Ended, QStringLiteral("call ended"));
            m_emergencyController->stateMachine().reset();
        }
    });
    connect(&SipManager::instance(), &SipManager::callFailed, this,
            [this](const QString &, const QString &reason, int) {
        if (m_emergencyCallActive) {
            m_emergencyCallActive = false;
            m_btnLocationUpdate->setEnabled(false);
            m_emergencyController->stateMachine().transition(
                EmergencyCallState::Failed, QStringLiteral("call failed: %1").arg(reason));
        }
    });

    populateDeviceCombos();
    resetStatusCards();
    refreshHoldButton();
    refreshRequestVideoButton();
    refreshRequestRttButton();
    refreshCards();
}

// ---------------------------------------------------------------------------

void CallWorkspacePanel::placeCall(const QString &uri)
{
    const QString raw = uri.trimmed();
    if (raw.isEmpty()) {
        emit statusMessageRequested(tr("Enter a SIP URI or number to call"), 4000);
        return;
    }
    const QString fallbackDomain = SipProfileManager::instance().activeProfile().sipDomain;
    const SipUriNormalizer::Result result = SipUriNormalizer::normalize(raw, fallbackDomain);
    if (!result.isValid) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Dial URI invalid: input=\"%1\" error=\"%2\"").arg(raw, result.error));
        emit statusMessageRequested(tr("Invalid URI: %1").arg(result.error), 5000);
        return;
    }
    if (m_targetInput && result.uri != raw)
        m_targetInput->setText(result.uri);

    CallType callType = CallType::AudioOnly;
    if (m_btnRequestVideo->isChecked() && m_btnRequestRtt->isChecked())
        callType = CallType::AudioVideoRtt;
    else if (m_btnRequestVideo->isChecked())
        callType = CallType::AudioVideo;
    else if (m_btnRequestRtt->isChecked())
        callType = CallType::AudioRtt;
    m_selectedMedia = CallMediaOptions::fromType(callType);
    m_callInfoModel->setSelectedMedia(m_selectedMedia);

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Placing call: uri=%1 type=%2").arg(result.uri, callTypeName(callType)));
    if (!SipManager::instance().makeCall(result.uri, m_selectedMedia)) {
        // makeCall() returns false without any other user-visible signal
        // (no callStateChanged/callFailed is ever emitted, since no SipCall
        // was created) — most commonly because a call is already active.
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Call to %1 rejected — see the warning above/in Diagnostics for the exact reason")
                .arg(result.uri));
        emit statusMessageRequested(tr("Call could not be started (see Diagnostics log for details)"), 5000);
    }
}

void CallWorkspacePanel::populateDeviceCombos()
{
    const MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    const QString curMic = sel.selectedMicrophone().id;
    const QString curSpk = sel.selectedSpeaker().id;
    const QString curCam = sel.selectedCamera().id;

    const auto fillCombo = [](QComboBox *combo, const QList<MediaDevice> &devices,
                              const QString &currentId, const QString &defaultLabel) {
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem(defaultLabel, QString());
        for (const MediaDevice &d : devices)
            combo->addItem(d.displayName, d.id);
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == currentId) {
                combo->setCurrentIndex(i);
                return;
            }
        }
    };

    fillCombo(m_micSelector, MediaDeviceManager::instance().listMicrophones(), curMic, tr("Default"));
    fillCombo(m_spkSelector, MediaDeviceManager::instance().listSpeakers(), curSpk, tr("Default"));
    fillCombo(m_cameraSelector, MediaDeviceManager::instance().listCameras(), curCam, tr("Default"));

    m_callInfoModel->setDeviceNames(m_micSelector->currentText(), m_spkSelector->currentText(),
                                     m_cameraSelector->currentText());
}

void CallWorkspacePanel::refreshHoldButton()
{
    const CallState state = SipManager::instance().callState();
    const bool held = (state == CallState::Held);
    const bool active = (state == CallState::Active || state == CallState::Held);
    QSignalBlocker blocker(m_btnHold);
    m_btnHold->setEnabled(active);
    m_btnHold->setChecked(held);
    m_btnHold->setText(held ? tr("Resume") : tr("Pause"));
    m_btnHold->setProperty("callRole", held ? QStringLiteral("resume") : QStringLiteral("pause"));
    m_btnHold->style()->unpolish(m_btnHold);
    m_btnHold->style()->polish(m_btnHold);
    m_callInfoModel->setHeld(held);
}

void CallWorkspacePanel::refreshRequestVideoButton()
{
    const bool videoActive = VideoMediaManager::instance().isVideoActive();
    const bool acceptMode = m_videoRequested && !videoActive;
    QSignalBlocker blocker(m_btnRequestVideo);
    if (videoActive) {
        m_btnRequestVideo->setText(tr("Video On"));
        m_btnRequestVideo->setProperty("callRole", QStringLiteral("videoActive"));
        m_btnRequestVideo->setProperty("videoAlert", false);
        m_btnRequestVideo->setEnabled(false);
        m_btnRequestVideo->setChecked(true);
        m_btnRequestVideo->setToolTip(tr("Video is active on this call"));
        m_btnRequestVideo->setAccessibleName(tr("Video active"));
        m_btnRequestVideo->setAccessibleDescription(QString());
    } else if (acceptMode) {
        m_btnRequestVideo->setText(tr("Accept Video"));
        m_btnRequestVideo->setProperty("callRole", QStringLiteral("acceptVideo"));
        m_btnRequestVideo->setProperty("videoAlert", m_videoRequestBlinker.isOn());
        m_btnRequestVideo->setEnabled(true);
        m_btnRequestVideo->setChecked(false); // must be unchecked so click fires toggled(true)
        m_btnRequestVideo->setToolTip(tr("Incoming video request — click to accept"));
        m_btnRequestVideo->setAccessibleName(tr("Accept incoming video request"));
        m_btnRequestVideo->setAccessibleDescription(
            tr("The remote party is requesting to add video to this call."));
    } else {
        m_btnRequestVideo->setText(tr("Request Video"));
        m_btnRequestVideo->setProperty("callRole", QStringLiteral("requestVideo"));
        m_btnRequestVideo->setProperty("videoAlert", false);
        m_btnRequestVideo->setEnabled(true);
        m_btnRequestVideo->setChecked(false);
        m_btnRequestVideo->setToolTip(tr("Request video for this call"));
        m_btnRequestVideo->setAccessibleName(tr("Request video"));
        m_btnRequestVideo->setAccessibleDescription(QString());
    }
    m_btnRequestVideo->style()->unpolish(m_btnRequestVideo);
    m_btnRequestVideo->style()->polish(m_btnRequestVideo);
    m_callInfoModel->setVideoRequested(m_videoRequested);
    m_callInfoModel->setVideoConnected(videoActive);
}

void CallWorkspacePanel::refreshRequestRttButton()
{
    const bool rttActive = SipManager::instance().rttSession()->isActive();
    const bool acceptMode = m_rttRequested && !rttActive;
    QSignalBlocker blocker(m_btnRequestRtt);
    if (rttActive) {
        m_btnRequestRtt->setText(tr("RTT Active"));
        m_btnRequestRtt->setProperty("callRole", QStringLiteral("rttActive"));
        m_btnRequestRtt->setProperty("rttAlert", false);
        m_btnRequestRtt->setEnabled(false);
        m_btnRequestRtt->setChecked(true);
        m_btnRequestRtt->setToolTip(tr("RTT is active on this call"));
        m_btnRequestRtt->setAccessibleName(tr("RTT active"));
        m_btnRequestRtt->setAccessibleDescription(QString());
    } else if (acceptMode) {
        m_btnRequestRtt->setText(tr("Accept RTT"));
        m_btnRequestRtt->setProperty("callRole", QStringLiteral("acceptRtt"));
        m_btnRequestRtt->setProperty("rttAlert", m_rttRequestBlinker.isOn());
        m_btnRequestRtt->setEnabled(true);
        m_btnRequestRtt->setChecked(false); // must be unchecked so click fires toggled(true)
        m_btnRequestRtt->setToolTip(tr("Incoming RTT (real-time text) request — click to accept"));
        m_btnRequestRtt->setAccessibleName(tr("Accept incoming RTT request"));
        m_btnRequestRtt->setAccessibleDescription(
            tr("The remote party is requesting to add real-time text to this call."));
    } else {
        m_btnRequestRtt->setText(tr("Request RTT"));
        m_btnRequestRtt->setProperty("callRole", QStringLiteral("requestRtt"));
        m_btnRequestRtt->setProperty("rttAlert", false);
        m_btnRequestRtt->setEnabled(true);
        m_btnRequestRtt->setChecked(false);
        m_btnRequestRtt->setToolTip(tr("Request RTT (real-time text) for this call"));
        m_btnRequestRtt->setAccessibleName(tr("Request RTT"));
        m_btnRequestRtt->setAccessibleDescription(QString());
    }
    m_btnRequestRtt->style()->unpolish(m_btnRequestRtt);
    m_btnRequestRtt->style()->polish(m_btnRequestRtt);
    m_callInfoModel->setRttRequested(m_rttRequested);
    m_callInfoModel->setRttConnected(rttActive);
}

void CallWorkspacePanel::refreshPresenceCard()
{
    if (m_remoteUri.isEmpty()) {
        m_cardPresence->setValue(QStringLiteral("—"));
        m_cardPresence->setStatus({});
        m_callInfoModel->setPresenceText(QString());
        return;
    }
    const PresenceInfo info = PresenceStore::instance().current(m_remoteUri);
    const QString text = info.subscriptionState == PresenceInfo::SubscriptionState::Unknown
        ? tr("Not available")
        : PresenceInfo::extendedStatusToString(info.extendedStatus);
    m_cardPresence->setValue(text);
    m_cardPresence->setStatus(info.subscriptionState == PresenceInfo::SubscriptionState::Unknown
        ? QString() : QStringLiteral("ok"));
    m_callInfoModel->setPresenceText(text);
}

void CallWorkspacePanel::onPresenceUpdated(const QString &entity)
{
    if (!m_remoteUri.isEmpty() && entity.compare(m_remoteUri, Qt::CaseInsensitive) == 0)
        refreshPresenceCard();
}

void CallWorkspacePanel::refreshCards()
{
    const CallState state = SipManager::instance().callState();
    const QString statusText = SipManager::instance().callStatusText();
    const bool active = (state == CallState::Active || state == CallState::Held);
    const bool audioActive = (state != CallState::Idle && state != CallState::Failed);
    const bool videoActive = VideoMediaManager::instance().isVideoActive();
    const bool localVideo = VideoMediaManager::instance().isLocalVideoAvailable();
    const bool remoteVideo = VideoMediaManager::instance().isRemoteVideoAvailable();
    const QString remoteUri = SipManager::instance().activeCallRemoteUri();
    const QString localAccount = SipProfileManager::instance().activeProfile().isNull()
        ? QString() : SipProfileManager::instance().activeProfile().effectiveSipUri();
    const VideoSettings vs = VideoQualityManager::instance().current();

    m_callInfoModel->setState(state, statusText);

    m_cardState->setValue(statusText.isEmpty() ? tr("—") : statusText);
    m_cardState->setStatus(active ? QStringLiteral("ok")
                                  : (state == CallState::Idle ? QString() : QStringLiteral("warn")));

    if (m_remoteUri != remoteUri) {
        m_remoteUri = remoteUri;
        m_callInfoModel->setRemoteUri(remoteUri);
        const QString name = displayNameFor(remoteUri);
        m_callInfoModel->setDisplayName(name);
        refreshPresenceCard();
    }
    const QString remoteName = displayNameFor(remoteUri);
    const QString remoteDisplay = remoteUri.isEmpty()
        ? QStringLiteral("—")
        : (remoteName.isEmpty() ? remoteUri : QStringLiteral("%1 (%2)").arg(remoteName, remoteUri));
    m_cardRemoteUri->setValue(remoteDisplay);
    m_cardRemoteUri->setStatus(remoteUri.isEmpty() ? QString() : QStringLiteral("ok"));

    m_cardAudio->setValue(audioActive ? tr("Connected") : QStringLiteral("—"));
    m_cardAudio->setStatus(audioActive ? QStringLiteral("ok") : QString());
    m_callInfoModel->setMuted(m_btnMute->isChecked());

    if (m_videoRequestFailed) {
        m_cardRemoteVideo->setValue(tr("Failed"));
        m_cardRemoteVideo->setStatus(QStringLiteral("err"));
    } else {
        const bool acceptMode = m_videoRequested && !videoActive;
        const bool noCapture = videoActive && !SipManager::instance().hasPjsipVideoCapture();
        m_cardRemoteVideo->setValue(videoActive
            ? (noCapture ? tr("Negotiated, no local capture") : tr("Active"))
            : (acceptMode ? tr("Accept Video")
                          : (m_btnRequestVideo->isChecked() ? tr("Requested") : tr("Not requested"))));
        m_cardRemoteVideo->setStatus(videoActive ? QStringLiteral("ok")
            : ((acceptMode || m_btnRequestVideo->isChecked()) ? QStringLiteral("warn") : QString()));
    }
    if (m_rttRequestFailed) {
        m_cardRtt->setValue(tr("Failed"));
        m_cardRtt->setStatus(QStringLiteral("err"));
    } else {
        const bool rttActive = SipManager::instance().rttSession()->isActive();
        m_cardRtt->setValue(rttActive ? tr("Active")
                                       : (m_btnRequestRtt->isChecked() ? tr("Requested") : tr("Not requested")));
        m_cardRtt->setStatus(rttActive ? QStringLiteral("ok")
                                        : (m_btnRequestRtt->isChecked() ? QStringLiteral("warn") : QString()));
    }
    // Task W113F: LMPE has no interoperable wire format yet and can never
    // become active/negotiated/selected in this build (see
    // docs/lmpe-disabled-status.md) -- always show that plainly instead of
    // an ambiguous "—" that could be misread as "not checked yet".
    m_cardLmpe->setValue(tr("Unavailable"));
    m_cardLmpe->setStatus({});
    m_cardLocalVideo->setValue(localVideo ? tr("On") : tr("Off"));
    m_cardLocalVideo->setStatus(localVideo ? QStringLiteral("ok") : QString());

    if (videoActive || localVideo) {
        const VideoCodecInfo neg = SipManager::instance().activeVideoCodecInfo();
        m_callInfoModel->setNegotiatedVideo(neg);
        if (neg.isValid()) {
            m_cardVideoCodec->setValue(neg.name);
            m_cardBitrate->setValue(neg.bitrate > 0
                ? QStringLiteral("%1 kbps").arg(neg.bitrate / 1000) : QStringLiteral("—"));
            m_cardResolution->setValue((neg.width > 0 && neg.height > 0)
                ? QStringLiteral("%1x%2").arg(neg.width).arg(neg.height) : QStringLiteral("—"));
        } else {
            m_cardVideoCodec->setValue(vs.codecOrder.isEmpty() ? QStringLiteral("—") : vs.codecOrder.first());
            m_cardBitrate->setValue(QStringLiteral("%1 kbps").arg(vs.bitrateKbps));
            m_cardResolution->setValue(QStringLiteral("%1x%2").arg(vs.resolution.width()).arg(vs.resolution.height()));
        }
        m_cardVideoCodec->setStatus(QStringLiteral("ok"));
        m_cardBitrate->setStatus(QStringLiteral("ok"));
        m_cardResolution->setStatus(QStringLiteral("ok"));
    } else {
        m_callInfoModel->setNegotiatedVideo(VideoCodecInfo());
        m_cardVideoCodec->setValue(QStringLiteral("—"));
        m_cardVideoCodec->setStatus({});
        m_cardBitrate->setValue(QStringLiteral("—"));
        m_cardBitrate->setStatus({});
        m_cardResolution->setValue(QStringLiteral("—"));
        m_cardResolution->setStatus({});
    }

    {
        const AudioCodecInfo neg = SipManager::instance().activeAudioCodecInfo();
        m_callInfoModel->setNegotiatedAudio(neg);
        if (audioActive && neg.isValid()) {
            m_cardAudioCodec->setValue(QStringLiteral("%1/%2").arg(neg.name).arg(neg.clockRate));
            m_cardAudioCodec->setStatus(QStringLiteral("ok"));
        } else {
            m_cardAudioCodec->setValue(QStringLiteral("—"));
            m_cardAudioCodec->setStatus({});
        }
    }

    m_cardFps->setStatus((videoActive || localVideo) ? QStringLiteral("ok") : QString());
    m_cardLocalAccount->setValue(localAccount.isEmpty() ? QStringLiteral("—") : localAccount);
    m_cardLocalAccount->setStatus(localAccount.isEmpty() ? QString() : QStringLiteral("ok"));

    if (m_remoteUri.isEmpty()) {
        m_cardInitialOffer->setValue(QStringLiteral("—"));
        m_cardInitialOffer->setStatus({});
    } else {
        m_cardInitialOffer->setValue(callTypeName(m_selectedMedia.type));
        m_cardInitialOffer->setStatus(QStringLiteral("ok"));
    }

    // RTP quality metrics from live RTCP stats — "—" when not reported.
    // Packet loss here is always the RTCP percentage, never conflated with
    // the video pipeline's own frame-drop count (see m_cardVideoDrops /
    // onVideoStatsUpdated, and docs/call-state-and-media-model.md).
    const RtpStatsSnapshot rtp = SipManager::instance().currentRtpStats();
    if (rtp.available && rtp.packetLossAvailable) {
        m_cardPacketLoss->setValue(QStringLiteral("%1 %").arg(rtp.packetLossPercent, 0, 'f', 1));
        m_cardPacketLoss->setStatus(rtp.packetLossPercent > 1.0 ? QStringLiteral("warn") : QStringLiteral("ok"));
    } else {
        m_cardPacketLoss->setValue(QStringLiteral("—"));
        m_cardPacketLoss->setStatus({});
    }
    if (rtp.available && rtp.jitterAvailable) {
        m_cardJitter->setValue(QStringLiteral("%1 ms").arg(rtp.jitterMs, 0, 'f', 1));
        m_cardJitter->setStatus(rtp.jitterMs > 50.0 ? QStringLiteral("warn") : QStringLiteral("ok"));
    } else {
        m_cardJitter->setValue(QStringLiteral("—"));
        m_cardJitter->setStatus({});
    }
    if (rtp.available && rtp.rttAvailable) {
        m_cardLatency->setValue(QStringLiteral("%1 ms").arg(rtp.rttMs, 0, 'f', 1));
        m_cardLatency->setStatus(rtp.rttMs > 300.0 ? QStringLiteral("warn") : QStringLiteral("ok"));
    } else {
        m_cardLatency->setValue(QStringLiteral("—"));
        m_cardLatency->setStatus({});
    }

    refreshRequestVideoButton();
}

void CallWorkspacePanel::resetStatusCards()
{
    m_remoteUri.clear();
    m_audioConnected = false;
    m_videoConnected = false;
    m_videoRequested = false;
    m_videoRequestFailed = false;
    m_localVideoActive = false;
    m_remoteVideoActive = false;
    m_rttConnected = false;
    m_rttRequested = false;
    m_rttRequestFailed = false;
    m_selectedMedia = CallMediaOptions();
    m_videoRequestBlinker.stop();
    m_rttRequestBlinker.stop();
    m_callInfoModel->reset();

    m_cardDuration->setValue(QStringLiteral("00:00:00"));
    m_cardDuration->setStatus({});
    StatusCard *allCards[] = {
        m_cardAudio, m_cardAudioCodec, m_cardLocalVideo, m_cardRemoteVideo, m_cardRtt,
        m_cardVideoCodec, m_cardBitrate, m_cardResolution, m_cardFps, m_cardRemoteUri, m_cardPresence,
        m_cardInitialOffer, m_cardLocalAccount, m_cardPacketLoss, m_cardVideoDrops, m_cardJitter, m_cardLatency
    };
    for (StatusCard *c : allCards) {
        c->setValue(QStringLiteral("—"));
        c->setStatus({});
    }
    // Task W113F: never "—" -- LMPE is permanently unavailable, not merely
    // "not checked yet" (see docs/lmpe-disabled-status.md).
    m_cardLmpe->setValue(tr("Unavailable"));
    m_cardLmpe->setStatus({});
    if (m_inputMeter) m_inputMeter->setLevel(0);
    if (m_outputMeter) m_outputMeter->setLevel(0);
}

QString CallWorkspacePanel::formatDuration(int seconds) const
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

void CallWorkspacePanel::onCallStateChanged(CallState state, const QString &, int)
{
    const bool hasCall = (state != CallState::Idle && state != CallState::Failed);
    m_btnAnswer->setVisible(state == CallState::IncomingRinging);
    m_btnReject->setVisible(state == CallState::IncomingRinging);
    m_btnHangup->setVisible(hasCall && state != CallState::IncomingRinging);
    m_btnHold->setEnabled(state == CallState::Active || state == CallState::Held);
    m_btnMute->setEnabled(state == CallState::Active);
    m_btnCall->setEnabled(SipManager::instance().registrationState() == RegistrationState::Registered
                          && (state == CallState::Idle || state == CallState::Failed));

    if (state == CallState::Idle || state == CallState::Failed) {
        m_durationTimer.stop();
        m_durationSeconds = 0;
        m_videoRequested = false;
        m_videoRequestBlinker.stop();
        m_rttRequested = false;
        m_rttRequestBlinker.stop();
        refreshRequestVideoButton();
        refreshRequestRttButton();
        resetStatusCards();
    }
    if (state == CallState::Active || state == CallState::Held)
        m_holdConfirmTimer.stop();
    refreshHoldButton();
    m_cardState->setValue(callStateDisplayText(state));
    refreshCards();
}

void CallWorkspacePanel::onCallConnected(const QString &)
{
    m_durationSeconds = 0;
    m_durationTimer.start();
    refreshCards();
}

void CallWorkspacePanel::onCallDisconnected(const QString &, const QString &, int)
{
    m_videoRequested = false;
    m_videoRequestBlinker.stop();
    m_rttRequested = false;
    m_rttRequestBlinker.stop();
    refreshRequestVideoButton();
    refreshRequestRttButton();
    refreshCards();
}

void CallWorkspacePanel::onCallFailed(const QString &, const QString &, int)
{
    m_videoRequested = false;
    m_videoRequestBlinker.stop();
    m_rttRequested = false;
    m_rttRequestBlinker.stop();
    refreshRequestVideoButton();
    refreshRequestRttButton();
    refreshCards();
}

void CallWorkspacePanel::onAudioMediaConnected() { m_audioConnected = true; refreshCards(); }
void CallWorkspacePanel::onAudioMediaDisconnected() { m_audioConnected = false; refreshCards(); }

void CallWorkspacePanel::onVideoMediaConnected()
{
    m_videoConnected = true;
    m_videoRequestFailed = false;
    m_videoRequested = false;
    m_videoRequestBlinker.stop();
    refreshRequestVideoButton();
    refreshCards();
}

void CallWorkspacePanel::onVideoMediaDisconnected()
{
    m_videoConnected = false;
    m_videoRequested = false;
    m_videoRequestBlinker.stop();
    refreshRequestVideoButton();
    refreshCards();
}

void CallWorkspacePanel::onVideoRequested()
{
    m_videoRequestFailed = false;
    m_videoRequested = true;
    refreshRequestVideoButton();
    m_videoRequestBlinker.start();
    refreshCards();
}

void CallWorkspacePanel::onLocalVideoStarted() { m_localVideoActive = true; refreshCards(); }
void CallWorkspacePanel::onLocalVideoStopped() { m_localVideoActive = false; refreshCards(); }
void CallWorkspacePanel::onRemoteVideoStarted() { m_remoteVideoActive = true; refreshCards(); }
void CallWorkspacePanel::onRemoteVideoStopped() { m_remoteVideoActive = false; refreshCards(); }

void CallWorkspacePanel::onRttMediaConnected()
{
    m_rttConnected = true;
    m_rttRequested = false;
    m_rttRequestFailed = false;
    m_rttRequestBlinker.stop();
    refreshRequestRttButton();
    refreshCards();
}

void CallWorkspacePanel::onRttMediaDisconnected()
{
    m_rttConnected = false;
    // A disconnect while a request was still pending (never reached Active)
    // means the offer was declined/withdrawn rather than a normal hangup of
    // an established RTT stream -- either way the alert must not keep
    // flashing for a request that's no longer live.
    m_rttRequested = false;
    m_rttRequestBlinker.stop();
    refreshRequestRttButton();
    refreshCards();
}

void CallWorkspacePanel::onRttRequested()
{
    m_rttRequestFailed = false;
    m_rttRequested = true;
    refreshRequestRttButton();
    m_rttRequestBlinker.start();
    refreshCards();
}

void CallWorkspacePanel::onVideoStatsUpdated(float fps, int dropsThisSec)
{
    if (!(m_videoConnected || m_localVideoActive))
        return;
    m_cardFps->setValue(QStringLiteral("%1 fps").arg(fps, 0, 'f', 1));
    m_cardFps->setStatus(fps < 10.f ? QStringLiteral("warn") : QStringLiteral("ok"));
    // Distinct from m_cardPacketLoss (RTCP %) — see refreshCards().
    m_cardVideoDrops->setValue(QStringLiteral("%1").arg(dropsThisSec));
    m_cardVideoDrops->setStatus(dropsThisSec > 0 ? QStringLiteral("warn") : QStringLiteral("ok"));
    m_callInfoModel->setVideoStats(fps, dropsThisSec);
}

void CallWorkspacePanel::onDurationTick()
{
    ++m_durationSeconds;
    const QString dur = formatDuration(m_durationSeconds);
    m_cardDuration->setValue(dur);
    m_cardDuration->setStatus(QStringLiteral("ok"));
    m_callInfoModel->setDurationSeconds(m_durationSeconds);
}

void CallWorkspacePanel::onVideoMutedChanged(bool muted)
{
    QSignalBlocker b(m_btnVideoMute);
    m_btnVideoMute->setChecked(muted);
    m_btnVideoMute->setText(muted ? tr("Unmute Video") : tr("Mute Video"));
    const bool streamActive = m_videoConnected || m_localVideoActive;
    m_btnVideoMute->setEnabled(streamActive);
    m_btnVideoMute->setToolTip(streamActive
        ? (muted ? tr("Video transmission stopped — click to resume") : tr("Stop video transmission"))
        : tr("No active video stream"));
}

void CallWorkspacePanel::onCameraEnabledChanged(bool enabled)
{
    QSignalBlocker b(m_btnCameraToggle);
    m_btnCameraToggle->setChecked(!enabled);
    m_btnCameraToggle->setText(enabled ? tr("Camera Off") : tr("Camera On"));
    m_btnCameraToggle->setProperty("callRole",
        enabled ? QStringLiteral("cameraOff") : QStringLiteral("cameraOn"));
    m_btnCameraToggle->style()->unpolish(m_btnCameraToggle);
    m_btnCameraToggle->style()->polish(m_btnCameraToggle);
    m_cardCamera->setValue(enabled ? tr("On") : tr("Off"));
    m_cardCamera->setStatus(enabled ? QStringLiteral("ok") : QStringLiteral("warn"));
    m_btnVideoMute->setEnabled(m_videoConnected || m_localVideoActive);
}

void CallWorkspacePanel::onAdvancedDiagnosticsToggled(bool expanded)
{
    m_advancedToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_advancedHost->setVisible(expanded);
    AppSettings::setCallWorkspaceAdvancedDiagnosticsExpanded(expanded);
}

// ---------------------------------------------------------------------------
// Emergency call slots (ported from the previously-orphaned CallPanel)
// ---------------------------------------------------------------------------

void CallWorkspacePanel::onEmergencyButtonClicked()
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
        this, tr("Confirm Emergency Test Call — TEST/LAB Only"), msg,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        Logger::instance().info(LogCategory::App,
            QStringLiteral("Emergency test call: user cancelled confirm dialog"));
        return;
    }

    Logger::instance().info(LogCategory::App,
        QStringLiteral("[EMERGENCY TEST/DEMO] Emergency call initiated — target=%1").arg(target));
    m_btnEmergency->setEnabled(false);

    const EmergencyCallProfile emProfile = EmergencyCallProfile::makeSos(target, QStringLiteral("NG112-TEST"));
    m_emergencyController->setProfile(emProfile);
    m_emergencyController->prepare();
}

void CallWorkspacePanel::onEmergencyReadyToDial(const EmergencyCallProfile &profile)
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
        m_emergencyController->stateMachine().transition(EmergencyCallState::Failed, reason);
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

void CallWorkspacePanel::onEmergencyStateChanged(EmergencyCallState state)
{
    const QString name = emergencyCallStateName(state);
    QString color = QStringLiteral("#aaa");
    switch (state) {
    case EmergencyCallState::Active: color = QStringLiteral("#50c878"); break;
    case EmergencyCallState::Dialing:
    case EmergencyCallState::Preparing:
    case EmergencyCallState::LocationPending:
    case EmergencyCallState::ReadyToDial: color = QStringLiteral("#e0b850"); break;
    case EmergencyCallState::Failed: color = QStringLiteral("#e05050"); break;
    case EmergencyCallState::Ended: color = QStringLiteral("#5090e0"); break;
    default: break;
    }
    m_emergencyStateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(color));
    m_emergencyStateLabel->setText(tr("State: %1").arg(name));

    const bool registered = (SipManager::instance().registrationState() == RegistrationState::Registered);
    const bool canDial = (state == EmergencyCallState::Idle || state == EmergencyCallState::Failed
                          || state == EmergencyCallState::Ended);
    m_btnEmergency->setEnabled(registered && canDial);
    m_btnLocationUpdate->setEnabled(state == EmergencyCallState::Active && m_manualLocationValid);
}

void CallWorkspacePanel::onEmergencyFailed(const QString &reason)
{
    Logger::instance().warn(LogCategory::App,
        QStringLiteral("[EMERGENCY] Preparation failed: %1").arg(reason));
    m_emergencyStateLabel->setStyleSheet(QStringLiteral("color: #e05050; font-size: 10px;"));
    m_emergencyStateLabel->setText(tr("State: Failed — %1").arg(reason));
    const bool registered = (SipManager::instance().registrationState() == RegistrationState::Registered);
    m_btnEmergency->setEnabled(registered);
    m_btnLocationUpdate->setEnabled(false);
}

void CallWorkspacePanel::onGeneratePidfClicked()
{
    bool latOk = false, lonOk = false, uncOk = false;
    const double lat = m_latInput->text().trimmed().toDouble(&latOk);
    const double lon = m_lonInput->text().trimmed().toDouble(&lonOk);
    const double unc = m_uncertaintyInput->text().trimmed().toDouble(&uncOk);

    QStringList errors;
    if (!latOk || lat < -90.0 || lat > 90.0) errors << tr("Latitude must be -90..90");
    if (!lonOk || lon < -180.0 || lon > 180.0) errors << tr("Longitude must be -180..180");
    if (!uncOk || unc < 0.0) errors << tr("Uncertainty must be >= 0");

    if (!errors.isEmpty()) {
        m_locationStatusLabel->setText(tr("Invalid: %1").arg(errors.join(QStringLiteral("; "))));
        m_locationStatusLabel->setStyleSheet(QStringLiteral("color: #e05050; font-size: 9px;"));
        m_pidfPreview->clear();
        m_manualLocationValid = false;
        m_btnLocationUpdate->setEnabled(false);
        return;
    }

    QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!ts.endsWith(QLatin1Char('Z'))) ts += QLatin1Char('Z');

    EmergencyLocation loc = EmergencyLocation::makeStatic(lat, lon, ts);
    loc.uncertaintyMeters = unc;
    loc.source = LocationSource::Manual;

    if (!loc.isValid()) {
        const QStringList ve = loc.validationErrors();
        m_locationStatusLabel->setText(tr("Location invalid: %1").arg(ve.join(QStringLiteral("; "))));
        m_locationStatusLabel->setStyleSheet(QStringLiteral("color: #e05050; font-size: 9px;"));
        m_pidfPreview->clear();
        m_manualLocationValid = false;
        m_btnLocationUpdate->setEnabled(false);
        return;
    }

    const QString cid = EmergencyCallAdapter::generateContentId();
    const PidfLoResult r = PidfLoBuilder(loc).setContentId(cid).build();
    if (!r.success) {
        m_locationStatusLabel->setText(tr("PIDF-LO build failed: %1").arg(r.error));
        m_locationStatusLabel->setStyleSheet(QStringLiteral("color: #e05050; font-size: 9px;"));
        m_pidfPreview->clear();
        m_manualLocationValid = false;
        m_btnLocationUpdate->setEnabled(false);
        return;
    }

    m_staticLocationProvider->setLocation(loc);
    m_pidfPreview->setPlainText(r.xml);
    m_locationStatusLabel->setText(
        tr("[TEST/LAB] Valid — lat=%1 lon=%2 unc=%3m — PIDF-LO ready")
            .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6).arg(unc, 0, 'f', 1));
    m_locationStatusLabel->setStyleSheet(QStringLiteral("color: #50c878; font-size: 9px;"));
    m_manualLocationValid = true;

    const bool emergencyActive = (m_emergencyCallActive
        && m_emergencyController->state() == EmergencyCallState::Active);
    m_btnLocationUpdate->setEnabled(emergencyActive);

    Logger::instance().info(LogCategory::App,
        QStringLiteral("[TEST/LAB] Manual PIDF-LO generated: lat=%1 lon=%2 unc=%3m")
            .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6).arg(unc, 0, 'f', 1));
}

void CallWorkspacePanel::onLocationUpdateClicked()
{
    if (!m_emergencyCallActive || !m_manualLocationValid) {
        Logger::instance().warn(LogCategory::App,
            QStringLiteral("[EMERGENCY] Location update ignored: active=%1 locationValid=%2")
                .arg(m_emergencyCallActive).arg(m_manualLocationValid));
        return;
    }

    const QString target = AppSettings::emergencyTarget();
    const int ret = QMessageBox::warning(
        this, tr("Confirm Location Update — TEST/LAB Only"),
        tr("Send a SIP UPDATE with updated location to the active emergency test call?\n\n"
           "Target: %1\n\n"
           "THIS IS A TEST/LAB OPERATION ONLY.\n"
           "Manual coordinates (not GPS) will be sent in a new PIDF-LO body.").arg(target),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        Logger::instance().info(LogCategory::App, QStringLiteral("[EMERGENCY] Location update: user cancelled"));
        return;
    }

    const QString contentId = EmergencyCallAdapter::generateContentId();
    const SipCallOptions opts = EmergencyCallAdapter::toLocationUpdateOptions(
        m_staticLocationProvider->pidfLo(), contentId);

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("[EMERGENCY TEST/DEMO] sendEmergencyLocationUpdate: contentId=%1").arg(contentId));

    const bool ok = SipManager::instance().sendEmergencyLocationUpdate(opts);
    if (ok) {
        m_locationStatusLabel->setText(tr("[TEST/LAB] Location UPDATE sent — contentId=%1").arg(contentId));
        m_locationStatusLabel->setStyleSheet(QStringLiteral("color: #50c878; font-size: 9px;"));
    } else {
        m_locationStatusLabel->setText(tr("Location UPDATE failed — see log (stub or not Active)"));
        m_locationStatusLabel->setStyleSheet(QStringLiteral("color: #e05050; font-size: 9px;"));
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("[EMERGENCY] sendEmergencyLocationUpdate returned false"));
    }
}
