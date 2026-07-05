#pragma once
#ifdef HAVE_PJSIP

// Captures full raw SIP messages (request-line/status-line + headers + body,
// including SDP) via a pjsip_module hooked into the endpoint's tx/rx chain,
// and forwards each one to SipTraceLogger (redacted, on the Qt main thread).
// Without this, SipManager only ever logs synthetic per-action summaries
// (method/from/to/callId) with no raw SIP text — the SIP Ladder detail view
// has nothing real to show.
namespace PjsipTraceModule {

// Registers the capture module on the given (already pjsua_get_pjsip_endpt())
// endpoint. Safe to call once after libInit()/libStart(). No-op if already
// installed.
void install();

// Unregisters the capture module. Safe to call even if never installed.
void uninstall();

} // namespace PjsipTraceModule

#endif
