<!-- ORCH-DIGEST:START -->
agent: W4-DELTA-01 | scope: delta c9df9df..HEAD (src/ tests/ only, 11 files +195/-12) | verdict: APPROVE-WITH-CHANGES
findings: BLOCKER 0 | MAJOR 1 | MINOR 3 | NIT 1
claims_audit: 6 claims checked / 2 overstated (snippet Save unconditional; delete-all 0-entries unqualified for lazy Load)
band_aids_found: R-W4-002 mostly root-cause-fixed; 1 residual: DictionaryStore::Clear defeated by lazy Load seed on first Dictionary visit
top_3_risks: (1) delete-all before first Dictionary open resurrects "btw" seed via Store::Load; (2) first-run snippet edits never reach disk (pre-existing, newly reachable); (3) zero new doctests for Clear/Reload/delete wiring
optimizations: DO-NOW 0 | DO-LATER 1 | NOT-WORTH-IT 0
reversal_check: done
confidence: med-high (full source trace + targeted reruns; Windows runtime/G3 unverifiable)
<!-- ORCH-DIGEST:END -->

# Code review — W4-DELTA-01 (delta reviewer, read-only)

## 0. Method
Diff range reviewed: `git diff c9df9df..HEAD -- src/ tests/` (docs excluded per scope). 11 files changed, all in
`src/`; `tests/` diff is empty. No source edits made (read-only mandate).
Blind pass first (source only), then read
`docs/agent-ops/20261107-studio/reports/w4-improve/IMPR-W4-01.md` for the claims audit.
Files read in full or at relevant offsets: `src/main.cpp` (300-399, 500-579, store decls 312/390-478),
`src/ui/MainWindow.{h,cpp}` (DataResetStores, DeleteLocalData 700-747, StringFieldRow 662-698,
SetLearnedHotkey, DrawSnippetsPage 1403-1410), `src/ui/pages/DictionaryPage.{h,cpp}`,
`src/ui/pages/SnippetsPage.{h,cpp}`, `src/core/DictionaryStore.{cpp:345-415,488-622}`,
`src/core/StatsStore.cpp:191-196,271-319`, `src/core/TransformStore.cpp:147-172,259-290,558-571`,
`src/core/SnippetStore.cpp:136-318`, `src/core/PersonalDictionary.cpp:91-191`,
`src/core/NotesStore.cpp:142-264`, `src/core/ApiKeyStore.cpp:29-115`, `src/core/HistoryStore.cpp:165-170`,
`src/ui/theme/StudioPalette.h:60-61`.
Commands run: `git diff c9df9df..HEAD --stat/name-only/log` and full `src/ tests/` diff;
`.\build\lwf_tests.exe --test-case="*dictionary page*"` -> 14/14 PASS (130 asserts);
`--test-case="*snippets page*"` -> 18/18 PASS (159 asserts). Full configure/build/ctest taken as reported
in IMPR-W4-01 §7 (not rerun; read-only delta scope). No Windows runtime, no zig G3.

## 1. Blind-pass findings (before reading implementer reports)

**R-W4-001 construction + wiring — FIXED, verified.**
`src/main.cpp:65` `#include "ui/pages/SnippetsPage.h"`,
`src/main.cpp:543` `full.SetSnippetsPage(std::make_unique<lwf::ui::pages::SnippetsPage>(snippets))`.
`snippets` is the same instance loaded at `main.cpp:393-397` (`Load(snippets.json)`, `SetMode`) and handed to
the pipeline at `main.cpp:412` (`pipeline.SetSnippets(&snippets)`), so page edits and pipeline expansion
share one `SnippetStore&`. Dispatch already existed: `MainWindow::DrawSnippetsPage` (MainWindow.cpp:1403-1410)
null-guards then calls `snippets_page_->Draw(scale)` with the stub fallback retained; rail `##nav_snippets`
reaches it via `Page::Snippets`. The `DataResetStores` block (`main.cpp:544-555`, `MainWindow.h:118-133`,
`MainWindow.cpp:295-297`) wires all 8 non-owning pointers. Lifetime is safe: every store is declared
before `MainWindow full` (`main.cpp:312,313,390,392,393,471,477,478` vs `534`), so the pointers outlive
every `RunOnce`/`DeleteLocalData`. `DeleteLocalData` null-guards each pointer (MainWindow.cpp:721-745),
so a `MainWindow` constructed without the wiring degrades to the old file-only delete.

