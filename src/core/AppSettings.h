#pragma once
#include <QSettings>
#include <QString>

// Thin wrapper for consistent settings keys across the application.
// Use QSettings directly for simple cases; extend this class for
// typed accessors as features are implemented.
class AppSettings
{
public:
    // Task W109A — lets two instances of this application on the same host
    // (e.g. an "Alice" and "Bob" test instance) use separate settings files
    // (separate SIP profiles, RTP port range, etc.) instead of silently
    // sharing the single per-Windows-user UserScope ini. Must be called
    // before the first call to settings() (i.e. at the very start of main())
    // to have any effect — settings() is a function-local static and only
    // constructs its QSettings object once. See --config-dir / the
    // SIPCLIENT_CONFIG_DIR environment variable in main.cpp.
    static void setConfigDirectoryOverride(const QString &dir)
    {
        configDirectoryOverride() = dir;
    }

    static QSettings &settings()
    {
        const QString &dir = configDirectoryOverride();
        if (!dir.isEmpty()) {
            static QSettings s_settingsOverride(
                dir + QStringLiteral("/SIPClient.ini"), QSettings::IniFormat);
            return s_settingsOverride;
        }
        static QSettings s_settings(
            QSettings::IniFormat,
            QSettings::UserScope,
            "SIPClient", "SIPClient");
        return s_settings;
    }

    // Layout
    static void saveWindowGeometry(const QByteArray &geom)  { settings().setValue("ui/geometry", geom); }
    static QByteArray loadWindowGeometry()                   { return settings().value("ui/geometry").toByteArray(); }
    static void saveWindowState(const QByteArray &state)     { settings().setValue("ui/state", state); }
    static QByteArray loadWindowState()                      { return settings().value("ui/state").toByteArray(); }
    static void saveSplitterState(const QString &key, const QByteArray &s) { settings().setValue("ui/splitter/" + key, s); }
    static QByteArray loadSplitterState(const QString &key)  { return settings().value("ui/splitter/" + key).toByteArray(); }

    // Logging
    static void setLogLevelEnabled(const QString &level, bool on) { settings().setValue("log/level/" + level, on); }
    static bool isLogLevelEnabled(const QString &level, bool def) { return settings().value("log/level/" + level, def).toBool(); }

    // Media device selection
    static void    saveSelectedMicrophone(const QString &id) { settings().setValue("media/device/microphone", id); }
    static QString loadSelectedMicrophone()                  { return settings().value("media/device/microphone").toString(); }
    static void    saveSelectedSpeaker   (const QString &id) { settings().setValue("media/device/speaker", id); }
    static QString loadSelectedSpeaker   ()                  { return settings().value("media/device/speaker").toString(); }
    static void    saveSelectedCamera    (const QString &id) { settings().setValue("media/device/camera", id); }
    static QString loadSelectedCamera    ()                  { return settings().value("media/device/camera").toString(); }

    // Media volume — 0-100, 100 = default/unity gain. Applied to the active
    // call's capture/playback device media (PJSIP AudDevManager) and used as
    // the default for future calls.
    static void saveMicrophoneVolume(int percent) { settings().setValue("media/volume/microphone", percent); }
    static int  loadMicrophoneVolume()             { return settings().value("media/volume/microphone", 100).toInt(); }
    static void saveSpeakerVolume   (int percent) { settings().setValue("media/volume/speaker", percent); }
    static int  loadSpeakerVolume   ()             { return settings().value("media/volume/speaker", 100).toInt(); }

    // Call type selector — persisted as int matching CallType enum
    // (0 = AudioOnly, the default when unset).
    static int  loadLastCallType()        { return settings().value(QStringLiteral("call/lastType"), 0).toInt(); }
    static void saveLastCallType(int type) { settings().setValue(QStringLiteral("call/lastType"), type); }

