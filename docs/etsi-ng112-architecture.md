# ETSI / NG112 Architecture

**Task:** 34 — ETSI / NG112 Architecture Skeleton
**Date:** 2026-06-26
**Branch:** `feature/project-handoff-002`
**Status:** Skeleton only — no real emergency calls, no geolocation, no PSAP routing

---

## 1. Scope

This document describes the architecture of the ETSI NG112 / NG-eCall extension
to the Iryme SIP client. The extension is designed to be:

- **Additive** — zero modifications to the existing SIP, audio, video, or RTT paths.
- **Cleanly separated** — all NG112 code lives in `src/emergency/`, isolated from
  the softphone modules in `src/sip/`, `src/media/`, `src/rtt/`, `src/gui/`.
- **Incrementally deliverable** — each subsequent task (35–38) adds a single
  well-defined capability without rewriting what came before.

Standards in scope:
- **ETSI TS 103 479** — NG-eCall over IMS (data part)
- **ETSI TS 103 480** — NG112 end-to-end architecture
- **ETSI TS 103 698** — NG112 caller location
- **RFC 5031** — A Uniform Resource Name (URN) for Emergency and Other Well-Known Services
- **RFC 4119** — PIDF-LO (Presence Information Data Format Location Object)
- **RFC 7852** — Additional Data Related to an Emergency Call
- **RFC 6442** — Location Conveyance for the Session Initiation Protocol

---

## 2. Current Architecture (pre-Task 34)

```
MainWindow
├── SidebarPanel
├── CallPanel ──── SipManager ──── SipCall ──── CallStateMachine
│                  │               │             AudioMediaManager
│                  │               │             VideoMediaManager
│                  │               └─── RttSession ──── RttPanel
│                  └── SipAccount (pjsua2)
├── VideoPanel
├── RttPanel
├── MediaPanel
└── DiagnosticsPanel
```

Key invariants that must be preserved:
- `SipManager::makeCall(uri)` — initiates a standard outgoing call
- `SipCall` — manages one call slot; no emergency-specific fields
- `RttSession` — RFC 4103 T.140 only; no ETSI data multiplexing
- No geolocation, no URN routing, no PSAP-specific headers in any existing class

---

## 3. What We Reuse

| Existing component | Reuse in NG112 |
|---|---|
| `SipManager::makeCall()` | Task 36: will call this with a PSAP SIP URI after preparation |
| `SipCall` | No change — the emergency INVITE uses the same SIP call slot |
| `RttSession` / `RttPanel` | RFC 4103 is the baseline text medium for NG112 as well |
| `AudioMediaManager` | Audio stream unmodified |
| `CredentialStore` | PSAP credentials (if needed) stored via same mechanism |
| `SipProfile.emergencyServiceUri` | Already persisted in `SipProfile`; used as routing hint |

---

## 4. New Components (Task 34 skeleton)

All new code lives in `src/emergency/`. No existing file is modified.

### 4.1 EmergencyCallProfile

**File:** `src/emergency/EmergencyCallProfile.h` / `.cpp`

Pure data struct + validation. No PJSIP dependency.

| Field | Type | Description |
|---|---|---|
| `serviceUrn` | `QString` | RFC 5031 URN, e.g. `urn:service:sos` |
| `routingTarget` | `QString` | PSAP SIP URI (placeholder) |
| `callerDisplayName` | `QString` | From display name in INVITE |
| `pidfLo` | `QString` | PIDF-LO XML body (populated by location provider) |
| `additionalDataUris` | `QStringList` | RFC 7852 URIs (reserved) |

Validation rules:
- `serviceUrn` must be non-empty and start with `urn:service:`
- `routingTarget` must be non-empty

Factory: `EmergencyCallProfile::makeSos(routingTarget, displayName)`

---

### 4.2 EmergencyCallStateMachine

**File:** `src/emergency/EmergencyCallStateMachine.h` / `.cpp`

8-state machine. Independent of PJSIP.

```
Idle
 └─→ Preparing
      ├─→ LocationPending
      │    └─→ ReadyToDial
      │         └─→ Dialing        (Task 36)
      │              └─→ Active    (Task 36)
      │                   └─→ Ended
      ├─→ ReadyToDial (when location not available — skipped)
      └─→ Failed ──→ Idle (reset)
```

Signal: `stateChanged(newState, oldState, reason)`
Signal: `transitionRejected(requested, current)`

---

### 4.3 EmergencyLocationProvider

**File:** `src/emergency/EmergencyLocationProvider.h` / `.cpp`

Abstract interface. Two statuses in Task 34:

| Status | Meaning |
|---|---|
| `NotImplemented` | No provider wired — geolocation not available (Task 34 default) |
| `Unavailable` | Provider exists but location cannot be obtained |

