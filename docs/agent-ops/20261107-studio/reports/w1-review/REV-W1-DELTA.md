# REV-W1-DELTA-01 — Wave 1 improvement-commit diff review (read-only on source)

## ORCH-DIGEST

**Verdict: PASS (production-accept).** The `a92ac4a..HEAD` improvement diff (20 files,
+1176/-129, all under `src/core/` + `tests/`) fixes every claimed item **by root cause,
not by band-aid**: Enforce self-ref hang (BLOCKER-1), 6 MAJORs (unlocked refs, Save
lost-update, PD malformed-JSON throw, Reload/Enforce race, Store↔Manager duplicate-name,
Upsert predicates), plus MINORs/NITs and all 4 TREV P1 pins. Zero new BLOCKERs/MAJORs
found in blind pass + adversarial reads; one residual behavioural note (below) is MINOR at
worst and already documented by IMPR-W1-01. Independent evidence [RAN]:
`ctest --test-dir build -R lwf-tests` → **Passed 21.27s**; `.\build\lwf_tests.exe` →
**348 cases / 13183 assertions / 0 failed** (binary timestamp 15:27:40 is newer than the
final source at 15:25:26 and commit `3571116` at 15:24:14, so it covers HEAD). No source
edited by this review.

---

## 1. Method & evidence base

- **Blind pass first (before opening any report).** Read the full HEAD state of every
  changed source (`PersonalDictionary.cpp`, `DictionaryStore.cpp`, `SnippetStore.cpp`,
  `TransformStore.cpp`, `StatsStore.cpp`, `NotesStore.cpp`, `InsightsModel.cpp` + all
  changed headers) and the `a92ac4a..HEAD` hunks per file. Traced every fix to its root
  cause, checked lock coverage on every public method, and reasoned through termination,
  Save-vs-Load locking, and whole-word semantics before reading IMPR/REV/TREV.
- **Then claim audit.** Checked each disposition in `reports/w1-improve/IMPR-W1-01.md`
  against the code line-by-line (table §2), and each acceptance criterion in
  `reports/w1-review/REV-W1-01.md §7` against the new code + tests.
- **Adversarial attempts (§3).** Hand-traced self-ref chains `a→b, b→a`, empty
  replacements, substring-only containment, 2000-entry/4000-char/2000-prompt boundaries,
  corrupt JSON in every legacy shape, `Save` under lock vs. concurrent `Enforce`/`Entries`.
- **Fail-without-fix audit.** Cannot rebuild mutants (read-only mandate), so verified by
  reading: every new regression test asserts the *new* contract and would fail (or hang)
  under the old code (see §2). IMPR-W1-01's recorded mutant evidence (4 mutants killed,
  Enforce-cursor hang reproduced under timeout) is consistent with the code I read.
- **API-compatibility audit.** `Entries()/Warnings()/Words()/Replacements()/DiffShortcut()`
  changed from `const&` to by-value/returns-copies. Grepped every `src/**/*.cpp` and
  `tests/*.cpp` call site: all uses index into the returned temporary within one full
  expression (valid C++20 temporary lifetime) or bind to a copy; the 2 sites IMPR fixed
  were the only `const Entry&` binds. `main.cpp` callers assign to value members (copies).
  The passing full build + 348-case suite against these headers confirms no dangling-ref
  user remains in-repo. (External ABI is N/A — same binary.)

## 2. Claim audit — what IMPR-W1-01 got right (verified, not assumed)