    // Theme (persisted as int matching AppTheme enum; 0 = Auto)
    static int  savedThemeIndex()        { return settings().value(QStringLiteral("ui/theme"), 0).toInt(); }
    static void saveThemeIndex(int idx)  { settings().setValue(QStringLiteral("ui/theme"), idx); settings().sync(); }

    // Emergency test mode — disabled by default; must be explicitly enabled in the INI file.
    // When false the 112 emergency button is hidden and no emergency call can be initiated.
    // To enable: set emergency/testMode=true in SIPClient.ini (user scope).
    static bool emergencyTestModeEnabled()
    {
        return settings().value("emergency/testMode", false).toBool();
    }
    static void setEmergencyTestModeEnabled(bool on)
    {
        settings().setValue("emergency/testMode", on);
    }

    // Messaging Diagnostics — max MessagingEvent rows retained in memory
    // (oldest evicted first once the limit is exceeded). Diagnostic-only
    // setting; has no effect on SIP/MSRP transport behavior.
    static int  loadMaxMessagingEventsRetained()        { return settings().value(QStringLiteral("messaging/maxEventsRetained"), 1000).toInt(); }
    static void saveMaxMessagingEventsRetained(int max) { settings().setValue(QStringLiteral("messaging/maxEventsRetained"), max); }

    // SIP MESSAGE Foundation (Task W092) — conservative defaults: sending is
    // off until explicitly enabled from the UI, CPIM wrapping is off, and
    // IMDN is not requested by default. None of these affect MSRP, which
    // remains permanently disabled regardless of these settings.
    static bool enableSipMessage()            { return settings().value(QStringLiteral("messaging/enableSipMessage"), false).toBool(); }
    static void setEnableSipMessage(bool on)  { settings().setValue(QStringLiteral("messaging/enableSipMessage"), on); }
    static bool enableCpim()                  { return settings().value(QStringLiteral("messaging/enableCpim"), false).toBool(); }
    static void setEnableCpim(bool on)        { settings().setValue(QStringLiteral("messaging/enableCpim"), on); }
    static bool requestImdnByDefault()        { return settings().value(QStringLiteral("messaging/requestImdnByDefault"), false).toBool(); }
    static void setRequestImdnByDefault(bool on) { settings().setValue(QStringLiteral("messaging/requestImdnByDefault"), on); }

    // IMDN Foundation (Task W096). Auto Send Delivered defaults ON (a
    // "delivered" report is a transport-level acknowledgement with no
    // privacy implication — it does not disclose whether/when the user
    // actually read the message). Auto Send Displayed defaults OFF (it
    // discloses that the user has read the message, so it requires explicit
    // opt-in; when off the user marks messages as read manually). Neither
    // setting affects MSRP, which remains permanently disabled.
    static bool autoSendDeliveredImdn()          { return settings().value(QStringLiteral("messaging/autoSendDeliveredImdn"), true).toBool(); }
    static void setAutoSendDeliveredImdn(bool on) { settings().setValue(QStringLiteral("messaging/autoSendDeliveredImdn"), on); }
    static bool autoSendDisplayedImdn()          { return settings().value(QStringLiteral("messaging/autoSendDisplayedImdn"), false).toBool(); }
    static void setAutoSendDisplayedImdn(bool on) { settings().setValue(QStringLiteral("messaging/autoSendDisplayedImdn"), on); }

    // Active is-composing (Task W097) — both default ON per the task spec;
    // is-composing carries no message content, only a typing-state
    // indication, so unlike Displayed-IMDN there is no privacy reason to
    // default it off. Never affects MSRP.
    static bool enableIsComposing()          { return settings().value(QStringLiteral("messaging/enableIsComposing"), true).toBool(); }
    static void setEnableIsComposing(bool on) { settings().setValue(QStringLiteral("messaging/enableIsComposing"), on); }
    static bool autoTypingNotifications()          { return settings().value(QStringLiteral("messaging/autoTypingNotifications"), true).toBool(); }
    static void setAutoTypingNotifications(bool on) { settings().setValue(QStringLiteral("messaging/autoTypingNotifications"), on); }

