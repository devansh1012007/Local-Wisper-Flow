<!-- ORCH-DIGEST:START -->
agent: W4-REV-01 | scope: Wave 4 — 5 pages + A2/A3/A4 | verdict: CHANGES-REQUIRED (1 BLOCKER)
findings: BLOCKER 1 | MAJOR 1 | MINOR 4 | NIT 2
claims_audit: 28 checked / 3 overstated (INT2 "integrated" leaves Snippets undrawn; usage_count chips empty as flagged; delete-all "all" overclaims)
band_aids_found: none (delete-all = partial delete + in-memory resurrection, see R-W4-002)
top_3_risks: Snippets page never constructed in main.cpp (placeholder in live build, tracked as N-5); Delete-all leaves history.jsonl/logs + all in-memory stores so next Save resurrects; deleted Dictionary entries keep enforcing via PersonalDictionary until restart/Refresh
optimizations: DO-NOW 0 | DO-LATER 2 | NOT-WORTH-IT 1
reversal_check: done
confidence: med-high (source-traced live paths + doctest reruns green; Windows runtime/G3 unverifiable here)
<!-- ORCH-DIGEST:END -->

# Code review — W4-REV-01

## 0. Method
Diff range reviewed: `git diff b541bff..HEAD -- src/ tests/ CMakeLists.txt scripts/` (docs excluded per scope).
[RAN git diff --stat b541bff..HEAD -- src/ tests/ CMakeLists.txt scripts/ -> 30 files, 8056 insertions, 26 deletions;
commits ad3085d, c9df9df, a8a7e6f, 095ef47, 53548cc, 76cf674, a2cbf64, 5c75638, 3c8ab32, 3b72804, 96fb22c, 6a87b39,
5a2a618, 43484d1, 44a59d3, acfaf21, 045b2de, fc96356, 6c73171, 67c955b, 46cdde4, acc7c5c.]
Files read in full: src/main.cpp, src/core/HotkeyBinding.{h,cpp}, src/core/SettingsStore.cpp (schema tail),
src/core/UiBus.h, src/os/FileDialogs.{h,cpp}, src/os/WindowsHotkeyListener.{h,cpp}, src/ui/MainWindow.{h,cpp},
src/ui/ScratchpadWindow.h (incl. ScratchpadRouteInjector) + .cpp (Refresh/Drain), src/core/TextPipeline.cpp,
src/core/SnippetStore.h, src/ui/pages/DictionaryPage.{h,cpp} (header helpers + Draw/Refresh/Delete),
src/ui/pages/SnippetsPage.{h,cpp} (ctor + delete helpers), src/ui/pages/ScratchpadPage.cpp (toolbar),
src/ui/pages/TransformsPage.h (ctor/SetHotkeyBindings/adapter), src/core/DictionaryStore.cpp (RecordUsage/merge),
src/core/PersonalDictionary.cpp (multi-format Load).
Order: blind pass (source only) → implementer reports in docs/agent-ops/20261107-studio/reports/w4-impl/
(W4-IMPL-D-T-016, -I-T-018, -T-T-019, -A2-T-023, -A3-T-024, -A4-T-025, -N-T-020, -S-T-017, -S2-T-017, -INT, -INT2).
Independent reruns: [RAN bash scripts/check_hygiene.sh -> hygiene: OK],
[RAN bash scripts/check_no_network.sh -> no-network: OK],
[RAN lwf_tests.exe --test-case="*Snippets*" -> 18 cases / 159 asserts PASS],
[RAN lwf_tests.exe --test-case="*Dictionary*page*,*dictionary page*,*DictionaryPage*" -> 14 cases / 130 asserts PASS].
No source edits (read-only). Full configure/build/ctest not rerun (scoped-check mandate; gate-w4-2 GREEN taken as
reported, 23/23). No Windows runtime, no zig G3.

## 1. Blind-pass findings (before claims audit)

