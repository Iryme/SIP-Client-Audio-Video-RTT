#pragma once
#include <QString>
#include <QStringList>

#include "msrp/MsrpPath.h"
#include "msrp/MsrpTypes.h"

// Splices a real "m=message" media section into a live, already-created
// pjmedia_sdp_session via the public pjmedia_sdp.h API (Task W101, Phase 2).
//
// Why this is safe and needs no vendored pjproject edit: pjsua2's
// Endpoint::on_call_sdp_created (.deps/pjproject/pjsip/src/pjsua2/endpoint.cpp)
// only re-parses OnCallSdpCreatedParam::sdp.wholeSdp back into the live SDP
// if that *text* changed after the callback returns; it never touches
// sdp.pjSdpSession itself. sdp.pjSdpSession already points at the live
// pjmedia_sdp_session that pjsua_media_channel_create_sdp() built and that
// flows straight through to the outgoing INVITE/answer. Appending to
// media[media_count++] here is the exact pattern pjsua_media.c itself uses
// internally (see e.g. its SRTP crypto-duplication and "disabled media
// clone" code paths) — so this reaches the wire without ever going through
// the 1024-byte wholeSdp text buffer (PJSUA2_MAX_SDP_BUF_LEN) and without
// editing any file under .deps. See docs/msrp-live-sdp-integration.md.
namespace MsrpSipMediaInjector {

struct InjectResult
{
    bool injected{false};
    QString errorMessage;
};

// pjSdpSession must point to a live pjmedia_sdp_session (as exposed by
// pjsua2::OnCallSdpCreatedParam::sdp.pjSdpSession, for both offer and
// answer paths). pjPool must point to a pj_pool_t whose lifetime outlives
// the SIP transaction carrying this SDP — the caller (SipCall) owns one
// pool per call, released on call teardown; see SipCall::Impl.
//
// port==0 encodes a rejected/disabled m=message section (RFC 3264 hold/
// reject semantics: still a valid, well-formed section, but with no
// negotiation attributes) — used when the local MSRP listener could not be
// started, so an invalid offer is never sent.
InjectResult injectMessageMedia(void *pjSdpSession, void *pjPool,
                                 const MsrpUri &localUri, MsrpSetup setup,
                                 const QStringList &acceptTypes,
                                 const QStringList &acceptWrappedTypes,
                                 int port);

// Task W102 Phase 2 (peer-initiated offer/answer): replaces the media
// section already at `index` in a live pjmedia_sdp_session with a real,
// accepted "m=message" answer. Unlike injectMessageMedia (which appends —
// correct only for the offerer path), an SDP *answer* must keep the same
// media count and ordering as the offer it responds to (RFC 3264 §6); by
// the time onCallSdpCreated fires for the answer, PJSIP's own
// pjmedia_sdp_neg has already cloned the offer's unmatched "message"
// section into the answer as a rejected (port 0) placeholder at the
// correct index (see pjmedia_sdp_neg.c's create_answer/media_used loop —
// it never finds a local capability for a media type PJSUA's own media
// manager doesn't know about, so it clones-and-deactivates). This
// function overwrites that placeholder with our real accepted section.
// Fails (does not modify the session) if `index` is out of range or the
// media type at that index is not "message" — a defensive check against
// ever answering into the wrong slot.
InjectResult answerMessageMediaAtIndex(void *pjSdpSession, void *pjPool, int index,
                                        const MsrpUri &localUri, MsrpSetup setup,
                                        const QStringList &acceptTypes,
                                        const QStringList &acceptWrappedTypes,
                                        int port);

// Reads the connection ("c=") address PJSIP already resolved for this SDP's
// audio/video media (session-level first, else the first media section that
// has one) — used as the advertised MSRP host when msrp/advertisedHost is
// left unset in config, so the client never has to hardcode a host/IP: it
// reuses the same real local/NAT address PJSIP already negotiated for this
// call. Returns an empty string if no connection info is present yet.
QString resolveSessionConnectionHost(void *pjSdpSession);

} // namespace MsrpSipMediaInjector