    // Typing state-machine timers (Task W097), seconds. refresh: how often
    // "active" is re-sent while the user keeps typing (RFC 3994 keep-alive,
    // and the throttle that stops every keystroke from sending a
    // notification). idle: seconds of no typing before "idle" is sent.
    // goneDelay: seconds after "idle" before "gone" is sent if typing never
    // resumes.
    static int  typingRefreshSeconds()        { return settings().value(QStringLiteral("messaging/typingRefreshSeconds"), 60).toInt(); }
    static void setTypingRefreshSeconds(int s) { settings().setValue(QStringLiteral("messaging/typingRefreshSeconds"), s); }
    static int  typingIdleSeconds()           { return settings().value(QStringLiteral("messaging/typingIdleSeconds"), 15).toInt(); }
    static void setTypingIdleSeconds(int s)    { settings().setValue(QStringLiteral("messaging/typingIdleSeconds"), s); }
    static int  typingGoneDelaySeconds()      { return settings().value(QStringLiteral("messaging/typingGoneDelaySeconds"), 30).toInt(); }
    static void setTypingGoneDelaySeconds(int s) { settings().setValue(QStringLiteral("messaging/typingGoneDelaySeconds"), s); }

    // SIP Presence Foundation (Task W098) — conservative defaults: presence,
    // subscribe, and publish are all off until explicitly enabled from the
    // UI, and no target URI is ever assumed. Auto-resubscribe only matters
    // once Presence + Subscribe are both enabled; it never overrides the
    // "no retry for rejected/noresource" rule (see
    // PresenceResubscribePolicy::shouldAutoRetry). Never touches MSRP.
    static bool enablePresence()               { return settings().value(QStringLiteral("presence/enablePresence"), false).toBool(); }
    static void setEnablePresence(bool on)     { settings().setValue(QStringLiteral("presence/enablePresence"), on); }
    static bool enablePresenceSubscribe()      { return settings().value(QStringLiteral("presence/enableSubscribe"), false).toBool(); }
    static void setEnablePresenceSubscribe(bool on) { settings().setValue(QStringLiteral("presence/enableSubscribe"), on); }
    // Publish is marked experimental — see docs/presence.md for why (relies
    // on AccountConfig.presConfig.publishEnabled, which pjsua2 only applies
    // at account creation time, so toggling this requires re-registration).
    static bool enablePresencePublish()        { return settings().value(QStringLiteral("presence/enablePublish"), false).toBool(); }
    static void setEnablePresencePublish(bool on) { settings().setValue(QStringLiteral("presence/enablePublish"), on); }
    static int  presenceDefaultExpiresSeconds()        { return settings().value(QStringLiteral("presence/defaultExpiresSeconds"), 300).toInt(); }
    static void setPresenceDefaultExpiresSeconds(int s) { settings().setValue(QStringLiteral("presence/defaultExpiresSeconds"), s); }
    static bool presenceAutoResubscribe()      { return settings().value(QStringLiteral("presence/autoResubscribe"), true).toBool(); }
    static void setPresenceAutoResubscribe(bool on) { settings().setValue(QStringLiteral("presence/autoResubscribe"), on); }
    static int  presenceMaxRetainedEvents()        { return settings().value(QStringLiteral("presence/maxRetainedEvents"), 500).toInt(); }
    static void setPresenceMaxRetainedEvents(int max) { settings().setValue(QStringLiteral("presence/maxRetainedEvents"), max); }
    // Default own-status selection for the (experimental) Publish control —
    // one of available/away/busy/do-not-disturb/offline.
    static QString presenceDefaultState()        { return settings().value(QStringLiteral("presence/defaultState"), QStringLiteral("available")).toString(); }
    static void setPresenceDefaultState(const QString &state) { settings().setValue(QStringLiteral("presence/defaultState"), state); }

