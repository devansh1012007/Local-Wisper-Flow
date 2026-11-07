# IMPR-W1-01 — Wave 1 review findings improver (W1-IMPR-01)

## ORCH-DIGEST

**Status: COMPLETE.** All 1 BLOCKER + 6 MAJORs FIXED root-cause, all 4 TREV P1 gaps pinned with tests, MINOR/NIT dispositioned finding-by-finding below (14 fixed, 5 deferred with reason, 1 dependency-deferred outside write-set). Full doctest suite green: **348 test cases, 13183 assertions, 0 failed** [RAN ctest -R lwf-tests -> Passed 18.69s]. Hygiene + no-network OK [RAN]. Main app target `local-wisper-flow` also builds against the changed headers [RAN]. No forbidden files touched (verified git diff --stat: only src/core stores + tests). Mutation evidence: 4 mutants killed (TextDiff tie, Dictionary Add validation, CategoryShares remainder, Enforce cursor hang reproduced under timeout mutant then restored).

## Context Report (pre-change investigation)

- BLOCKER root: `PersonalDictionary::Enforce` looped per rule until no whole-word match; a replacement containing its own `from` as a whole word re-matched forever [VERIFIED src/core/PersonalDictionary.cpp:266-280 pre-change]. Reachable via `DictionaryStore::Add` with no replacement validation [VERIFIED src/core/DictionaryStore.cpp:434-477 pre-change].
- MAJOR-1: Snippet/Transform accessors returned `const&` to mutex-guarded state without the lock [VERIFIED SnippetStore.h:26-28, TransformStore.h:31-37 pre-change]. Precedent for copies: DictionaryStore/NotesStore/StatsStore already returned copies under lock.
- MAJOR-2: Snippet/Transform `Save` snapshotted under lock then wrote unlocked [VERIFIED SnippetStore.cpp:199-235, TransformStore.cpp:421-461 pre-change]; DictionaryStore `Save` held the lock across the whole write (precedent followed).
- MAJOR-3: PersonalDictionary legacy replacements used unchecked `.get<std::string>()` [VERIFIED PersonalDictionary.cpp:142-149 pre-change]; sole caller main.cpp:370 has no try/catch (read-only check, not edited).
- MAJOR-4: PersonalDictionary had no mutex; new `Reload` wrote `words_/replacements_/path_` while `Enforce` read them on the engine thread [VERIFIED PersonalDictionary.h:21-32 pre-change].
- MAJOR-5: TransformStore had no duplicate-name check; TransformManager::LoadFile throws on exact duplicate name [VERIFIED TransformManager.cpp:89-94, read-only file outside write-set].
- MAJOR-6: Snippet `Upsert` used `trigger OR id` predicates in both empty and non-empty paths [VERIFIED SnippetStore.cpp:410-432 pre-change].
- Callers of changed APIs verified by grep before changing signatures: main.cpp assigns `dictionary.Words()` to `vector<string>` keyterms/keywords members (copy-safe) [VERIFIED src/main.cpp:414,425,433 + provider headers]; TextPipeline uses only SnippetStore::Loaded/Expand [VERIFIED src/core/TextPipeline.cpp:100-101]; tests bound `const Entry&` to `Entries()[i]` in exactly 2 places (fixed to copies).
- Invariants preserved: no code comments (AGENTS.md, hygiene OK); JSON v2 shapes and `.bak`/`.bad` sidecars unchanged; all previously pinned exact messages unchanged except the new duplicate-transform message (new contract, pinned by new test); caps now count code points (ASCII behavior identical, existing boundary tests still pass).

## Phased Changes Executed

