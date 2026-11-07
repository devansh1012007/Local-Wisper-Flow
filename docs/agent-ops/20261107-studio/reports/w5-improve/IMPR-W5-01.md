# IMPR-W5-01 — Wave 5 improver (W5-IMPR-01)

<!-- ORCH-DIGEST:START -->
agent: W5-IMPR-01 | scope: Wave 5 review fixes REV-W5-01 R-W5-001..R-W5-009 (T-026,T-021-review) | status: IN-PROGRESS
findings: MAJOR 3 | MINOR 5 | NIT 1 — FIXED: R-W5-001 (try_lock single-flight, Busy), R-W5-002 (Privacy/About exhaustive), R-W5-004 (504/412), R-W5-005 (dump wrap), R-W5-006 (choices[0].error); DEFERRED: R-W5-003 (HeatLevel, write-set mismatch), R-W5-007, R-W5-008, R-W5-009. See §8 table.
<!-- ORCH-DIGEST:END -->

## 1. Context report (pre-edit)

Required reading done: STANDING-ORDERS.md, AGENTS.md, 00-mission.md, 01-work-ledger.md,
REV-W5-01.md (full), W5-IMPL-O-T-026.md (D1-D7 + DECISION-1), W5-IMPL-INT.md (gate-w5-1),
plan §§J,K,L (J: llm_provider=openrouter sends text, opt-in default local, must be stated on
Privacy + About; K: phases green, C last behind default local; L: R-02 resolved by docs).
Branch `feature/studio-pages`, HEAD 091ec36.

Root cause / entry points:
- R-W5-001: OpenRouterProvider::Process holds mutex_ across transport_.Post (blocking WinHTTP,
  no cancel token); cancel checked only before/after. DictationCleanup caller times out at 2000ms
  (schema default) + 3s drain, but provider Post bounded by 8000ms transport timeout (main.cpp:394
  kDefaultTimeoutMs). Next cross-caller Process (command/transform on engine thread, sharing same
  provider pointer) blocks on mutex_ up to ~8s. Cleanup's own WorkerQueue is single-threaded, so
  second cleanup task queues behind the hung Post regardless of provider mutex.
- R-W5-002: MainWindow.cpp:1337-1338 says "only opt-in exception" (OpenRouter) and :1376-1379 says
  "unless OpenRouter cleanup is enabled" — both omit pre-existing STT-cloud egress (gnani/
  elevenlabs/muse + stt_consent_accepted + keys, ConsentGate.cpp table).
- R-W5-003: HeatLevel defined in StudioPalette.h:75-90 (>=), NOT in Display.cpp (only caller at
  Display.cpp:211). CalendarLevel in InsightsModel.cpp:190-199 uses integer quarter buckets.
- R-W5-004/006: StatusToCategory (OpenRouterProvider.cpp:64-87) maps 504->Server; ParseSuccessBody
  never checks choice.error.

Affected files / invariants:
- Write-set: src/llm/OpenRouterProvider.{h,cpp}, src/llm/OpenRouterContract.h,
  src/ui/MainWindow.{h,cpp} (Privacy/About only), src/core/DictationCleanup.{h,cpp} (timeout
  constant only), src/ui/widgets/Display.cpp (HeatLevel only), tests/test_openrouter.cpp,
  tests/test_dictation_cleanup.cpp (timeout tests only if needed).
- Invariants: ILlmProvider string-only (empty=failure, no throw for wire failures); single-flight
  Post per Process; redaction of detail; local default untouched; no code comments; copy lint
  (no provider/STT/LLM/Whisper/! in display strings).
- Consumers: DictationCleanup (WorkerQueue), CommandModeController, DictationEngine transform —
  all treat empty as fallback (raw / LlmFailed+restore).

## 2. Scoped verification before edits

