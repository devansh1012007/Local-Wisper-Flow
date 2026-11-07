<!-- ORCH-DIGEST:START -->
agent: W5-DELTA-01 | scope: delta git diff 091ec36..HEAD -- src/ tests/ (4 files, +116/-39; docs excluded) | verdict: APPROVE
findings: BLOCKER 0 | MAJOR 0 | MINOR 2 | NIT 1 (new; residuals only — no new regressions)
claims_audit: IMPR-W5-01 §5/§8 checked against code — FIXED claims verified for R-W5-001 (de-wedge half), R-W5-002, R-W5-004, R-W5-005, R-W5-006; timeout-value alignment REJECTED-with-write-set-evidence, HeatLevel DEFERRED as NEED (not reopened)
band_aids_found: none — Busy fail-fast is root-cause at the provider-mutex seam; transport-level cancel remains correctly carried as NEED-4, not masked
top_3_risks: (1) hung Post still occupies cleanup WorkerQueue thread up to 8000ms HTTP timeout (NEED-4, pre-dating delta); (2) HeatLevel/CalendarLevel divergence still open (NEED, StudioPalette.h); (3) choice-error with code-only (no error_type) on 200 loses Server identity → Unknown (fail-closed, observability only)
optimizations: DO-NOW 0 | DO-LATER 0 | NOT-WORTH-IT 0 (delta introduces no perf surface; Busy path is O(1) fail-fast)
reversal_check: done (revert = provider h/cpp + 2 UI sentences + test file; Busy enum is additive)
confidence: high (blind code trace + doctest 23/23 rerun + hygiene/no-network gates; Windows runtime / G3 zig / live-key NOT-RUN)
<!-- ORCH-DIGEST:END -->

# Code review — W5-DELTA-01 (delta reviewer, read-only)

## 0. Method
Diff range reviewed: `git diff 091ec36..HEAD -- src/ tests/` (docs excluded per scope). 4 files:
`src/llm/OpenRouterProvider.cpp` (+44/-2), `src/llm/OpenRouterProvider.h` (+3), `src/ui/MainWindow.cpp`
(+8/-4), `tests/test_openrouter.cpp` (+100/-39). No source edits made (read-only mandate).
Blind pass first (diff + full `OpenRouterProvider.{h,cpp}`, `OpenRouterContract.h`,
`DictationCleanup.{h,cpp}`, `WorkerQueue.h`, `MainWindow.cpp` Privacy/About, `SettingsStore.cpp`
schema, `ConsentGate.cpp`, `main.cpp` wiring), then read `reports/w5-improve/IMPR-W5-01.md`
for the claims audit. Commands run (verbatim outcomes):
- [RAN `git diff 091ec36..HEAD --stat` -> 4 files, commits 8bae0c8 + 4fb18be on `feature/studio-pages`]
- [RAN `.\build\lwf_tests.exe --test-case="*openrouter*"` -> `test cases: 23 | 23 passed | 0 failed | 495 skipped / assertions: 137 | 137 passed / Status: SUCCESS!`]
- [RAN `ctest --test-dir build` (partial) -> `1/1 Test #1: lwf-tests Passed 19.98 sec`; re2 suites timed out at 120s tool cap — no delta-scope failures observed]
- [RAN `bash scripts/check_hygiene.sh` -> `hygiene: OK`]
- [RAN `bash scripts/check_no_network.sh` -> `no-network: OK`]
- NOT-RUN: full ctest to completion, `cmake --build` (prebuilt tree used), G3 zig (no zig on box), Windows runtime dictation, live authenticated OpenRouter call (no key).

## 1. Blind-pass verification (per-fix, before reading IMPR-W5-01)

