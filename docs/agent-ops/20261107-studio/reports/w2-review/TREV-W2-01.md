# W2-TREV-01 — Wave 2 test review (read-only on source)

## ORCH-DIGEST

**Verdict: FAIL — 1 P0 (397-line transform suite never runs in CI), 6 P1 gaps, 10 mutation survivors (§6).** Scope `git diff 3571116..HEAD -- tests/` [VERIFIED `git diff --numstat` -> 4 files, 1134 insertions / 0 deletions: `test_engine.cpp` 343, `test_transform_runflow.cpp` 397, `test_scratchpad_target.cpp` 176, `test_apikeystore.cpp` 218]. Case census [VERIFIED `Select-String TEST_CASE`]: test_engine 28 total (14 new stats/pipeline + 14 pre-existing), test_transform_runflow 11 `TEST_CASE` (+7 `SUBCASE`), test_scratchpad_target **8** (E3 report claims 9 — §5 TB-6), test_apikeystore 9. [RAN `./build/lwf_tests.exe -tc="*stats store*,*stats record*,*pipeline*,*engine works*,*scratchpad*,*apikey*,*transform*" -> 45/45 PASS, 229 assertions, 3x identical, exit 0; full `ctest --test-dir build` -> 1/1 PASS, 379/379 cases, 13358 assertions, 21.37s] — but those 45 **exclude every runflow case** (§2.2, §5 TB-1): [RAN `-tc="*transform success*...*transform request*" -> 0 cases / 0 assertions / "SUCCESS"`; `--list-test-cases | Select-String "transform success|restores clipboard"` -> empty]. `CMakeLists.txt:137-186` lists `test_apikeystore.cpp`/`test_scratchpad_target.cpp` but **not** `test_transform_runflow.cpp` — the file is tracked at HEAD (`ea22607`, [VERIFIED `git show --stat HEAD`]) yet dead. Two strongest runnable suites: engine stats exclusions (Generation+Snapshot dual pin) and apikeystore precedence; weakest: scratchpad source-scan (16 string-existence checks, no behavior) and the unrun runflow file. **Fail-before evidence: none recorded for any of the 4 files** (§3). **P1s prioritised in §7**; highest after P0: transform stats-exclusion untested, transform injector-throw restore untested, scratchpad "never-elsewhere" vacuous, redaction prefix-echo survivor.

---

## 1. Method & evidence base

- Read all 4 test files in full plus units under test: `src/core/{DictationEngine,TextPipeline,ApiKeyStore,ConsentGate,Redact,NotesStore,StatsStore,TextDiff}.cpp/.h`, `tests/{Fakes,EngineHarness}.h`, `src/main.cpp` wiring, `CMakeLists.txt:137-190`. Judged assertions as behavior-pins, not implementation restatements; traced each exclusion to its early-return in `StopAndTranscribe`/`HandleTransform`.
- Implant reports `W2-IMPL-{E1,E2,E3}-*.md` used only as hypotheses; every green claim re-derived against the HEAD binary. E1/E2 green runs predate interleaving and, for E2, predate CMake registration altogether (standalone `g++` binary, [VERIFIED W2-IMPL-E2 §4]) — not CI evidence.
- No build, no source edit (read-only mandate). All runs are filtered invocations of the existing `build/lwf_tests.exe` (HEAD `ea22607`) plus `ctest`. Mutation analysis (§6) is static (assertion-vs-implementation) — no mutants executed.
- Report template: `TREV-W1-01.md` shape (brief references a "review-tests template" not present in `templates/`, which contains only `STANDING-ORDERS.md`).

### Runnable vs. in-tree census

| File | In-tree cases | In `lwf_tests` binary | Filter evidence |
|---|---|---|---|
| test_engine.cpp (new 14) | 14 | YES | `*stats store*,*stats record*,*pipeline*,*engine works*` -> 24 cases incl. neighbours, 114 asserts |
| test_transform_runflow.cpp | 11 (+7 sub) | **NO — P0** | `*transform*` -> 5 cases (transformstore/manager only); runflow filters -> 0/0 |
| test_scratchpad_target.cpp | 8 | YES | `*scratchpad*` -> 9 cases (8 + 1 engine stats), 58 asserts |
| test_apikeystore.cpp | 9 | YES | `*apikey*` -> 9 cases, 47 asserts |

## 2. Per-file review

### 2.1 `test_engine.cpp` — new stats cases (14) — STRONG, two masking nits

- **Behavior quality: STRONG.** Success pins `Generation==1`, dictations 1, words 2, corrected/fixes 0, `speaking_ms==120` (deterministic 4×30ms FakeClock), `apps["unknown"]==2` [test_engine.cpp:335-357]. Capture-time attribution is genuinely pinned: slack→outlook flip after press, outlook absent, `by_category["Work messages"]==2` [359-396]. Dictionary path pins corrected==fixes==1 with injected-text equality [398-422]. Pipeline unit cases pin the `replaced+inserted+deleted` definition via ScriptedLlm-inserted "brave" (1) and identity (0/0) [424-461] — see M-9.
- **Exclusions: STRONGEST in wave.** Cancelled capture, cancel-during-transcription, empty final, failed injection (plus `InjectFailed` code), command mode (injects yet 0 record), paste-last (Generation stays 1 after 2nd inject), scratchpad flag, toggle-off, null-store — each asserts `Generation()==0`/`Snapshot().empty()` (or the stays-1 variant) [463-615]. Dual `Generation`+`Snapshot` pinning is correct: `Record` bumps generation only on persist success [StatsStore.cpp:171-173], so Snapshot alone would miss a persist-fail mutation and Generation alone would miss a words miscount.
- **Fail-before: NONE recorded** (§3). E1 §5 "mutation mental" lists the right mutants but cites no red run.
- **Hermeticity: ADEQUATE, not parallel-safe (LOW).** `StatsPath(name)` uses fixed per-case names in the shared temp root with remove-before/after [37-41, each case]. `.tmp` sidecars (`StatsStore.cpp:289`) never removed — harmless (Load reads only `path_`) but not self-cleaning. Two concurrent `lwf_tests` processes with the same filter would interleave on the same filenames. `Snapshot().size()==1` is midnight-safe here (single record; DateKey is wall-now via `timestamp_local=0`, [StatsStore.cpp:151-155]) — the W1 midnight concern does not apply.
- **Durations:** `speaking_ms==120` exact equality is brittle only to the Harness default `Speak(4)` — correctly deterministic, acceptable.
- **Nits:** (a) empty-final case uses `stt.transcript=""` only; whitespace-only final (`"   "`) is untested and would exercise the same guard via normalization — see §7 P1-5. (b) E1 report claims "17 new cases" but the diff contains 14 (§5 TB-6); the two pipeline unit cases bring the §5 enumeration to 14, not 17.

### 2.2 `test_transform_runflow.cpp` (11 cases, +7 subcases) — GOOD on paper, **DEAD in CI (P0)**

- **Behavior quality (in source): GOOD.** Success pins inject, capture/restore counts, clipboard restoration, exactly one `<selection>` wrap, the "data, not instructions" sentence, and the 40s retention boundary both sides (40000 valid / 40001 expired) [109-142] — the `>` vs `>=` off-by-one is pinned both directions (M-8). Validation pins `< 3x+200` both sides (205 ok / 206 rejected for input "hi") plus three preamble families [175-237]. Opt-in-off pins zero clipboard/LLM contact [239-252]; disabled+invalid slot both report "not available" [254-270]; custom `{text}` single-wrap pins `CountOccurrences==1` [272-288]; LastText + FSM-IDLE preserved [290-309]; cancel-during-LLM via `HookLlm::during` restores and discards [311-327]; restores-every-branch loop covers 4 scenarios [329-361]; no-UI-block measures `RequestTransform<250ms` and `UiBus::PollCommand<250ms` against a 600ms-blocking LLM with completion `>=500ms` [363-397] — the last is the wave's only real-thread timing test.
- **P0 (TB-1):** none of this executes. [VERIFIED `CMakeLists.txt:181-185` omits the file; `git diff 3571116..HEAD -- CMakeLists.txt` adds only ApiKeyStore.cpp + 2 test files]. E2 flagged the CMake NEED [W2-IMPL-E2 §5] and E3 explicitly deferred it ("NOT added … ONLY two test files", [W2-IMPL-E3 §4 Phase 3]) — at HEAD `ea22607` the integrator committed the test file without the one-line registration, so `ctest` green (379/379) overstates transform coverage. Doctest returns SUCCESS on zero matches, masking the gap in filtered runs.
- **Fail-before: NONE in CI.** E2's standalone 11/104 green [W2-IMPL-E2 §4] proves compilation, not regression protection. One review-caught fix (prompt `{text}` recursion, advancing-index repair) [W2-IMPL-E2 §3] never had a recorded red run — the custom-prompt test pins the fixed behavior only post-fix.
- **Determinism (if registered): MEDIUM RISK.** The no-UI-block case uses real `std::this_thread::sleep_for` (ScriptedLlm `delay_ms=600` holds its mutex during the sleep, [Fakes.h:212-229]) and wall thresholds `<250ms`. On a loaded CI runner a scheduler hiccup can exceed 250ms without any regression — classic timing flake shape. Thresholds are generous (2.4× headroom vs 600ms block) but unproven: no CI run of this case exists to observe. The 500-iteration × 10ms poll loop (5s ceiling) is the right pattern; thresholds could be widened or split into a deterministic "RequestTransform never calls Process synchronously" check (call-count sampled inside the blocking LLM) plus a loose wall-clock check.
- **Error-path gaps (if registered):** injector-throw restore (M-16), whitespace-only selection (M-17), null-deps `ModelMissing` [DictationEngine.cpp:709-712], busy `dictation busy` [697-700], and transform stats non-record (zero `StatsStore` references in the file [VERIFIED grep]) are all untested — see §7.

### 2.3 `test_scratchpad_target.cpp` (8 cases) — MIXED, weakest behavior proof

- **Buffer/injector: GOOD.** Append/clear/copy-isolation, empty no-op, newline separator, `Text()` copy semantics [37-53]; injector appends two lines [55-66]. Real `ScratchpadBuffer`/`ScratchpadInjector` (mutex-guarded, [NotesStore.cpp:525-550]) — no mocking of the unit. Missing: append of text already ending in `\n`, embedded newlines, concurrent Append/Text (mutex unproven), injector-level `InjectText("")`.
- **Engine routing: VACUOUS "elsewhere" (TB-2/TB-3, P1).** `"writes only to the buffer"` constructs a `RecordingInjector elsewhere` that is never wired to anything — `elsewhere.InjectionCount()==0` passes under any implementation including one that also writes to the system clipboard (M-14). `"receives … nowhere else"` builds the engine with `ScratchpadInjector` as the constructor argument and checks the untouched `Harness` injector is 0 [68-84] — tautological: it proves the constructor honored its argument, not that production routing picks the scratchpad or that no second sink exists. What *is* proven, well: second dictation appends as a new line, `LastText` updated, status "injected", stats-flag pair (flag-true → Generation 0 [86-103]; flag-false → Generation 1 [105-121]) — the flag-off case usefully pins that the *flag*, not the injector type, controls `Record` [DictationEngine.cpp:611].
- **Bus/schema: GOOD.** UiBus delivery + single-consume [123-130]; `scratchpad_flow_bar` default OFF + hot-apply + persist + `keep_usage_stats` ON [159-176].
- **Source-scan: WEAKEST TEST (P1).** `"overlay button wakes the badge…"` asserts 16 substrings across 4 source files [132-157] — `SetScratchpadVisible`, `InvisibleButton("##note"`, `poll_note_hover_ = poll_note_hover`, etc. It passes if the tokens exist anywhere (comment, dead code, wrong hit-test) and fails on any rename — pure implementation restatement. It does not prove geometry, visibility gating (`scratchpad_visible_`), pass-through expansion, or that the main consumer (`full.SetVisible(true)` placeholder [W2-IMPL-E3 §4]) routes anywhere. House precedent exists (`test_overlay_settings_ui.cpp:353,433` reads `src/ui/Overlay.cpp` the same way), so this is a house-pattern weakness, not a new sin — but 16 string checks for one button is high cost for zero behavioral coverage. CWD fragility (TB-5): relative `ReadSource("src/ui/…")` passes only because CTest sets `WORKING_DIRECTORY` to source root [CMakeLists.txt:190]; `./build/lwf_tests.exe` from `build/` fails `REQUIRE_FALSE(overlay_h.empty())`. The `LWF_SOURCE_DIR` compile definition [CMakeLists.txt:188] exists precisely to avoid this and is unused here.
- **Fail-before: NONE recorded.**

### 2.4 `test_apikeystore.cpp` (9 cases) — STRONG on precedence, THIN on redaction & error paths

- **Behavior quality: GOOD.** Store-wins [60-71], empty-store→env [73-85], missing-file→env-only [87-100] prove precedence in both directions against the single env table (`ApiKeyEnvFor`, [ConsentGate.cpp:45-61]). Round-trip pins overwrite, `.bak` existence, Remove, reload-persistence, and openrouter as a reserved key [102-130]. Corrupt→`.bad` [147-159], non-object/non-string + empty-string ignored [161-179], unknown-key preservation across save [181-197] mirror sibling-store contracts. `ResolveApiKey` ordering (stored non-empty wins, `getenv` only if `ApiKeyEnvFor` non-null) is exactly the implemented priority [:50-61].
- **Redaction: PARTIAL (P1).** One case [199-218] constructs log lines by hand and checks `RedactSecret`/`RedactAuthValues` remove the full key bytes. It does **not** capture any production log line (no `Log` sink, no `SttAttemptStats::detail` path), so a call-site mutant logging `api_key` raw (M-4b) survives; and it checks full-key absence only, so a prefix-echo mutant (`sk-` + `[redacted]`) survives (M-4). `RedactSecret` with a key that is a substring of innocent text, or a key appearing twice, is also untested. The helpers are correct ([Redact.h:8-74]); the *contract* (every stored/logged line redacted) is untested at the sink.
- **Error paths: THIN (P1).** No persist-fail rollback (`Set` false + memory==disk), no `Reload()`-picks-external, no `Remove(missing)==true` / `Set(missing,"")==true` idempotence, no `.bak` content/overwrite-once pin (existence only), no empty-file → `.bad` vs missing distinction, no concurrency. File fixture correctly uses per-case `TestDir(name)` + `remove_all` [15-21] (best hermeticity in wave alongside scratchpad).
- **Env hygiene: LEAK (P2, TB-4).** `ClearAllEnvs()` clears only the 3 cloud env vars [39-43] and never restores the caller's original values. The openrouter case sets `OPENROUTER_API_KEY` [138] which `ClearAllEnvs` never clears — it leaks into later cases in-process (invisible only because `ApiKeyEnvFor("openrouter")==nullptr`). Any future env-sensitive test after this file inherits clobbered process env (developer with a real `GNANI_API_KEY` exported also loses it). Sequential doctest makes this deterministic today; graceful save/restore (snapshot `getenv` before, restore after) is the fix.
- **Fail-before: NONE recorded.**

## 3. Fail-before evidence — summary

| File | Fail-before recorded? | Evidence |
|---|---|---|
| test_engine.cpp (stats) | NO | E1 green runs only (49/49 scoped, 19/19 narrow); "mutation mental" without a red log |
| test_transform_runflow.cpp | NO | E2 standalone 11/104 green pre-integration; `{text}`-recursion fix review-caught without red run |
| test_scratchpad_target.cpp | NO | E3 scoped 30/30 green; CMake link-fix red predates the tests (not a test fail-before) |
| test_apikeystore.cpp | NO | E3 scoped green; corrupt/.bad behavior never shown failing |

Standing Order 10 ("must fail without the fix") is evidenced in **0 of 4 files**. The W1 wave demonstrated the standard (W1-FIX-* red logs); Wave 2 agents recorded only post-fix greens. For new-feature suites this is recoverable cheaply: one revert-and-rerun per file (delete the `Record` call; delete one `RestoreClipboard`; swap `ResolveApiKey` order; flip the scratchpad flag) would satisfy the order — see §6 for exactly which reverts must flip which cases.

## 4. Determinism / isolation / flakiness

- [RAN combined new-filter 3x (45/45, 229 asserts each) + `*scratchpad*` 9/9 + `*apikey*` 9/9 + `*stats store*...` 24/24 + full suite 379/379, 13358 asserts, exit 0 each]. No flake observed in-session for runnable files. Runflow flake is **unknown** (0 CI runs).
- **Fakes vs. seams (acceptable, stated limits):** fake clipboard (`FakeSelectionCapture`), ScriptedLlm, RecordingInjector, FakeClock are the correct seams (unit under test is never mocked). Limits, not defects: the clipboard fake models a string-only clipboard synchronously — it cannot catch OS multi-format loss (M-5, prod `WindowsSelectionCapture` preserves `CF_UNICODETEXT` only [W2-IMPL-E2 §2]) or Ctrl+C poll timing; `HookLlm`/`on_transcribe` synchronous hooks are deterministic and well-chosen. `ScriptedLlm` sleeping with its mutex held is safe only because no `CallCount()` is sampled during the block — fragile if extended.
- **Timing:** only TB-8/runflow no-UI-block uses wall-clock thresholds (see §2.2). Everything else is FakeClock/ProcessOne-driven — exemplary.
- **Isolation:** apikey/scratch per-case dirs (GOOD); engine stats fixed file names (LOW, §2.1); env mutation without restore (P2, §2.4); test artifacts left in temp root after each case (house pattern, acceptable).

## 5. Test bugs found (structural)

1. **[P0] Unregistered suite masking (TB-1).** `test_transform_runflow.cpp` tracked but absent from `CMakeLists.txt:137-186`; 0 cases run, doctest "SUCCESS" on 0 matches. Structural signature: `git ls-files tests/*.cpp` vs. CMake source list diff must be empty — grep-able gate. Fix: add the one line, rebuild, rerun (also closes E2's DEFERRED-DEPENDENCY). **Nothing in §2.2/§6 transform coverage can be claimed until this lands.**
2. **[P1] Vacuous elsewhere-spy (TB-2).** `RecordingInjector elsewhere` never wired; passes under any code. Same family as W1's vacuous atomic-crash case (TREV-W1-01 §5.5).
3. **[P1] Tautological nowhere-else (TB-3).** `h.injector` never passed to the engine under test; 0 is structural, not behavioral.
4. **[P2] Env leak (TB-4).** `OPENROUTER_API_KEY` set [138] never cleared (not in `ClearAllEnvs` [39-43]); no save/restore of pre-existing values.
5. **[P2] CWD-dependent source scan (TB-5).** Relative paths; passes only via CTest `WORKING_DIRECTORY`. Use `LWF_SOURCE_DIR`.
6. **[LOW, docs] Report count mismatches (TB-6).** E1 "17 new" vs 14 in diff; E3 "9 scratchpad" vs 8 `TEST_CASE`. Tests unaffected; wave bookkeeping should reconcile.
7. **[LOW] Parallel-unsafe fixed stats filenames (TB-7).** Per-case names deduplicate within one process but collide across processes.
8. **[MEDIUM, latent] Wall-threshold test unproven (TB-8).** Runflow no-UI-block `<250ms` assertions have never run in CI; risk is flake, not false-pass.

No hex-escape, unparseable-fixture, forgotten-seed, or non-movable-store patterns (W1 §5 classes) recur in this diff.

## 6. Mutation thinking — 19 plausible bugs, caught / surviving / unrun

"CAUGHT (source)" = at least one assertion in the file flips; "SURVIVES" = no assertion flips; "UNRUN" qualifies every transform row in CI today (TB-1).

| # | Mutant | Verdict | Why / why not |
|---|---|---|---|
| M-1 | `Record` moved before empty-final return | **SURVIVES** (masked) | Engine returns before Record anyway for empty; StatsStore also ignores `words==0` [StatsStore.cpp:147-150] — double guard means the engine-level empty test cannot distinguish the two guards. Generation stays 0 either way. Deleting the *engine* empty guard alone is caught (injects `""`, `HasInjected` flips); deleting the *store* guard alone survives. |
| M-2 | `RestoreClipboard` deleted in LLM-throw catch | CAUGHT (source) / UNRUN (CI) | `restore_count==1` + clipboard equality [159-173] flip. |
| M-3a | Preamble match made case-sensitive (no lowercasing) | CAUGHT (source) / UNRUN | All 3 preamble tests use capitalised inputs ("Sure/Here/Certainly") vs lowercased candidates — removing the fold [DictationEngine.cpp:74-98] makes them parse as valid results and inject → caught. |
| M-3b | Preamble matched only for capitalised inputs (lowercase `"sure thing"` missed) | **SURVIVES** / UNRUN | No lowercase/word-boundary-accept case ("Surely …" must pass, `"sure"` alone must fail) exists. Boundary chars (`space : , !`, end) half-pinned (`!`, space) but `:`/`,`/end untested. |
| M-4a | `RedactSecret` echoes key prefix (`sk-[redacted]`) | **SURVIVES** | Test checks full-key absence + `[redacted]` presence only [199-218]; prefix bytes pass. |
| M-4b | Call-site logs `api_key` raw (helper untouched) | **SURVIVES** | No production log line is captured; redaction is tested on hand-made strings only. |
| M-5 | Clipboard image/other formats lost on capture/restore | **SURVIVES** (structural) | Fake clipboard is string-only; prod `WindowsSelectionCapture` is text-only by construction. No test can observe image loss — accepted limitation should be stated, not silently assumed. |
| M-6 | Cap `>= 3x+200` instead of `>` (206 accepted) | CAUGHT (source) / UNRUN | 205-ok / 206-rejected pair [220-236] pins `<` (prod `result.size() >= input*3+200`, [100-108]). |
| M-7 | `Enabled()` consulted after slot check (opt-in-off still captures) | CAUGHT (source) / UNRUN | `capture_count==0` + `CallCount==0` [239-252] flip. |
| M-8 | Retention `>= 40000` instead of `>` (expired at exactly 40s) | CAUGHT (source) / UNRUN | 40000-valid / 40001-expired pair [137-141] pins `>` (prod [TextDiff.cpp:163]). |
| M-9 | `words_corrected` = `replaced` only (insertions invisible) | CAUGHT | ScriptedLlm "brave" insertion expects 1 [450-461]; replaced-only yields 0. |
| M-10 | App captured at release instead of press | CAUGHT | slack→outlook flip expects slack only [359-396]. |
| M-11 | `Record` before Inject try/catch (failed injection records) | CAUGHT | Failed-inject expects Generation 0 + `InjectFailed` [517-534]. |
| M-12 | Scratchpad flag ignored (remove `!scratchpad_target_`) | CAUGHT | Two independent cases (engine [583-598], scratchpad [86-103]) expect Generation 0. |
| M-13 | `ResolveApiKey` checks env first | CAUGHT | Store-wins [60-71] flips. |
| M-14 | `ScratchpadInjector` also writes clipboard/system | **SURVIVES** | No clipboard spy; elsewhere-spy vacuous (TB-2). Closed by construction in prod (single `Append`, [NotesStore.cpp:548-550]) but unobserved. |
| M-15 | `HandleTransform` calls `stats_->Record` | **SURVIVES** / UNRUN (double) | Zero stats refs in runflow file; command-mode case is only a proxy. No transform+stats case exists anywhere. |
| M-16 | `RestoreClipboard` deleted in injector-throw catch | **SURVIVES** / UNRUN | No transform injector-fail case (`fail_inject` never set in runflow file); restores-loop covers success/no-selection/LLM-throw/preamble only — the 5th branch [DictationEngine.cpp:752-758] is the one untested. |
| M-17 | Whitespace-only selection accepted (remove `TrimCopy` check) | **SURVIVES** / UNRUN | Only `nullopt` tested [144-157]; fake returns `"   "` verbatim, prod [726] would restore+report. |
| M-18 | `.bak` never written / `.bad` not renamed | PARTIAL | `.bad` existence [157] and `.bak` existence [113] caught; `.bak` *content* and *overwrite-once* (W1 standard) unpinned → content-swap mutant survives. |
| M-19 | Duration `now_ms==0` fallback / negative clamp removed | **SURVIVES** | No zero-stamp (Tick-path) or backwards-clock case; store double-clamps negatives [StatsStore.cpp:156], masking engine-clamp removal. |

**Survivors (runnable files): M-1 (partial, masked), M-3b, M-4a, M-4b, M-5, M-14, M-18 (partial), M-19.** **Survivors (unrun file, all rows): M-15, M-16, M-17 plus every transform "CAUGHT" row in CI.** Classes: double-guard masking (M-1/M-19), case/boundary edges (M-3b), secret-handling sinks (M-4a/b), unobservable seams (M-5/M-14), missing branches (M-15/16/17).

## 7. Missing edge cases — prioritized, with sketches

### P1

1. **Register runflow suite + transform stats exclusion** (closes TB-1, M-15). One-line CMake fix, then:
   ```cpp
   // in test_transform_runflow.cpp
   StatsStore stats(path, true); // temp file per StatsPath pattern
   engine->SetStatsStore(&stats);
   // success transform -> CHECK(stats.Generation()==0 && stats.Snapshot().empty());
   ```
   Plus `TEST_CASE("transform with no stats store attached")` for the null path.
2. **Transform injector-throw restore** (M-16): `fx.h.injector.fail_inject=true; fx.Run(1); CHECK(restore_count==1 && clipboard restored && errors[0]==InjectFailed && !HasTransformDiff())`. Add as 5th block in `restores clipboard on every branch`.
3. **Scratchpad production routing** (TB-2/TB-3, M-14): replace the vacuous spy with a clipboard-observing fake (`RecordingInjector` wired as the *system* injector) OR, cheaper, mark the routing decision as main.cpp's responsibility and test `main.cpp`'s selection logic via a small factory the tests can call. Do not keep a spy that observes nothing.
4. **Redaction at the sink** (M-4a/b): capture the actual `Log`/attempt-stats line (or the strings `ConsentGate`/`ApiKeyStore` emit) and assert: full key absent, **first 4 bytes absent**, `[redacted]` present; key appearing twice; `Authorization` header via `RedactAuthValues` (already covered) plus an `api_key=` query-style line if any prod path emits one.
5. **Stats duration/whitespace edges** (M-1/M-19): whitespace-only final text (`stt.transcript="   "`) → injects nothing, Generation 0; zero-stamp path (hands-free Tick with FakeClock at 0, or direct `now_ms==0` reasoning via existing harness) → `speaking_ms==0`, still records; test documents the store's negative clamp as the backstop.
6. **Preamble boundary pins** (M-3b): accept: `"Surely this is fine"`, `"Here's the thing"` (contraction, not `"here is"`); reject: lowercase `"sure thing"`, `"sure"` (end-of-string), `"CERTAINLY: …"` (colon). Each as a `SUBCASE` in the existing validation case.

### P2

7. **Apikeystore error paths:** `Set` to unwritable dir → false + `Get` unchanged (rollback); `Reload()` after external edit; `Remove(missing)==true`; empty file → `.bad` (vs missing → no `.bad`); `.bak` byte-content + never-overwritten-twice (W1 dictionarystore standard); keys with JSON-special chars (`"a\"b\nc"`) round-trip.
8. **Transform null/busy/whitespace branches:** `selection_==nullptr` → `ModelMissing`; `transform_llm_==nullptr`; `RequestTransform` while FSM != IDLE → "dictation busy"; selection `"   "` → restore + "Select some text first" (M-17); two queued `RequestTransform` calls coalesce to one run (single-exchange semantics).
9. **Env hygiene:** snapshot/restore env around every case; add `OPENROUTER_API_KEY` to `ClearAllEnvs` (or delete the var in the openrouter case); assert a pre-set env value is restored after the suite.
10. **Source-scan hardening:** build paths from `LWF_SOURCE_DIR`, and replace at least the poll-assignment check with a behavioral overlay test (hover → `poll_note_hover_` → pass-through off → click posts `OpenScratchpad`) — or accept the string check explicitly as an inventory test and drop 10 of the 16 substrings.

### P3

11. Scratchpad: trailing-`\n` append, concurrent Append/Text (100× append + read, exact final line count), `InjectText("")` no-op.
12. Stats: process name case (`"SLACK.EXE"` → `apps["slack.exe"]` via `NormalizeProcess`), resolver-null + window-set (process recorded, profile empty), `SetUsageStatsEnabled` toggle-on-after-off mid-process (main hot-apply path).
13. Pipeline: snippet-expansion counting definition (pins E1 FINDING: snippet edits count as `words_corrected`), guard-rejection counting (rejected LLM edit counts 0 — INV §10-4), whitespace-only raw (`ProcessWithStats("  ") == 0/0`, empty text).

## 8. Over-mocking / abstraction assessment

**No over-mocking of units under test.** Engine tests exercise the real `DictationEngine`+`TextPipeline`+`StatsStore` against OS seams (`ISelectionCapture`, `ILlmProvider`, `ITextInjector`, `IActiveWindow`, `IClock`) — the correct seam choices, matching the CommandModeController pattern. Stores use the real filesystem. The one abstraction failure is inverse: the scratchpad source-scan (§2.3) mocks the compiler by string-matching it, and the "elsewhere" spy mocks an observer that observes nothing. `FakeSelectionCapture` is a *simplification* (sync string clipboard) rather than an over-mock — legitimate at the seam, provided §7 P1-3 records what it cannot prove (M-5). Test-helper duplication (`TestDir`/`StatsPath`/`ReadFile` now in 4 files with fixed-vs-suffixed variants) repeats the W1 §8 finding; a shared `tests/StoreFixture.h` would fix TB-4/TB-7 class-wide.

## 9. Remaining risks / NOT-RUN

- **NOT-RUN: runflow suite in any form this session** (0 cases in binary). E2's standalone run is cited, not reproduced (rebuilding it would need a CMake-adjacent compile outside the read-only mandate; it also predates HEAD's interleaving). **All transform §2.2/§6 "CAUGHT" claims are source-level until TB-1 is fixed and the suite re-runs.**
- **NOT-RUN: mutation reverts** (read-only) — M-1..M-19 are static. M-15/16/17 sketches are ready to drop into the suite.
- **NOT-RUN: real clipboard/focus/overlay on Windows hardware** (manual checklists own this; fakes prove the seam contract only). G3 zig pending (pre-existing).
- **Pre-existing uncommitted diffs** (`01-work-ledger.md`, `COMMS.md` modified; `briefs/`, `w1-review/`, E1 report untracked) are orchestrator-owned, not reviewed here.

## 10. Long-run test (standing order 8)

- **Source:** the wave's best patterns (dual Generation+Snapshot pinning, both-sides boundaries, capture-time attribution, flag-vs-injector decoupling) are exactly what made W1 suites strong. The worst patterns (vacuous spy, string-scan-as-behavior, unregistered file) are the same "test cannot fail" family as W1 §5 — recommend the wave-close rule: every test name must be falsifiable by reverting the unit's primary behavior *and* appearing in `--list-test-cases`.
- **Load:** no unbounded loops; the 600ms LLM block + 5s poll is bounded. `Diff` O(n·m) per dictation unchanged from W1 acceptance.
- **Hostile:** apikeystore corrupt/non-object/unknown-key and transform oversize/preamble are well chosen; hostile *content* (huge key, deeply nested JSON, Unicode keys, whitespace selection) is the thin layer — §7 P2/P3.
- **Operability:** env clobbering (TB-4) is the only cross-test operability risk; fix with snapshot/restore.
- **Reversal:** 0 files have proven reversal (§3). One revert per file before wave close would make Wave 2 match W1's evidence standard.

---
*Reviewer: W2-TREV-01 (read-only on source; did not write the code). No source files edited. This report is the sole artifact.*
