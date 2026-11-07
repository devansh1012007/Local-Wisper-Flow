# Brief — W2-IMPL-E1 (implementer) · wave 2 · run 20261107-studio

## 1. Role and mission
Mission: engine stats wiring — StatsStore::Record after successful injection only (plan P12), with TextDiff words_corrected + PersonalDictionary dictionary_fixes counts (P13/P14). Root cause: zero stats plumbing exists; hook must be at the LIVE injection site.
Role: implementer.

## 2. Required reading (in order)
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; never break existing dictation behavior; tests unchanged except where incorrect; hermetic fixtures)
3. docs/agent-ops/20261107-studio/00-mission.md, 01-work-ledger.md
4. Approved plan: §§C (hook at DictationEngine.cpp:475, exclusions :458/:463-472/:527/CommandModeController:134), D (stats data path: TextPipeline::ProcessResult overload), E (P12-P14), H, I.
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-enginehooks.md — INCLUDING corrections: HistoryStore::Append has ZERO prod callers (do NOT mirror it blindly); WindowsClipboardInjector is dead (live technique = WindowsSelectionCapture:366-437 — NOT your area, Wave 2 E2); engine does NOT have app context or duration today (duration exists at capture timestamp :379 discarded as (void) — you must plumb it in, see §3).
6. Wave 1 reports: reports/w1-impl/W1-IMPL-02B-T-00510.md (StatsStore API), W1-IMPL-02A (TextDiff), W1-IMPL-02C (Enforce count overload).

## 3. Task / acceptance (T-011)
- DictationEngine.cpp: `StopAndTranscribe` after `injector_.InjectText(final_text)` succeeds (path past :475), before `CompleteToIdle`, call `StatsStore::Record(...)` — EXACTLY there, never Command/PasteLast (structural exclusions). Engine gets an optional StatsStore* (nullable in tests; never construct one inside DictationEngine ctor — main wires it).
- App/duration plumbing (the plan's R-07 gap): app name + duration are NOT available at :475 today. Plumb them in from where they live: foreground process via the existing OS capture path (find how DictationEngine knows current app — search for AppContextResolver usage / getpid / capture start time; capture timestamp_now at CaptureStart if not already). Process name via AppContextResolver category-first match (StatsStore needs process_name; category defaults from T-010 in Wave 1).
- TextPipeline: additive `ProcessResult{text, words_corrected, dictionary_fixes}`; old `Process()` delegates. words_corrected via TextDiff LCS between raw and cleaned; dictionary_fixes from PersonalDictionary::Enforce count overload (Wave 1 fix: single-pass; your hook uses the count, never re-runs Enforce).
- Record exclusions (all with tests): cancelled capture, empty final_text, failed injection (exception path), command mode, PasteLast, transform runs, scratchpad target — call `StatsStore::Record` zero times. Existing engine tests must pass UNCHANGED (prove with ctest diff).
- Stats toggle `keep_usage_stats` (default ON per plan §4.1): when OFF, Record() is a no-op (StatsStore already supports SetUsageStatsEnabled — wire the config key through main.cpp? NO — main.cpp is outside your write-set this wave; expose the bool and post NEED for main.cpp wiring (exact lines) so the toggle can be wired in Wave 2 E3's integration if not already done).
- Tests (extend tests/test_engine*.cpp or new tests/test_stats_wiring.cpp — pick the existing engine-test file that covers StopAndTranscribe; do NOT create parallel suites): new cases proving zero-record for each exclusion, one-record-with-correct-words/corrected/fixes on success, toggle-off no-op, app/duration recorded correctly.
Acceptance: exclusions proven by tests; existing engine tests unchanged (cite git diff -- tests/ shows no expectation edits); lwf_tests scoped green for engine+stats filters; hygiene OK.

## 4. Ownership
WRITE-SET: src/core/DictationEngine.{h,cpp}, src/core/TextPipeline.{h,cpp}, src/core/StatsStore.{h,cpp} (accessor fixes only if needed; do NOT change its storage format), tests/test_engine*.cpp (only the file covering StopAndTranscribe — check CMakeLists.txt:134-167 for the exact name), tests/test_stats_wiring.cpp (if new)
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt, scripts/check_no_network.sh, src/main.cpp, src/ui/**, src/os/**, docs/agent-ops (report only), AGENTS.md.

## 5. Siblings this wave
W2-IMPL-E2: transform run flow (DictationEngine transform mode + Clipboard technique via WindowsSelectionCapture + UiBus StartTransform + main loop arm — SEQUENCED around StopAndTranscribe; check its brief for its seam — if your file intersects, post CONFLICT immediately) · W2-IMPL-E3: scratchpad target + A1 ApiKeyStore + integration pass owns main.cpp. Your StatsStore calls must be defensive (nullable store = no crash in existing engine tests).

## 6. Traps
Do NOT hook main.cpp:193 RunEngine (dead code, zero call sites). Do NOT hook PasteLast (:527). Do NOT log transcript. Do NOT invent app-context capture — reuse the existing OS foreground-app source if any, else post NEED. No code comments.

## 7. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests (safe if build dir free — wait/retry once on lock); ctest -R lwf-tests -V or filtered engine/stats; bash scripts/check_hygiene.sh. Never full ctest (shared gate owns it).

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w2-impl/W2-IMPL-E1-T-011.md (ORCH-DIGEST first). Commits explicit paths only; trailers Task: T-011 / Agent: W2-IMPL-E1 / Wave: 2 / Report: <path>. Final <=8 lines.