**Delete-all clears in-memory holders — FIXED, with one ordering defect (R-W4-DELTA-001).**
`MainWindow::DeleteLocalData` (MainWindow.cpp:700-747) removes primary files plus `.bak`/`.tmp` for
stats/dictionary/snippets/transforms/api_keys/history, then clears RAM: `DictionaryStore::Clear()`,
legacy `PersonalDictionary::SetWords({})+SetReplacements({})`, `SnippetStore::SetEntries({})`,
`StatsStore::Clear()`, `TransformStore::Reload()`, `NotesStore::Reload()`, `ApiKeyStore::Reload()`,
`HistoryStore::Clear()`. The per-store choices are correct against each store's missing-file semantics
(verified): `DictionaryStore::Load` on a missing file calls `SeedLocked` with one `"btw"` entry
(DictionaryStore.cpp:351-356), so `Clear` (not `Reload`) is the only true empty; `PersonalDictionary::Load`
on a missing file returns false *without* clearing `words_` (PersonalDictionary.cpp:95-96), so `Set*` is
the only way to empty enforcement; `SnippetStore::Load` on missing returns false without clearing
(SnippetStore.cpp:138-140), so `SetEntries({})` is correct; `NotesStore::Reload` on a missing dir yields
empty (NotesStore.cpp:150-152); `ApiKeyStore::Reload` on missing yields empty (ApiKeyStore.cpp:85-88);
`HistoryStore::Clear` truncates (HistoryStore.cpp:165-170); `StatsStore::Clear` persists `"{\n}\n"` and
bumps `generation_` (StatsStore.cpp:191-196) so Insights refreshes; `TransformStore::Reload` re-loads
from `path_` and, on a missing file, seeds the 10-slot defaults (TransformStore.cpp:259-279). All new
methods are lock-correct: `DictionaryStore::Clear` and `StatsStore::Clear` hold the (non-recursive) mutex
once; `TransformStore::Reload` copies `path_` under lock then delegates to `Load(path)` (no lock held),
and its empty-`path_` branch calls `SeedDefaults()` (which locks internally, TransformStore.cpp:148) without
holding the lock. No double-lock, no deadlock.

**No double-Save resurrection — verified.**
`DictionaryStore::Clear()` writes nothing (DictionaryStore.cpp:408-415); `SnippetStore::SetEntries({})`
writes nothing (SnippetStore.cpp:292-318); `StatsStore::Clear` and `TransformStore::Reload` re-persist
only the *cleared/defaults* state, never old entries. Every store's later `Save()` dumps its current
(empty) RAM, so a post-delete `Save` cannot restore old bytes. A pending dictionary commit after `Clear`
is a no-op for resurrection: `CommitPendingDelete` does `Remove` -> false, `Save` writes the empty array,
and `dict->Reload` is guarded by `removed && saved` (DictionaryPage.h:184-191), so no stale reload.
**Delete-all twice is idempotent** in RAM and on disk (re-running the removals is `error_code`-tolerant;
`Clear`/`SetEntries({})`/second `Reload`-on-missing all converge to the same empty/defaults state), with
the single exception of R-W4-DELTA-001 below.

