# Agent Prompt — Task W113F: Simplify Client Messaging, Hide Protocol XML and Force LMPE Inactive

Condensed from the full task spec provided by the project owner in chat
(`W113F-simplify-client-messaging-and-disable-lmpe.txt`) — this file
summarizes the requirements rather than duplicating every line.

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `fix/w113f-simplify-client-messaging-and-disable-lmpe`.
- Starting branch (per spec): `fix/w113e-rtt-request-visual-alert`. This
  matched reality exactly — that branch was already complete (80/80 →
  81/81 CTest, pushed) at the time this task began.
- No merge into `main`/`release`. `pjproject` not modified. No new
  protocols added.

## Version note

Spec assumed current version 1.6.5, new version 1.6.6. Real current
version at task start was already **1.6.6** (bumped by W113E, which the
spec's own author apparently hadn't accounted for). Per the spec's own
"if the real starting version differs, identify it and apply a patch
bump, document it" instruction: bumped **1.6.6 → 1.6.7** instead, and this
substitution is documented here and in the release notes.

## Problems reported

1. LMPE appears in the UI without being permanently marked
   unavailable/disabled.
2. Client Messaging is effectively unusable: XML tags appear after
   messages, technical payloads render as if they were user messages, and
   it's unclear whether a message was sent/delivered or what transport was
   used.
3. Client view needs to become simple and user-oriented.
4. Tools must keep and extend full technical visibility (raw SIP, raw
   MSRP, CPIM, IMDN XML, is-composing XML, PIDF XML, transport selection,
   fallback, Message-ID, transaction IDs, delivery correlation, errors,
   diagnostics).

## Approach taken

1. Audited the full messaging pipeline (outgoing/incoming, both SIP
   MESSAGE and MSRP transports) and every LMPE UI surface via an Explore
   agent plus direct code reading, before making any change.
2. Found and fixed the actual root cause: two inbound paths (plain SIP
   MESSAGE, MSRP) each had incomplete or entirely missing CPIM/IMDN/
   is-composing classification, so protocol payloads landed in
   `MessageHistoryStore` as plain messages. Unified both paths behind one
   new `SipManager::routeInboundMessagingPayload()`.
3. Added `MessageHistoryEntry::isProtocolEvent()` and
   `ConversationModel::userVisibleHistoryFor()` so the Client UI (history
   list, unread count, conversation preview) excludes protocol-event rows,
   while Tools' Message History table keeps showing everything.
4. Simplified `ClientMessagingView`: protocol-level controls moved behind
   a collapsed-by-default "Messaging options" disclosure; CPIM removed as
   a manual content-type choice (already automatic via a separate global
   setting).
5. Forced LMPE permanently unavailable at every UI surface found in the
   audit (profile editor checkbox, `RttPanel`'s interactive tab,
   `CallWorkspacePanel`'s status card) and at the profile-load level
   (`SipProfileManager`), with a redacted warning log for stale config.
6. Added test coverage for the new filtering/placeholder/LMPE-disable
   behavior; corrected three docs that described a dead-code class
   (`MsrpPayloadDispatcher`) as if it were the real production path.

## Global rules (same discipline as every task this session)

Debug + Release + `ENABLE_PJSIP=ON` builds; all existing tests pass; new
tests for new behavior; docs; `docs/project-status.md` update;
agent-prompt/agent-result pair; push (no merge); no pjproject changes; no
manual GUI test claimed PASS without an actual live peer.

See [W113F-simplify-client-messaging-and-disable-lmpe-result.md](../agent-results/W113F-simplify-client-messaging-and-disable-lmpe-result.md)
for the full 51-item final report.