    // XCAP Foundation (Task W099) — disabled by default, no root URI ever
    // assumed. The XCAP password is never stored here; it goes through
    // CredentialStore (same secure-backend mechanism as SIP profile
    // passwords), keyed by the fixed pseudo-profile id "xcap" + xcapUsername.
    static bool enableXcap()               { return settings().value(QStringLiteral("xcap/enable"), false).toBool(); }
    static void setEnableXcap(bool on)     { settings().setValue(QStringLiteral("xcap/enable"), on); }
    static QString xcapRoot()              { return settings().value(QStringLiteral("xcap/root"), QString()).toString(); }
    static void setXcapRoot(const QString &uri) { settings().setValue(QStringLiteral("xcap/root"), uri); }
    static QString xcapXui()               { return settings().value(QStringLiteral("xcap/xui"), QString()).toString(); }
    static void setXcapXui(const QString &xui) { settings().setValue(QStringLiteral("xcap/xui"), xui); }
    static QString xcapUsername()          { return settings().value(QStringLiteral("xcap/username"), QString()).toString(); }
    static void setXcapUsername(const QString &user) { settings().setValue(QStringLiteral("xcap/username"), user); }
    // One of "none" / "basic" / "digest" (see xcapAuthModeFromString/ToString in XcapModels.h).
    static QString xcapAuthentication()    { return settings().value(QStringLiteral("xcap/authentication"), QStringLiteral("none")).toString(); }
    static void setXcapAuthentication(const QString &mode) { settings().setValue(QStringLiteral("xcap/authentication"), mode); }
    static bool validateXmlBeforePut()     { return settings().value(QStringLiteral("xcap/validateXmlBeforePut"), true).toBool(); }
    static void setValidateXmlBeforePut(bool on) { settings().setValue(QStringLiteral("xcap/validateXmlBeforePut"), on); }
    static int  xcapTimeout()              { return settings().value(QStringLiteral("xcap/timeoutSeconds"), 15).toInt(); }
    static void setXcapTimeout(int seconds) { settings().setValue(QStringLiteral("xcap/timeoutSeconds"), seconds); }
    static bool xcapVerifyTls()            { return settings().value(QStringLiteral("xcap/verifyTls"), true).toBool(); }
    static void setXcapVerifyTls(bool on)  { settings().setValue(QStringLiteral("xcap/verifyTls"), on); }

    // MSRP Foundation (Task W100) — disabled by default; TCP/TLS only
    // become usable once enableMsrp + the specific transport flag are both
    // on. Fallback to SIP MESSAGE defaults ON so messaging never silently
    // stops working while MSRP is being trialled. No bind/advertised host
    // is ever assumed. TLS peer verification defaults ON.
    static bool enableMsrp()               { return settings().value(QStringLiteral("msrp/enable"), false).toBool(); }
    static void setEnableMsrp(bool on)     { settings().setValue(QStringLiteral("msrp/enable"), on); }
    static bool enableMsrpTcp()            { return settings().value(QStringLiteral("msrp/enableTcp"), false).toBool(); }
    static void setEnableMsrpTcp(bool on)  { settings().setValue(QStringLiteral("msrp/enableTcp"), on); }
    static bool enableMsrpTls()            { return settings().value(QStringLiteral("msrp/enableTls"), false).toBool(); }
    static void setEnableMsrpTls(bool on)  { settings().setValue(QStringLiteral("msrp/enableTls"), on); }
    static bool preferMsrp()               { return settings().value(QStringLiteral("msrp/prefer"), false).toBool(); }
    static void setPreferMsrp(bool on)     { settings().setValue(QStringLiteral("msrp/prefer"), on); }
    static bool allowSipMessageFallback()  { return settings().value(QStringLiteral("msrp/allowSipMessageFallback"), true).toBool(); }
    static void setAllowSipMessageFallback(bool on) { settings().setValue(QStringLiteral("msrp/allowSipMessageFallback"), on); }
    // messagingTransportMode: one of sip-message-only / msrp-preferred / msrp-required / automatic.
    static QString messagingTransportMode()        { return settings().value(QStringLiteral("msrp/transportMode"), QStringLiteral("automatic")).toString(); }
    static void setMessagingTransportMode(const QString &mode) { settings().setValue(QStringLiteral("msrp/transportMode"), mode); }