**Dictionary Reload on mutations — FIXED, verified for all three paths.**
`SubmitAdd`/`SubmitUpdate` already reloaded (`dict->Reload()` after `Save`); the diff adds the same call to
`CommitPendingDelete` (DictionaryPage.h:178-192), to the immediate commit-of-previous inside
`RequestDelete` (DictionaryPage.h:227-236), and the expired-path wrapper (198-208). `DictionaryPage::Draw`
now calls `CommitExpiredPending(store_, state_, dict_, ...)` (DictionaryPage.cpp:127) and `DrawRows` calls
`RequestDelete(store_, state_, dict_, ...)` (DictionaryPage.cpp:324). The old 3/4-arg signatures are
preserved as `nullptr` wrappers (DictionaryPage.h:194-196, 206-208, 248-251), so existing tests compile
unchanged. `PersonalDictionary::Reload` reads the same `dictionary.json` the store just saved and parses
the `schema_version: 2` entries shape (PersonalDictionary.cpp:108-129), so a mid-pipeline dictionary edit
is visible to `TextPipeline` on the next dictation without restart. The `RequestDelete` strengthening
(discarding a stale pending instead of `return false` when the previous id is already absent) only changes
the absent-id case; the covered case (previous pending present) commits identically.

## 2. Claims audit
| Implementer claim | Verified? | Evidence / finding |
|---|---|---|
| R-W4-001 fixed; shared `snippets` instance + Save path (IMPR §7) | PARTIAL | Construction + shared instance verified (above). "Save path" verified only when `snippets.json` existed at startup (`Load` succeeds -> `path_` set, SnippetStore.cpp:231). First-run missing file leaves `path_` empty (`Load` returns false at :138-140 without setting `path_`; `SetEntries` does not set it) and `Save()` then returns false (SnippetStore.cpp:280-290) while `SubmitSnippet` silently ignores it (`(void)store.Save()`, SnippetsPage.h:97). See R-W4-DELTA-002 (pre-existing, newly reachable). |
| R-W4-002: all 8 in-memory stores cleared; next Save resurrects only new entries (IMPR §R-W4-002) | PARTIAL | 8 clears verified. Overstated for one order: delete-all *before* the first Dictionary open is defeated by lazy `Load` reseeding `"btw"` (see R-W4-DELTA-001). Otherwise exact, including `stats "{}` + Generation bump and `transforms` defaults-by-design. |
| R-W4-003: delete commit reloads pipeline dict; mutations covered (IMPR §R-W4-003) | YES | Three reload sites + `removed && saved` guard verified; old signatures still compile (suite 14/130 green rerun). |
| R-W4-004: `usage_count` deferred, no producer invented; needs phrase->id bridge (IMPR §8) | YES (honest) | `RecordUsage` still has zero callers in `src/` (definition DictionaryStore.cpp:613 + header + tests only; `TextPipeline`/`PersonalDictionary` produce only aggregate `dictionary_fixes`). `StatsStore` carries no phrase/id. Deferral is the correct call; proposed T-027 is appropriate. |
| 5s-undo residual: single-entry Undo stash survives delete-all; Dictionary has no risk (IMPR §§6,10) | YES (honest) | `toast_dwell_s = 5.0f` (StudioPalette.h:60-61); Snippets `RequestDelete` removes+`Save`s immediately (SnippetsPage.h:119-120) so Undo re-`Upsert`s one entry within 5s; Dictionary `RequestDelete` only stages (no `Remove` until commit) and `UndoPendingDelete` merely drops the pending (DictionaryPage.h:210-213), restoring nothing. The snippet side is bounded (explicit Undo click, 5s expiry) and correctly documented as accepted. |
| `tests/` = 0 lines; no new doctest for Clear/Reload/delete wiring (IMPR §10) | YES (gap disclosed) | `git diff c9df9df..HEAD -- tests/` is empty. Existing suites rerun green, but nothing fails CI if the wiring is later deleted (see R-W4-DELTA-003). |

## 3. Long-run test (independent answers)
- **Source:** The fix lives at the right seams. Stores stay the single writers of their files; `MainWindow`
  holds non-owning store pointers (the narrowest bridge available without touching `UiBus`, which is
  legitimately outside the write-set). No new abstraction, no format/schema change.
- **Load:** Per-frame cost added is one `RoleForConfigKey` branch per `StringFieldRow` plus one
  `LoadBindings` per hotkey deactivation (config strings only, no disk). Delete-all is O(files+entries).
  No hot-path change.
