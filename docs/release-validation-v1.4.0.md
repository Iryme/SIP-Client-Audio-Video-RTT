# Release Validation — v1.4.0

Manual validation checklist for the v1.4.0 Windows release.

- **Branch:** `release/v1.4.0`
- **Commit:** `3713f93` — Use structured codec information in diagnostics
- **Package:** `dist/SIP-Client-Audio-Video-RTT-v1.4.0-windows.zip`
- **Date:** 2026-07-05

## Automated validation (completed)

- [x] Release build (`build-release`, NMake, Release PJSIP) — PASS
- [x] CTest Release — 32/32 PASS
- [x] Windows package regenerated with windeployqt (Qt 6.11.1 MSVC)
- [x] Smoke test — app starts from dist, no missing DLLs, main window created,
      event loop entered, `SIP backend initialized (PJSIP/pjsua2)`, clean shutdown

## Manual validation checklist

### Audio

- [ ] Register
- [ ] Outgoing call
- [ ] Incoming call
- [ ] Two-way audio
- [ ] Hold
- [ ] Resume

### Video

- [ ] Video request
- [ ] Accept video
- [ ] Reject video
- [ ] Camera Off
- [ ] Camera On

### RTT

- [ ] RTT request
- [ ] RTT session
- [ ] RTT messages

### Diagnostics

- [ ] Timeline
- [ ] Bundle ZIP
- [ ] Codec information
- [ ] RTP statistics

### History

- [ ] Call History
- [ ] Search
- [ ] Filters
- [ ] Export

### Media

- [ ] Device selection
- [ ] Volume
- [ ] Test speaker
- [ ] Test microphone
