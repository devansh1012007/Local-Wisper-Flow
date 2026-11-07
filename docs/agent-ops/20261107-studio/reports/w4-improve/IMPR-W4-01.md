# IMPR-W4-01 — Wave 4 improver (W4-IMPR-01)

<!-- ORCH-DIGEST:START -->
agent: W4-IMPR-01 | scope: Wave 4 review fixes R-W4-001..R-W4-008 (T-016..T-025 review) | status: COMPLETE — 6 FIXED, 2 DEFERRED with reasons (R-W4-004, R-W4-008)
findings: BLOCKER 1 | MAJOR 1 | MINOR 4 | NIT 2 — FIXED: R-W4-001, R-W4-002 (+history.jsonl, +bak cleanup), R-W4-003, R-W4-005, R-W4-006, R-W4-007; DEFERRED: R-W4-004 (new Wave-5 task needed; no producer invented), R-W4-008 (NOT-WORTH-IT per review). See §8 table.
r001_snippets_wiring: FIXED (main.cpp include + SetSnippetsPage(snippets) [Path: rail ##nav_snippets → DrawSnippetsPage → snippets_page_->Draw, shared SnippetStore& Save path]) | r002_delete_resurrect: FIXED (files + history.jsonl + .bak/.tmp removed; all 8 in-memory stores cleared via MainWindow DataResetStores; config.json kept) | r003_dict_reload: FIXED (Commit/CommitExpired/RequestDelete dict overloads; Draw passes dict_; old signatures preserved) | r004_usage_count: DEFERRED to proposed Wave-5 T-027 (needs TextPipeline/PD/engine — outside write-set; §8) | r005_textfield_stale: FIXED (mark-stale from SetLearnedHotkey + Clear) | r006_text_validation: FIXED (StringFieldRow validates hotkey_* via ValidateRecorderChord vs pre-save LoadBindings; empty=clear; error in Other-hotkeys card) | r007_saved_toast: FIXED (ShowSaved removed after delete) | r008_loadbindings_cache: DEFERRED NOT-WORTH-IT (µs/frame; review verdict; §8)
gates: configure EXIT 0 | build local-wisper-flow+lwf_tests EXIT 0 (only pre-existing gRPC empty_bases warnings) | ctest -R lwf-tests 1/1 Passed | *snippets page* 18/159 PASS | *dictionary page* 14/130 PASS | hygiene OK | no-network OK (verbatim §7)
<!-- ORCH-DIGEST:END -->

## 1. Context report (findings from §-required reading, pre-edit)

Required reading done: STANDING-ORDERS.md, AGENTS.md, 00-mission.md, 01-work-ledger.md,
REV-W4-01.md (full), W4-IMPL-S2-T-017.md (N-3 signature), W4-IMPL-INT2.md, plan §§H,I
(excerpts: H test traceability incl. T-reload/T-val/T-contrast/T-a4; I edge table incl.
"Pipeline dictionary stale after page edit — main.cpp dual load — shared instance reloads D-OP2/T-reload").
Branch `feature/studio-pages`, HEAD ad3085d.
Write-set (brief): src/main.cpp, src/ui/MainWindow.{h,cpp},
src/ui/pages/SnippetsPage.{h,cpp}, src/ui/pages/DictionaryPage.{h,cpp},
src/core/SnippetStore.{h,cpp}, src/core/DictionaryStore.{h,cpp}, src/core/StatsStore.{h,cpp},
src/core/ApiKeyStore.{h,cpp}, src/core/NotesStore.{h,cpp}, src/core/TransformStore.{h,cpp}
(clear/reload methods only if needed). FORBIDDEN: CMakeLists.txt, scripts/**, src/os/**, docs/agent-ops
(report only). UiBus, TextPipeline, PersonalDictionary, HistoryStore, tests/ are read-only for this
task (edit only via existing public APIs).

Root cause / entry points:
- R-W4-001: SnippetsPage class exists (lwf::ui::pages::SnippetsPage, ctor SnippetStore&,
  Draw(float)) and MainWindow has SetSnippetsPage + DrawSnippetsPage delegation + rail case,
  but main.cpp never constructs it. Evidence: zero `SetSnippetsPage|SnippetsPage` in HEAD main.cpp;
  4 sibling pages constructed at main.cpp:534-541. INT2 §3/§5 documents the exact 2-line fix (N-5).
- R-W4-002: MainWindow::DeleteLocalData (MainWindow.cpp:654-664) removes 6 paths (stats/dictionary/
  snippets/transforms/notes/api_keys) but (a) misses history.jsonl (HistoryStore main.cpp:470,
  DictationRecords incl raw text when keep_raw_in_history) and (b) never clears any in-memory store,
  so next Save() of any store resurrects its file with old entries. Every store's Save() dumps full
  in-memory state (verified DictionaryStore::Save→EntriesToJson, SnippetStore::Save, TransformStore::Save,
  StatsStore::PersistLocked, NotesStore WriteIndex, ApiKeyStore Persist). config.json correctly kept
  per spec (A4 report, brief).
- R-W4-003: DictionaryPage SubmitAdd/SubmitUpdate call dict_->Reload() (DictionaryPage.h:155,173),
  but CommitPendingDelete (DictionaryPage.h:178-188) and RequestDelete's immediate-commit-of-previous
  (DictionaryPage.h:214-219, Remove+Save) do not. Draw → CommitExpiredPending is the commit path.
- R-W4-004: DictionaryStore::RecordUsage (DictionaryStore.cpp:604) zero callers in src/;
  PersonalDictionary::Enforce + TextPipeline::ProcessWithStats (TextPipeline.cpp:97-100) produce
  only a dict_fixes count, no entry ids; DictionaryStore is not passed to the pipeline
  (main.cpp:391 standalone + 410 pipeline.SetDictionary(&dictionary) legacy only).
  No trivially wireable seam from StatsStore (StatsStore records only aggregate dictionary_fixes
  int64 in DictationRecord, no phrase/id). Wiring needs TextPipeline/PersonalDictionary/
  DictationEngine changes (outside write-set) + phrase→id bridge. DEFER (see §8).
- R-W4-005: MainWindow.h TextField::Seed seeds once (seeded flag, never refreshes). SetLearnedHotkey
  (MainWindow.cpp:233-244) clears learn state but never marks the 4 hotkey TextFields stale, so the
  "Other hotkeys" card shows the pre-recorder chord until restart. Review suggests re-seed-on-change
  OR mark-stale from SetLearnedHotkey; the latter is cheaper and fits write-set.
- R-W4-006: StringFieldRow (MainWindow.cpp:641-652) unconditionally SetString+ApplyAndSave on
  DeactivatedAfterEdit, even for hotkey_* keys, while the recorder path validates via
  HotkeyBinding::ValidateRecorderChord (HotkeyBinding.cpp:256-284) + FindConflict. Typed invalid/
  conflicting chords save and silently die (LoadBindings first-wins dedup, :387-401). Fix per
  review: validate in StringFieldRow for the 4 non-PTT keys, block ApplyAndSave with the error.
  Constraints: empty must stay valid (Clear = SetString("") is the intended clear path,
  DrawGeneralHotkeyRow Clear + LoadBindings empty→nullopt); ValidateRecorderChord("") returns
  "A shortcut needs a key" (HotkeyBinding.cpp:267-269), so empty needs an explicit bypass.
  Error display: "Other hotkeys" card currently shows no error; DrawCleanupPage/DrawSpeechPage
  show validation_error_ (pattern to mirror).
- R-W4-007: Privacy Confirm-delete calls DeleteLocalData() then ShowSaved() (MainWindow.cpp:1260-1263).
- R-W4-008: DrawGeneralHotkeyRow LoadBindings ×N per frame; review verdict NOT-WORTH-IT (µs, cache
  invalidation risk > gain). DEFER.

Affected files / consumers / invariants:
- Files: main.cpp (1 include + 1 construction + delete-all store wiring), MainWindow.{h,cpp}
  (store pointers for delete-all, TextField staleness, StringFieldRow validation, Privacy copy/toast),
  DictionaryPage.{h,cpp} (delete reload), DictionaryStore/StatsStore/TransformStore (clear/reload
  only if needed). SnippetsPage/SnippetStore/ApiKeyStore/NotesStore likely need no change
  (existing SetEntries({})/Reload() suffice — verified below).
- Consumers: MainWindow pages (Draw via RenderPages switch), TextPipeline (shared dictionary/snippets),
  DictationEngine (stats via SetStatsStore), InsightsPage (StatsStore Generation-gated snapshot),
  engine transform store, scratchpad notes, ConsentGate/SttRouter api-key resolution.
- Invariants: config.json kept (never in delete list, never cleared); unknown config keys preserved;
  no code comments (AGENTS.md + hygiene gate); no windows.h outside src/os; no transcript logging;
  schema additive-only (no schema change in this task); JSON files never broken (every store Save
  is tmp+rename); stores are mutex-guarded so clearing on the UI thread is safe; TransformStore
  always has 10 slots + built-ins (clear = reseed defaults, not zero entries); DictionaryStore
  Load() missing-file = seed "btw" (so delete-all must use explicit Clear to empty, NOT Reload);
  PersonalDictionary::Load() missing-file = return false WITHOUT clearing words_ (so delete-all
  must SetWords({})+SetReplacements({}), NOT Reload); SnippetStore::Load() missing-file = false
  without clearing entries_ (so delete-all must SetEntries({}), NOT Load); HistoryStore::Clear()
  truncates file + clears RAM (use it); NotesStore::Reload() missing-dir = empty (use it);
  ApiKeyStore::Reload() missing-file = empty (use it).

Clear/reload method audit (pre-edit, [VERIFIED]):
- DictionaryStore: Load()/Reload() (=Load) → missing file SeedLocked() with 1 seed entry.
  No Clear/SetEntries. ImportJson path exists but is wrong semantic for delete-all.
  → need NEW Clear() (entries_.clear(), next_id_=1, loaded_=true, no file write, no seed).
- StatsStore: ctor LoadLocked(), Record()→Evict+Persist, Snapshot()/Generation(). No Reload/Clear.
  days_ drives every Insights surface; Generation gates InsightsPage::SyncIfStale.
  → need NEW Clear() (days_.clear(), PersistLocked() → "{\n}\n"-shape empty object, generation bump).
- TransformStore: Load(path) missing → SeedDefaults()+Save(path) (recreates file with built-ins).
  No Reload(). Entries() always 10 slots. → need NEW Reload() re-load from stored path_
  (path_ set by Load at main.cpp:478) so MainWindow can reset without knowing the path; missing
  file → SeedDefaults+Save is the correct "empty" for transforms (built-ins are not user data).
  Behavior check vs tests: Reload() is additive (existing Load untouched), zero risk to callers.
- SnippetStore: SetEntries({}) exists (sets loaded=true, keeps path_/mode_). Load() missing returns
  false and would keep old entries. → NO new method: use SetEntries({}) from MainWindow.
  Note: main.cpp Load-fail path already does SetEntries({}) (main.cpp:393-395); production path_
  is set on success so future Save() keeps working after clear.
- NotesStore: Reload() exists → LoadLocked(); missing notes_dir_ → notes_ empty, true, no dir
  recreated (recreated lazily by next WriteBodyLocked). → NO new method.
- ApiKeyStore: Reload() exists → LoadLocked(); missing api_keys.json → keys_ empty.
  → NO new method. (On in-memory resurrection: Set/Remove PersistLocked() would resurrect old
  keys_ if not cleared; Reload() after file delete fixes it.)
- HistoryStore (read-only header): Clear() exists → entries_.clear() + truncate file.
  history.jsonl file removal is MainWindow's job (filesystem); Clear() is MainWindow/main.cpp's job.
  Decision on scope (see §8): history.jsonl IS in scope (R-W4-002 explicitly recommends it and
  history is the most sensitive file). logs/ is OUT: Log::Write opens/closes per line
  (Log.cpp:113-117 fopen/fclose, no held handle) but Log::Init is the only create_directories
  call (Log.cpp:36) and Write never recreates the dir — deleting logs/ would permanently break file
  logging until restart, and the in-memory ring (RecentLines) survives any file delete anyway.
  models.json / app_profiles.json are OUT: not dictation/user text, not in the brief's delete list
  (stats/dictionary/snippets/transforms/notes/api_keys), and the brief says "config.json kept per
  spec" as the only explicit keep; deleting app_profiles without resetting AppContextResolver would
  desync engine paste-chord state (resolver holds loaded profiles, no reload path in main loop).
  → leave both, document.

Technical risks / edge cases:
- MainWindow does not own the stores (main.cpp does) and UiBus.h is outside the write-set, so the
  review's suggested "pass data reset through UiBus" cannot be implemented without breaching
  scope. Chosen seam (see §5 DECISION-DELETE-ALL): MainWindow holds non-owning store pointers set
  from main.cpp (one `SetDataResetStores` struct setter), DeleteLocalData clears them on the UI
  thread after file removal. All stores lock internally; engine stats writes (StatsStore::Record on
  engine thread) serialize on the same mutex. HistoryStore::Append (EngineBridge on engine thread)
  likewise locked. Order: files first, then RAM clears; StatsStore::Clear re-persists "{}"
  (file exists but empty — either absent or "{}" satisfies "no resurrection"; next Record()
  appends to the cleared map only). TransformStore::Reload re-persists defaults file (by design).
- .bak/.tmp/.bad residue: SnippetStore/TransformStore/ApiKeyStore Save() copy to .bak on every
  save; DictionaryStore .bak on migrated/truncated. Leaving .bak after delete-all would leave
  deleted data recoverable by rename. DeleteLocalData must also remove .bak/.tmp for the 4 JSON
  stores (error_code tolerant, like existing removes). Crash files live in logs/ (out of scope above).
- ScratchpadWindow holds open note tabs beyond NotesStore (review §8). Update() on a deleted id
  returns false (no resurrection via autosave), but a new-note tab created before delete-all and
  saved after (Create()) would create one new note — that is new user data typed after the delete,
  not resurrection. Acceptable; editor buffers are new input, not replay. Document, do not fix
  (editor outside write-set).
- DictionaryPage pending-delete + delete-all interaction: if a pending delete is armed when the user
  hits Privacy→Delete all, the pending commit (Remove+Save) after Clear would be a no-op (id absent
  → Remove false) and dict Reload would re-read the cleared file (empty) — harmless. No special case.
- StringFieldRow validation: LoadBindings(settings_) must be the PRE-save registry (same as main.cpp
  learn path :661). Candidate "" (empty/whitespace) bypasses validation and saves "" (clear).
  Non-empty invalid/conflicting → do NOT SetString/ApplyAndSave; set validation_error_ (cleared on
  page switch already at rail press) and revert the buffer to the current setting so the field shows
  the live value, not the rejected text. Display validation_error_ in the "Other hotkeys" card
  (mirror DrawCleanupPage pattern). Must not break the T-a4 every-key scan (keys still appear as
  literals at StringFieldRow call sites) nor the "Not available yet." ==5 pin (untouched, until
  R-W4-001 lands — after R-W4-001 it stays 5 because the stub fallback code path remains verbatim).
- TextField staleness: mark-stale from SetLearnedHotkey (review's second option). SetLearnedHotkey
  runs on the main thread between poll and RunOnce (outside the ImGui frame); setting seeded=false
  there is race-free w.r.t. the next frame's Seed (no concurrent ImGui access). Clear button in
  DrawGeneralHotkeyRow must also mark the corresponding field stale (sets "" behind the field's back).
  Non-hotkey fields (mic_device etc.) have no recorder path and never go stale.
- R-W4-004: do NOT invent a producer. StatsStore::Record is called from DictationEngine with only
  aggregate counts; nothing there maps to a dictionary id. The only seam is DictionaryStore::
  RecordUsage(id), which needs a kit phrase→id bridge inside TextPipeline/PersonalDictionary
  (forbidden files). → DEFER to a new Wave 5 task (proposed T-027; T-021/T-026 do not cover it).

Expected behavior / required test cases:
- R-W4-001: rail Snippets → DrawSnippetsPage → snippets_page_->Draw(scale) (real page), with
  shared SnippetStore& + Save path (page already Save()s the shared instance — text proves it, new
  construction completes the path). Verify: source trace + build (app target compiles SnippetsPage.cpp
  + main.cpp reference) + existing 18/159 Snippets cases still green. No test file in write-set, so
  no new doctest case; instead hand-verify the wiring chain and note the missing construction-scan
  test as orchestrator/orchestrator-via-INT follow-up.
- R-W4-002: after Privacy delete-all: stats/dictionary/snippets/transforms/notes/api_keys/history
  files absent-or-empty (transforms re-seeded defaults, stats "{}"), ALL in-memory stores empty
  (dictionary 0, snippets 0, stats 0 days, transforms defaults-only, notes 0, api keys 0,
  PersonalDictionary words 0, HistoryStore 0), config.json untouched, no ".bak" residue for the
  JSON stores, next Save resurrects only new entries. Also verify Insights refreshes (Generation bump).
- R-W4-003: delete commit → PersonalDictionary no longer enforces the phrase (Enforce returns text
  unchanged for the deleted phrase, dict.Words() lacks it) — existing test "dictionary page delete
  commits only after the five second undo window" must stay green using the OLD 3-arg call
  (overloads preserve it); new dict overload covered by a new test only if tests/ were editable —
  they are not in this write-set, so cover via the Draw-path source trace + existing suite green.
- R-W4-005: recorder-save of hotkey_handsfree → next frame's StringFieldRow re-seeds the new chord.
- R-W4-006: type conflicting/invalid chord → not saved (setting unchanged), error shown, buffer
  reverted; type empty → clears (saved ""); type valid unique → saved (existing happy path).
- R-W4-007: delete-all confirm no longer shows "Saved".
- Gates: configure + `cmake --build build --config Release --parallel 4 --target local-wisper-flow+lwf_tests`
  (brief spelling expanded to `--target local-wisper-flow lwf_tests`? Must keep brief verbatim in
  report; use the two-target form) + `ctest --test-dir build -R lwf-tests` + hygiene + no-network.
  Existing StaleSeed/validation logic has no harness here, so scoped doctest reruns:
  *Snippets*, *Dictionary*page*, *dictionary page*, plus test_overlay_settings_ui scans.

## 2. Scoped verification before edits (evidence)

- [VERIFIED main.cpp:62-65, 392-411, 470-478, 533-541] includes Dictionary/Insights/Scratchpad/
  Transforms pages but NOT SnippetsPage; snippets loaded+SetMode+pipeline.SetSnippets present;
  4 pages constructed, Snippets absent. → R-W4-001 confirmed.
- [VERIFIED MainWindow.cpp:386-413 RenderPages switch] Page::Snippets cased; DrawSnippetsPage
  null-guard + stub fallback (:1312-1319). Rail items include ##nav_snippets (:442).
- [VERIFIED MainWindow.cpp:654-664 DeleteLocalData] removes stats/dictionary/snippets/transforms/
  notes( remove_all)/api_keys only; delete_data_armed_=false. No store clearing. Privacy copy
  (:1256) enumerates stats/dictionary/snippets/transforms/notes/API keys + "Settings are kept".
- [VERIFIED DictionaryPage.h:142-232] SubmitAdd/Update reload dict; CommitPendingDelete/
  CommitExpiredPending/RequestDelete do not. Tests test_dictionary_page.cpp:214-260 use the
  3-arg/4-arg forms → any signatur change must be overload-additive.
- [VERIFIED HotkeyBinding.cpp:256-284 ValidateRecorderChord] empty→"A shortcut needs a key";
  Parse fail→same; no-modifier→"A shortcut needs a modifier key"; FindConflict→"That shortcut
  is already in use". LoadBindings (:343-402) PTT fallback + first-wins dedup. RoleForConfigKey
  covers the 5 hotkey_* keys.
- [VERIFIED MainWindow.cpp:641-652 StringFieldRow] no validation; :1412-1427 "Other hotkeys" card
  has the 4 hotkey StringFieldRows and no error display. DrawGeneralHotkeyRow recorder UI is at
  :747-792. SetLearnedHotkey :233-244. TextField :153-162 (seeded-once).
- [VERIFIED Log.cpp:31-39,89-117] Init creates logs dir; Write opens/closes lwf.log per line,
  never recreates dir; RotateIfNeeded only touches files inside. → logs/ delete would break logging.
- [VERIFIED HistoryStore.cpp:165-170 Clear()] truncates file; DictationEngine RecordHistory →
  EngineBridge::RecordHistory → HistoryStore::Append (main.cpp:135). Zero other writers of
  history.jsonl (HistoryStore only). models.json (Onboarding ParseModelsJson read-only at
  DrawWelcomePage :1551) and app_profiles.json (AppContextResolver load-once) are not
  dictation/user-text stores.
- [VERIFIED usage seam] grep RecordUsage → def only (DictionaryStore.cpp:604) + tests +
  docs flags; TextPipeline.cpp:97-104 Enforce→dict_fixes int only; StatsStore DictationRecord
  fields (StatsStore.h:9-18) carry only aggregate dictionary_fixes; no phrase/id anywhere in the
  enforce→stats path. main.cpp lemn: pipeline gets &dictionary (PersonalDictionary), never
  DictionaryStore. → DEFER.
- [VERIFIED test_overlay_settings_ui.cpp:653] `count_occurrences(content, "Not available yet.") == 5`
  (must preserve; R-W4-001 keeps the fallback verbatim) and font_sources list (:255-282, already
  includes SnippetsPage.*) + Welcome ApplyAndSave >=2 (:537, untouched).
- [VERIFIED ScratchpadWindow] Update() returns false for unknown ids (NotesStore.cpp:385-388);
  new-tab Create() after delete-all is new input, not replay. Buffer DrainDictation is
  post-expansion routing (review §5), unaffected.
- Not yet verified this session: Windows runtime (picker, recorder capture, editor UX, DPI) —
  no GUI run (per prior waves, honest limit). MSVC lane is CI-only.

## 3. Working files (write-set check)

Touched (planned): src/main.cpp; src/ui/MainWindow.h, src/ui/MainWindow.cpp;
src/ui/pages/DictionaryPage.h, src/ui/pages/DictionaryPage.cpp;
src/core/DictionaryStore.h, src/core/DictionaryStore.cpp;
src/core/StatsStore.h, src/core/StatsStore.cpp; src/core/TransformStore.h, src/core/TransformStore.cpp.
Untouched by design: SnippetsPage.{h,cpp} (page complete, only main.cpp wiring missing),
SnippetStore.{h,cpp} (SetEntries suffices), ApiKeyStore.{h,cpp} (Reload suffices),
NotesStore.{h,cpp} (Reload suffices), UiBus/TextPipeline/PersonalDictionary/HistoryStore/CMakeLists/scripts/tests/src/os (forbidden/read-only).

## 4. Sessions Log

- Investigation + this report created; COMMS START+CLAIM appended (CLAIM line missing `[` bracket
  in append — noted; START appended correctly after; COMMS is orchestrator-owned, not fixed here).
- Phase 1 (R-W4-001): main.cpp include + SetSnippetsPage.
- Phase 2 (store clears): DictionaryStore::Clear, StatsStore::Clear, TransformStore::Reload.
- Phase 3 (R-W4-003): DictionaryPage.h dict overloads + DictionaryPage.cpp call sites.
- Phase 4 (R-W4-002/005/006/007): MainWindow DataResetStores + DeleteLocalData + Privacy copy/toast
  + SetLearnedHotkey/Clear staleness + StringFieldRow validation + Other-hotkeys error display.
- Phase 5: configure EXIT 0; build EXIT 0; ctest lwf-tests 1/1 Passed; snippets 18/159;
  dictionary-page 14/130; stores 19/151; hygiene OK; no-network OK; self-review fixes
  (unused `last` removed, empty-clear buffer normalised).

## 5. Phases executed

### Phase 1 — SnippetsPage construction (R-W4-001 BLOCKER)
Files: src/main.cpp only.
- Added `#include "ui/pages/SnippetsPage.h"` next to the other page includes.
- Added `full.SetSnippetsPage(std::make_unique<lwf::ui::pages::SnippetsPage>(snippets));`
  after SetScratchpadPage, using the existing pipeline-shared `snippets` instance
  (Loaded, path set, handed to pipeline at :411 — the exact INT2 N-5 lines).
- Saving path (already correct in SnippetsPage): SubmitSnippet/DeleteEdit → store.Upsert/
  RemoveById → (void)store.Save() on the shared instance; pipeline observes edits by construction.
  No SnippetsPage/SnippetStore change needed.
- // DECISION: follow INT2 verbatim (placement after SetScratchpadPage) instead of planner's
  alphabetical next-to-Dictionary — matches the 534-541 construction block and wave precedent
  for the setter/member ordering in INT2 §3.

### Phase 2 — Store clear/reload APIs (R-W4-002 prerequisites)
Files: src/core/DictionaryStore.{h,cpp}, src/core/StatsStore.{h,cpp}, src/core/TransformStore.{h,cpp}.
- `DictionaryStore::Clear()` — `entries_.clear(); next_id_=1; migrated_=false;
  bak_pending_=false; loaded_=true;` under lock; NO file write (file stays deleted until next
  Save). Why not Reload(): `Load()` on a missing file calls `SeedLocked()` with the "btw→by the
  way" seed (DictionaryStore.cpp:351-356) — delete-all would immediately reseed one entry.
  Clear is the true empty state. Path preserved so the next Save creates a fresh entry-only file.
  // INVARIANT: `Clear()` empties RAM without touching disk; caller deletes files first, then
  Clear() — either order is safe because Clear never writes.
- `StatsStore::Clear()` — `days_.clear(); (void)PersistLocked(); generation_.fetch_add(1);`
  under lock. PersistLocked writes `"{\n}\n"` (empty object) so stats.json becomes `{}` rather
  than corrupt; generation bump forces InsightsPage::SyncIfStale (Generation-gated) to refresh
  and drop the cached snapshot. PersistLocked expects the lock held (LoadLocked/PersistLocked are
  Locked variants) — Clear holds it, no double-lock. Next Record() appends only to the cleared map.
- `TransformStore::Reload()` — re-loads from stored `path_` (copied out under lock, then
  `Load(path)` without holding the lock). Missing file → `SeedDefaults()+Save` (recreates
  built-ins Polish/Prompt Engineer + empty slots; enabled_=false). `path_` empty →
  `SeedDefaults()` + `loaded_=true`. Why reseed not zero: TransformStore invariant is 10 slots
  (slots 1-10 always exist, built-ins are not user data); "clear" = defaults, matching the page's
  Reset-to-defaults semantics. No new behaviour for other callers (existing Load untouched).
- NO new methods for SnippetStore (existing `SetEntries({})` preserves path_/mode_), NotesStore
  (`Reload()` on missing dir → empty), ApiKeyStore (`Reload()` on missing file → empty). Verified
  rather than assumed (§2 audit). SnippetsPage/SnippetStore/ApiKeyStore/NotesStore files untouched.
- // DECISION: additive-only methods; no signature changes, no schema/JSON changes. All new TUs
  compile into the existing lwf_core target (no CMakeLists change needed — no new files).

### Phase 3 — Dictionary delete reloads the pipeline dictionary (R-W4-003 MINOR)
Files: src/ui/pages/DictionaryPage.h, src/ui/pages/DictionaryPage.cpp.
- Added `CommitPendingDelete(store, state, dict)`, `CommitExpiredPending(store, state, dict, now)`,
  and `RequestDelete(store, state, dict, id, now)` primaries that mirror SubmitAdd/SubmitUpdate:
  after a committed `Remove+Save`, `dict->Reload()` if `removed && saved && dict != nullptr`.
  The immediate commit of a previous pending delete inside RequestDelete also reloads.
- Kept the old `CommitPendingDelete(store, state)`, `CommitExpiredPending(store, state, now)`,
  `RequestDelete(store, state, id, now)` as wrappers delegating with `nullptr` — tests/
  test_dictionary_page.cpp:214-260 (3-arg/4-arg forms) compile and pass unchanged.
- DictionaryPage::Draw now calls `CommitExpiredPending(store_, state_, dict_, GetTime())`;
  DrawRows calls `RequestDelete(store_, state_, dict_, entry.id, GetTime())`, so the shipped page
  always reloads the shared PersonalDictionary (D-OP2) on delete commits, not just add/update.
- // DECISION: overload-additive rather than signature-breaking (write-set excludes tests/; the
  review's recommended fix reads "pass PersonalDictionary* into CommitPendingDelete" — overloads
  satisfy this while preserving the test contract).
- Related cleanup (behaviour-preserving for the covered case): RequestDelete's previous-commit
  changed from `if (!store.Remove(previous_id)) return false;` (blocking the new delete and
  leaving the stale pending) to `previous_removed = Remove(...); Save(); if (previous_removed &&
  dict) dict->Reload();` then always staging the new delete. When the previous entry is present
  (the only case the existing tests cover), Remove succeeds identically; when it is already absent
  (e.g. external Reload/Clear removed it), the stale pending is discarded instead of wedging the
  new delete — the prior early-return was the only path that could block a legitimate delete.

### Phase 4 — Delete-all resurrection + hotkey staleness/validation (R-W4-002 MAJOR, R-W4-005/006/007)
Files: src/ui/MainWindow.h, src/ui/MainWindow.cpp, src/main.cpp (store wiring).
- MainWindow.h: forward declarations for the 8 store types + `struct DataResetStores` (8 nullable
  pointers) + `void SetDataResetStores(DataResetStores)` + `DataResetStores reset_stores_;` member.
  MainWindow.cpp: added the 8 core store includes (needed for the Clear/Reload/Set* calls in
  DeleteLocalData); `SetDataResetStores` stores the pointers by value (non-owning).
- main.cpp: after SetSnippetsPage, fills `DataResetStores` with all 8 live instances
  (`dictionary_store`, `snippets`, `usage_stats`, `transforms`, `notes`, `api_keys`, `history`,
  `dictionary`) and calls `full.SetDataResetStores(...)`. `// DECISION:` non-owning pointers
  (not UiBus) because UiBus.h is outside this task's write-set and MainWindow must not own the
  stores (main.cpp does). Lifetime is safe: stores are constructed before MainWindow and destroyed
  after it (stack order), so MainWindow's pointers are valid for every RunOnce/DeleteLocalData.
  All stores lock internally, so the UI-thread clear serializes with engine-thread Record/Append.
- `MainWindow::DeleteLocalData` (rewritten): removes, in order, stats.json, dictionary.json,
  snippets.json, transforms.json, notes/ (remove_all), api_keys.json, history.jsonl, plus
  `.bak`/`.tmp` residue for stats/dictionary/snippets/transforms/api_keys/history (error_code
  tolerant, same pattern as before). `.bad` quarantine files (Dictionary/Notes corrupt renames)
  are intentionally NOT removed: they are no longer read by any loader after the clear (Load reads
  the primary path only), they do not feed resurrection, and they are the only diagnostic trace of
  corrupt files. Then clears RAM: `dictionary->Clear()`, `dictionary_legacy->SetWords({}) +
  SetReplacements({})` (NOT `Reload()` — PersonalDictionary::Load on a missing file returns false
  WITHOUT clearing words_; Set* is the only way to empty enforcement), `snippets->SetEntries({})`,
  `stats->Clear()` (re-persists "{}"), `transforms->Reload()` (re-persists defaults),
  `notes->Reload()`, `api_keys->Reload()`, `history->Clear()` (truncates to 0 bytes), then
  `delete_data_armed_ = false`. config.json is never touched (per brief spec + A4).
- Privacy copy: "Removes statistics, dictionary, snippets, transforms, notes and saved API keys."
  → "Removes statistics, dictionary, snippets, transforms, notes, history and saved API keys.
  Settings are kept." (matches the new history deletion; the "restart to reseed empty files"
  sub-note stays accurate: helpers reseed on next Load where applicable).
- R-W4-007: removed `ShowSaved()` after `DeleteLocalData()` in the Confirm-delete branch
  (destructive action no longer flashes "Saved"). The two-step arm/disarm is the feedback.
- R-W4-005: `SetLearnedHotkey` now marks the matching hotkey TextField stale
  (`seeded=false` for handsfree/cancel/command/paste_last; PTT has no text field → no-op).
  The recorder Clear button in DrawGeneralHotkeyRow also marks its field stale after
  `SetString(key, "")`, so both paths that change a hotkey behind the text field's back are
  covered. Next frame's `Seed` re-reads the live setting. Main-thread only (SetLearnedHotkey runs
  between poll and RunOnce, outside the ImGui frame) — the next Seed cannot race an in-progress
  edit across the frame boundary.
- R-W4-006: `StringFieldRow` now validates when `RoleForConfigKey(key)` has a value (the 4 hotkey
  fields; other keys — mic_device/stt_language/remote_endpoint/cleanup_language_policy — keep the
  exact old behaviour). Empty/whitespace-only (same `find_first_not_of(" \t")` test as
  `HotkeyBinding::Parse`:267) bypasses validation and saves `""` (clear; ValidateRecorderChord
  would reject empty with "A shortcut needs a key"). Non-empty: `LoadBindings(settings_)` is the
  PRE-save registry (same as the main.cpp learn path :661) and
  `ValidateRecorderChord(candidate, registry, *role)` — on non-empty error: `validation_error_ =
  error`, buffer reverted to `settings_.GetString(key)` via snprintf, NO SetString/ApplyAndSave,
  NO `restart_needed_` side effect, return. On valid: `validation_error_.clear()` + save. The
  "Other hotkeys" card shows `validation_error_` when non-empty (same TextColored err pattern as
  DrawCleanupPage/DrawSpeechPage; it is already cleared on rail/tab page switches). The empty-clear
  path also normalises the buffer to "" so the field does not show the old whitespace.
  `// SECURITY:` typed input can no longer create a conflicting/unparseable binding that
  LoadBindings would silently drop; the field now shows the live value plus the recorder's exact
  message, mirroring the recorder contract.

## 6. Review audit

Self-review per phase (re-read diff from scratch; verdicts CRITICAL/MAJOR/MINOR with evidence):

- Phase 1 — CRITICAL: none. The Include alone without the Setter, or the Setter alone without
  the Include, would fail the app link (verified by build). MAJOR: none — the shared `snippets`
  instance (Load+SetMode+pipeline.SetSnippets at :392-411) is the same object the page receives;
  post-Phase-1 `git diff src/main.cpp` matches INT2's 2 lines exactly. Mutation check: removing
  either line returns the rail to the stub (the pre-existing "Not available yet." fallback) — no
  other caller depends on SnippetsPage construction.
- Phase 2 — CRITICAL: none. MAJOR (1, corrected before build): the planned `StringFieldRow`
  empty-test used both `first` and `last` with `last` unread → -Wunused-variable would have
  broken the "zero new warnings" G1 gate; removed `last` and normalised the empty-clear buffer.
  MINOR (1, accepted): TransformStore::Reload with an empty path_ calls SeedDefaults() then sets
  loaded_=true without clearing load_warnings_ — matches Load(missing-file) semantics
  (Load also leaves warnings from SeedDefaults); non-empty path_ (the production case) delegates
  to Load(path) and is exact. MINOR (2, accepted): StatsStore::Clear persists "{}" — file exists
  but empty; "absent or {}" both satisfy no-resurrection, and "{}" keeps the file valid JSON for
  any external reader. Insights Generation bump verified for the refresh path.
- Phase 3 — CRITICAL: none. MAJOR: none — old signatures still compile into test_dictionary_page
  (18-case suite green, §7). Check: 2-arg wrappers delegate with nullptr → zero reload, matching
  the tests' store-only expectations (existing SubmitAdd tests pass dict explicitly and pin reload).
  Mutation mental check: deleting `dict->Reload()` from the new CommitPendingDelete would make a
  dict-aware delete case fail (dict.Words() would retain the phrase while the store shows it gone)
  — no such doctest case exists in this write-set (tests/ forbidden), so the Draw-path trace is
  the evidence (see §10 residual). The RequestDelete previous-commit strengthening (discard stale
  pending instead of blocking) is covered by construction; the "second delete commits the first"
  suite still passes unchanged.
- Phase 4 — CRITICAL: none. MAJOR (1, deliberate + documented): file-then-RAM order with Stats
  Clear persisting "{}" and Transform Reload persisting defaults means 3 files (stats, transforms,
  history-0B) exist as empty/defaults immediately after delete-all while the other 4 stay absent
  until next Save/Create. All 8 carry zero old user bytes; the distinction is reseed timing,
  not privacy. MINOR (1, accepted + tiny residual): pending single-entry Undo stashes (Dictionary/
  Snippets/Scratchpad, 5 s window) survive delete-all in the pages' private state — an explicit
  Undo click within that window after delete-all restores that ONE entry by design but would look
  like resurrection. Fixing it would need private-state clearing APIs on SnippetsPage/
  ScratchpadPage (ScratchpadPage is outside this write-set) and is bounded by the 5 s expiry +
  the explicit click. Documented in §10, not added. MINOR (2, verified safe):
  `StringFieldRow` valid-text saves store the buffer verbatim including surrounding spaces
  (Pre-existing LoadBindings Parse trims ends). The non-PTT text fields only: mic/stt/remote/
  cleanup_language_policy keep exact old behaviour (role nullopt → skip).
- Final review — cross-phase: (a) Delete-all + R-W4-003 interaction is safe: pending Dictionary
  commit after Clear → Remove false → no dict Reload (guarded by `removed && saved`); no crash.
  (b) Delete-all + StringFieldRow/DeleteLocalData `reset_stores_` null-safety: every use is
  null-guarded; MainWindow without SetDataResetStores (tests, other hosts) degrades to the old
  file-only delete. (c) No comments/TODO/sleep added (hygiene OK). (d) No `windows.h`, no network
  APIs (no-network OK). (e) No test files / CMake / scripts / src/os touched
  (`git status` §11; `git diff --stat` §11). (f) `Not available yet.` == 5 preserved
  (stub fallback verbatim; test_overlay_settings_ui green). (g) Supply: SnippetsPage.cpp already
  in CMake WIN32 block; DictionaryPage.h overloads need no new TU.
- Independent review: not dispatched in this environment (per brief, orchestrator gates);
  S2's fresh-context recommendation for SnippetsPage.cpp rendering paths (Windows hardware) is
  still open and recorded in §10. Per Standing Orders §3, status is COMPLETE on my phases with
  the hardware-verification residual explicitly listed; if the orchestrator requires a
  fresh-context sign-off before merge, treat this as REVIEW_REQUIRED — otherwise COMPLETE.

## 7. Verification evidence (verbatim)

Brief-mandated commands (run from repo root, `feature/studio-pages`):

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
  -> EXIT 0, "-- Configuring done (4.5s)", "-- Generating done (20.8s)",
  "-- Build files have been written to: D:/dev work/coding/Projects/Local-Wisper-Flow/build".
  (Only pre-existing CMake deprecation / FetchContent_Populate / ONNX Runtime-not-found notices.)
- `cmake --build build --config Release --parallel 4 --target local-wisper-flow lwf_tests`
  -> EXIT 0. Second (final) run: "[100%] Built target local-wisper-flow",
  "[100%] Built target lwf_tests". Only pre-existing gRPC `empty_bases` attribute warnings from
  vendored headers (D:/lwf/_deps-src/grpc-full2/include/grpc/event_engine/slice.h) + pre-existing
  CMake deprecation notices. Builds observed: DictionaryStore.cpp.obj, StatsStore.cpp.obj,
  TransformStore.cpp.obj, main.cpp.obj, MainWindow.cpp.obj, DictionaryPage.cpp.obj and the
  reconciled rebuild of InsightsPage/TransformsPage (header-touch rebuild) — all clean.
  NOTE on brief spelling: the brief writes `--target local-wisper-flow+lwf_tests`; CMake's
  spelling is the two-target form `--target local-wisper-flow lwf_tests`, used here.
- `ctest --test-dir build -R lwf-tests` (final run after the `last` cleanup)
  -> `1/1 Test #1: lwf-tests ........................   Passed   21.60 sec` /
  `100% tests passed out of 1` / `Total Test time (real) =  21.64 sec`, EXIT 0.
- Scoped doctest reruns (same binary that ctest runs, lwf_tests.exe):
  - `.\build\lwf_tests.exe --test-case="*snippets page*"` ->
    `[doctest] test cases:  18 |  18 passed | 0 failed | 473 skipped` /
    `assertions: 159 | 159 passed | 0 failed |` / `Status: SUCCESS!`
  - `.\build\lwf_tests.exe --test-case="*dictionary page*"` ->
    `[doctest] test cases:  14 |  14 passed | 0 failed | 477 skipped` /
    `assertions: 130 | 130 passed | 0 failed |` / `Status: SUCCESS!`
  - `.\build\lwf_tests.exe --test-case="*dictionary store*,*stats store*,*transform store*,*notes store*"` ->
    `[doctest] test cases:  19 |  19 passed | 0 failed | 472 skipped` /
    `assertions: 151 | 151 passed | 0 failed |` / `Status: SUCCESS!`
  (Counts identical to the pre-change baseline in A2/INT2 + gate-w4-2; old-signature wrappers
  keep the DictionaryPage delete cases compiling.)
- `bash scripts/check_hygiene.sh` -> `hygiene: OK`, EXIT 0.
- `bash scripts/check_no_network.sh` -> `no-network: OK`, EXIT 0.
- Path verification for R-W4-001 (issue mandates "Path: rail click → DrawSnippetsPage → page Draw;
  test or line evidence"):
  - [VERIFIED src/ui/MainWindow.cpp:442] `{"##nav_snippets", "Snippets", Page::Snippets}` rail item;
    the pressed arm sets `page_ = item.page; PersistPage();` (:480-488).
  - [VERIFIED src/ui/MainWindow.cpp:386-413] `RenderPages` switch: `case Page::Snippets:
    DrawSnippetsPage(scale);` (:400-402).
  - [VERIFIED src/ui/MainWindow.cpp:1312-1319] `DrawSnippetsPage` null-guards then
    `snippets_page_->Draw(scale); return;` with the verbatim stub fallback retained.
  - [VERIFIED src/main.cpp:543] `full.SetSnippetsPage(std::make_unique<
    lwf::ui::pages::SnippetsPage>(snippets));` — main.cpp now contains the SetSnippetsPage call
    (grep: zero matches before, one match after). `snippets` is the reference at :392-411
    (Load(path), SetEntries({}) on failure, SetMode, pipeline.SetSnippets(&snippets)).
  - [VERIFIED src/ui/pages/SnippetsPage.h:159] `explicit SnippetsPage(SnippetStore& store)` +
    SnippetsPage.h:77-99 `SubmitSnippet` → Validate + Upsert + Save(); SnippetsPage.cpp DeleteEdit/
    row delete → RequestDelete + Save(). This is the "Save path" (shared store, tmp+rename+.bak
    via SnippetStore::Save).
  - [RAN .\build\lwf_tests.exe --test-case="*snippets page*" -> 18 cases / 159 asserts PASS]
    The snippet suite + build success above is the construction-path test evidence available
    in this write-set; the suggested main.cpp source-scan construction test (review R-W4-001
    "Recommended fix" second clause) was NOT added because tests/ is outside this write-set —
    see §10 follow-up.
- Windows runtime and zig G3: NOT-RUN (no GUI/doctor run in this MinGW env; no zig on box per
  AGENTS.md). MSVC compile is CI-only. Marked NOT-RUN, not claimed.

## 8. Finding dispositions (FIXED / REJECTED-with-evidence / DEFERRED-with-reason)

| ID | Sev | Disposition | Evidence |
|---|---|---|---|
| R-W4-001 | BLOCKER | FIXED | main.cpp:65 include + :543 SetSnippetsPage(snippets) with the pipeline-shared instance; Path rail→DrawSnippetsPage→snippets_page_->Draw traced in §7 (lines); app target links SnippetsPage.cpp; snippets suite 18/159 green post-fix. |
| R-W4-002 | MAJOR | FIXED (scope decision recorded) | DeleteLocalData now removes history.jsonl (+.bak/.tmp for the JSON stores) and clears all 8 in-memory stores via MainWindow::DataResetStores wired in main.cpp:544-555; config.json untouched. DECISIONS: **history.jsonl IN** (review's explicit recommendation + most sensitive file; HistoryStore::Clear truncates + clears RAM). **logs/ OUT** — Log::Write opens/closes per line (Log.cpp:113) but Init is the only create_directories (Log.cpp:36) and Write never recreates the dir, so deleting logs/ would break file logging until restart; the in-memory RecentLines ring survives any file delete regardless. **models.json / app_profiles.json OUT** — neither is dictation/user text nor in the brief's mandated set (stats/dictionary/snippets/transforms/notes/api_keys); app_profiles has no reload seam (engine resolver holds it), so deleting the file without resetting the resolver would desync paste-chord state. Concrete per-store contract: dictionary → Clear (no seed), snippets → SetEntries({}), stats → Clear ({} file + Generation bump), transforms → Reload (built-in defaults file), notes → Reload, api_keys → Reload, PersonalDictionary → SetWords({})+SetReplacements({}), History → Clear (0-byte file). See §2 audit for why each is not a Reload/Load. |
| R-W4-003 | MINOR | FIXED | DictionaryPage.h dict-overloads (CommitPendingDelete/CommitExpiredPending/RequestDelete with `removed && saved` guard + previous-commit reload); Draw + DrawRows call sites pass dict_; old 3/4-arg signatures preserved (dictionary-page suite 14/130 green). |
| R-W4-004 | MINOR | DEFERRED with reason | Zero callers of RecordUsage confined; producer needs a phrase→id bridge through TextPipeline/PersonalDictionary/DictationEngine (all outside write-set; DictionaryStore not even passed to the pipeline). StatsStore carries no phrase/id (aggregate dictionary_fixes int64 only), so "wire it from StatsStore" is not trivially possible. Per brief, no producer was invented. Deferred to late-arrival Wave-5 task linked as **proposed T-027 (usage-count producer via enforce-hit callback)** for orchestrator to ledger (current ledger W5 = T-021 polish + T-026 OpenRouter covers neither; inventing a producer in this wave was explicitly forbidden). When T-027 lands it will also need a doctest that fails without the producer (old: Enforce→RecordUsage path). |
| R-W4-005 | MINOR | FIXED | Mark-stale (`seeded=false`) from SetLearnedHotkey for the 4 hotkey fields + from the recorder Clear button; next StringFieldRow Seed re-reads the live setting. Non-hotkey fields untouched (no behind-the-back writer). |
| R-W4-006 | MINOR | FIXED | StringFieldRow validates hotkey_* (non-empty via ValidateRecorderChord vs pre-save LoadBindings, empty = clear) and blocks the save on error with buffer revert + validation_error_ shown in the "Other hotkeys" card. The A3-documented alternative (delete the text card entirely) was NOT taken: it would duplicate-delete UI the A4 every-key scan counts on the literals and is a design change beyond "cheap + safe" in this wave (see §10 follow-up for the orchestrator call). |
| R-W4-007 | NIT | FIXED | ShowSaved() removed from the Confirm-delete branch; arm/disarm remains the feedback. |
| R-W4-008 | NIT | DEFERRED with reason | Per the review (`NOT-WORTH-IT`): ~µs/frame for O(5) parses vs invalidation-machinery risk. No change (staying inside review's "below the fold" designation). Revisit only if LoadBindings ever reads disk/network per call. |

## 9. Long-run answers (Source / Load / Hostile / Operability / Reversal)

- Source: Each fix is at the seam the wave already established: pages stay thin over shared stores
  and the MainWindow helper idioms (ApplyAndSave, validation_error_ partition per page); the
  delete-all seam lives in MainWindow because UiBus is out of this write-set, but every store is
  still the single writer of its file. No new abstraction was introduced; the one new struct
  (DataResetStores) is the narrowest bridge main.cpp can fill in one call.
- Load: Per-frame cost added is one `RoleForConfigKey` branch per StringFieldRow (no parse) + one
  `LoadBindings` per hotkey deactivation (same order as today’s per-frame DrawGeneralHotkeyRow
  reads — config strings only, no disk). Delete-all is O(files + entries) like before. Dictionary
  page hot paths unchanged.
- Hostile: Malformed hotkey input (`Ctrl+Shift+`, `NotAKey`, whitespace) can no longer be saved
  from the text card — ValidateRecorderChord is total (rejects empties/lead-+/dups/unknown tokens)
  and conflicts, the buffer reverts to the live value, and LoadBindings keeps first-wins. Delete-all
  with missing files/`.bak` is idempotent (error_code removes). Hand-corrupted config still cannot
  crash the recorder (unchanged ValidateRecorderChord/LoadBindings behaviour, now mirrored).
- Operability: Recorder errors and text-field errors now share the exact same messages
  ("A shortcut needs a key", "A shortcut needs a modifier key", "That shortcut is already in use"),
  shown red in the expected card; delete-all disarms without a misleading "Saved"; Insights
  Generation bump makes stats delete visible on the next frame without a restart.
- Reversal: Reverting this task is deleting the 2 main.cpp lines + the MainWindow struct wiring +
  3 additive store methods + the DictionaryPage overloads (old signatures still compile) —
  snippet/dictionary/transform/stats file formats unchanged, no schema/JSON change, so a pre-task
  binary still reads task-written files. Delete-all remains intentionally irreversible (no undo).

## 10. Unverified assumptions & remaining risks

- Windows runtime is NOT VERIFIED (picker, recorder capture, text-field validation round-trip
  visibility, 440px snippet sheet, pencil glyph, truncation widths, Privacy confirm flow, DPI).
  Only the MinGW build + doctest suites ran — same honest limit as every prior wave. S1/S2 manual
  checklists (S2 §8) need Devansh on Windows hardware, now including: type a conflicting chord in
  "Other hotkeys" → error + revert; record Hands-free → text field shows the new chord next frame;
  Privacy→Delete all→ add a dictionary word → file contains only the new word.
- G3 zig type-check pending (no zig on box per AGENTS.md). MSVC lane is CI-only.
- Residual privacy edge (MINOR, documented in §6): pending single-entry Undo stashes (default 5 s)
  in Dictionary/Snippets/Scratchpad pages survive delete-all; an explicit Undo click inside the
  window restores that one entry (Scratchpad via NotesStore::Restore, Snippets via Upsert+Save;
  Dictionary Undo only drops the pending, restoring nothing — no risk there). Requires an explicit
  Undo click, bounded by toast_dwell_s. Orchestrator follow-up (T-021 polish) should decide whether
  delete-all must also invalidate page undo state; ScratchpadPage files are outside this write-set.
- Construction-scan test for R-W4-001 was NOT added (tests/ outside write-set; the review's
  recommended main.cpp scan case needs integrator+tests). The chain is line-verified in §7 and the
  app links, but nothing fails CI if someone later deletes the SetSnippetsPage line. Suggest the
  orchestrator/integrator add the test_overlay_settings_ui.cpp scan case asserting
  `main.cpp contains SetSnippetsPage(snippets)` alongside the font_sources pin.
- Follow-up design call (not taken, §8 R-W4-006): whether the "Other hotkeys" text card should be
  deleted wholesale now that the recorder covers the same 4 keys (A3's integration note). Validation
  makes the card safe to keep; deletion is a product choice for orchestrator/T-021.
- COMMS CLAIM line appended by this agent lacks the leading `[` bracket; not corrected here
  (COMMS is orchestrator-owned, append-only).

## 11. Git state

- Branch: `feature/studio-pages` (verified `git branch --show-current` before committing).
- Code commit (explicit paths) — 98f7685 `fix(ui): fix W4 review blockers
  R-W4-001/002/003/005/006/007 (T-016..T-025-review)` (11 files, +195/-12): src/core/
  DictionaryStore.{h,cpp}, src/core/StatsStore.{h,cpp}, src/core/TransformStore.{h,cpp},
  src/ui/MainWindow.{h,cpp}, src/ui/pages/DictionaryPage.{h,cpp}, src/main.cpp.
  SnippetsPage.{h,cpp}, SnippetStore.{h,cpp}, ApiKeyStore.{h,cpp}, NotesStore.{h,cpp}
  intentionally have NO diff (existing SetEntries({})/Reload() suffice, §2 audit).
- Docs commit (explicit path) — this report + gate record under
  docs/agent-ops/20261107-studio/reports/w4-improve/IMPR-W4-01.md, with trailers
  Task: T-016..T-025-review / Agent: W4-IMPR-01 / Wave: 4 /
  Report: docs/agent-ops/20261107-studio/reports/w4-improve/IMPR-W4-01.md.
  COMMS.md NOT committed here (orchestrator-owned at wave boundaries).
- Working tree after commits: no tracked modifications from this agent remain; only
  pre-existing untracked briefs/reports (unchanged) + the COMMS.md modification appended
  by this agent (also unchanged by others) are outstanding for the orchestrator.
- Forbidden ops not used: no CMakeLists/scripts/src/os/tests edits, no `git add -A`,
  no stash/reset/checkout/clean/rebase/merge/push, no `--no-verify`, no index.lock.