    static QString msrpLocalBindAddress()  { return settings().value(QStringLiteral("msrp/localBindAddress"), QString()).toString(); }
    static void setMsrpLocalBindAddress(const QString &addr) { settings().setValue(QStringLiteral("msrp/localBindAddress"), addr); }
    static QString msrpAdvertisedHost()    { return settings().value(QStringLiteral("msrp/advertisedHost"), QString()).toString(); }
    static void setMsrpAdvertisedHost(const QString &host) { settings().setValue(QStringLiteral("msrp/advertisedHost"), host); }
    // msrpPortMode: "automatic" (OS-assigned ephemeral port) or "fixed" (msrpFixedPort).
    static QString msrpPortMode()          { return settings().value(QStringLiteral("msrp/portMode"), QStringLiteral("automatic")).toString(); }
    static void setMsrpPortMode(const QString &mode) { settings().setValue(QStringLiteral("msrp/portMode"), mode); }
    static int  msrpFixedPort()            { return settings().value(QStringLiteral("msrp/fixedPort"), 0).toInt(); }
    static void setMsrpFixedPort(int port) { settings().setValue(QStringLiteral("msrp/fixedPort"), port); }

    static int  msrpConnectionTimeoutMs()  { return settings().value(QStringLiteral("msrp/connectionTimeoutMs"), 10000).toInt(); }
    static void setMsrpConnectionTimeoutMs(int ms) { settings().setValue(QStringLiteral("msrp/connectionTimeoutMs"), ms); }
    static int  msrpTransactionTimeoutMs() { return settings().value(QStringLiteral("msrp/transactionTimeoutMs"), 30000).toInt(); }
    static void setMsrpTransactionTimeoutMs(int ms) { settings().setValue(QStringLiteral("msrp/transactionTimeoutMs"), ms); }
    static int  msrpIdleTimeoutSeconds()   { return settings().value(QStringLiteral("msrp/idleTimeoutSeconds"), 300).toInt(); }
    static void setMsrpIdleTimeoutSeconds(int s) { settings().setValue(QStringLiteral("msrp/idleTimeoutSeconds"), s); }

    static int  msrpMaxFrameBytes()        { return settings().value(QStringLiteral("msrp/maxFrameBytes"), 16384).toInt(); }
    static void setMsrpMaxFrameBytes(int n) { settings().setValue(QStringLiteral("msrp/maxFrameBytes"), n); }
    static int  msrpMaxMessageBytes()      { return settings().value(QStringLiteral("msrp/maxMessageBytes"), 2097152).toInt(); }
    static void setMsrpMaxMessageBytes(int n) { settings().setValue(QStringLiteral("msrp/maxMessageBytes"), n); }
    static int  msrpChunkSizeBytes()       { return settings().value(QStringLiteral("msrp/chunkSizeBytes"), 2048).toInt(); }
    static void setMsrpChunkSizeBytes(int n) { settings().setValue(QStringLiteral("msrp/chunkSizeBytes"), n); }
    static int  msrpMaxConcurrentSessions(){ return settings().value(QStringLiteral("msrp/maxConcurrentSessions"), 4).toInt(); }
    static void setMsrpMaxConcurrentSessions(int n) { settings().setValue(QStringLiteral("msrp/maxConcurrentSessions"), n); }