**Fix A — single-flight fail-fast (R-W5-001 de-wedge). VERIFIED.**
`OpenRouterProvider.cpp:311` now `std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock)`;
loser returns empty with `OpenRouterError::Busy` (`:312-318`), logging `busy` as a failure. The
Busy branch returns *before* `cancel_requested_.store(false)` (`:320`), so it neither clears a
pending cancel for the in-flight holder nor arms one — the correct order (Verified by reading
`:311-320`). Holdership still spans the whole `transport_.Post(request)` (`:356-358`) plus
post-Post cancel check (`:387`), preserving at-most-one-Post-in-flight and the cooperative
cancel contract. `OpenRouterError::Busy` is additive (`OpenRouterProvider.h:24,50-51`; existing
ordinals unchanged); `grep LastOutcome()/Busy` over `src/` shows **zero production callers** of
the enum, so Busy is observability-only and every existing caller (cleanup empty→raw,
command/transform empty→`LlmFailed`+restore) is unaffected. Concurrency tests pin both
directions: `serializes concurrent process calls` (second joins empty/Busy before release,
exactly one wire request) and `second process does not wedge while first post is hung`
(second returns empty/Busy in `<5000ms` with `RequestCount()==1`, first then succeeds) — both
fail against the old blocking lock by construction (second would park on `mutex_` until the
10s gate deadline). GateTransport expiry (10s) is a failure bound, not a sleep-to-pass.
`TrackingTransport` removal is dead-code cleanup of a test premised on the old wedging
behavior; no production consequence.

**Fix B — cancel threading + timeout constants. VERIFIED (values intentionally unchanged).**
`OpenRouterContract.h:35` `kDefaultTimeoutMs = 8000` is byte-identical to baseline; the diff
does not touch it, `SettingsStore.cpp:35` (`cleanup_timeout_ms` default 2000), `main.cpp:394`
(provider timeout sourced from the constant) or `main.cpp:453` (cleanup timeout from schema).
So the 8000-vs-2000 budget mismatch flagged in R-W5-001 is **not** value-aligned in this
delta — the wedge is removed at the mutex layer (Fix A) while per-caller budget coherence is
left to a transport-cancel seam (`IHttpTransport` has no cancel token; `WindowsHttpClient`
uses blocking WinHTTP). This is the correct reading of the delta: `DictationCleanup.cpp:193-197`
(2000ms `wait_for` → `RequestCancel()` → 3s drain) still returns raw at ~5s, but the worker
thread occupancy is bounded by the HTTP timeout and a *cross-caller* second `Process`
(worker-queue-free paths such as command/transform threads) now fails fast with Busy instead
of parking behind the first. A second cleanup task still queues behind the hung Post on the
single-threaded `WorkerQueue` (`WorkerQueue.h:10-24`) — pre-existing, unchanged by the delta,
and honestly the residual NEED-4. Mid-Post cancel semantics otherwise unchanged: stale-cancel
reset (`:320`) + pre-Post check (`:327`) + post-Post check (`:387`) all intact, pinned by the
pre-existing `stale cancel does not poison next process` and `cancel during post is observed`
cases (both green in the rerun).

**Fix C — Privacy/About exhaustive egress (R-W5-002). VERIFIED.**
Privacy (`MainWindow.cpp:1337-1340`): *"No telemetry. By default no network calls except
explicit model downloads. Cloud speech transcription and OpenRouter cleanup are the only
opt-in exceptions, both off by default. Each sends only what its feature needs, and only
while you have it enabled."* About (`:1378-1381`): *"Processing stays on this machine unless
you opt into cloud speech transcription or OpenRouter cleanup."* The false "only OpenRouter"
scope is gone from both. Checked against STT-cloud reality: `stt_provider`
(`gnani|elevenlabs|muse`) + `stt_consent_accepted` + per-provider key, enforced by
`EvaluateCloudConsent` (`ConsentGate.cpp:63-87`) and `main.cpp:463-504`, defaults
`stt_provider=local` / `stt_consent_accepted=false` (`SettingsStore.cpp:47-48`), sends audio
clips only while enabled — matching "only while you have it enabled". The three conditional
"Cleanup uses OpenRouter … when cleanup is on" warnings (Cleanup/Privacy/About) are accurate
in isolation and untouched. Exhaustiveness sweep: `grep "No telemetry|no network|stays on
this machine"` over `src/` shows these two sites are the only unconditional egress claims;
the only other network paths are explicit model downloads (stated on Privacy) and the
loopback legacy `remote_endpoint` (`localhost:50051`, not third-party egress). No
exact-string pins for the old sentences exist in `tests/` (copy-lint suite green in the
rerun scope). Wording introduces no banned whole-words/`!` per the lint constraints.

