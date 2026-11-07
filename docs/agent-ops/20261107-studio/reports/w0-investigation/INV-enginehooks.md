# INV-enginehooks — W0-INV-02 (engine hooks for P12–P16)

ORCH-DIGEST: All five plan pin-claims verified TRUE against `feature/studio-pages` @ 0e655ec (source rows below). Hook at DictationEngine.cpp:475 is the live success point (RunEngine is dead, zero callers); :458 command early-return, :463–472 cancel/empty, :527 PasteLast, :66 worker thread all confirmed. TextPipeline::Process (74–104) ordering cleanup→dictionary→snippets confirmed; Enforce has NO count today (P13 needed) and returns string-only (P14 needed). No DictationSpeedRecorder/StatsStore exists; HistoryStore::Append has ZERO production callers (RecordHistory defined only at main.cpp:125) — stats wiring is greenfield, not a migration. Transform clipboard technique lives in WindowsSelectionCapture (not the unused WindowsClipboardInjector). Scratchpad plugs at ITextInjector (single-method iface); no iface change needed. UiCommandType needs +StartTransform, +OpenScratchpad (8 existing).

## 1. Question and scope

Question: verify (a) the five DictationEngine.cpp pin-claims, (b) TextPipeline/PersonalDictionary pipeline order + fuzzy rule + fix-count gap, (c) existing stats collection + HistoryStore::Append call sites + data available at inject time for StatsStore::Record, (d) transform clipboard technique + UiCommandType additions, (e) scratchpad plug point in ITextInjector.
Scope: `src/core/DictationEngine.cpp/.h`, `src/core/TextPipeline.cpp/.h`, `src/core/PersonalDictionary.cpp/.h`, `src/core/HistoryStore.cpp/.h`, `src/main.cpp`, `src/core/CommandModeController.cpp/.h`, `src/core/UiBus.h`, `src/core/ITextInjector.h`, `src/os/WindowsTextInjector.cpp/.h`, `src/os/WindowsClipboardInjector.cpp/.h`, `src/core/DictationCleanup.cpp/.h`, `tests/Fakes.h`, `tests/EngineHarness.h`. Branch `feature/studio-pages` verified via git branch (0e655ec). Plan input: C:\Users\devan\.opencode\plan\2026-11-07-studio-pages-and-settings-gui.md §§C/D/E (P12–P16) read; brief is complete (no missing input).

## 2. Summary

- FACT: :475 `injector_.InjectText(final_text)` is the live success hook; structural exclusions (command :458, cancel/empty :463–472, PasteLast :527) all confirmed. Worker thread spawns at :66. [VERIFIED src/core/DictationEngine.cpp:66,458,463-472,475,527]
- FACT: Pipeline order is spoken-sentinel→cleanup→dictionary→snippets→NormalizeLines; inject is engine-side after pipeline returns (DictationEngine.cpp:461,475), never inside pipeline. [VERIFIED src/core/TextPipeline.cpp:74-104]
- FACT: `words_corrected`/`dictionary_fixes` are NOT available today — Process returns `std::string` only; Enforce returns `std::string` with no count. P13/P14 additive overloads are both required. [VERIFIED src/core/TextPipeline.h:21, src/core/PersonalDictionary.h:20]
- FACT: HistoryStore::Append has NO production call sites (only definition EngineBridge::RecordHistory main.cpp:125 + tests). Existing HistoryEntry already carries {timestamp, raw, text, app_profile, duration_ms, language}. [VERIFIED grep repo-wide: src/main.cpp:125 only; src/core/HistoryStore.h:9-16]
- INFERENCE (confidence: high): app + duration are NOT currently captured at the hook — engine has no app context (app_context lives main.cpp:335, only for paste-chord lambda) and `now_ms` param is `(void)`-discarded (:379); both need new plumbing before Record(app, duration) is possible.

## 3. Expected vs actual behavior