**NullLocationProvider** — default implementation. Always `NotImplemented`.
`requestLocation()` emits `locationStatusChanged(NotImplemented)` immediately.

Future providers (Task 37):
- `GpsLocationProvider` — queries Windows Location API
- `ManualLocationProvider` — user-entered coordinates in settings dialog

---

### 4.4 EmergencyCallController

**File:** `src/emergency/EmergencyCallController.h` / `.cpp`

Orchestrates preparation. Owns neither SipManager nor location provider.

Responsibilities in Task 34:
1. Validate `EmergencyCallProfile` via `isValid()`
2. Drive `EmergencyCallStateMachine`
3. Request location from `EmergencyLocationProvider`
4. If location unavailable: skip to `ReadyToDial`
5. Emit `readyToDial(profile)` — consumer (Task 36) will call `SipManager::makeCall()`

What it does NOT do in Task 34:
- Does NOT call `SipManager::makeCall()`
- Does NOT inject SIP headers
- Does NOT add PIDF-LO to SDP or SIP body
- Does NOT interact with PJSIP in any way

---

## 5. Conceptual Emergency Call Flow

```
[Emergency Button]
       │
       ▼
EmergencyCallController::prepare()
       │
       ├── EmergencyCallProfile::isValid()  ──→ preparationFailed() if invalid
       │
       ├── SM: Idle → Preparing
       │
       ├── EmergencyLocationProvider::requestLocation()
       │       │
       │       ├── [Location available]  → SM: Preparing → LocationPending → ReadyToDial
       │       │                            profile.pidfLo populated
       │       │
       │       └── [Not available]       → SM: Preparing → ReadyToDial (no PIDF-LO)
       │
       ├── emit readyToDial(profile)
       │
       ▼  [Task 36 takes over here]
SipManager::makeCall(profile.routingTarget)
       │
       ├── SIP INVITE with:
       │     Route: urn:service:sos (geolocation header, RFC 6442)   [Task 35]
       │     Geolocation: <cid:...>                                   [Task 37]
       │     Content-Type: multipart/mixed                            [Task 38]
       │     Body part 1: application/pidf+xml (PIDF-LO)             [Task 37]
       │     Body part 2: application/EmergencyCallData.* (RFC 7852) [Task 38]
       │
       ▼
      PSAP
       │
       ▼
SipCall Active (audio + RTT) ──→ hangup ──→ EmergencyCallController cleanup
```

---

## 6. SIP Header Placeholders (Future Use)

The following structures are reserved for future injection into SIP INVITEs.
**None are injected in Task 34.**

### Service URN (RFC 5031)
```
Request-URI: sip:112@psap.example.com
```
Alternative routing via `Route: <urn:service:sos>` depends on NG112 proxy support.

### Geolocation Header (RFC 6442)
```
Geolocation: <cid:target123@atlanta.example.com>
Geolocation-Routing: yes
```

### PIDF-LO body (RFC 4119)
```xml
<?xml version="1.0"?>
<presence xmlns="urn:ietf:params:xml:ns:pidf"
          entity="pres:alice@example.com">
  <tuple id="sg89ae">
    <status><basic>open</basic></status>
  </tuple>
  <device id="pc122">
    <geopriv>
      <location-info>
        <Point xmlns="http://www.opengis.net/gml" srsName="urn:ogc:def:crs:EPSG::4326">
          <pos>47.6062 -122.3321</pos>
        </Point>
      </location-info>
      <usage-rules/>
    </geopriv>
  </device>
</presence>
```

### Additional Data (RFC 7852)
```
Call-Info: <https://example.com/callerdata/abc123>;purpose=EmergencyCallData.ProviderInfo
```

---

## 7. Integration Points with Existing Softphone

No existing file is modified in Task 34. Future integration points:

| Where | What | Task |
|---|---|---|
| `SipManager::makeCall()` | Called by controller after `readyToDial` | 36 |
| `SipCall` (PJSIP path) | Add custom SIP headers to INVITE | 35 |
| `SipAccount::onRegState()` | Emergency re-registration after network change | 35 |
| `SipProfile::emergencyServiceUri` | Already exists; routing target for NG112 | 36 |
| `RttPanel` / `RttSession` | Used as-is — NG112 requires RFC 4103 text | unchanged |

---

## 8. Build Integration

The `src/emergency/` module compiles independently:
- No PJSIP headers — no `HAVE_PJSIP` guard needed
- No Qt Widgets dependency — `Qt6::Core` only
- Tests use `QTEST_GUILESS_MAIN` — no display required

To add to the main `SIPClient` target, append to `CMakeLists.txt` (Task 36):
```cmake
target_sources(SIPClient PRIVATE
    src/emergency/EmergencyCallProfile.cpp
    src/emergency/EmergencyCallStateMachine.cpp
    src/emergency/EmergencyLocationProvider.cpp
    src/emergency/EmergencyCallController.cpp
)
```