**Snippet live path (top check) — expansion works; the page is what dies.**
[VERIFIED src/main.cpp: SnippetStore snippets; Load(snippets.json); SetMode; pipeline.SetSnippets(&snippets)] —
the pipeline's store instance is the same one the page must hold. [VERIFIED src/core/TextPipeline.cpp:102-104
ProcessWithStats: snippets_->Expand(cleaned) when Loaded()] runs BEFORE injection; engine injects the expanded
text via injector_.InjectText(result). Snippet expansion does NOT die at the injector — it is pre-injection text
processing, and the scratchpad router only diverts POST-expansion text to the ScratchpadBuffer when the editor is
focused [VERIFIED ScratchpadWindow.cpp:408-418 RefreshDictationActive = visible && focused]. So the live
expansion feature works TODAY for hand-edited snippets.json.
[VERIFIED git show HEAD:src/main.cpp | Select-String SetSnippetsPage|SnippetsPage -> zero matches] — but the
SnippetsPage is NEVER constructed in main.cpp (only Dictionary/Insights/Transforms/Scratchpad get Set.*Page calls).
DrawSnippetsPage falls to the Wave-3 null-guard → "Not available yet." So in the live build the Snippets rail
item is a dead placeholder: page code compiles, its 18 tests pass, and users cannot add/edit/see any snippet in
the app. See R-W4-001 (tracked orchestrator NEED N-5).

**Page routing (all 5) — dispatch is total and correct.**
[VERIFIED RenderPages switch: Page::Welcome/Home/Insights/Dictionary/Snippets/Transforms/Scratchpad/Settings all
cased] [VERIFIED Draw*Page each null-guard then delegate page_->Draw(scale), fallback chrome+stub]. 4 of 5 pages
are constructed with their stores in main.cpp (dictionary_store + legacy PersonalDictionary pair, usage_stats,
transforms + engine diff source, notes+store+ui_bus). Snippets is the only unwired one.

**Dictionary coherence — add/update OK; delete stale.**
[VERIFIED DictionaryPage.h SubmitAdd/SubmitUpdate: store Save() then dict->Reload()] so the pipeline's legacy
PersonalDictionary sees adds/updates immediately. Deleted entries: Draw → CommitExpiredPending → (header)
Remove+Save only, NO dict_->Reload() → deleted phrase keeps enforcing in the pipeline until manual Refresh or
restart. See R-W4-003.

**usage_count — chips empty, confirmed.**
[VERIFIED DictionaryStore::RecordUsage is defined (DictionaryStore.cpp:604) with ZERO callers in src/;
PersonalDictionary::Enforce (the dictation hot path) never calls it; the only writes are merge-max during
Load (DictionaryStore.cpp:103-104) and load/save of stored values.] Banner TopPhrases requires usage_count>0,
"Most-used" sort keys on it, per-row "Used N times" needs it. All three surfaces are permanently dead in the
live app. Implementer-flagged ("no producer yet") — accurate flag, still a dead feature. See R-W4-004.

**Recorders — persistence + conflict surface verified.**
Recorder writes SetString(target)+config.Save()+UpdateBindings → survives restart (config-backed, verified
main.cpp learned-chord block). ValidateRecorderChord enforces: needs a key, needs a modifier (Escape-only
excepted for Cancel), rejects conflicts vs registry excluding the target role [VERIFIED HotkeyBinding.cpp:256-284].
LoadBindings dedups conflicting bindings FIRST-WINS in key order ptt→handsfree→cancel→command→paste_last
[VERIFIED :387-401] — silent drop, no log. Keycap/chord conflicts vs dictation: learn mode suspends dictation
binding fire (A3 dispatch capture), so recording during dictation cannot double-fire; subset-conflict semantics
mean Ctrl+Shift and CtrlXXX are mutually exclusive bindings (conservative, safe). TextField staleness: see
R-W4-005. Parse/round-trip: F1-F9 (size-2), F10-F24 (size-3), single letters/digits, 0x; CanonicalName emits
"F10" etc. — round-trips OK. No bug there.