Expected (per plan C, P12–P16): stats hook at :475 records exactly successful dictations; transform run flow reuses command-mode clipboard technique on the worker thread; scratchpad is a new injection target.
Actual (all verified):
- :475 is the ONLY InjectText in StopAndTranscribe's success path; exclusions are structural (command branch returns at :458 before pipeline :461; cancel/empty return :463–472 before :475; PasteLast is separate method :516–532, inject at :527; CommandModeController injects separately at :134). RunEngine contains src/main.cpp:193,254 but has ZERO call sites (grep `RunEngine\(`: 1 match, the definition). Hooking only :475 therefore records exactly successful dictations — plan claim HOLDS.
- Command-mode `Run(raw, 8000)` is called at :446 with an 8000ms timeout — same-thread synchronous call inside StopAndTranscribe (blocking the worker, not the UI thread). Transform flow must follow the same pattern (see §4).
- History is unwired: EngineBridge::RecordHistory (main.cpp:125) is never called by DictationEngine or RunEngine — history.jsonl will be empty in production until T-011 wires a call. This is a pre-existing gap the hook must close, not extend.
- WindowsClipboardInjector (the "clipboard technique" injector in os/) is DEAD code: grep finds only its own definitions (WindowsClipboardInjector.h:9-13, .cpp:170,184,191), never instantiated in src/. Live injector is WindowsTextInjector (main.cpp:333). The live selection/clipboard technique is WindowsSelectionCapture in WindowsTextInjector.cpp. [VERIFIED grep WindowsClipboardInjector: 0 instantiation sites; src/main.cpp:333 constructs WindowsTextInjector]

## 4. Affected files and flows