- **Hostile:** Malformed/conflicting typed chords can no longer be saved (mirror of the recorder contract);
  delete-all with missing files/baks is `error_code`-tolerant. Watch item: the dictionary seed reseed below
  is the one hostile input (first-visit ordering) that reintroduces data after a "delete all".
- **Operability:** Insights refresh on stats delete via the `generation_` bump; transforms show built-ins
  (by design, not user data); the misleading `ShowSaved` after delete is gone. The lazy-load seed (below)
  is the one operator-visible surprise left.
- **Reversal:** Reverting is the 2-line `main.cpp` removal + struct wiring + 3 additive store methods +
  DictionaryPage overloads (old signatures still compile). No file-format change, so pre-task binaries read
  task-written files. Delete remains intentionally irreversible.

## 4. Findings
### R-W4-DELTA-001 [MAJOR] Delete-all before first Dictionary visit resurrects the `"btw"` seed
- Location: `src/ui/pages/DictionaryPage.cpp:116-121` × `src/core/DictionaryStore.cpp:351-356` ×
  `src/core/DictionaryStore.cpp:408-415`, `src/main.cpp:392` (store never pre-loaded).
- Evidence: `main.cpp` constructs `DictionaryStore` but never calls `Load()`; the only `Load()` is the
  lazy first-`Draw` (`!state_.load_attempted` -> `store_.Load()`). `Load()` on a missing file calls
  `SeedLocked()` (one `"btw"->"by the way"` entry). `DeleteLocalData` deletes the file then calls
  `Clear()` (RAM empty, file absent). If the user never opened the Dictionary page before Privacy ->
  Delete all, the *next* Dictionary `Draw` calls `Load()`, finds no file, and reseeds `"btw"`. A later
  `SubmitAdd` then reloads the legacy dict from a file containing `btw` + the new word, so the pipeline
  enforces a phrase the user explicitly deleted. This contradicts the implementer's own expected state
  ("dictionary 0", IMPR §R-W4-002) and breaks delete-all-twice idempotency across the lazy-load boundary:
  delete-all after visiting stays empty; delete-all before visiting returns one entry.
- Why it matters: Low privacy harm (factory seed, not user text) but a contract violation of "delete all"
  and operator confusion ("I deleted everything, where did this come from?"). The fix in `Clear` correctly
  avoids `Reload` for exactly this seed reason — the lazy `Load` then reintroduces it through the back door.
  `StatsStore::Clear` does not have this bug because it persists `"{}"` (file exists, so no reseed).
- Recommended fix (root-cause): make `Clear` leave a loadable empty file (mirror `StatsStore::Clear`:
  persist the empty `EntriesToJson({})` under the same lock, or persist then keep RAM empty), so the
  subsequent lazy `Load()` reads an empty entries array instead of the missing-file seed path. Alternative
  (worse): gate the lazy `Load` on `store_.Loaded()` — note `Clear` sets `loaded_ = true` precisely so this
  would work, but gating on `Loaded()` alone would also skip the legitimate first-ever load. The persist-empty
  fix is one method, matches the stats precedent, and keeps `Clear`'s callers unchanged. Add a doctest:
  `Clear` on a store whose file was deleted does not reseed on next `Load`.

### R-W4-DELTA-002 [MINOR] First-run snippet edits never reach disk (pre-existing, now reachable)
- Location: `src/core/SnippetStore.cpp:136-140,231,280-290`, `src/main.cpp:393-397`,
  `src/ui/pages/SnippetsPage.h:97` (pre-existing file, not in diff; exposed by the R-W4-001 wiring).
- Evidence: `SnippetStore::Load` sets `path_` only on the success path (:231). On a missing/corrupt file it
  returns false without recording the path, and the `main.cpp` fallback `SetEntries({})` does not set the
  path either. `Save()` then returns false; `SubmitSnippet`/`RequestDelete` ignore the failure with
  `(void)store.Save()`, so the edit shows "Saved" in memory and expands for the session but is lost on
  restart. The existing disk test (`tests/test_snippets_page.cpp:259-276`) first does `Save(path)` then
  `Load(path)`, so it only covers the already-exists case. This is *not* introduced by this diff, and I do
  not count it as a new BLOCKER/MAJOR for the delta — but the delta is what makes it user-reachable, and
  the "snippet edit persists" claim needs the qualifier above.