| Claim (IMPR) | Verdict | Evidence |
|---|---|---|
| BLOCKER-1: Enforce terminates on self-ref | **VERIFIED** | `PersonalDictionary.cpp:291-314` — per rule, single pass over `source` snapshot; cursor strictly increases (`cursor=end`, `end>begin` since non-empty `from` is skipped at `:292`) so the loop runs ≤ #matches; inserted text is never rescanned. Test `self referential replacement terminates in Enforce` (`foo→"foo foo"`: `"foo bar foo"→"foo foo bar foo foo"`, count 2) pins it. Chaining across rules preserved (`a→b, b→c` still yields `c` because rule 2 scans rule 1's output). |
| BLOCKER-1 second layer: Add/Update/Import reject self-ref | **VERIFIED** | `ContainsWholeWord` (`DictionaryStore.cpp:40-62`, ASCII-alnum boundaries mirroring Enforce) enforced in `Add` (`:505`), `Update` (`:564`), `ImportJson` validated pre-mutation (`:643-651`). `foo→foobar` allowed (substring-only), `foo→foo foo`, `bar→BAR` rejected. Test `dictionary add update and import reject self referential replacements` pins exact message. |
| MAJOR-1: unlocked `const&` refs | **VERIFIED** | `SnippetStore::Entries/Warnings` copy under lock (`SnippetStore.cpp:325-333`); `TransformStore::Entries/Warnings/DiffShortcut` copy under lock (`TransformStore.cpp:558-592`); `PersonalDictionary::Words/Replacements` copy under lock (`PersonalDictionary.cpp:210-218`); `Loaded()` locks in all three. Header signatures updated (`PersonalDictionary.h:16-17`, `SnippetStore.h:26-27`, `TransformStore.h:31-37`). |
| MAJOR-2: Save lost-update | **VERIFIED** | `SnippetStore::Save(path)` and `TransformStore::Save(path)` now hold the lock across serialize + tmp-write + `.bak` + rename; no-arg `Save()` copies `path_` under a separate lock scope and releases before delegating (no double-lock, `SnippetStore.cpp:280-290`, `TransformStore.cpp:546-556`). Deadlock audit: `TransformStore::Load` calls `Save` only outside lock scopes (missing-file `:270`, v1 `:481`; the seed at `:301` locks internally with no outer lock held); `SnippetStore::Load` never calls `Save`; `DictionaryStore::Load` never calls `Save`; `NotesStore` already held lock across writes. |
| MAJOR-3: PD malformed-JSON crash | **VERIFIED** | Legacy `from`/`to` now guarded by `is_string` (`PersonalDictionary.cpp:146-147`); v2 `phrase`/`replacement` were already guarded. Test `personal dictionary malformed legacy replacements do not throw` feeds `{"from":42}, {"from":"ok"}` and asserts load succeeds with only the valid pair kept. The only caller `main.cpp:370` needs no try/catch. |
| MAJOR-4: PD Reload/Enforce race | **VERIFIED** | New `mutable mutex_` (`PersonalDictionary.h:28`); `Load` holds it for the whole parse; `Reload` copies `path_` under lock then calls `Load` (released before call — no self-deadlock, `:168-178`); `SetWords/SetReplacements/AddWord` lock; `InitialPrompt`/`Enforce` snapshot vectors under lock then compute unlocked (no torn reads, no lock during O(tokens×dict) compute). Test `reload and enforce are thread safe` (50 reloads × 500 enforces) passes [RAN]. |
| MAJOR-5: Store↔Manager duplicate-name | **VERIFIED** | `ValidateLocked` (`TransformStore.cpp:624-647`) rejects case-insensitive duplicate names across *other* slots including built-ins ("Transform name already exists"); `Validate`/`SetSlot` share the locked variant (no double-lock). `Load` prunes v2 duplicates first-wins and v1 skips duplicates incl. built-in names (`:319-333`, `:435-452`), so a Store-loaded file can never be one the Manager throws on. Tests `duplicate transform names are rejected by the store` + `v2 file with duplicate names keeps the first` pin both sides. |
| MAJOR-6: Upsert `trigger OR id` predicates | **VERIFIED** | Empty-expansion delete and non-empty find now match **by id alone** when `exclude_id` is set, otherwise by trigger (`SnippetStore.cpp:458-481`); collision on rename still rejected by `ValidateLocked` before mutation. Tests `upsert with exclude id renames only that entry` + `upsert rename onto another trigger does not delete the other entry` pin the two failure modes REV-W1-01 described. |
| MINOR-1/-2/-4/-7, NIT-1/-3/-4/-5, MINOR-5 | **VERIFIED** | Stats `PersistLocked` returns bool, `Generation` bumps only on success, tmp cleaned on failure (`StatsStore.cpp:142-174,264-312`); negative clamp on load (`:225-239`); `bak_pending_` on import + truncating load (`DictionaryStore.cpp:398-400,663`) and atomic `ExportFile` (`:667-692`); `MoveToBad` retry (`DictionaryStore.cpp:64-76`, NotesStorage analog); Notes `Create`/`Restore` remove body on index failure (`NotesStore.cpp:374,449`); `CountCodePoints` caps in Snippet (4000) + Transform (2000); reserved-id generation (`SnippetStore.cpp:165-222`); `ParseDayKey` month-length + leap validation (`InsightsModel.cpp:48-55`); TokenSet-normalized shortcut dedup + stamped built-in fills (`TransformStore.cpp:222-257`). |
| TREV P1-1..P1-4 test pins | **VERIFIED** | `test_textdiff`: LCS-oracle battery (9 pairs) + full op-sequence pin `a x→x a` = Delete/Equal/Insert. `test_insights`: exact `{a:34,b:33,c:33}` recipient + sort order. `test_dictionarystore`: `dictionary mutation error paths` + `import corrupt and unsupported leave entries unchanged`. `test_transformstore`: `validate and set slot reject...`, `empty slot is never slot enabled...`, `custom diff shortcut persists...`, duplicate-name cases. All [RAN] green in full-suite run. |
| "No weakened assertions" | **VERIFIED** | `test_atomicfile` vacuous case replaced with a real dir-target failure + tmp-cleanup case plus binary/NUL roundtrip (stronger, not weaker); the only other test-site edits are the 2 `const&`→copy fixes required by the API change (:42, :50 `test_transformstore.cpp`). Test count went 308→348 (+40), assertions 12407→13183 — net addition. |