    static bool msrpRequestReports()       { return settings().value(QStringLiteral("msrp/requestReports"), true).toBool(); }
    static void setMsrpRequestReports(bool on) { settings().setValue(QStringLiteral("msrp/requestReports"), on); }
    static QString msrpAcceptTypes()       { return settings().value(QStringLiteral("msrp/acceptTypes"), QStringLiteral("text/plain message/cpim")).toString(); }
    static void setMsrpAcceptTypes(const QString &types) { settings().setValue(QStringLiteral("msrp/acceptTypes"), types); }
    static QString msrpAcceptWrappedTypes(){ return settings().value(QStringLiteral("msrp/acceptWrappedTypes"), QStringLiteral("text/plain text/html message/imdn+xml application/im-iscomposing+xml")).toString(); }
    static void setMsrpAcceptWrappedTypes(const QString &types) { settings().setValue(QStringLiteral("msrp/acceptWrappedTypes"), types); }

    static bool msrpTlsVerifyPeer()        { return settings().value(QStringLiteral("msrp/tlsVerifyPeer"), true).toBool(); }
    static void setMsrpTlsVerifyPeer(bool on) { settings().setValue(QStringLiteral("msrp/tlsVerifyPeer"), on); }
    static QString msrpTlsCaPath()         { return settings().value(QStringLiteral("msrp/tlsCaPath"), QString()).toString(); }
    static void setMsrpTlsCaPath(const QString &path) { settings().setValue(QStringLiteral("msrp/tlsCaPath"), path); }

    // Marks experimental sub-features (e.g. the raw-frame diagnostic test
    // tool) — disabled by default, independent of enableMsrp itself.
    static bool msrpExperimental()         { return settings().value(QStringLiteral("msrp/experimental"), false).toBool(); }
    static void setMsrpExperimental(bool on) { settings().setValue(QStringLiteral("msrp/experimental"), on); }

