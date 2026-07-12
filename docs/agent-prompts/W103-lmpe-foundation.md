# Task-W103 — LMPE Foundation (agent prompt, condensed)

## Repository / Branches

`SIP-Client-Audio-Video-RTT`. Branch chain (must be executed strictly in
order): `feature/w102-msrp-live-interoperability` →
**`feature/w103-lmpe-foundation`** → `feature/w104-msrp-file-transfer` →
`feature/w105-advanced-msrp-hardening`. No merge into main/release/other
integration branches without explicit request.

## Starting condition (mandatory gate)

Do not begin LMPE implementation until: the SIP-Server-RTT repository has
been inspected; the real LMPE format has been identified; the authoritative
documentation or source has been identified; the content type has been
confirmed; the framing has been confirmed; the version and required fields
have been confirmed.

If the format cannot be confirmed: do not invent the protocol; stop codec
implementation; document the result; report W103 as BLOCKED; only neutral
interfaces or explicitly-marked fixtures may be implemented, without
claiming compatibility.

## What was actually done

1. Searched for a `SIP-Server-RTT` repository: not present as a git remote,
   not present as a sibling directory anywhere under `f:\Project\Iryme`,
   and `https://github.com/Iryme/SIP-Server-RTT` returns HTTP 404 (not
   found / not accessible) on the configured GitHub account.
2. Searched the entire local workspace (this repo, its three sibling repos,
   and their docs) for any existing authoritative LMPE wire-format
   definition. Found only design placeholders: `docs/lmpe.md` and
   `docs/etsi-compatibility/ts-103-698-mapping.md` in this repo (both
   explicitly `Status: NOT STARTED`, no content-type/framing/field list),
   and `PJSIP-windows-app-emergency/lmpe/{LmpeMessage,LmpeSession,
   LmpeProtocolAdapter}.{h,cpp}` in a sibling repo, whose `.cpp` files are
   empty stubs and whose `.h` files define only a UI-facing message struct
   with no wire format at all.
3. Confirmed the existing LMPE UI tab in `RttPanel` (`onLmpeSend`) is a
   pure local-echo placeholder with no transport, no encoder, no decoder —
   consistent with `docs/lmpe.md`'s own "NOT STARTED" status.
4. Per the task's own gating rule, the real LMPE codec is **BLOCKED**. No
   protocol logic was written. A neutral interface (`LmpeCodec`) and a
   fail-closed placeholder implementation (`UnconfirmedLmpeCodec`) were
   added as the only in-scope deliverable, together with a regression test
   that guards against the placeholder ever silently reporting success.

See `docs/agent-results/W103-lmpe-foundation-result.md` for the full
report.