**Fix D — error mapping (R-W5-004/006) + serialization (R-W5-005). VERIFIED.**
`StatusToCategory` now maps `504→Timeout` alongside `408` (`:68`) *before* the `>=500→Server`
arm (`:77`), and `412→InvalidRequest` explicitly (`:74`). `error_type` precedence is unchanged
(`ClassifyFailure:227-228`). `ParseSuccessBody` checks non-null `choice.error` *before*
content extraction (`:274-284`), reusing `ExtractErrorType`/`ExtractErrorDetail` over a
one-member wrapper — fail-closed (empty, typed outcome), partial `message.content` is never
returned when `error` is present. `request_id` is taken from the top-level `id` (`:249-252`)
before the branch, so it survives. `BuildRequest` is now invoked inside `try/catch`
(`:335-352`) mapping serialization throws (nlohmann `dump()` on invalid UTF-8/OOM) to
`Malformed` + empty. Test coverage added matches each mapping: status-table rows
`504→Timeout` and `412→InvalidRequest` (`test_openrouter.cpp:187,191`), choice-error with
partial content → `Server` (`:226-233`), choice-error with null content → `RateLimited`
(`:236-243`), invalid-UTF-8 input → `Malformed` with zero wire requests (`:246-253`).

**Regression checks (keep defaults / network surface / redaction). ALL CLEAN.**
- *Local mode untouched*: diff touches no schema, no `main.cpp` wiring, no store/path code;
  defaults remain `llm_provider=local` (`SettingsStore.cpp:69`), `stt_provider=local` (`:47`),
  `stt_consent_accepted=false` (`:48`), `use_remote=false` (`:41`). Copy changes never name a
  new default.
- *No new network surface*: diff adds no host/path/header/socket/WinHTTP token (grep of the
  two touched `src/` files → zero hits; `openrouter.ai` literal remains confined to
  `OpenRouterContract.h`, outside the diff); `check_no_network.sh` allowlist byte-stable,
  gate green.
- *Redaction intact*: every new detail path goes through `RedactDetail` (Busy is a constant
  string; choice-error vendor text at `:282` and serialization `what()` at `:341,348` are
  redacted and 500-char capped). No key/transcript reaches logs (log line is
  `provider/status/error/latency/detail-redacted`, success silent). Pre-existing redaction
  test (`redacts key from failure detail`) green in the rerun.

## 2. Claims audit (IMPR-W5-01 read after blind pass)
| IMPR claim | Verdict | Evidence |
|---|---|---|
| R-W5-001 fixed via try_lock single-flight + Busy; timeout-value alignment rejected (main.cpp:394/:453 + schema outside write-set; needs ILlmProvider change) | YES, with the stated residual | Fix A verified in code + hung-test. Value sources confirmed outside the delta (`main.cpp`, `SettingsStore.cpp` — neither in diff). Residual worker occupancy disclosed as NEED-4 in §10. |
| R-W5-002 Privacy/About exhaustive | YES | Fix C wording quoted in §1; matches ConsentGate-enforced STT reality; copy-lint green. |
| R-W5-004 504→Timeout, 412→InvalidRequest explicit, pinned | YES | Fix D rows + rerun `maps vendor statuses` green. Note (see §3): "504" here is the R-W5-004 *bug* name (code previously classified 504 as Server); the fix maps 504→Timeout per the live typed table, consistent with brief wording once read against REV-W5-01. |
| R-W5-005 dump-wrap → Malformed, invalid-UTF-8 pinned | YES | Fix D try/catch + `hello \xFF world` test, green. |
| R-W5-006 choices[0].error fail-closed + typed, 2 tests | YES | Fix D branch + both choice-error cases, green. |
| HeatLevel (R-W5-003) deferred — StudioPalette.h:75-90 + test_overlay_settings_ui pins outside write-set; no duplicate classifier added | YES, deferral accepted | Confirmed: definition is in `StudioPalette.h`, call site in `Display.cpp:211`; delta diff contains neither. Carrying as NEED is preferable to a divergent copy-paste classifier. Not reopened. |
| R-W5-007/008/009 deferred as DO-LATER / out-of-scope | AGREE | All three are explicitly DO-LATER or NIT in REV-W5-01; no delta behavior depends on them. |
| Gates: configure/build/ctest/hygiene/no-network green; doctest 23/137 | MATCHES rerun (scoped) | Reran `*openrouter*` → 23/23, 137/137; hygiene/no-network OK; `lwf-tests` ctest 1/1 passed (full suite capped by tool timeout). |
| Behavior change disclosed: second caller now fails fast (Busy) instead of late success | ACKNOWLEDGED | Verified: `serializes` test rewritten to the gated form; callers' empty-fallback covers Busy identically. No production caller branches on `Busy`, so no latent ABI/behavior hazard. |

