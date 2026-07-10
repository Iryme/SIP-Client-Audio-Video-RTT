# Task W096 — IMDN Foundation — Result

1. **Branch**: `feature/w096-imdn-foundation`

2. **Branch de pornire**: `feature/w095-deflate-rcs-diagnostics`

3. **Fișiere modificate/adăugate**:
   - Noi: `src/sip/ImdnGenerator.h`/`.cpp`, `docs/imdn.md`,
     `docs/agent-prompts/W096-imdn-foundation.md`,
     `docs/agent-results/W096-imdn-foundation-result.md`.
   - Modificate: `src/sip/ImdnInfo.h`, `src/sip/ImdnParser.cpp`,
     `src/sip/SipMessageComposer.h`/`.cpp`, `src/core/AppSettings.h`,
     `src/sip/SipAccount.h`/`.cpp`, `src/sip/SipManager.h`/`.cpp`,
     `src/sip/MessageHistoryEntry.h`, `src/sip/MessageHistoryStore.h`/`.cpp`,
     `src/gui/panels/MessagingDiagnosticsPage.h`/`.cpp`,
     `src/sip/MessagingEvent.h`, `src/sip/MessagingEventStore.cpp`,
     `src/sip/InteropTraceExporter.h`/`.cpp`, `CMakeLists.txt`,
     `tests/CMakeLists.txt`, `tests/test_imdn_parser.cpp`,
     `tests/test_message_history.cpp`, `tests/test_sip_message_foundation.cpp`,
     `docs/message-history.md`, `docs/messaging-diagnostics.md`,
     `docs/messaging-event-store.md`, `docs/sip-message.md`,
     `docs/windows-trace-json-export.md`, `docs/project-status.md`.

4. **Cum se generează IMDN**: `ImdnGenerator::generate(messageId, disposition,
   originalRecipient, finalRecipient, datetime)` (pure Qt/text, no PJSIP
   dependency) builds an RFC 5438 `<imdn>` XML document — `delivered`/
   `failed`/`forbidden` wrapped in `<delivery-notification>`, `displayed`/
   `error`/`processed` in `<display-notification>` (this client only ever
   *generates* delivered/displayed/failed/error). Returns an empty string
   when `messageId` is empty or `disposition == None`. Element names
   (`<message-id>`, `<original-recipient>`, `<final-recipient>`) intentionally
   match what the pre-existing `ImdnParser` already recognizes so a
   generated report round-trips through the unmodified parser.
   `SipMessageComposer::composeImdnReport()` wraps this into a
   `ComposedSipMessage` with `contentType = "message/imdn+xml"`, then it is
   sent via the unchanged `SipManager::sendSipMessage()` path (same as any
   other SIP MESSAGE).

5. **Auto Delivered**: `SipAccount::onInstantMessage` extracts the
   `Message-ID` and `Disposition-Notification` headers directly from the
   parsed `pjsip_msg` via `pjsip_msg_find_hdr_by_name` (the same low-level
   pattern already used for `Call-ID`/`From`) and forwards them through the
   widened `instantMessageReceived` signal.
   `SipManager::onAccountInstantMessageReceived` checks whether
   `Disposition-Notification` contains `positive-delivery`; if so and
   `AppSettings::autoSendDeliveredImdn()` is true (**default ON**), it
   builds and sends a `delivered` report via `sendImdnReport()`, then marks
   the inbound entry's `deliveredImdnSent` flag so it is never sent twice
   for the same message.

6. **Cum funcționează Displayed**: never sent automatically by default
   (`AppSettings::autoSendDisplayedImdn()` defaults **OFF**). When ON, the
   same auto path as Delivered applies for `positive-display`. When OFF
   (default), the user marks a message as read manually: select the
   inbound row in Message History and click **Mark as Read**, which calls
   `SipManager::sendDisplayedImdnForEntry(entryId)` — this checks the entry
   exists, is inbound, has a `Message-ID`, and has not already had a
   Displayed report sent (`!displayedImdnSent`) before sending.

7. **Corelarea după Message-ID**: outbound — `ComposedSipMessage::messageId`
   (a structural copy of the `Message-ID` header `SipMessageComposer::compose()`
   already generated when `requestImdn` is set) is captured onto the
   `MessageHistoryEntry` by `MessageHistoryStore::appendOutbound()`. Inbound
   — an incoming `message/imdn+xml` body is parsed by the unmodified
   `ImdnParser`; its `ImdnInfo::messageId` (the *original* message being
   reported on) is passed to `MessageHistoryStore::correlateDelivery()`,
   which finds the most recent **outbound**, non-report entry whose
   `messageId` matches and upgrades its `deliveryState`
   (`None → Delivered/Displayed/Failed/Error`). No match found → no-op, no
   signal emitted (idempotent for unknown/evicted Message-IDs).