- [VERIFIED src/main.cpp:391-394] openrouter timeout = kDefaultTimeoutMs (8000); [VERIFIED
  src/main.cpp:453] cleanup timeout = schema cleanup_timeout_ms (default 2000, SettingsStore.cpp:35).
  Per-caller plumbing needs main.cpp + ILlmProvider change = outside write-set / High Risk.
- [VERIFIED src/core/WorkerQueue.h:10-66] single thread per queue; [VERIFIED
  src/core/DictationCleanup.cpp:188-197] submit + wait_for(timeout) + RequestCancel + 3s drain.
- [VERIFIED src/stt/cloud/WindowsHttpClient.cpp:67-167] Post uses per-call locals (session/
  connection/handle), WinHttpSetTimeouts(ms,ms,ms,ms); no cancel token on IHttpTransport.
- [VERIFIED live webfetch https://openrouter.ai/docs/api_reference/errors-and-debugging
  2026-11-07] typed table: timeout->504, precondition_failed->412; Chat Completions non-streaming
  provider error embedded as choices[{message:{partial}, finish_reason:"error", error:{code,
  message, metadata.error_type}}]. Status list 400/401/402/403/408/429/502/503; 200-with-error
  shape confirmed. Contract VERIFIED-from-docs / UNVERIFIED-against-live (no key, no live call).
- [VERIFIED src/ui/MainWindow.cpp:1302-1387] Privacy + About copy as quoted in §§; no exact-string
  pins in tests/; copy lint at tests/test_overlay_settings_ui.cpp:967-1040.
- [VERIFIED] No Oct-2026 check dates exist in tree (grep 2026-10 hits only BENCHMARKS/insight test
  fixtures). STT docs state 2026-11-04 (docs/STT_CLOUD.md:139,165); OpenRouter header
  kDocsChecked=2026-11-07. No invented dates put in UI copy.

## 3. Working files (write-set check)