## 3. Adversarial checks (blind pass + post-read)

1. **Self-ref chains `a→b, b→a`.** Hand-trace: `Enforce("a b")` with `{a→b, b→a}` → rule 1
   scans `"a b"` → `"b b"`; rule 2 scans `"b b"` → `"a a"`. Terminates (2 passes). Old code
   also terminated here (result≠out loop until fixpoint) — same output. For the hang case
   `a→"a a"`: new code → `"a a"` in one pass; old code → unbounded growth. No regression,
   hang fixed, cross-rule chaining semantics preserved.
2. **Empty replacements.** `from=""` skipped (`:292`) — no `WholeWordFind` on empty needle
   (which would immediately match at every position). `to=""` allowed (deletion): cursor
   advances past the match in the *source* snapshot, so no infinite loop and no missed
   matches. `DictionaryStore` treats empty replacement as "no replacement" — consistent.
3. **Substring-only containment.** `foo→foobar` allowed by both layers (whole-word check
   fails right-boundary). `Enforce("foo")` with `foo→foobar`: single pass → `"foobar"`,
   terminates (match consumed in source). A text *starting* with `"foobar"` never
   re-expands because rule output is not rescanned. Correct.
4. **Case-only identity (`bar→BAR`).** `ContainsWholeWord` is case-insensitive so
   `DictionaryStore::Add("bar","BAR")` is rejected; `Enforce` still counts a case-only
   change as a fix (to ≠ source substring). Legacy files with such pairs load fine and
   terminate. Consistent.
5. **2000-entry boundary.** `Add` rejects at `>= 2000` with exact message; `Load`/`ImportJson`
   truncate to exactly 2000 with warning. Test loops to exactly 2000 then asserts rejection.
   No off-by-one and no over-truncation observed. (Minor ordering quirk: full-store
   duplicate phrase reports "maximum 2000 entries" rather than "Word already exists." —
   cosmetic, pre-existing order, not introduced here.)
6. **Corrupt JSON in legacy shapes (all four).** Bare-array non-strings skipped
   (`DictionaryStore.cpp:139`); `{words}` non-strings skipped; `replacements` non-`from`/`to`
   skipped in both `DictionaryStore` and `PersonalDictionary`; v2 `schema_version` with
   missing/non-array `entries` → `Load` returns false and *preserves* prior in-memory state
   (PD parses into locals, assigns at end — `:105-165`; DictionaryStore moves to `.bad` and
   clears — both pre-existing intended semantics). `SnippetStore`/`TransformStore` tolerant
   loaders type-guard `id`/`created_at`/`updated_at`/`enabled`/`diff_shortcut` etc. — bonus
   same-class fixes, verified in code. No uncaught throw path found in the tolerant loaders.