- Recommended fix (follow-up, outside this delta): have `Load` record `path_` before the open attempt
  (mirroring `PersonalDictionary::Load`, PersonalDictionary.cpp:93) so `Save()` can create the file on
  first write, and surface `Save()` failure in `SubmitSnippet` instead of discarding it. Add a first-write
  doctest (fresh store, `Load` on absent path, `SubmitSnippet`, reload from disk).

### R-W4-DELTA-003 [MINOR] Zero new regression coverage for the fixed blockers (no `tests/` in diff)
- Location: `git diff c9df9df..HEAD -- tests/` (empty).
- Evidence: `DictionaryStore::Clear`, `StatsStore::Clear`, `TransformStore::Reload`, `MainWindow::DeleteLocalData`
  clearing, `DataResetStores` wiring, and the dict-overload reload path have no doctest. Grep of `tests/` for
  `Clear()` hits only unrelated fakes/history/scratchpad. The existing dictionary-page (14/130) and snippets-page
  (18/159) suites rerun green and pin the old signatures, but nothing fails CI if `SetSnippetsPage` is deleted
  again (the construction-scan gap the implementer already disclosed in IMPR §10, still open) or if a future
  change reintroduces resurrection. The implementer could not add tests (tests/ outside write-set); the
  integrator/orchestrator should.
- Recommended fix: add three doctests — (1) `Clear` leaves the store empty through a `Reload`/`Load` cycle
  (would catch R-W4-DELTA-001), (2) `DataResetStores`-style clear of every store leaves no bytes resurrecting
  on the next `Save`, (3) a `main.cpp` source scan asserting `SetSnippetsPage(snippets)` (alongside the
  existing font-sources pin).

### R-W4-DELTA-004 [NIT] `TransformStore::Reload` empty-path branch leaves stale warnings
- Location: `src/core/TransformStore.cpp:558-571` vs `259-279`.
- Evidence: The `path_.empty()` branch calls `SeedDefaults()` and sets `loaded_ = true` without clearing
  `load_warnings_`/`warnings_`, unlike `Load(missing)` which clears them after seeding. Production never hits
  this (main sets `path_` via `Load`), so impact is nil; noted for symmetry only.

## 5. Attempts to break it
- **Snippet edit persists (existing file):** traced `OpenEdit/SaveEdit` -> `SubmitSnippet` -> `Upsert` +
  `Save()` on the shared instance (`SnippetsPage.h:226-240, 77-99`); pipeline reads the same instance.
  Existing snippet suite green (18/159). **First-run (missing file):** `path_` empty -> `Save()` false,
  silently dropped — confirmed by source path, see R-W4-DELTA-002.
- **Delete-all twice:** reasoned both orders. Post-first-visit: idempotent (all clears converge; stats
  re-persist `"{}"`, transforms re-persist defaults, history re-truncates, notes/api_keys reload-empty,
  snippet/dictionary RAM empty + file absent). Pre-first-visit: **not** idempotent — lazy `Load` reseeds
  (R-W4-DELTA-001). No double-Save resurrection in either order (cleared RAM is what `Save` dumps).
- **Dictionary edit mid-pipeline:** `SubmitUpdate` -> `Save` -> `dict->Reload` -> `PersonalDictionary::Load`
  parses the v2 entries shape -> next `Enforce` sees the new phrase. Delete commit follows the same path
  (`removed && saved` guard); undo drops the staging without touching the file. No stale-enforcement window
  beyond the normal pending-delete dwell (5s by design).
- **Pending-delete × delete-all:** dictionary commit after `Clear` -> `Remove` false -> `Save` writes empty,
  no dict reload, no resurrection. Snippet stash after delete-all -> one-entry restore only via explicit
  Undo click within 5s (documented, accepted). Scratchpad new-note-after-delete is new input, not replay.