Currently the module is only compiled by the 3 new CTest suites:
- `test_emergency_state_machine` (11 tests)
- `test_emergency_profile` (10 tests)
- `test_emergency_location_provider` (10 tests)

---

## 9. Emergency SIP INVITE Builder (Task 36)

**Status:** Complete — declarative builder implemented, no real INVITE sent.

### 9.1 What it produces

`EmergencyInviteBuilder` takes an `EmergencyCallProfile` and optional configuration
and returns an `EmergencyInvite` — a pure data struct with no PJSIP dependency.

| Field | Content |
|---|---|
| `requestUri` | PSAP SIP URI from `EmergencyCallProfile::routingTarget` |
| `routeTarget` | Same as `requestUri` (separate field reserved for proxy route) |
| `serviceUrn` | RFC 5031 URN from `EmergencyCallProfile::serviceUrn` |
| `headers` | Deterministic ordered list (Accept, Supported, Geolocation if available) |
| `contentType` | Empty placeholder — populated in Task 38 (PIDF-LO / multipart) |
| `body` | Empty placeholder — populated in Task 38 |
| `mediaPolicy` | `requireAudio=true`, `requireRtt=true`, `allowVideo=false` (defaults) |
| `hasLocation` | Set by `setLocationAvailable()` on the builder |
| `locationRequired` | Set by `setLocationRequired()` on the builder |

### 9.2 What it does NOT produce

- No real PJSIP INVITE is sent.
- No PIDF-LO XML is generated (Task 38).
- No multipart body is assembled (Task 38).
- No PSAP routing decision is made (Task 38 / proxy config).
- No `P-Preferred-Identity` or `P-Asserted-Identity` headers (Task 37).
- No network I/O of any kind.

### 9.3 Validation rules

`EmergencyInviteBuilder::validate(invite)` returns errors and warnings:

**Hard errors (isValid() = false):**
- `serviceUrn` empty or not starting with `urn:service:`
- `requestUri` (routing target) empty
- `mediaPolicy.requireAudio = false`
- `mediaPolicy.requireRtt = false`
- `locationRequired = true` and `hasLocation = false`

**Warnings (isValid() = true):**
- `mediaPolicy.allowVideo = false`
- `hasLocation = false` and `locationRequired = false`

### 9.4 Generated headers

| Header | Condition |
|---|---|
| `Accept: application/sdp` | Always |
| `Supported: geolocation` | Always (RFC 6442 support indicator) |
| `Geolocation: <placeholder-cid@ng112>` | Only when `hasLocation=true` |
| `Geolocation-Routing: yes` | Only when `hasLocation=true` |

Header order is deterministic — same input always produces the same list.

### 9.5 Future integration with SipCall/SipManager

In a future task, `EmergencyCallController` will:
1. Call `EmergencyInviteBuilder(profile).build()` after `readyToDial` fires.
2. Pass `EmergencyInvite::requestUri` to `SipManager::makeCall()`.
3. Inject `EmergencyInvite::headers` into the PJSIP INVITE via `pjsua2::CallOpParam`.

No existing `SipCall`, `SipManager`, or `SipAccount` code needs to change until that
integration task.

### 9.6 What remains for subsequent tasks

| Item | Task |
|---|---|
| PIDF-LO XML generation (`EmergencyCallProfile::pidfLo` populated) | Task 37 |
| `GpsLocationProvider` / `ManualLocationProvider` | Task 37 |
| Multipart INVITE body builder (`multipart/mixed`) | Task 38 |
| PJSIP header injection into real INVITE | Task 38 |
| `EmergencyCallController` → `SipManager::makeCall()` wiring | Task 38 |
| GUI Emergency button | Task 38 |
| PSAP routing via outbound proxy or `Route:` header | Task 38 |

---

## 10. Roadmap

### Task 37 — Location Provider
- `GpsLocationProvider` — Windows Location API (COM)
- `ManualLocationProvider` — PIDF-LO from user-entered coordinates
- PIDF-LO XML generation (`EmergencyLocationProvider::pidfLo()`)
- Location injected into `EmergencyCallProfile.pidfLo` before `readyToDial`

### Task 38 — Multipart INVITE Body + Real Emergency Call (ETSI TS 103 479 / RFC 7852)
- `EmergencyCallController::readyToDial` connected to `SipManager::makeCall()`
- `multipart/mixed` body builder (PIDF-LO + RFC 7852 additional data)
- PJSIP header injection via `pjsua2::CallOpParam`
- GUI: Emergency button in `CallPanel`
- Validation against ETSI TS 103 479 conformance checklist