### Phase 1 — PersonalDictionary (BLOCKER, MAJOR-3, MAJOR-4)
- `Enforce`: per rule, single left-to-right pass over a snapshot of the current text (`source`), never rescanning inserted text; cursor advances to end of each match in the snapshot, so termination is structural (cursor strictly increases for non-empty `from`). Sequential rules still chain (next rule scans the previous rule's output), preserving legitimate cross-rule behavior. Identity replacements count 0, exactly as before.
- `DictionaryStore::Add/Update/ImportJson` (Phase 2) reject replacements containing the phrase as a whole word (case-insensitive, alnum boundaries mirroring `Enforce`), message `replacement cannot contain the phrase`. Substring-only containment (`foo -> foobar`) remains allowed and terminates.
- Thread safety: added `mutable std::mutex`; `Load` holds it for the whole parse; `Reload` copies `path_` under lock then calls `Load`; `SetWords/SetReplacements/AddWord` lock; `Words()/Replacements()` return copies under lock; `Loaded()` locks; `InitialPrompt` and `Enforce` snapshot state under lock then compute unlocked (Reload is never blocked by a long Enforce, Enforce never sees torn vectors).
- MAJOR-3: legacy `from`/`to` now guarded by `is_string` (skip invalid, matching the v2 branch and DictionaryStore::ParseLegacy).
- DECISION: copy-on-read snapshot in Enforce (copies up to 2000 short strings per dictation) over holding the lock during compute — Reload latency is bounded by a memcpy, Enforce latency is unchanged asymptotically.

### Phase 2 — DictionaryStore
- Self-reference validation in Add/Update/ImportJson (above); ImportJson validates before mutating, leaving the store unchanged on rejection.
- MINOR-2: new `bak_pending_` flag — first `Save` after `ImportJson` or after a truncating `Load` creates `.bak` from the on-disk file (create-if-absent, existing once semantics). `ExportFile` now writes tmp+flush+rename with tmp cleanup instead of a torn direct write.
- MINOR-7: `MoveToBad` helper — on rename failure, remove existing `.bad` and retry once, else log. Used by both corrupt branches.
- Save tmp flush-failure now closes + removes tmp (was left behind).

### Phase 3 — SnippetStore (MAJOR-1, MAJOR-2, MAJOR-6, MINOR-5, MINOR-10, NIT-1)
- `Entries()/Warnings()` return copies under lock; `Loaded()` locks. `Save(path)` holds the lock across serialize+tmp-write+`.bak`+rename (no-arg `Save()` still extracts the path under a separate lock scope, then delegates — no double-lock).
- `Upsert`: when `exclude_id` is set, both empty-expansion delete and non-empty find match by id alone; otherwise by trigger. Rename onto another entry's trigger with empty expansion now deletes only the edited entry; with non-empty expansion, `ValidateLocked` (already id-alone for self) rejects the collision before any mutation.
- Caps count code points (`CountCodePoints`: non-continuation bytes) in Load/Validate/Upsert — 4000 CJK chars (12000 bytes) now pass, 4001 fail; ASCII identical.
- NIT-1: Load pre-scans file-provided ids into a reserved set; generated ids advance past reserved + already-assigned ids; duplicate explicit ids are regenerated (no two entries share an id).
- MINOR-10: load warnings still returned via `Warnings()` for the UI, but the log line is now a count only (`snippets: N load warnings`) — no user trigger text in the log file.
- Bonus tolerant-load fix in the same class as MAJOR-3: `id`/`created_at`/`updated_at` now type-guarded instead of `value()` (which throws on type mismatch).

### Phase 4 — TransformStore (MAJOR-1, MAJOR-2, MAJOR-5, MINOR-5, NIT-5)
- `Entries()/Warnings()/DiffShortcut()` return copies under lock; `Loaded()` locks. `Save(path)` holds the lock across the whole write.
- MAJOR-5 contract (Store rejects, matching Manager): `ValidateLocked` (new; public `Validate` locks and delegates, `SetSlot` calls the locked variant — no double-lock) returns `Transform name already exists` (case-insensitive, excluding the slot being edited) for any non-empty name already used by another slot including built-ins. `Load` prunes duplicates first-wins with a warning (v2 post-pass; v1 skips duplicates incl. built-in names) so a Store-loaded file can never be one the Manager throws on. Empty names stay allowed (Manager skips empty slots in v2).
- NIT-5: `EnsureAllSlotsLocked` stamps `created_at/updated_at` for filled built-in slots; duplicate-shortcut detection keys on the normalized `TokenSet` (so `Control+Alt+1` collides with `Ctrl+Alt+1`).
- Caps count code points in Validate. Bonus tolerant-load guards: v2 `enabled/diff_shortcut/slot/name/prompt/shortcut/built_in/timestamps` now type-guarded instead of throwing `value()`.

### Phase 5 — StatsStore, NotesStore, InsightsModel (MINOR-1, MINOR-4, MINOR-7, NIT-3, NIT-4)
- StatsStore: `PersistLocked` returns bool (open/flush/rename failures clean tmp and return false). `Record` advances `Generation()` only on successful persist. Empty path returns true (intentionally unpersisted in-memory store is not a failure — documented choice).
- StatsStore `LoadLocked` clamps all negative day/category/app values to 0 (mirrors `Record` clamping).
- NotesStore: `Create`/`Restore` remove the body file when index write fails (previously the orphan was resurrected by the next Load's adoption sweep). `.bad` rename retries after removing an existing `.bad`.
- InsightsModel `ParseDayKey` validates month lengths incl. leap years (Feb 31 / Feb 29 non-leap / Apr 31 rejected).

### Phase 6 — Tests (TREV P1 + P2, regression pins)
- test_dictionarystore: BLOCKER regression (foo->foo foo terminates, count 2), substring-only allowed, identity no-op, Add/Update/Import self-ref rejection, mutation error paths (empty phrase, missing id for Remove/SetStarred/RecordUsage/Update, duplicate Update), Import corrupt/unsupported unchanged, Import-then-Save `.bak`, ExportFile atomicity, `.bad` replacement, PD malformed legacy (no throw), PD Reload+Enforce thread test (50 reloads x 500 enforces).
- test_textdiff: independent LCS oracle battery (9 pairs, Equal==LCS + token-count conservation), full op-sequence pin for the tie case `a x -> x a` = [Delete a, Equal x, Insert a].
- test_insights: CategoryShares exact recipient/order `{a:34,b:33,c:33}` from scrambled insertion, sort order, invalid day keys (Feb31/Feb29-nonleap/Apr31/malformed), unsorted/duplicate-day streaks, Wpm negatives, CalendarLevel 24 boundary.
- test_statsstore: negative clamping via Record and via file load, empty process -> `unknown` app + Other tasks, Generation stays 0 on persist failure (missing dir), CountWords NBSP/ideographic (fixes mislabeled case).
- test_snippetstore_v2: per-case temp dirs (was one fixed dir), exclude_id rename (keeps id, other entry untouched), rename-onto-other rejected + empty-expansion deletes only self, RemoveByTrigger, contains-mode equal-length tie-break (aa->bb->cc pins alphabetical), code-point cap, id-collision-free generation, non-string id/timestamps tolerated.
- test_transformstore: per-case temp dirs, slot range/name validation, duplicate-name rejection (incl. built-in, self-edit allowed), empty slot never enabled, custom DiffShortcut persistence, v1 overflow/empty/duplicate skips, v2 duplicate-name prune + filled built-in timestamps, code-point cap, token-normalized shortcut conflict.
- test_notesstore: failed Create leaves no resurrectable body (index path blocked by a directory), duplicate index ids keep first.
- test_atomicfile: replaced the vacuous case (never called the unit) with a real directory-target failure + tmp-cleanup case; added binary/NUL roundtrip.
- Fixed 2 test call sites that bound `const Entry&` to a by-value `Entries()` temporary (now copies).

## Disposition table

| ID | Finding | Disposition | Evidence |
|---|---|---|---|
| BLOCKER-1 | Enforce infinite loop on self-referential replacement | FIXED (both layers) | Single-pass Enforce + Add/Update/Import validation; test `self referential replacement terminates in Enforce` + `dictionary add update and import reject self referential replacements` PASS; cursor mutant reproduced hang (TIMED-OUT 15s), restored version passes |
| MAJOR-1 | Unlocked Entries/Warnings/Loaded/DiffShortcut refs | FIXED | All return copies under lock (Snippet/Transform); PD Words/Replacements likewise. Full suite PASS incl. new thread test |
| MAJOR-2 | Save lost-update (snapshot then unlock) | FIXED | Snippet/Transform Save hold lock across serialize+write+rename; deadlock audit: Load->Save call sites hold no lock |
| MAJOR-3 | PD legacy .get crash on malformed JSON | FIXED | is_string guards; test `personal dictionary malformed legacy replacements do not throw` PASS |
| MAJOR-4 | PD Reload/Enforce thread race | FIXED | mutex + snapshot Enforce; test `personal dictionary reload and enforce are thread safe` PASS (50x500) |
| MAJOR-5 | Store allows duplicate names Manager throws on | FIXED (Store rejects, matching Manager) | ValidateLocked duplicate check + Load prune first-wins; tests `duplicate transform names are rejected by the store`, `v2 file with duplicate names keeps the first...` PASS |
| MAJOR-6 | Upsert rename/delete collides two entries | FIXED | id-alone predicates when exclude_id set; tests `upsert rename onto another trigger does not delete the other entry`, `upsert with exclude id renames only that entry` PASS |
| MINOR-1 | Stats Generation bumps on failed persist; tmp left | FIXED | PersistLocked bool + tmp cleanup; test `stats generation does not advance when persistence fails` PASS |
| MINOR-2 | Import-then-Save no .bak; ExportFile torn write | FIXED | bak_pending_ flag; ExportFile tmp+rename; tests `...import replaces entries and next save creates a bak`, `...export file writes atomically...` PASS |
| MINOR-3 | Truncation/skip-on-load destroyed by next Save | FIXED (with residual, see Risks) | bak_pending_ on truncating Load + Import; Snippet/Transform first Save already .baks the pre-save file. Load still logs a warning (never silent) |
| MINOR-4 | Notes Create/Restore rollback resurrects body | FIXED | RemoveBodyLocked on index failure; test `failed create removes its body...` PASS |
| MINOR-5 | Caps are bytes not chars | FIXED | CountCodePoints in Snippet (4000) + Transform (2000); tests pin 4000/4001 CJK + 2000/2001 CJK |
| MINOR-6 | TransformManager v1 exception drift | DEFERRED-DEPENDENCY | TransformManager.* is outside my write-set (forbidden to edit); valid-file contract unchanged, drift is on invalid files only. NEED posted in report for orchestrator |
| MINOR-7 | .bad rename failure ignored | FIXED | MoveToBad (Dictionary) + retry (Notes); test `dictionary corrupt load replaces an existing bad file` PASS. Snippet/Transform intentionally still do not .bad-rename corrupt files (existing test pins file-untouched) |
| MINOR-8 | ASCII-only casefolding | DEFERRED (design) | Full Unicode case folding needs tables + per-store claim audit; ASCII-only is consistent across all stores (inherited). Downgrading claims is a docs decision for orchestrator |
| MINOR-9 | AtomicFile zero callers, divergent copies | DEFERRED (design) | Converging 5 stores onto WriteAtomic (with .bak/mkdir policy) is a wave-scale refactor with regression risk; all copies now share the same tmp-cleanup discipline. Recommend as Wave 2 follow-up |
| MINOR-10 | Snippet trigger text in logs | FIXED | Log line is now count-only; warnings_ content retained for UI display |
| NIT-1 | Snippet generated-id collision | FIXED | Reserved-set generation; test `load generates ids that do not collide...` PASS |
| NIT-2 | TextDiff O(n*m) no size guard | DEFERRED (design) | A cap needs a specified degraded return (empty? truncated?) before Wave 2 feeds transcripts; no silent wrong answer shipped instead. No prod caller feeds unbounded input today |
| NIT-3 | ParseDayKey accepts Feb 31 | FIXED | Month-length + leap validation; test `invalid day keys never form streaks` PASS |
| NIT-4 | Stats Load accepts negatives | FIXED | Clamped to 0 on load; test `stats load clamps negative day values to zero` PASS |
| NIT-5 | Zero timestamps + string-exact shortcut dedup | FIXED | Built-in fill stamped; TokenSet-normalized dedup key; tests pin both |
| NIT-6 | NotesStore no size cap | DEFERRED (design) | Cap value/count is a product decision; scratchpad bodies are small today. No unbounded-growth prod path identified |
| REV Opt-1..4 | Enforce/Expand/Add perf, AtomicFile convergence | DEFERRED | No behavior change needed; Enforce now snapshots (memcpy-bound). Opt-4 = MINOR-9 above |
| TREV P1-1 | TextDiff Equal==LCS + tie order unpinned | FIXED | Oracle battery + full sequence pin; `>`->`>=` mutant killed (5 assertions failed under mutant) |
| TREV P1-2 | CategoryShares recipient + sort unpinned | FIXED | Exact `{a:34,b:33,c:33}` test; remainder-to-last mutant killed (2 assertions failed) |
| TREV P1-3 | Dictionary mutation error paths untested | FIXED | `dictionary mutation error paths` + `dictionary import corrupt and unsupported...` |
| TREV P1-4 | Transform slot/name validation untested | FIXED | `validate and set slot reject...`, `duplicate transform names...`, `empty slot is never slot enabled...`, `custom diff shortcut persists...` |
| TREV P2 (partial) | Snippet tie/exclude/RemoveByTrigger, Stats negatives/NBSP, Transform v1>8/DiffShortcut, Notes dup ids | FIXED | All pinned (see Phase 6). Remaining P2/P3 (Notes duplicate-id already done; Atomic binary/dir done; Dict legacy entries-shape variant not added — ParseAny treats unknown shapes as corrupt->.bad, existing behavior) |
| TREV hermeticity | Snippet/Transform fixed temp dir; Stats .tmp sidecar | FIXED (Snippet/Transform) / ACCEPTED (Stats) | Per-case dirs for snippet/transform. Stats TempPath still shares root by name (matches house pattern); .tmp is never read back by Load so residue is inert |
| TREV test bugs 5-6 | Vacuous atomic case; mislabeled CountWords | FIXED | Vacuous case replaced with real dir-target case; CountWords now actually exercises NBSP/ideographic space |

## Review Audit

- Phase 1 self-review: Enforce cursor strictly increases in the source snapshot (from non-empty, end>begin) so the per-rule loop runs at most #matches times — termination proof by construction, plus empty-`from` skip. Fix-count semantics preserved (identity counts 0, case-only change counts 1, multi-occurrence counts per occurrence) — verified against existing fix-count tests (still PASS). Reload lock ordering: Reload releases before calling Load (no self-deadlock). Severity of remaining issue: none.
- Phase 2: ContainsWholeWord mirrors Enforce boundaries (alnum only, ASCII fold) — `foo->foobar` allowed (test), `foo->foo foo` and `bar->BAR` rejected (test). Import validation runs before the lock/mutation, so a rejected import changes nothing (test asserts size + phrases). `bak_pending_` is never cleared by Load, only set — conservative (extra .bak creation is safe). Severity: none.
- Phase 3: Save lock covers copy+write+rename; no other method calls Save while holding the lock (Load does not call Save in SnippetStore). Upsert empty-path with exclude_id for a missing id is a no-op returning true — documented in report; no test relied on the old delete-both behavior. Reserved-id generation cannot collide with future explicit ids (pre-scan). Severity: none.
- Phase 4: Validate split verified no double-lock (SetSlot -> ValidateLocked; public Validate -> lock -> ValidateLocked). Load v1/v2 duplicate pruning verified against Manager semantics (Manager skips empty names in v2, throws on exact duplicates — Store now prevents exact and case-insensitive duplicates, a strict superset that is Manager-safe). Save lock: Load calls Save only outside lock scopes (missing-file and v1 paths re-checked). Severity: none.
- Phase 5: Stats empty-path-returns-true is the one deliberate semantic choice (see Risks). Notes rollback uses the existing RemoveBodyLocked (best-effort remove, returns bool ignored — same pattern as Delete's existing rollback). Insights leap rule (divisible by 4, except centuries not divisible by 400) — tested 2024 leap + 2026 non-leap. Severity: none.
- Final review: git diff --stat shows only write-set files (COMMS.md modification is pre-existing sibling state, not staged by me; briefs/ and w1-review/ untracked are sibling state). No code comments added (hygiene OK), no TODO/FIXME, no weakened assertions (only 2 reference-binding test sites changed to copies, required by the API fix; 1 vacuous test replaced with a stronger real test). Full suite + main app build green.
- Independent review: NOT obtained (single-shot subagent; no reviewer dispatched). Status is COMPLETE on my own gates; orchestrator may still dispatch a fresh-context re-review of the BLOCKER/MAJOR diffs per REV-W1-01 section 7 acceptance.

## Verification Evidence

Verbatim commands and outcomes (Windows MSYS2/MinGW box):

1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` -> `Configuring done (1.7s) / Generating done (13.5s) / Build files have been written` (only pre-existing deprecation warnings from vendored deps).
2. `cmake --build build --config Release --parallel 4 --target lwf_tests` -> `[100%] Built target lwf_tests` (one interim failure: doctest forbids `||` in CHECK at test_transformstore.cpp:287 — fixed by restructuring to an if+CHECK; rebuilt clean).
3. `ctest --test-dir build -R lwf-tests --output-on-failure` -> first run 347/348 passed, 1 failed (`expansion cap counts characters not bytes`: test used the 2000 transform cap for the 4000 snippet cap — test bug, fixed to 4000/4001 CJK); rerun -> `1/1 Test #1: lwf-tests ........   Passed   18.44 sec / 100% tests passed`. Final rerun after mutation restores -> `Passed 18.69 sec`. Total: **348 test cases, 13183 assertions, 0 failed**.
4. `cmake --build build --config Release --parallel 4 --target local-wisper-flow` -> `[100%] Built target local-wisper-flow` (only pre-existing grpc `empty_bases` header warnings) — proves main.cpp compiles against the by-value API changes.
5. `bash scripts/check_hygiene.sh` -> `hygiene: OK`. `bash scripts/check_no_network.sh` -> `no-network: OK`.
6. Mutation evidence (each mutant built, filtered test run, then restored and re-verified green):
   - TextDiff `>` -> `>=`: tie test FAILED `tokens[0].op 1==2` etc (5 assertions) — mutant killed.
   - DictionaryStore Add validation disabled (`false &&`): `dictionary add update and import reject...` FAILED (5 assertions, Add returned true) — mutant killed.
   - CategoryShares remainder to last share: remainder test FAILED `shares[0].second 33==34` (2 assertions) — mutant killed.
   - PersonalDictionary cursor `end` -> `begin`: `self referential replacement terminates in Enforce` TIMED-OUT after 15s (hang reproduced) — killed by timeout; restored version passes in ms.

## Unverified Assumptions & Remaining Risks

- TSAN/heaptrack thread testing NOT-RUN (no TSAN on this box). Thread safety is by lock-coverage reasoning + the new concurrent test, not by sanitizer. Recommend a TSAN pass before EngineBridge wiring (echoes REV-W1-01 section 6).
- G3 zig type-check NOT-RUN (no zig on box, per AGENTS.md standing note). Changed files are portable core (no windows.h added — hygiene OK proves).
- Windows rename-over-existing on MSVC NOT-RUN (this box is MinGW); MinGW rename semantics verified by the passing suite.
- No fsync anywhere (flush only) — power-loss durability is atomicity-only, unchanged from Wave 1. Accept if documented; recommend orchestrator records it.
- MINOR-3 residual: if a `.bak` already exists when a truncating Load/Import happens, DictionaryStore's create-if-absent `.bak` will not preserve the over-long original (once semantics). Truncation is still always warned at Load. Snippet/Transform `.bak` overwrites on every save, so the pre-truncation original survives in `.bak` for exactly one save cycle. Full refusal-to-save would wedge the store with no acknowledgement API — deferred as a design decision.
- StatsStore empty-path Generation semantics: empty path returns persist-success (in-memory-only store still signals changes). If Wave 2 wants Generation to mean strictly "on disk", that is a one-line change plus a test.
- DictionaryStore::Path() still returns `const&` — safe because `path_` is set only in the constructor and never mutated; left unchanged deliberately.
- Enforce/Expand perf optimizations (REV section 4) not done; Enforce snapshots add one vector copy per call (memcpy-bound, no asymptotic change).

## Long-run test (standing order 8)

- Source: root causes fixed at the shared layer (Enforce algorithm, validation at every mutation entry point, lock coverage on every accessor, single duplicate-name contract). The same bug class (unlocked ref, snapshot-then-unlock write, unchecked .get) was grepped across all 10 write-set files; the two bonus `value()` guards (Snippet/Transform Load) close the same class in sibling loaders.
- Load: 2000-entry dict + 4000-char snippet + 2000-char prompt boundaries all still pinned; Enforce is O(rules x text) single-pass (better than the old O(matches x text) re-scan loop); TextDiff O(n*m) unchanged (NIT-2 deferred with reason).
- Hostile: malformed JSON of every legacy shape now type-guarded in PD/Snippet/Transform loaders; self-referential, duplicate-name, duplicate-id, Feb-31, negative-stats hostile inputs all pinned by tests.
- Operability: no new log content (snippet log redacted to counts); all writes tmp+rename with tmp cleanup on every failure path now; `.bak`/`.bad` semantics preserved.
- Reversal: every fix is a small, revertible hunk; fail-before demonstrated by 4 executed mutants (above), not just asserted.

## Git State

Branch: feature/studio-pages [VERIFIED git branch --show-current]. Code commit + report commit (explicit paths, trailers Task: T-002..T-010-review / Agent: W1-IMPR-01 / Wave: 1) — SHAs recorded in the final message. COMMS.md pre-existing modification and untracked briefs//w1-review/ are sibling state, never staged by this agent. No forbidden files in the diff.