- **`usage_count`/5s-undo honesty:** `RecordUsage` zero-caller status reconfirmed by grep (definition +
  tests + prior reports only); deferral to T-027 is honest. 5s dwell reconfirmed as
  `toast_dwell_s = 5.0f` with `delete_at = now + toast_dwell_s` on both pages.

## 6. Dimension checklist
- Correctness: FIXES R-W4-001/002/003 as claimed; one new ordering defect (R-W4-DELTA-001).
- Root cause vs symptom: Pass — clears happen at the store seam, not by masking saves; stale pending
  strengths are the documented immediate-commit, not a workaround.
- Regression/compat: Pass — additive methods and overload-preserving signatures; suite green; no format change.
- Concurrency/idempotency: Pass except R-W4-DELTA-001 ordering; mutex discipline verified, no lock-order issue.
- Error handling: Pass — `Save` failures on the new reload paths are guarded where it matters (`removed && saved`);
  pre-existing silent `(void)Save` on snippets noted (R-W4-DELTA-002).
- Security/abuse: Pass — delete-all removes `.bak`/`.tmp` residue so deleted bytes are not recoverable by
  rename; hotkey validation closes the typed-conflict hole. `.bad` quarantine files intentionally retained
  (not read by loaders; diagnostic only) — acceptable.
- Performance/scalability: Pass — no per-frame I/O added; delete-all cost unchanged in kind.
- Migration/rollback: Pass — no schema/JSON change; plan-file revert restores old reading.
- Observability: Pass — Insights refresh via `generation_`; delete disarm has no fake "Saved".
- Maintainability/conventions: Pass — naming mirrors existing store idioms; hygiene/no-network reported OK
  (taken as reported; no `windows.h`/network in diff by inspection).
- Docs impact: Pass — Privacy copy updated for history; transforms-defaults and the undo-stash residual are
  accurately documented in IMPR-W4-01.

## 7. Optimization and simplification opportunities
| # | Location | Evidence | Impact | Effort | Risk | Verdict (DO NOW / DO LATER / NOT WORTH IT) |
|---|---|---|---|---|---|---|
| 1 | `DictionaryStore::Clear` persist-empty (from R-W4-DELTA-001) | stats precedent; closes the lazy-load reseed | Correctness, small | S | Low | DO LATER (follow-up task; not a merge blocker given seed is factory data) |
| 2 | Snippet first-write `path_` + surfaced `Save` failure (R-W4-DELTA-002) | first-run edits lost | Correctness, small | S | Low | DO LATER |
| 3 | Three doctests (R-W4-DELTA-003) | no CI tripwire | Regression prevention | S | None | DO LATER (integrator) |

## 8. Cross-agent / cross-task interactions observed
- The `DictionaryPage` lazy-load design (load on first `Draw`) interacts with the new `DataResetStores` seam
  — the only place where two individually-correct changes compose into a bug (R-W4-DELTA-001). Neither the
  implementer (write-set excluded `DictionaryPage.cpp` lazy load semantics) nor the original review flagged
  the ordering.
- `PersonalDictionary` (read-only in this task) correctly *requires* `Set*` over `Reload` for delete-all;
  the implementer's choice here matches its missing-file contract and is the reason the legacy side has no
  equivalent of R-W4-DELTA-001.
- The `Not available yet. == 5` stub fallback is preserved (stub path verbatim), so the overlay-settings pin
  is unaffected by the wiring.

## 9. What I could not verify
- Windows runtime (Privacy confirm flow, snippet sheet, recorder round-trip, DPI) — no GUI run in this
  environment; manual checklist in IMPR-W4-01 §10 still stands, now plus "delete-all before opening
  Dictionary -> Dictionary must be empty".
- Full `cmake` configure/build + `ctest` — taken as reported in IMPR-W4-01 §7; only the two scoped doctest
  suites were rerun here.
- zig G3 type-check (no zig on box per AGENTS.md) and the MSVC lane (CI-only).
- Live ingest of a real `dictionary.json` through `PersonalDictionary::Reload` after `DictionaryStore::Save`
  (format traced, not executed against the Windows data dir).