| File:line | Role | Verified claim |
|---|---|---|
| src/core/DictationEngine.cpp:66 | `worker_ = std::thread([this]{Run();})` | Worker thread spawns here — CONFIRMED |
| src/core/DictationEngine.cpp:79-90 | `Run()` loop pops events → HandleEvent + Tick | StopAndTranscribe executes on worker thread in prod |
| src/core/DictationEngine.cpp:440-459 | Command mode branch; `command_->Run(raw, 8000)` at :446; `return` at :458 | Command early-return BEFORE pipeline — CONFIRMED |
| src/core/DictationEngine.cpp:461 | `pipeline_.Process(raw)` | Pipeline runs before inject |
| src/core/DictationEngine.cpp:463-472 | Cancel check :463; empty check :468 (`final_text.empty()`) | Cancel/empty early-return — CONFIRMED |
| src/core/DictationEngine.cpp:475 | `injector_.InjectText(final_text)` in try/catch (InjectionException :476, std::exception :480) | LIVE SUCCESS HOOK — CONFIRMED |
| src/core/DictationEngine.cpp:516-532 | `HandlePasteLast()`; `InjectText(text)` at :527 | PasteLast is separate method, never reached from :475 — CONFIRMED |
| src/core/DictationEngine.cpp:378-379 | `StopAndTranscribe(now_ms, timeout_fired)`; both `(void)`-cast | now_ms is discarded — duration needs clock_ |
| src/core/DictationEngine.cpp:385,393,399-402 | `full` PCM vector; `min_tap_samples` guard; `speech_ratio` from chunk_count_/speech_chunk_count_ | Sample counts available; speech_ratio exists but is unused for stats today |
| src/core/DictationEngine.cpp:403,404,461 | `raw` (STT transcript, streaming or one-shot) → pipeline → `final_text` | raw + cleaned both in scope at :475 |
| src/core/DictationEngine.cpp:351-352 | `press_ms_ = now_ms; capture_start_ms_ = now_ms` in BeginCapture | Duration origin available; end time must use clock_.NowMs() |
| src/core/DictationEngine.cpp:165-167 | `SetCommandController(CommandModeController*)` | Command controller injected post-ctor (main.cpp:492) — transform controller can follow same setter pattern |
| src/core/TextPipeline.h:21 | `std::string Process(const std::string&)` | String-only return — P14 overload required |
| src/core/TextPipeline.cpp:74-104 | Process body | Order: NormalizeWhitespace :75 → spoken ToSentinels :79 → cleanup_.Process :86 → ExpandSentinels :93 → dictionary_.Enforce :96-98 → snippets_.Expand :100-102 → NormalizeLines :104 |
| src/core/PersonalDictionary.h:20 | `std::string Enforce(const std::string&, bool)` const | No count output — P13 additive out-param required |
| src/core/PersonalDictionary.cpp:206-228 | `EditDistance` Levenshtein; `rows*cols > 64*64` → 999 | Fuzzy uses EditDistance==1, both strings >= 6 chars (:263-276) |
| src/core/PersonalDictionary.cpp:230-284 | `Enforce` body | Exact case-fix :257-261; fuzzy :263-276; replacements first :233-246 |
| src/core/DictationCleanup.h:21 | `std::string Process(const std::string&)` | Cleanup is string-in/string-out; words_corrected must be computed outside (diff of raw vs cleaned) or via new overload |
| src/core/HistoryStore.h:9-16 | `HistoryEntry{timestamp, raw, text, app_profile, duration_ms, language}` | Existing stats shape already carries raw/cleaned/app/duration |
| src/main.cpp:125 | `EngineBridge::RecordHistory` → `history_.Append(entry)` | Defined, NEVER called (grep repo-wide) — greenfield wiring |
| src/main.cpp:453-457 | `HistoryStore history(...)` ctor (enabled/keep_raw/max_entries/max_age_days from config) | History store constructed but idle |
| src/main.cpp:335-343 | `AppContextResolver app_context` + paste-chord provider lambda | App identity exists ONLY in main.cpp; engine has no access |
| src/core/EngineTypes.h:20-31 | `EngineSettings` (no app field) | No app context flows into engine today |
| src/core/CommandModeController.cpp:73-148 | `Run(instruction, timeout_ms)` | Transform template: WorkerQueue+future :97-101, timeout :103-111, preamble guard :125, InjectText :134, RestoreClipboard :143-144 |
| src/core/CommandModeController.cpp:60-71 | `BeginSelection()` → `capture_.CaptureSelection()` | Selection capture entry point for transform |
| src/os/WindowsTextInjector.cpp:366-437 | `WindowsSelectionCapture::CaptureSelection()` | Save clipboard :370-386; Ctrl+C SendInput :388-402; poll ≤300ms, 20ms sleep :404-436; returns selected iff != saved :431 |
| src/os/WindowsTextInjector.cpp:439-470 | `RestoreClipboard()` | Restores saved CF_UNICODETEXT |
| src/os/WindowsClipboardInjector.cpp:191-249 | `InjectText`: save formats :73-97, set CF_UNICODETEXT :229-231, 30ms settle :243, Ctrl+V :244, restore after restore_delay_ms (default 300) :246-247 | Dead code — do NOT copy as P15 template; use SelectionCapture + live injector |
| src/os/WindowsTextInjector.cpp:339-364 | `WindowsTextInjector::InjectText` | Live path: InjectViaClipboard :352 → typing fallback :360 |
| src/core/UiBus.h:10-19 | `enum class UiCommandType` (8 values) | P15/P16 need +`StartTransform`, +`OpenScratchpad` here |
| src/main.cpp:541-577 | UiBus drain: `PollCommand` loop with `switch(command.type)` | Synchronous main-loop drain — LLM work must NOT run here (plan red #7) |
| src/core/ITextInjector.h:5-10 | Single-method iface `InjectText(const std::string&)` | Scratchpad plug point: new impl/decorator, NO iface change |
| src/core/DictationEngine.h:89 | `ITextInjector& injector_` (ctor :49; wired main.cpp:489) | Main chooses impl at wiring; scratchpad target = alternate impl or decorator + routing flag |

Call flow (verified): Hotkey → engine.PostEvent (main.cpp:587-590) → engine worker Run() :79 → HandleEvent :281 → HandlePress :306 → BeginCapture :350 (press_ms_ set :351) → STT (:411 streaming or :422 one-shot) → command? return :458 → pipeline :461 → cancel/empty :463-472 → InjectText :475 → last_text_ :488 → CompleteToIdle :491.

Transform run flow (verified template lines): CommandModeController::Run :73-148 already implements copy→LLM→validate→inject→restore on the CALLING thread (worker, since called from StopAndTranscribe :446). P15's flow (save→Ctrl+C→300ms→LLM→validate preamble/3x+200→paste→restore, 40s retention) maps to SelectionCapture :366-437 (300ms poll :407-410) + WorkerQueue timeout :103-111 + StartsPreamble :13-36 + InjectText :134 + RestoreClipboard :143.

## 5. Existing patterns and conventions to follow

- Engine-thread blocking LLM with timeout: CommandModeController.cpp:97-111 (WorkerQueue + packaged_task + future.wait_for + RequestCancel + 3s drain). DictationCleanup.cpp:188-197 uses identical pattern (timeout_ms_ default 2000, DictationCleanup.h:32). [VERIFIED]
- Clipboard save→copy→poll→restore: WindowsTextInjector.cpp:366-437 (300ms bound, 20ms poll). [VERIFIED]
- Setter-injection of optional collaborators: DictationEngine::SetCommandController (DictationEngine.h:62, main.cpp:492 wiring) and SetToneFeedback (:63) — transform/scratchpad controllers can follow; ctor deps are refs (DictationEngine.h:85-93). [VERIFIED]
- Test harness pattern: tests/EngineHarness.h:59-70 (MakeEngine + PressAndSpeak via PostEvent + ProcessOne) and RecordingInjector (tests/Fakes.h:167-208, `injected` vector, InjectionCount, LastInjected). T-no-record must extend this (inject count + Record call count per path). [VERIFIED]
- No-code-comments rule and test-fixture conventions per AGENTS.md (doctest, temp_directory_path). [VERIFIED AGENTS.md]

## 6. Interfaces, invariants, and contracts that must be preserved

- ITextInjector: `virtual void InjectText(const std::string&) = 0` (ITextInjector.h:9). Single method — scratchpad target is a new impl, not a new method. [VERIFIED]
- ISelectionCapture: `CaptureSelection() = 0; RestoreClipboard() = 0` (CommandModeController.h:9-14). P15 must reuse, not duplicate. [VERIFIED]
- IEngineSink: only `OnStatus` + `OnError` (IEngineSink.h:10-11). Stats Record is NOT on this iface today — P12 must add it (either extend IEngineSink or a new collaborator wired via EngineBridge/main.cpp:459,489). [VERIFIED]
- UiBus drain is synchronous on main thread (main.cpp:541-577); R10/UI-freeze constraint means StartTransform handler must only enqueue to engine (cf. hotkey path main.cpp:587-590), never run LLM inline. [VERIFIED]
- TextPipeline ctor takes `DictationCleanup&` (TextPipeline.h:12); dictionary/snippets/spoken are nullable raw pointers (:24-27) guarded before use. New overload must preserve this. [VERIFIED]
- HistoryEntry JSON keys: `app`, `dur` written only when non-empty/>0 (HistoryStore.cpp:87-98,133-137); loader reads same keys (:53-58). [VERIFIED]

## 7. Hidden coupling and downstream effects

- Dual dictionary load: `PersonalDictionary dictionary` loads once (main.cpp:369-370) and `pipeline.SetDictionary(&dictionary)` (:389); cloud keyterms reuse `dictionary.Words()` (:414,425,433). D-OP2 shared-instance requirement: page edits need reload on the SAME object or pipeline + keyterms go stale independently. [VERIFIED]
- App identity is only consulted inside the paste-chord lambda (main.cpp:339-343, via CreateActiveWindow → app_context.Resolve). Engine never sees process/title. Stats app field requires NEW IActiveWindow capture (either at BeginCapture in engine or in main.cpp loop) — no existing pipe. [VERIFIED]
- Cleanup determinism: DictationCleanup::Process short-circuits when disabled/no-LLM/below min_words (DictationCleanup.cpp:179-186) returning trimmed input; ApplyGuards (:158-177) may reject to raw (counted in GuardRejections() :136-138). words_corrected (P14) must define whether guard-rejections count as 0. [VERIFIED]
- Streaming path changes what `raw` is: streamed result (:410-413) vs one-shot (:422). Stats raw/cleaned pair must use the same `raw` var (not re-derive), or streaming/non-streaming diverge. [VERIFIED]
- Tick() can also invoke StopAndTranscribe (:586 hands-free, :590 max-recording) on the worker loop — hook at :475 covers these too, but Reentrancy: Record() is called on worker thread → StatsStore needs mutex (plan P05 already requires it). [VERIFIED]
- RecordingInjector fakes record injects but NOT sink calls beyond OnStatus/OnError — T-no-record needs a Record-counting fake. [VERIFIED tests/Fakes.h:167-208]

## 8. Test landscape

- Engine tests using ProcessOne (sync, no worker needed): test_command.cpp, test_errors.cpp, test_resilience.cpp, test_streaming.cpp, engine_test.cpp (EngineHarness.h:59-70). They pin RecordingInjector::InjectionCount / LastInjected. T-no-record (cancel/empty/cmd/pastelast/transform/scratchpad → 0 records) must extend harness with a Record spy. What exists: inject-count assertions. Missing: any Record/HistoryStore assertion in engine tests (RecordHistory uncalled → no test could exist). [VERIFIED]
- Pipeline tests: test_appcontext.cpp:91, test_spoken.cpp:63 (TextPipeline construction with ScriptedLlm). Enforce count (T-count) and ProcessResult (T-zero-when-no-cleanup) are greenfield. [VERIFIED]
- History tests: test_history.cpp:46-156 exercise Append/Search/Export directly (not via engine). They prove Append works when called. [VERIFIED]
- Commands to run: `ctest --test-dir build` (doctest suite) + `scripts/check_hygiene.sh` + `scripts/check_no_network.sh` (AGENTS.md gates G1/G2/G4). Run status: NOT-RUN by investigator (full Windows build needs configured deps; see §9).

## 9. Baseline results

- NOT-RUN (investigator, read-only scope + Windows full build requires configured dependency tarballs per AGENTS.md). Searched who would answer: any W1/W2 implementer must run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build -j4; ctest --test-dir build; scripts/check_hygiene.sh; scripts/check_no_network.sh` before/after T-011..T-013.
- What WAS verified by direct source read + grep (see §4) on branch feature/studio-pages @ 0e655ec (git branch/log verified, status clean except untracked docs/agent-ops/). No pre-existing failures observed via read-only means. [VERIFIED shell git branch/log/status]

## 10. Risks (ranked, with evidence)

1. App/duration for Record do not exist at hook — plan P12 Record contract implicitly assumes data that engine lacks (app_context only in main.cpp:335; now_ms discarded :379). Evidence high. Any T-011 design must add IActiveWindow capture + clock-based duration or drop those fields.
2. Transform "clipboard technique" misdirection: WindowsClipboardInjector looks like the pattern but is dead (0 instantiations, grep). Copying it duplicates ~80 lines and risks divergent restore semantics vs live SelectionCapture. Evidence high.
3. History greenfield trap: EngineBridge::RecordHistory exists (main.cpp:125) and HistoryStore ctor is wired (:453) — appears connected, but no caller exists. A T-011 that hooks Record() only (not History) leaves history.jsonl permanently empty; a T-011 that hooks History silently changes a dead path without tests. Evidence high.
4. words_corrected definition ambiguity: guard-rejections (DictationCleanup.cpp:158-177) return raw — is that 0 corrected or N? T-diff must pin this or Insights WPM/accuracy math drifts. Evidence medium.
5. ProcessOne vs worker divergence: tests call HandleEvent synchronously (EngineHarness.h:64-70) while prod uses Run thread (:79). Code touching Tick/StopAndTranscribe must be correct in both callers (Tick thread-safety contract DictationEngine.h:44-53). Evidence medium.

## 11. Unknowns and how each could be resolved

- UNKNOWN: Intended StatsStore::Record signature (does it take raw? cleaned? counts? app? duration_ms?) — plan P12 says "Record after successful inject" and P14 supplies counts, but no struct defined yet (P05 is W1 parallel). Resolved by T-005 (StatsStore) landing before T-011; orchestrator must sequence.
- UNKNOWN: Whether TransformManager (throws on missing file, plan F: TransformManager.cpp:38) participates in P15 runtime or only TransformStore (P08). Resolved by T-008; engine-hook code must target TransformStore, not the throwing ctor.
- UNKNOWN: Scratchpad buffer ownership (NotesStore P09 vs in-memory ImGui buffer) for the ITextInjector impl. Resolved by T-009/T-013 design; interface allows either (single method).
- UNKNOWN: UiBus payload convention for StartTransform (slot id as string?) and OpenScratchpad (empty payload?). Resolved by P15/P16 implementers following the `payload == "1"` precedent (main.cpp:552) + StartLearnHotkey empty-payload precedent (:554).

## 12. Out-of-scope observations

- `RunEngine` (main.cpp:193) is dead CLI-era code containing :254 InjectText — plan already directs "RunEngine NOT touched — dead" (plan F). Not investigated deeper per scope discipline.
- `WindowsClipboardInjector` dead code (0 uses) — construction never happens; removal candidate. Not fixed (scope).
- `UiBus::ReadStatus` zero callers per plan C — not verified here (out of scope).
- `IOptions`/`quiet_speech_boost`: schema question (D-OP4) is W1/A4 domain, not engine hooks.

## 13. Scratch files created

none (all evidence gathered by direct source reads and grep; no reproduction scripts needed — claims are static and fully verifiable from source).