Touched (planned): src/llm/OpenRouterProvider.h, src/llm/OpenRouterProvider.cpp,
src/ui/MainWindow.cpp (Privacy/About sentences only), tests/test_openrouter.cpp.
Untouched by design: src/llm/OpenRouterContract.h (kDocsChecked already 2026-11-07 = re-check date,
no value change needed), src/core/DictationCleanup.* (timeout-value alignment rejected, §8),
src/ui/widgets/Display.cpp + src/ui/theme/StudioPalette.h (HeatLevel DEFERRED, §8),
tests/test_dictation_cleanup.cpp (no such file; timeout tests not needed — hung-test lives in
test_openrouter.cpp against the provider mutex, the actual wedge site).
Forbidden respected: CMakeLists.txt, scripts/**, SettingsStore.cpp, main.cpp, src/os/**.

## 4. Sessions Log

- Investigation + this report created; COMMS START+CLAIM appended (via edit tool — the
  printf one-liner from standing orders is mangled by this box's PowerShell quoting, so the
  append was done with the edit tool against the last COMMS line instead; real UTC timestamp
  from Get-Date, not invented).
- Phase 1 (provider): Busy enum + try_lock single-flight + dump wrap + 504/412 + choice.error.
- Phase 2 (copy): Privacy + About exhaustive egress sentences.
- Phase 3 (tests): 504/412 rows, choice-error x2, serialization, hung-test, gated serialization;
  TrackingTransport removed (superseded).
- Phase 4: configure + build + scoped + full ctest + hygiene/no-network green; self-review clean.

## 5. Phases executed

### Phase 1 — Provider de-wedge + error mapping (R-W5-001, R-W5-004, R-W5-005, R-W5-006)

Files: src/llm/OpenRouterProvider.h, src/llm/OpenRouterProvider.cpp, tests/test_openrouter.cpp.

- R-W5-001 root cause: `mutex_` held across the blocking `transport_.Post`. Fix: `Process` now
  takes `std::unique_lock(mutex_, try_to_lock)`; a caller that arrives while another `Post` is in
  flight returns empty immediately with new `OpenRouterError::Busy` ("busy", logged as failure),
  touching neither the cancel flag nor the wire. Single-flight is preserved (at most one `Post`
  in flight, so no new thread-safety requirement is placed on `IHttpTransport` — the transport
  contract does not promise thread-safety and the scripted fakes are not thread-safe).
  // DECISION: fail-fast Busy over release-and-concurrent-Post. Releasing the lock would let two
  `Post`s overlap, share one cancel flag (one caller's cancel would cancel the other's post-check,
  and the second entry's `cancel_requested_.store(false)` would un-cancel the first), and assume
  undocumented transport thread-safety. Busy converts the 8s head-of-line block into the same
  empty→fallback the callers already handle for every other failure, with one honest new outcome.
  Timeout-value alignment (8000 vs 2000) was REJECTED here (see §8): production values come from
  `main.cpp:394` (contract constant) and `main.cpp:453` (schema), neither in write-set, and
  per-caller plumbing needs an `ILlmProvider` interface change (High Risk). The residual
  worker-thread occupancy of a hung `Post` (bounded by transport timeout) needs transport-level
  cancel (IHttpTransport + WindowsHttpClient, outside write-set) — carried as NEED-4, not fixed.
- R-W5-004: `StatusToCategory`: 504 joined 408→Timeout; 412 made explicit→InvalidRequest (was
  already covered by the generic 4xx bucket; now pinned). `error_type` precedence unchanged.
- R-W5-006: `ParseSuccessBody` checks `choice.error` (non-null) before content extraction and
  fails closed with the typed outcome via the existing `ExtractErrorType`/`ExtractErrorDetail`
  helpers over a one-member wrapper — no partial-content extraction when `error` is present.
  Verified against live vendor docs (see §2): the non-streaming embed carries partial
  `message.content` alongside `choice.error`, which the old code returned as clean success.
  `message.refusal` (success-path refusal, `finish_reason: content_filter`) still falls to
  Malformed ("no content field") — fail-closed, observability-only, left as-is.
- R-W5-005: `BuildRequest` is now called inside `try`/`catch` in `Process`; serialization throws
  (nlohmann `dump()` on invalid UTF-8/OOM) map to `Malformed` + empty, restoring the
  "never throws across the seam" invariant for this condition too.
- `OpenRouterError::Busy` added additively (existing values unchanged; `ToString` extended).
  No caller branches on the enum (all treat empty uniformly), so the new value is observability
  only.

### Phase 2 — Privacy/About exhaustive egress (R-W5-002)

Files: src/ui/MainWindow.cpp (Privacy + About sentences only).

- Privacy now reads: "No telemetry. By default no network calls except explicit model downloads.
  Cloud speech transcription and OpenRouter cleanup are the only opt-in exceptions, both off by
  default. Each sends only what its feature needs, and only while you have it enabled."
- About now reads: "Local-first voice dictation. On-device speech recognition and optional text
  cleanup, clipboard injection. Processing stays on this machine unless you opt into cloud speech
  transcription or OpenRouter cleanup."
- Both name both networked opt-ins (STT cloud + OpenRouter), state defaults (off), and name no
  other egress. The word "only" now scopes to the pair, not to OpenRouter alone. Conditional
  warnings ("Cleanup uses OpenRouter: ... when cleanup is on.") are accurate in isolation and
  were left untouched. Copy-lint constraints honored (no `provider`/`STT`/`LLM`/`!` in display
  strings; `OpenRouter` is the AGENTS.md-required egress name).
- "Oct 2026 check dates" (brief): no such dates exist in the tree (see §2). Vendor check dates
  are contract metadata (`OpenRouterContract.h` `kDocsChecked=2026-11-07`, STT docs 2026-11-04),
  not user-facing privacy disclosure, and the brief assigns the date update to the contract
  header (MAJOR3), not to MainWindow. Putting developer check dates in Privacy copy would be
  inventing content, so no dates were added to the UI. The OpenRouter header already carries
  today's re-check date (2026-11-07, re-verified this session via live fetch), so
  `OpenRouterContract.h` intentionally has no diff.

### Phase 3 — Tests (tests/test_openrouter.cpp)

- Status table: +504→Timeout, +412→InvalidRequest rows (both fail without the mapping fix:
  504 classified as Server; 412 passed only via the generic bucket with no pin).
- `choice error as failure even with partial content` (fails without fix: returned the partial
  content as success) and `choice error without content as typed failure` (fails without fix:
  classified Malformed instead of RateLimited).
- `serialization failure is malformed outcome not a throw` with `hello \xFF world` (fails
  without fix: `dump()` threw out of `Process`). Passes with fix (doctest confirms nlohmann
  throws on invalid UTF-8 and the provider converts it).
- `second process does not wedge while first post is hung`: gated Post, second `Process` returns
  empty/Busy/<5000ms with one wire request before the gate opens; first then succeeds. Fails
  without fix (second blocks until the gate's 10s deadline and returns content).
- `serializes concurrent process calls` rewritten to the gated single-flight form (second thread
  joins empty before release; exactly one request). The old TrackingTransport version assumed
  both callers succeed, which is exactly the wedging behavior being fixed; TrackingTransport
  was removed as superseded dead code. Mutation mental check: deleting the `try_to_lock` makes
  both rewritten tests fail (second blocks / returns content, request count becomes 2).

## 6. Review audit

Self-review per phase (re-read diff from scratch; CRITICAL/MAJOR/MINOR with evidence):

- Phase 1 — CRITICAL: none. MAJOR (considered, rejected alternative): release-lock concurrency
  (see DECISION in §5) would share `cancel_requested_` across concurrent calls — the Busy path
  deliberately does not reset or check it. MINOR (accepted): Busy overwrites `last_outcome_`
  while the in-flight call still runs; the in-flight call's terminal `StoreOutcome` overwrites
  it again, so the final value is the in-flight call's (same race as any terminal write).
  Empty-text-while-busy returns Busy instead of None — both empty, callers identical.
  R-W5-005 detail could in theory contain nlohmann error text; it goes through `RedactDetail`
  like every other detail and does not echo inputs (verified passing redaction suite).
- Phase 2 — CRITICAL: none. Copy scanned against the F9 lint (banned whole-words + `!`):
  clean. No exact-string test pins exist for these sentences (grep tests/ → none), so no
  pin update was needed or made.
- Phase 3 — Test rigor: every new case fails for its specific reason without its fix (mutation
  mental checks above); no `.skip`, no loosened matchers, no mocking of the unit under test
  (fakes speak HTTP exactly as before). The GateTransport 10s deadline remains a failure
  bound, not a sleep-to-pass.
- Final — `git diff` inspected: only write-set files changed (provider h/cpp, MainWindow.cpp
  two sentences, test_openrouter.cpp) plus COMMS/report. No `//` comments added (hygiene OK),
  no `windows.h`, no network-API strings (no-network OK), no forbidden files. No independent
  review obtained (W5 parallel window, same as W4/W5 implementers); per Standing Orders §3
  this is recorded, and the orchestrator may dispatch a fresh-context reviewer if it classifies
  the single-flight change as High Risk — otherwise COMPLETE on my phases.

## 7. Verification evidence (verbatim)

Brief-mandated commands (run from repo root, `feature/studio-pages`):

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` -> `-- Configuring done (1.2s)` /
  `-- Generating done (12.1s)` / build files written. (Only pre-existing CMake deprecation +
  FetchContent + ONNX-not-found notices.)
- `cmake --build build --config Release --parallel 4 --target lwf_tests` -> EXIT 0:
  `[0%] Building CXX object .../src/llm/OpenRouterProvider.cpp.obj`, `Built target lwf_tests`.
- `cmake --build build --config Release --parallel 4 --target local-wisper-flow` (added to
  compile-check the MainWindow.cpp edit, which `lwf_tests` alone does not build) -> EXIT 0,
  `Built target local-wisper-flow`; only pre-existing gRPC `empty_bases` warnings.
- `ctest --test-dir build -R lwf-tests` -> `1/1 Test #1: lwf-tests Passed 19.76 sec` /
  `100% tests passed out of 1` / `Total Test time (real) = 19.88 sec`.
- Scoped doctest: `.\build\lwf_tests.exe --test-case="*openrouter*"` ->
  `test cases: 23 | 23 passed | 0 failed | 495 skipped` / `assertions: 137 | 137 passed` /
  `Status: SUCCESS!` (baseline 19/118; +4 cases, +19 asserts).
- Scoped doctest: `.\build\lwf_tests.exe --test-case="*cleanup*,*privacy*,*settings*,*copy lint*,*widget sweep*,*studio*,*F9*,*palette*,*contrast*,*heat*"`
  -> `24 passed | 12597 passed asserts SUCCESS` (copy lint green with the new Privacy/About copy).
- `bash scripts/check_hygiene.sh` -> `hygiene: OK`.
- `bash scripts/check_no_network.sh` -> `no-network: OK`.
- Live vendor re-check this session: `webfetch https://openrouter.ai/docs/api_reference/
  errors-and-debugging` (2026-11-07) confirms `timeout→504`, `precondition_failed→412`,
  non-streaming `choices[0].error` embed with partial content, 200-with-error shape, and the
  `error{ code, message, metadata.error_type }` envelope. Live authenticated call: NOT-RUN
  (no key; contracts remain VERIFIED-from-docs / UNVERIFIED-against-live). G3 zig: NOT-RUN
  (no zig on box per AGENTS.md). Windows/hardware dictation: NOT-RUN.

## 8. Finding dispositions (FIXED / REJECTED-with-evidence / DEFERRED-with-reason)

| ID | Sev | Disposition | Evidence |
|---|---|---|---|
| R-W5-001 | MAJOR | FIXED (de-wedge; transport-cancel residual DEFERRED as NEED-4) | try_lock single-flight + Busy (§5); hung-test pins no-wedge. Timeout-value alignment REJECTED: production values live in main.cpp:394/:453 + schema (all outside write-set); per-caller plumbing needs an ILlmProvider interface change (High Risk). Hung Post still occupies the cleanup WorkerQueue thread up to the transport timeout — mid-Post abort needs a cancel token on IHttpTransport + WindowsHttpClient (NEED-4, outside write-set). |
| R-W5-002 | MAJOR | FIXED | Privacy + About now name both opt-ins, defaults off, no false "only" (§5 Phase 2). Copy-lint suite green. |
| R-W5-003 | MAJOR (brief: align+test) | DEFERRED-DEPENDENCY | HeatLevel is defined in src/ui/theme/StudioPalette.h:75-90, not Display.cpp (Display.cpp:211 is the sole call site), and its pins live in tests/test_overlay_settings_ui.cpp:778-783 — all outside this write-set (Display.cpp reaches only the call). Aligning requires StudioPalette.h + test_overlay_settings_ui.cpp (canonical-direction decision: CalendarLevel integer buckets vs quartile bands). NEED posted in COMMS. No copy-paste local classifier was added in Display.cpp — that would duplicate the divergence the review flags. |
| R-W5-004 | MINOR | FIXED | 504→Timeout, 412→InvalidRequest explicit; pinned rows in the status table. Matches live typed table fetched this session. |
| R-W5-005 | MINOR | FIXED | BuildRequest wrapped in try/catch → Malformed; invalid-UTF-8 test pins no-throw. |
| R-W5-006 | MINOR | FIXED | choice.error checked before content, fail-closed, typed via existing helpers; two tests pin partial-content and null-content shapes. |
| R-W5-007 | MINOR | DEFERRED with reason | Review verdict DO-LATER. Tables match live vendor docs today (re-verified §2); a cross-table test needs src/stt/cloud/HttpStatus.* (outside write-set) for ~zero present drift. Revisit when either table changes. |
| R-W5-008 | MINOR | DEFERRED with reason | Review verdict DO-LATER. A ProviderFailures() counter needs a DictationCleanup API change (write-set allows timeout constant only) + provider-type coupling. Counter conflation is disclosed in LastOutcome; no behavior change. |
| R-W5-009 | NIT | DEFERRED with reason | Review: correct, silent-ish, acceptable best-effort. DictionaryStore.* is outside write-set; surfacing write-failure in UI needs a MainWindow/MainWindow.h change beyond Privacy/About scope. |

## 9. Long-run answers (Source / Load / Hostile / Operability / Reversal)

- Source: Fix is at the provider seam the wave established — single-flight enforced by the
  provider itself, outcome enum is the observability source callers already read. No new
  abstraction, no interface change, no new dependency. HeatLevel/CalendarLevel duality is
  honestly carried as a NEED rather than papered over with a third copy.
- Load: Busy path is O(1) (one failed lock attempt, one outcome store). No extra thread,
  no queue, no polling. In-flight Post rate unchanged (≤1, human cadence).
- Hostile: Vendor `choice.error` with string/number/array shapes fails closed (Unknown +
  "provider error", empty) — never partial content. Invalid UTF-8 input normalizes to
  Malformed instead of unwinding. Busy rejection sends zero bytes (no egress for the loser).
- Operability: `LastOutcome.error=busy` + the existing `Log::Warning` line distinguish
  "previous request hung" from auth/timeout/network. Timeout budgets unchanged, so field
  behavior for slow-but-healthy vendors is identical.
- Reversal: Reverting is the provider h/cpp diff + MainWindow two sentences + test file.
  `OpenRouterError::Busy` is additive; removing it restores the old enum exactly. No schema,
  no config, no data-format change.

## 10. Unverified assumptions & remaining risks

- Windows runtime NOT VERIFIED (hotkey/tray/overlay/injector paths, real-hardware dictation,
  live OpenRouter call). Only the MinGW build + doctest + gates ran. G3 zig NOT-RUN. MSVC lane
  is CI-only.
- Busy fail-fast is a deliberate behavior change: a second caller that previously waited up to
  ~8s and then succeeded now fails fast to the caller's existing fallback (raw / LlmFailed).
  For the dictation-cleanup path this is a latency win; for an explicit command/transform
  racing a hung cleanup Post it trades a possible late success for prompt fallback. The old
  serialize-and-succeed test pinned the removed behavior and was rewritten to gated
  single-flight (see §5 Phase 3).
- Mid-Post abort still open (NEED-4): cleanup worker occupancy up to the 8000ms transport
  timeout remains; next cleanup still queues behind it on the single WorkerQueue thread.
- HeatLevel alignment still open (NEED, §8 R-W5-003): heat grid shows post-F9 `>=` edges
  (25→2, 50→3, 75→4) until the canonical-direction decision lands with StudioPalette.h +
  test_overlay_settings_ui.cpp.

## 11. Git state

- Branch: `feature/studio-pages` (verified `git branch --show-current` before committing).
- Code commit (explicit paths, separate from docs commit per standing orders):
  src/llm/OpenRouterProvider.h, src/llm/OpenRouterProvider.cpp, src/ui/MainWindow.cpp,
  tests/test_openrouter.cpp.
- Docs commit (explicit path): this report only, with trailers Task: T-026,T-021-review /
  Agent: W5-IMPR-01 / Wave: 5 / Report: docs/agent-ops/20261107-studio/reports/w5-improve/
  IMPR-W5-01.md.
- COMMS.md NOT committed here (orchestrator-owned at wave boundaries).
- Forbidden ops not used: no CMakeLists/scripts/SettingsStore/main.cpp/src/os edits, no
  `git add -A`, no stash/reset/checkout/clean/rebase/merge/push, no `--no-verify`, no
  index.lock. (An artifact file `ORCH` created by a mangled shell invocation was removed
  with Remove-Item; it was never staged.)