    // RFC 4976 MSRP relay call-integration config (Task W108). Mode is one
    // of "disabled" / "automatic" / "required" and defaults to "disabled" —
    // relay allocation is never attempted for any real call unless a user or
    // config explicitly turns this on. No relay host/port/credential ever
    // has a non-empty compiled-in default; see MsrpCallPreparationController.
    static QString msrpRelayMode()         { return settings().value(QStringLiteral("msrp/relay/mode"), QStringLiteral("disabled")).toString(); }
    static void setMsrpRelayMode(const QString &mode) { settings().setValue(QStringLiteral("msrp/relay/mode"), mode); }
    static QString msrpRelayHost()         { return settings().value(QStringLiteral("msrp/relay/host"), QString()).toString(); }
    static void setMsrpRelayHost(const QString &host) { settings().setValue(QStringLiteral("msrp/relay/host"), host); }
    static int  msrpRelayPort()            { return settings().value(QStringLiteral("msrp/relay/port"), 0).toInt(); }
    static void setMsrpRelayPort(int port) { settings().setValue(QStringLiteral("msrp/relay/port"), port); }
    static bool msrpRelayUseTls()          { return settings().value(QStringLiteral("msrp/relay/useTls"), true).toBool(); }
    static void setMsrpRelayUseTls(bool on) { settings().setValue(QStringLiteral("msrp/relay/useTls"), on); }
    static QString msrpRelayUsername()     { return settings().value(QStringLiteral("msrp/relay/username"), QString()).toString(); }
    static void setMsrpRelayUsername(const QString &user) { settings().setValue(QStringLiteral("msrp/relay/username"), user); }
    // Identifies which CredentialStore entry supplies the relay password;
    // the password itself is never stored here (see CredentialStore).
    static QString msrpRelayCredentialProfileId() { return settings().value(QStringLiteral("msrp/relay/credentialProfileId"), QString()).toString(); }
    static void setMsrpRelayCredentialProfileId(const QString &id) { settings().setValue(QStringLiteral("msrp/relay/credentialProfileId"), id); }
    static bool msrpRelayTlsVerifyPeer()   { return settings().value(QStringLiteral("msrp/relay/tlsVerifyPeer"), true).toBool(); }
    static void setMsrpRelayTlsVerifyPeer(bool on) { settings().setValue(QStringLiteral("msrp/relay/tlsVerifyPeer"), on); }
    static QString msrpRelayTlsCaPath()    { return settings().value(QStringLiteral("msrp/relay/tlsCaPath"), QString()).toString(); }
    static void setMsrpRelayTlsCaPath(const QString &path) { settings().setValue(QStringLiteral("msrp/relay/tlsCaPath"), path); }
    static int  msrpRelayConnectTimeoutMs(){ return settings().value(QStringLiteral("msrp/relay/connectTimeoutMs"), 8000).toInt(); }
    static void setMsrpRelayConnectTimeoutMs(int ms) { settings().setValue(QStringLiteral("msrp/relay/connectTimeoutMs"), ms); }
    static int  msrpRelayAuthTimeoutMs()   { return settings().value(QStringLiteral("msrp/relay/authTimeoutMs"), 8000).toInt(); }
    static void setMsrpRelayAuthTimeoutMs(int ms) { settings().setValue(QStringLiteral("msrp/relay/authTimeoutMs"), ms); }
    static int  msrpRelayRefreshMarginSeconds() { return settings().value(QStringLiteral("msrp/relay/refreshMarginSeconds"), 30).toInt(); }
    static void setMsrpRelayRefreshMarginSeconds(int s) { settings().setValue(QStringLiteral("msrp/relay/refreshMarginSeconds"), s); }
    static int  msrpRelayMaxRetries()      { return settings().value(QStringLiteral("msrp/relay/maxRetries"), 2).toInt(); }
    static void setMsrpRelayMaxRetries(int n) { settings().setValue(QStringLiteral("msrp/relay/maxRetries"), n); }
    // Overall call-preparation timeout (Phase 5): how long makeCall() will
    // wait for relay allocation before applying fallback policy.
    static int  msrpRelayPreparationTimeoutMs() { return settings().value(QStringLiteral("msrp/relay/preparationTimeoutMs"), 12000).toInt(); }
    static void setMsrpRelayPreparationTimeoutMs(int ms) { settings().setValue(QStringLiteral("msrp/relay/preparationTimeoutMs"), ms); }

    // Lab PSAP target URI — used only in emergency test mode, never a real PSAP.
    static QString emergencyTarget()
    {
        return settings().value("emergency/target",
                                QStringLiteral("sip:psap@10.2.0.180")).toString();
    }
    static void setEmergencyTarget(const QString &uri)
    {
        settings().setValue("emergency/target", uri);
    }

    // RTP/RTCP media port range (Task W109A) — applied to
    // pj::AccountConfig::mediaConfig::transportConfig at account creation
    // (see SipAccount::startRegistration() and RtpPortRangeConfig.h).
    // Defaults preserve the same starting port pjsua2 would otherwise pick
    // (4000) but bound it, which is required so two instances on the same
    // host can be given distinct, non-overlapping ranges — see
    // docs/rtp-port-range-configuration.md and
    // docs/multiple-instances-same-host.md.
    static int  rtpPortRangeStart()        { return settings().value(QStringLiteral("media/rtpPortStart"), 4000).toInt(); }
    static void setRtpPortRangeStart(int p) { settings().setValue(QStringLiteral("media/rtpPortStart"), p); }
    static int  rtpPortRangeEnd()          { return settings().value(QStringLiteral("media/rtpPortEnd"), 4998).toInt(); }
    static void setRtpPortRangeEnd(int p)   { settings().setValue(QStringLiteral("media/rtpPortEnd"), p); }

private:
    static QString &configDirectoryOverride()
    {
        static QString s_dir;
        return s_dir;
    }
};