**Delete-all scope — does NOT touch config, but under-deletes and resurrects.**
[VERIFIED MainWindow.cpp DeleteLocalData: remove stats.json, dictionary.json, snippets.json, transforms.json,
notes/ dir, api_keys.json — config.json NOT touched (kept, per brief spec S-P1; A4 report states this)]. NOT
deleted: history.jsonl (HistoryStore, holds dictations; keep_raw stores raw text), logs/, models.json,
app_profiles.json. In-memory stores (DictionaryStore, SnippetStore, TransformStore, StatsStore, NotesStore,
HistoryStore) are NEVER cleared/reloaded → next Save of any of them resurrects its file INCLUDING the old
entries. Repro: add dict word → Privacy → Delete all → add another word → file resurrects with BOTH words.
User-believed-deleted data survives in RAM and on disk. See R-W4-002.

**AGENTS.md/hygiene** — hygiene + no-network scripts OK (rerun). windows.h/commdlg.h only in src/os/FileDialogs.*
(allowed, src/os/*). New-file comment scan: only `// namespace` closers + one example URL string (pre-existing
pattern). Zero Log:: calls in src/ui/pages (no transcript logging). No TODO/FIXME/HACK/sleep in the diff.
llm_provider OpenRouter disclosure present on Cleanup + Privacy + About pages [VERIFIED diff hunks], satisfying
the D2 rule. Schema additive-only (2 appended entries). F3 screenshots/DPI runtime unverifiable here.

## 2. Claims audit
| # | Implementer claim | Verified? | Evidence / finding |
|---|---|---|---|
| 1 | D: Dictionary page closed-loop, 14 cases/130 asserts | Yes | [RAN doctest *Dictionary* -> 14 pass/130 asserts] |
| 2 | D: SubmitAdd/Update reload PersonalDictionary without restart | Yes | [VERIFIED DictionaryPage.h:155,173 dict->Reload()] |
| 3 | D: NEEDs (CMake/MainWindow/font_sources) integrator-owned | Yes | All three subsequently integrated (INT report; CMake verified in diff) |
| 4 | I: Insights 14 cases/201 asserts, WPM floor/streaks/gap/month nav | Yes (suite green standalone per report; not re-run here — scoped) | No contradicting source evidence; InsightsPage ctor wired with usage_stats |
| 5 | I: Top-0.1% banned, "Not available yet." voice tab pinned | Yes (structural) | No contradicting evidence |
| 6 | T: Transforms 18 cases/197 asserts, header-inline portable logic | Yes (suite registered in lwf_tests per CMake diff; construction + SetHotkeyBindings wired in main.cpp) | [VERIFIED TransformsPage.h:398 SetHotkeyBindings; main.cpp calls it after recorder save] |
| 7 | T: transform chord registration absent (A3-owned), page owns recorder+viewer only | Yes | No Win+Alt+1..0 registration in main.cpp diff; documented limitation |
| 8 | A2: FileDialogs portable helpers + TU compile; link pending integrator | Yes | Integrated (CMake diff registers both targets); windows.h contained in src/os |
| 9 | A2: manual-entry fallback preserved, ValidateModelPath unchanged | Yes | [VERIFIED PickAndApplyModelPath validates then SetString+ApplyAndSave] |
| 10 | A3: learn mode generalized to 5 config keys, payload plumbing | Yes | [VERIFIED main.cpp BeginLearnMode(target)/TryTakeLearnedBinding(chord,target), unknown target ignored+logged] |
| 11 | A3: NEEDs main.cpp/CMake/tests integrator-owned | Yes | main.cpp wiring landed (76cf674+a2cbf64); test cases NOT found in diff (test_hotkey.cpp untouched) — see note below |
| 12 | A4: 54/54 keys scanned in MainWindow.cpp | Yes | Controls verified in diff (incl. previously-missing vad_threshold, min_hold_ms, streaming, stt_timeout_ms, llm_provider, quiet_speech_boost) |
| 13 | A4: quiet_speech_boost was dead pre-schema; schema registration fixes persistence | Yes | [VERIFIED SettingsStore.cpp:68 appended Bool key; toggle pre-existed] |
| 14 | A4: delete-all removes stats/dict/snippets/transforms/notes/api_keys, config kept per spec | Yes for listed files; No for "all" | history.jsonl/logs/models.json/app_profiles untouched + in-memory resurrection — R-W4-002 |
| 15 | N: Scratchpad page+editor, caret-aware dictation through buffer when focused, per-tab model | Yes | [VERIFIED RefreshDictationActive visible&&focused; RouteInjector 293-311; DrainDictation caret insert] |
| 16 | N: stats suppressed when scratchpad target (engine +2 lines) | Yes | [VERIFIED DictationEngine.cpp:641 !scratchpad_target_ gate; pre-existing line, correctly wired by SetScratchpadTarget] |
| 17 | N: editor own ImGui+GL context, CreateContext return kept | Yes | [VERIFIED ScratchpadWindow.h members; loop calls RenderFrame after full.RunOnce with context re-asserts in both] |
| 18 | S2: Snippets page 18 cases/159 asserts standalone-green, imgui-free header model | Yes | [RAN doctest *Snippets* -> 18 pass/159 asserts] |
| 19 | S2: NEEDs N-1..N-6 integrator/orchestrator-owned | Yes | N-1/N-2/N-3/N-4/N-6 resolved (CMake+MainWindow+font_sources verified in diffs); N-5 open |
| 20 | INT: gate-w4-1 GREEN 23/23 (679s), write-set respected | Yes (taken as reported; build+ctest not rerun here) | CMake hunks match report; forbidden paths untouched |
| 21 | INT: Snippets page missing upstream — no CMake ref (would break configure) | Yes | Confirmed: SnippetsPage.* absent at INT time; S2 later supplied it |
| 22 | INT2: Snippets suite in-target 18/159, compiles+links MinGW exe | Yes | [RAN doctest -> 18/159 in current build]; CMake registration verified |
| 23 | INT2: "MainWindow setter + font scan + CMake done; only 2-line main.cpp wiring (N-5) remains; stub fallback by design until then" | Yes — honest | [VERIFIED zero SnippetsPage construction in HEAD main.cpp]. The wave ships one page unusable; the NEED is documented, not hidden. See R-W4-001 |
| 24 | INT2: N-5 verify-only: main.cpp store wiring verified at 392-411 | Yes (store→pipeline part) | [VERIFIED Load+SetMode+pipeline.SetSnippets] — the flag's phrasing ("injection missing") is imprecise: expansion is pipeline-side and present; what's missing is page construction. See below |
| 25 | Implementers: "usage_count has no producer yet (chips empty)" | Yes — accurate flag | [RAN callers scan: zero callers of RecordUsage] — R-W4-004 |
| 26 | A3 note: test_hotkey.cpp cases outside integrator write-set, deferred to orchestrator | Unsupported (no tests added) | [RAN git diff -- tests/ shows NO test_hotkey.cpp change]. The recorder/conflict/persistence logic shipped without committed regression tests. Existing suite doesn't cover ValidateRecorderChord/FindConflict/multi-target BeginLearnMode. |
| 27 | N/INT: OpenScratchpad drain now opens editor with payload id (was only SetVisible) | Yes | [VERIFIED main.cpp: OpenNote(payload)/OpenNewNote] |
| 28 | INT: Snippets deferral is "upstream missing-page blocker, not integration failure" | Overstated at INT time | S2 subsequently supplied the page and INT2 integrated it; the remaining gap is only the main.cpp construction. Reasonable at the time, now technically moot — but still open (R-W4-001) |

N-5 wording correction (relevant to the task's top check): the integrator's "injection missing" conflates two
things. SnippetStore→pipeline injection EXISTS (expansion works live). What is "missing" is SnippetsPage
construction in main.cpp. Until R-W4-001's 2-line fix, shipping snippets requires hand-editing snippets.json.

## 3. Long-run test (independent answers)
Source: Each page consumes its store by reference and centralizes draw/dispatch; the recorder generalizes the
learn seam at its root (target-key plumbing + shared validation) rather than five one-off recorders. Shape good.
Load: Per-frame cost is one integer/bool/string read per control; the only re-entrant work is DrawGeneralHotkeyRow
calling LoadBindings (parse + sort of 5 chords) once per row per frame — O(5) tiny. Delete-all is O(files).
Hostile: malformed hotkey input cannot crash — Parse is total (rejects empties/lead-+/dups/unknown tokens),
ValidateRecorderChord never saves an unparseable or conflicting chord; LoadBindings dedup keeps the earliest
binding even if config is hand-corrupted with duplicates. GetOpenFileName-style picker is bounded (32768 wchars,
FILEMUSTEXIST); NormalizeModelPath trims/quotes/backslash-normalizes and ValidateModelPath still gates empty/500+.
Delete-all with missing files: std::filesystem::remove with error_code — no throw, silent idempotent. The
hostile gap is privacy, not robustness (see R-W4-002).
Operability: Recorder errors surface in-page (SetRecorderError → "That shortcut is already in use"); conflict
warning rows show for text-entered chords; Delete-all requires two-step confirm arm/disarm. Failure of the editor
window init only warns and continues [main.cpp scratchpad_editor.Init check]. No crash path found.
Reversal: Reverting Wave 4 = delete 5 pages + ScratchpadWindow/FileDialogs TUs + CMake lines + main.cpp hooks +
1 schema line; snippets.json/dictionary.json formats are versioned and read by legacy loaders (PersonalDictionary
parses schema_version 2 + entries, verified), so a pre-Wave-4 binary reading a Wave-4 dictionary.json still
works. Delete-all is irreversible by design (no undo, no backup) — called out in UI text.

## 4. Findings

### R-W4-001 [BLOCKER] SnippetsPage is compiled and tested but never constructed — live Snippets rail shows the stub
- Location: src/main.cpp — after `full.SetScratchpadPage(...)` (≈:541): constructions exist for Dictionary,
  Insights, Transforms, Scratchpad, but zero for Snippets. Evidence: [RAN git show HEAD:src/main.cpp |
  Select-String "SetSnippetsPage|SnippetsPage" → no matches] while MainWindow::SetSnippetsPage exists and
  the null-guard fallback renders "Not available yet." [VERIFIED MainWindow.cpp DrawSnippetsPage].
- Why it matters: every Snippets feature (view/add/edit/delete/undo) is unreachable in the shipped binary;
  snippet expansion itself works only for hand-edited snippets.json. This is the tracked orchestrator NEED N-5
  (INT2 report §A7 gives the exact 2 lines) — a known, small fix, but until it lands the wave's claim "5
  pages live" is false for 1 of 5. Recurrence risk: gates only know the page compiles + its doctest suite
  passes, so nothing in CI distinguishes this until a test asserts construction.
- Recommended fix: the 2-line main.cpp wiring from INT2 (include + `full.SetSnippetsPage(make_unique<
  SnippetsPage>(snippets))` with the existing pipeline-shared instance), plus a structure-scan test asserting
  main.cpp contains the SetSnippetsPage call through the same pattern the font_sources/studio scans already use.

### R-W4-002 [MAJOR] "Delete all local data" under-deletes and resurrects — history/logs kept; in-memory stores not cleared
- Location: src/ui/MainWindow.cpp DeleteLocalData. (a) history.jsonl [VERIFIED HistoryStore main.cpp:470] is not
  in the removal list, nor logs/, models.json, app_profiles.json — while history is the most sensitive file
  (DictationRecords incl. raw text when keep_raw_in_history). (b) No store is cleared/reloaded after removal:
  DictionaryStore, SnippetStore, TransformStore, StatsStore, NotesStore all re-Save their full in-memory state
  on later edits, resurrecting files WITH THE OLD ENTRIES. Repro: add dictionary word → Delete all → add a
  second word → dictionary.json reappears containing both words; the "deleted" word is back and enforcing.
  (config.json is correctly KEPT per brief spec S-P1 — [VERIFIED A4 report]; the bug is everything else.)
- Why it matters: privacy overclaim with security weight: a user who deletes "all local data" reasonably
  believes dictations/history are gone; they persist on disk and in RAM. This is not fixed by restart either —
  any Save before restart resurrects; after restart the resurrected file IS the data.
- Recommended fix: also remove history.jsonl (+ logs per ORCH spec decision), and after removal clear/reload each
  live store (SetEntries({})/Reload of empty file / explicit Clear on NotesStore+StatsStore) from the main.cpp
  owner, passing a "data reset" through UiBus; keep the two-step confirm.

### R-W4-003 [MINOR] Dictionary delete does not reload the pipeline's PersonalDictionary — deleted words keep enforcing
- Location: src/ui/pages/DictionaryPage.cpp:127 CommitExpiredPending → src/ui/pages/DictionaryPage.h
  CommitPendingDelete (Remove+Save, no `dict_->Reload()`), unlike SubmitAdd/SubmitUpdate (:155,:173) and the
  Refresh button (:194-196).
- Why it matters: "Deleted" appears in the page, but the pipeline's legacy instance retains the pair and
  TextPipeline::ProcessWithStats still Enforce()s it — dictation keeps spelling the deleted word until a manual
  Refresh click or restart. Undo-window commit is exactly the path that commits deletes.
- Recommended fix: pass PersonalDictionary* into CommitPendingDelete (or reload in Draw after a committed delete),
  mirroring SubmitAdd/Update.

### R-W4-004 [MINOR] usage_count has no producer — banner chips, "Most-used" sort, per-row usage all permanently dead
- Location: src/core/DictionaryStore.cpp:604 RecordUsage (zero callers; [RAN callers scan → only the definition
  and the declaration]). PersonalDictionary::Enforce has no Entry ids to bump; the legacy↔store phrase mapping
  doesn't exist.
- Why it matters: three shipping UI surfaces render as if data existed. Implementers flagged this honestly, but
  it should be tracked as a known-dead feature (or the chips/sort hidden) rather than background "no producer
  yet" drift. Fixing it means a phrase→id bridge from the enforce hit back to DictionaryStore — non-trivial
  (needs an enforce-time callback + id map), which is presumably why it was deferred.
- Recommended fix: wire RecordUsage via an enforce-hit callback keyed on normalized phrase, or gate the chips/sort
  UI behind usage availability until then.

### R-W4-005 [MINOR] TextField::Seed seeds once — hotkey text fields go stale after recorder changes
- Location: src/ui/MainWindow.h TextField (Seed sets seeded=true on first frame, never refreshes). Used by the
  "Other hotkeys" card (hotkey_handsfree/cancel/command/paste_last) and mic/stt/remote/cleanup fields.
- Why it matters: user records a new Hands-free chord → returns to the card → the text field still shows the
  OLD chord for the window's lifetime (ApplyAndSave persists the recorder value; the visible field lies).
  Editing the stale field then overwrites the recorded chord on DeactivatedAfterEdit.
- Recommended fix: re-seed when the setting differs from the buffer while the item is not active; or mark fields
  stale from SetLearnedHotkey.

### R-W4-006 [MINOR] Text-entered hotkey chords bypass validation; invalid/conflicting chords save and silently die
- Location: src/ui/MainWindow.cpp StringFieldRow for hotkey_* (no ValidateRecorderChord call on
  DeactivatedAfterEdit — unconditionally SetString+ApplyAndSave), while the recorder path DOES validate and
  reject. DrawGeneralHotkeyRow shows a "already in use" warning for a saved chord, but the save still happened;
  LoadBindings then silently drops the conflict loser (PTT wins by order, [VERIFIED HotkeyBinding.cpp:387-401]).
- Why it matters: same feature, two contracts — recorder users get rejection; text users get a dead hotkey with
  only a warning and no log. A typed typo (e.g. "Ctrl+Shift+") yields a nullopt binding that is saved as-is.
- Recommended fix: run ValidateRecorderChord against LoadBindings(registry) in StringFieldRow for the four non-PTT
  keys and block ApplyAndSave with the error message, mirroring the recorder path.

### R-W4-007 [NIT] Delete-all shows "Saved" toast (ShowSaved) after a destructive action
- Location: MainWindow.cpp Privacy Confirm-delete branch calls ShowSaved(). Misleading signal; should show a
  neutral confirmation or nothing. Cosmetic.

### R-W4-008 [NIT] Per-frame LoadBindings ×5 for conflict indicators — trivial, keep one frame of caching
- Location: DrawGeneralHotkeyRow (FindConflict + LoadBindings per row per frame). O(5) parse of short strings;
  negligible but pure waste; could cache per settings Generation. Below the fold — NOT-WORTH-IT to add
  invalidation machinery for this.

## 5. Attempts to break it
- Snippet expansion live path: (1) Set a snippet via?? — page unreachable (R-W4-001), so hand-write snippets.json
  with {"entry": {trigger, expansion}}; dictation of the trigger → pipeline Expand replaces it → injector types
  the expansion. Expansion is pre-inject (TextPipeline.cpp:103), so R-W4-001 does NOT starve expansion — it
  starves EDITING. (2) Dictate into focused Scratchpad editor: expanded text routes to ScratchpadBuffer and is
  inserted at caret — snippets expand INTO the scratchpad too (router is downstream of the pipeline). Outcome:
  expansion is live and correct; the only dead piece is the page.
- Page routing: PageForRoute for Dictionary/Snippets/Transforms/Scratchpad/Insights resolves to dedicated pages;
  null-guard means missing construction degrades to a stub, never a crash. 5/5 dispatch cases present (verified).
  Outcome: routing correct, construction 4/5.
- Keycap/chord conflict: set PTT="Ctrl+Shift" and Cancel="Ctrl" (subset) via config → LoadBindings keeps PTT,
  drops Cancel silently; recording the chord for a target whose binding was dropped still correctly conflicts
  with PTT (registry lacks the dropped binding but has PTT). Recorder with F2-only chord → rejected ("needs a
  modifier key") — by design; F-keys never count as modifiers.
  Attempted recorder escape hatch: BeginLearnMode twice quickly (MainWindow disables other Record buttons while
  any_armed; target overwritten otherwise) — guarded in practice.
- Recorder persistence across restart: recorder → SetString+Save → restart → LoadBindings parses the persisted
  chord → UpdateBindings at startup (existing main path). One gap: transforms.SetHotkeyBindings is updated only
  on the learn-save path [main.cpp]; a chord edited by hand later leaves TransformsPage's binding list stale
  until the next successful learn. Minor.
- Delete-all: attempted the resurrection repro on reasoning above (Save() dumps full in-memory entries:
  [VERIFIED DictionaryStore::Save → EntriesToJson(entries_)]). Also confirm config untouched (kept per spec) —
  delete of api_keys.json while api_keys (ApiKeyStore) holds entries → same resurrection class. Outcome: bug
  confirmed by code trace; not executed (no Windows GUI run here).
- F-key round trip: Serialize(0x74)=F10 → Parse("f10") → 0x74. Verified by code path (size-3 branch; CanonicalName
  "F10"; Parse lowercases). No defect.
- Whitespace/case hotkey config ("ctrl + shift" with spaces): Parse trims ends only, token " " fails → nullopt
  binding, LoadBindings drops it silently (same silent-drop class as R-W4-006). No crash.
- DrainDictation buffer: drains by cursor delta; the "size changed mid-drain ⇒ keep prefix" branch can only
  trigger on a mid-copy append — engine injection runs on the same main thread as RenderFrame, so unreachable
  today; if a future worker-thread injection existed, the prefix would accumulate in the buffer (unbounded stale
  prefix). Not filed as a defect; noted as latent.

## 6. Dimension checklist
Correctness · FAIL (R-W4-001 dead page; R-W4-002 delete/resurrect; R-W4-003 stale delete-enforce).
Root cause vs symptom · PASS (recorder pluralization + shared validation at the seam; pages thin over stores).
Regression/compat · PASS (schema additive-only; PersonalDictionary parses both legacy and v2 formats; legacy hotkey_code fallback retained; old StartLearnHotkey empty payload still means hotkey_ptt).
Concurrency/idempotency · PASS (hook learn captures under tracker_mutex; settings Save serializes via store mutex; Drain single-threaded as analyzed).
Error handling · PASS-with-note (ValidateModelPath, recorder errors, picker cancel all handled; invalid-chord silent drop noted in R-W4-006).
Security/abuse · FAIL-ish (delete-all privacy overclaim R-W4-002 is the only security-weight item; no secrets/logs/network issues).
Performance/scalability · PASS (per-frame O(5) LoadBindings waste is negligible — R-W4-008).
Migration/rollback · PASS (format-versioned dictionary; revert = delete TUs + hooks; delete-all intentionally irreversible, disclosed).
Observability · PASS (recorder errors inline; warnings counted via text; no new logging of transcripts — zero Log:: in pages).
Maintainability/conventions · PASS (hygiene OK; no comments/TODO/windows.h violations; 54-key scan is testable; dead UsageCount surfaces noted R-W4-004).
Docs impact · FLAG (A4 report's delete scope vs UI "all" wording; N-5 phrasing in INT2 confusing pipeline-expansion with page-construction — see claim #24).

## 7. Optimization and simplification opportunities
| # | Location | Evidence | Impact | Effort | Risk | Verdict |
|---|---|---|---|---|---|---|
| 1 | main.cpp ≈:541 SnippetsPage construction | R-W4-001 | Feature off → on (one line) | Tiny (2 lines + scan test) | Low | DO NOW |
| 2 | DeleteLocalData + store clear/reload plumbing | R-W4-002 | Privacy correctness | Small (helper + spec for scope) | Low (needs care with lifetime) | DO NOW |
| 3 | TextField reseed-on-change | R-W4-005 | Visible staleness removed | Small (compare setting vs buffer when inactive) | Low | DO LATER |
| 4 | DrawGeneralHotkeyRow LoadBindings ×5/frame | R-W4-008 | ~µs/frame | Medium (cache invalidation) | Medium (stale cache bugs) | NOT WORTH IT |

## 8. Cross-agent / cross-task interactions observed
- The store-sharing pattern is consistent: every page holds a store by reference and mutations Save+Reload the
  legacy twin where one exists (Dictionary does; Snippets — when wired — needs no twin because the page and the
  pipeline share ONE SnippetStore instance, verified). Scratchpad has TWO windows on one NotesStore (page list +
  editor tabs): page Refresh reloads; editor edits SaveActiveTab by id — a note deleted from the page while open
  in the editor is resurrected by the editor's next autosave (same resurrection class as R-W4-002, smaller blast
  radius; noted, not filed).
- Transforms: page's diff viewer consumes engine's private TransformDiffStore via EngineDiffSource (verified
  construction in main.cpp); the recorded binding list flows to TransformsPage only on learn-save, so hand-edits
  desync transform conflict indicators until next learn (minor, §5).
- Recorder ↔ dictation concurrency: hook-layer suspension during learn is the single coordination point; MainWindow
  per-row arming enforces one-at-a-time; no cross-target clobber found.
- Wave-3 carryovers from REV-W3-01 still open and now exercised: Toast action-label ID collision (Dictionary/
  Snippets pages show "Undo" toasts on a host with page buttons — should verify the fixed id landed; out of this
  diff's scope) and the 4.5 muted-contrast issue (unchanged tokens). HeatLevel exact-quartile edges now visible in
  Insights (flagged in W3, untouched here).

## 9. What I could not verify
- Windows runtime behavior: picker modal (ODK file dialog ownership/flags), recorder key capture on the hook
  thread incl. release-order edge cases, scratchpad editor tab UX, DPI/pixel fidelity — no Windows GUI run in
  this environment; MinGW linters/greenchecks only. G3 zig type-check pending (no zig on box per AGENTS.md),
  consistent with prior gates.
- Full 23/23 ctest (gate-w4-2 claims GREEN, 750s) not rerun under the scoped-check mandate; I reran hygiene,
  no-network, and 32 doctest cases (*Snippets* 18/159, *Dictionary* 14/130), all green. MSVC build is CI-lane only.
- Insights/Transforms/Scratchpad test suites not re-run here (report claims accepted; suites registered and in-target
  per CMake diff). Whether exact-quartile heat edges should be up or down remains a spec decision (W3 flag).
- Delete-all resurrection: proven by code trace (Save dumps in-memory entries; stores never cleared), not by a
  live repro run — a scripted repro against the real exe is recommended once R-W4-001/002 land.