## 3. New findings in the delta (residuals, none blocking)
### D-01 [MINOR] choices[0].error with no `error_type` on HTTP 200 classifies `Unknown`, discarding the embedded status code
- Location: `OpenRouterProvider.cpp:274-284` (fallback `StatusToCategory(response.status)` with `status==200` → `Unknown`).
- Evidence: a vendor embed `{"code":502,"message":"…"}` *without* `metadata.error_type` lands in `Unknown`, not `Server`, even though `code` carries the status. The top-level 200-with-error path (`ClassifyFailure`) has the same shape pre-existing. Fail-closed either way (empty → caller fallback), so this is observability-only.
- Suggested follow-up (DO-LATER, not required): also consult `error.code` when `error_type` is absent; add one code-only embed test. No code change in this delta.
### D-02 [MINOR] Busy overwrites `LastOutcome` while the in-flight call still runs
- Location: `OpenRouterProvider.cpp:313-317` (`StoreOutcome` on the Busy path, outside `mutex_`).
- Evidence: the in-flight call's terminal `StoreOutcome` overwrites Busy afterward, so a reader polling `LastOutcome()` after both calls sees the in-flight terminal (often `None`), not Busy; the Busy record survives only in the warning log line. Same race shape as any terminal write; harmless today because **zero** production callers read `LastOutcome()`. Disclosed in IMPR-W5-01 §6; accepted as-is.
### D-03 [NIT] Empty input while another request is in flight reports `Busy`, not `None`
- Location: `OpenRouterProvider.cpp:311-319` (try_lock precedes the empty-text early-out at `:321`).
- Evidence: `Process("", sys)` during a hung Post stores `Busy` instead of the usual `None`. Both are empty; no caller distinguishes them. No change required — noting only so a future metrics reader is not surprised.

## 4. Dimension checklist
- Correctness: delta changes are correct and minimal; fail-closed posture preserved on every new branch.
- Root cause vs symptom: Busy fail-fast is root-cause at the provider mutex (the seam the `ILlmProvider` contract exposes); transport-level cancel is correctly deferred, not faked with retries/sleeps.
- Regression/compat: additive enum value only; no schema/config/data-format change; defaults and local path byte-stable; no interface change.
- Concurrency / idempotency: single-flight preserved; cancel flag untouched by the loser; no new thread-safety requirement on `IHttpTransport`.
- Error handling: mapping fixes are precedence-preserving; serialization honors the never-throws-for-wire-failures invariant.
- Security / abuse: no new egress, no new log surface, redaction on all new details; no key/transcript leakage introduced.
- Performance: O(1) Busy path, O(body) parse unchanged; no pools/queues/retries added.
- Migration / rollback: config-free revert (4 files).
- Observability: Busy adds one honest outcome; D-01/D-02 residuals limit fidelity only.
- Maintainability: reuses existing helpers (`ExtractErrorType`/`ExtractErrorDetail`, `ClassifyFailure` shape); no new abstraction, no comments added (hygiene green).

## 5. What I could not verify
- Live authenticated OpenRouter happy path / emit of a real `choices[0].error` embed (no key; contracts remain VERIFIED-from-docs / UNVERIFIED-against-live).
- Windows runtime concurrency (two UI threads racing the provider) — verified via doctest gate fakes only.
- Full `ctest` to completion in this session (re2 `-fa` suites exceeded the tool cap); scoped `lwf-tests` + full `*openrouter*` doctest cover the delta surface, and IMPR-W5-01's full-ctest claim is taken as reported, not rerun.
- MSVC-only warnings (CI lane); build is pre-existing MinGW.