8. **Modificările de infrastructură**: the pre-implementation byte-safety
   check required by the task was performed — **no infrastructure change
   was needed**. The live SIP MESSAGE send/receive path
   (`SipAccount::sendMessage`/`onInstantMessage`) uses pjsua2's high-level
   `std::string`-based `SendInstantMessageParam`/`OnInstantMessageParam`,
   which PJSIP itself already decodes/encodes as text — there is no
   `QString`/`toLatin1()` raw-bytes round-trip in this path (unlike the raw
   SIP trace *parser*, which Task W095 already made binary-safe via
   `SipBodyExtractor`). IMDN report bodies are UTF-8 XML text, so no
   binary-safety concern applies to them either.

9. **Modificările în exportul JSON**: `generatedImdn` (bool),
   `receivedImdn` (bool), `correlatedMessageId` (string), `deliveryState`
   (string) added to every event in both `MessagingEventStore::exportToJson()`
   and `InteropTraceExporter`'s per-event object — purely additive, mapped
   in `MessagingEventStore::mapFromTraceEntry()` from the already-parsed
   `MessagingTraceEntry::imdn` (no new parsing). `InteropTraceExporter::kSchemaVersion`
   **stays 2** (no v2 field removed/renamed).

10. **Ce rămâne neimplementat**:
    - Presence, XCAP — not implemented, per the task's explicit rule.
    - MSRP — remains fully disabled everywhere.
    - No retry/backoff for a failed IMDN send (reported once, not retried).
    - No aggregate "unread message count" UI badge — only the per-row
      `received (unread)` status text.
    - Adler/CRC-equivalent integrity is not applicable here (plain XML
      text, no compression) — this task does not touch W095's deflate
      decoding.

11. **Teste rulate**: full build with `ENABLE_PJSIP=ON` (`nmake`, clean —
    zero errors after the two `CALL_SOURCES`/`MESSAGING_DIAGNOSTICS_SOURCES`
    CMake link fixes described in section 12 below) followed by
    `ctest --output-on-failure`: **100% tests passed, 0 failed, 46 total**
    (same total test-suite count as W095 — this task extended 3 existing
    suites, `test_imdn_parser`, `test_sip_message_foundation`,
    `test_message_history`, rather than adding a new suite).

12. **Limitări**:
    - Deflate/RCS fixtures and W090–W095 regression coverage are untouched
      and still pass, confirming no behavioral change to the diagnostics
      pipeline.
    - Discovered during the build (not a design limitation, but worth
      recording): `tests/CMakeLists.txt`'s `test_sip_manager`/other
      `CALL_SOURCES`-based test targets needed `MessagingContentKind.cpp`/
      `ImdnParser.cpp`/`ImdnGenerator.cpp`/`SipMessageComposer.cpp`/
      `CpimBuilder.cpp`/`SipUriNormalizer.cpp` added to `CALL_SOURCES`,
      since `SipManager.cpp` now calls directly into
      `MessagingContentKindDetector::detect`, `ImdnParser::parse`, and
      `SipMessageComposer::composeImdnReport` (previously it only
      referenced `ComposedSipMessage`'s fields, needing no link
      dependency). Verified no duplicate-symbol risk: no test target links
      both `CALL_SOURCES` and `SIP_MESSAGE_FOUNDATION_SOURCES`/
      `MESSAGING_DIAGNOSTICS_SOURCES` together.
    - `ImdnGenerator`'s Huffman/XML-escaping path is exercised via explicit
      unit tests (UTF-8 + special characters), but no real-world Linphone
      capture of a generated report was available to validate against — as
      with prior W-series tasks, only synthetic/hand-built fixtures were
      used.

13. **Commituri** (6, on `feature/w096-imdn-foundation`):
    - `c92588f` feat(imdn): implement imdn document generator
    - `dc6a153` feat(imdn): implement automatic delivered notifications
    - `068b602` feat(imdn): correlate imdn with message history
    - `eee67ab` feat(ui): display imdn delivery state
    - `e73377b` test(imdn): add imdn interoperability tests
    - (this commit) docs(imdn): document imdn workflow

14. **git status**: clean after this commit (to be confirmed in the final
    chat report).

15. **Push status**: pending — pushed to `origin/feature/w096-imdn-foundation`
    after this commit, per the mandatory workflow. No merge into
    main/release.
