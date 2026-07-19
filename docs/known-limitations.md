# Known Limitations

## Video interoperability

The macOS PJSIP 2.17 build provides native AVFoundation capture and Metal
rendering but has no VPX, OpenH264, or FFmpeg video codec. Local Qt preview is
available; a PJSIP video call still requires a codec shared with the peer.
W113I prevents absent/inactive video media from crashing Camera On/Off, but it
does not add or simulate video and does not certify live video interoperability.

## Release validation

Developer ID signing, notarization, a second clean Apple Silicon machine, and
Windows/live cross-platform regression require external infrastructure. An
ad-hoc signed engineering bundle is not a notarized production release.

LMPE remains permanently unavailable. Client Messaging displays user content,
not raw CPIM, IMDN, is-composing, or other protocol XML.