7. **`Save` during concurrent `Read` / `Enforce`.** All `Save` implementations hold the
   mutex for the full write, so concurrent `Entries()/Expand()/Enforce` serialize rather
   than tear. `PersonalDictionary` has no `Save` (Load/Reload are the write path; `Load`
   serializes vs `Enforce`'s snapshot). No deadlock: no `Save` is ever invoked while its
   own mutex is held (audited above). New cost: `Save` blocks readers for file-I/O
   duration — bounded, matches the pre-existing `DictionaryStore`/`NotesStore` precedent.
8. **`SnippetStore::Load` reserved-id generation.** Pre-scan includes ids from elements
   that later get skipped (bad trigger) → generated ids are conservative (gaps only, never
   collisions). Duplicate explicit ids regenerated. `id_taken` checked against the growing
   output; generated ids advance past `reserved_ids` which grows as they're assigned.
   `RemoveById` over-deletion class closed.

### Residual behavioural note (MINOR, documented)

`SnippetStore::Load` now logs `snippets: N load warnings` (count only) instead of one line
per warning containing the trigger text. This is the MINOR-10 fix (no user text in logs) and
the full warning list is still returned via `Warnings()` for UI display — intentional and
correct. Not a defect; noted here so a future log-grep audit isn't surprised.

## 4. Findings

- **BLOCKER: none.**
- **MAJOR: none.**
- **MINOR (1):** none actionable. Two pre-existing/minor items worth naming: (a) the full-store
  duplicate error-priority quirk in §3.5 (cosmetic, pre-existing); (b) `Save` holding the
  lock across file I/O extends reader blackout during saves (performance, not correctness;
  precedent-consistent). Neither blocks Wave 2 wiring.
- All 7 REV-W1-01 acceptance criteria (§7) are met and test-pinned:
  (1) both-layer BLOCKER fix + `foo→"foo foo"` regression test; (2) by-value accessors +
  lock-held `Save`; (3) `is_string` guards + malformed regression test; (4) `PersonalDictionary`
  mutex + snapshot `Enforce`; (5) Store rejects duplicate names + id-alone `Upsert`
  predicates; (6) `bak_pending_` covers truncating-Load/Import so the first `Save` preserves
  the pre-save file (residual once-semantics documented in IMPR §"Risks" — accepted,
  still warned at load).

## 5. Self-review of this review

- Read the diff at `a92ac4a..HEAD` limited to `src/core/ tests/` per brief (docs excluded).
- Blind pass (code + tests, then reports) — findings above are independently derived;
  where the reports match the code I say VERIFIED with line evidence, not by trust.
- No mutation/revert executed (read-only mandate); "tests fail without the fix" verified
  by reading assertions vs. old code paths, corroborated by IMPR's recorded mutant kills.
- Full suite re-run [RAN]: `ctest -R lwf-tests` Passed, direct binary 348/348 (an earlier
  unscoped `ctest --test-dir build` hit the 600s tool timeout running *all* suites — the
  scoped re-run passed in 21s, so this is a tool-timeout artifact, not a test failure).

## 6. Remaining risks / NOT-RUN

- TSAN/heaptrack NOT-RUN (no TSAN on this box) — thread safety is by lock-coverage reasoning
  + the 50×500 concurrent test, not sanitizer-proven. Same recommendation as IMPR/REV.
- G3 zig type-check NOT-RUN (no zig on box per AGENTS.md). Changed files are portable core;
  `scripts/check_hygiene.sh` / `check_no_network.sh` results claimed by IMPR were not
  re-run here (not in the `src/core/ tests/` scope) — out of scope for this delta.
- Windows MSVC rename-over-existing NOT-RUN (MinGW box); rename semantics covered by suite.
- No fsync anywhere (flush only) — power-loss durability unchanged from Wave 1; document or
  accept per IMPR's risk note.

## 7. Long-run test (standing order 8)

- **Source:** fixes are at the shared layer (Enforce algorithm + validation at every mutation
  entry point, lock coverage on every accessor, single duplicate-name contract) — the same
  bug classes (unlocked ref, snapshot-then-unlock write, unchecked `.get`) were grepped
  across all changed stores, with the two bonus `value()`→guard fixes in sibling loaders.
- **Load:** 2000-entry / 4000-char / 2000-prompt borders all still pinned; Enforce is now
  O(rules × text) single-pass (strictly better than the old re-scan loop).
- **Hostile:** self-referential, duplicate-name, duplicate-id, Feb-31, negative-stats,
  malformed-legacy hostile inputs all pinned by tests.
- **Operability:** no new log content (snippet warnings redacted to counts); all writes
  tmp+rename with tmp cleanup on every failure path; `.bak`/`.bad` semantics preserved.
- **Reversal:** fail-before demonstrated by 4 recorded mutants (IMPR) + every new regression
  test reads as fail-first against the old code paths; reverting any single fix hunk
  re-opens exactly one test. Each fix is a small revertible hunk.

---
*Reviewer: W1-DELTA-01 (read-only on source). No source files edited. This report is the sole artifact.*
