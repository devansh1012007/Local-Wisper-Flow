# REV-W1-01 — Wave 1 data layer code review (read-only)

## ORCH-DIGEST

**Verdict: CHANGES-REQUESTED — 1 BLOCKER, 6 MAJOR, 10 MINOR, 6 NIT.** Wave 1 data layer is broadly sound (atomic tmp+rename everywhere, `.bad`/`.bak` migration, additive contracts, hygiene + no-network OK [RAN], full doctest 308/308 [RAN]), but not merge-safe for Wave 2 wiring until the BLOCKER/MAJORs below are fixed or formally accepted. **BLOCKER:** `PersonalDictionary::Enforce` loops forever on a self-referential replacement (e.g. `foo -> "foo foo"` via `DictionaryStore::Add`, no validation) — engine-thread hang on next dictation. **MAJORs:** (1) `SnippetStore::Entries/Warnings` + `TransformStore::Entries/Warnings/Loaded/DiffShortcut` return locked-state references without the lock (data race/UB despite the new mutex); (2) `SnippetStore::Save`/`TransformStore::Save` snapshot under lock then write after unlock — lost update under concurrent edit; (3) `PersonalDictionary::Load` legacy `from`/`to` unchecked `.get<std::string>()` throws `nlohmann` type_error out of a tolerant loader (main.cpp:370 has no try/catch) — malformed `dictionary.json` crashes startup; (4) new `PersonalDictionary::Reload` (`path_/words_/replacements_`, no mutex) races `Enforce` (engine thread) — the exact Wave 2 call pattern; (5) `TransformStore` allows duplicate slot names that `TransformManager::LoadFile` rejects with throw — Store can produce a file the Manager crashes on; (6) `SnippetStore::Upsert` empty-expansion path deletes by `trigger OR exclude_id` (renaming onto another entry's trigger deletes both) and non-empty path finds only the first of two colliding entries, leaving duplicate triggers. No band-aids (no swallowed-to-pass, no weakened assertions) found in the fix commits; the 4 FIX reports are accurate test-only/src-minimal root-cause fixes.

Counts: BLOCKER 1, MAJOR 6, MINOR 10, NIT 6 (23 findings). Top risks: BLOCKER hang reachable from the new Add API; reference-race makes the new stores' advertised thread-safety false; Save lost-update; startup crash on legacy dictionary; Store↔Manager contract break.

---

## 1. Method & evidence base

- **Blind pass first:** read the full diff `0e655ec5..HEAD -- src/core/ tests/ CMakeLists.txt scripts/` (31 files, +4946/-38) and every new/modified source in context before opening any implementer report.
- **Then claim audit:** checked each report in `reports/w1-impl/` (02A AtomicFile/TextDiff/Insights, 02B Stats+Category, 02C Dictionary, 02D Snippet+Transform, 02E Notes, INT wiring, FIX-DICT/DIFF/SNIP/TRANS, GATE) against the code line-by-line (root cause, tests, "no contract change", edge cases). Attempted breaks: malformed JSON of every shape, 2000-entry/4000-char/2000-prompt borders, torn/missing `.tmp`/`.bak`/`.bad`, ASCII-only casefold dupes, missing-file handling, multi-thread contention (read the locks; reasoned about each public method's lock coverage), band-aid hunt (grep `TODO|FIXME`, `catch {}`, `CHECK`/`REQUIRE` diffs in FIX commits).
- **AGENTS.md:** no-comments (hygiene OK proves), additive keys (verified: `AppContextResolver` category string preserved/discriminated, `SettingsStore`-style unknown-key preservation in Stats by_category/apps), no-transcript logging (verified: no `Log` in Notes/Stats; Dictionary/Transform log counts/slots only; Snippet logs trigger text — MINOR-10), keys/secrets untouched.
- **Independent test evidence [RAN]:** `.\build\lwf_tests.exe` full → **308/308 passed, 12407 assertions, SUCCESS**. `bash scripts/check_hygiene.sh` → `hygiene: OK`. `bash scripts/check_no_network.sh` → `no-network: OK`. No source edits made by this review.

## 2. Claim audit — what the implementers got right (verified, not assumed)

| Claim (report) | Verdict | Evidence |
|---|---|---|
| 02A: AtomicFile tmp+flush+rename; stale `.tmp` truncated; failure removes tmp | VERIFIED | `AtomicFile.cpp:9-33` exactly; test_atomicfile 5 cases pass |
| 02A: TextDiff tie-break Insert-on-tie ⇒ Equal == LCS | VERIFIED (code), UNPINNED (test) | `TextDiff.cpp:108` strict `>`; correct alignment hand-traced. But no test asserts `Equal==LCS` or full tie order (TREV M-1) |
| 02A: InsightsModel formulas (Wpm floor 60s, shares sum 100, streaks via days-from-civil) | VERIFIED | `InsightsModel.cpp:77-191`; thirds case distributes remainder in sorted order; month-boundary streak correct |
| 02B: StatsStore 730-day eviction (keep today-729..today) | VERIFIED | `StatsStore.cpp:287-300` lexicographic cutoff; test plants 731 days → 730 |
| 02B: AppContextResolver category additive, process-first/title-second, built-ins | VERIFIED | `AppContextResolver.cpp:242-286`, `:76-78` (unknown category preserved), `:149-165` built-ins |
| 02C: Dictionary 4-shape migration, `.bak`-once, seed `btw`, capped dup message, `Word already exists.` | VERIFIED | `DictionaryStore.cpp:83-238,290-406,434-477`; FIX-DICT test fixes were the test's fault (seed count, missing brace) |
| 02C: PersonalDictionary `Enforce` fix-count additive, old 2-arg callers compile | VERIFIED | `PersonalDictionary.h:20` default `nullptr`; `PersonalDictionary.cpp:261-324` counting after `result==out` guard |
| 02D: Snippet v2 4 exact messages + order; TransformStore 10 slots + `{text}` append + v1→v2 | VERIFIED as written | `SnippetStore.cpp:351-380`, `TransformStore.cpp:237-419,537-607`. W1-FIX-TRANS's 2000-pre-append cap is correct per brief/plan (02D's self-review reasoning was wrong, fix right) |
| 02E: NotesStore md-first-then-index; orphan ADOPT; dangling drop; hostile id blocked; pin doesn't bump `modified_at` | VERIFIED | `NotesStore.cpp:147-258,354-479`; strongest test suite in the wave |
| INT: CMake wiring 7 sources + 8 tests; allowlist adds exactly 5 stores `.cpp`-only | VERIFIED | `CMakeLists.txt` diff; `check_no_network.sh:30` — network/endpoint regex untouched [RAN OK] |
| FIX-* (DICT/DIFF/SNIP/TRANS): test-only or minimal src fixes, no weakened assertions | VERIFIED | FIX-DICT 2 test lines; FIX-DIFF 1 literal split; FIX-SNIP out-param (6 sites); FIX-TRANS 3 hunks, `file.close()` root cause (Windows open-handle blocks rename) — all confirmed against code |
| "No contract change" (PersonalDictionary Enforce, AppContextResolver, TransformManager) | MOSTLY VERIFIED, 2 exceptions | Enforce/Category additive OK. Exceptions: MANOR-5 (Store↔Manager duplicate-name), MINOR-6 (Manager exception type/message drift for v1 invalid elements) |

## 3. Findings

### BLOCKER-1 — `Enforce` infinite loop on self-referential replacement
- **Where:** `src/core/PersonalDictionary.cpp:266-280` (loop), reachable from `src/core/DictionaryStore.cpp:434` `Add` (no replacement validation).
- **What:** for each `[from,to]`, loop replaces `from→to` until `result==out`. If `to` contains `from` as a whole word (e.g. `foo → "foo foo"`), every iteration replaces the first `foo` again, growing the string forever. The `result==out` guard only covers identity (`to==from`), not containment.
- **Break attempt:** `Add("foo","foo foo")` succeeds (trimmed non-empty, not a dupe); next `Enforce("foo", …)` on the engine thread never returns, `fix_count` increments unboundedly. Pre-existing loop, but Wave 1 is the first code path that lets a user create such a pair through a supported API; no test pins it.
- **Fix direction:** validate at `Add`/`Update`/`ImportJson` (reject `to` containing `from` as whole word, at minimum `to==from`-class), and/or make `Enforce` single-pass per rule (advance a cursor past the inserted text). Either alone is insufficient for arbitrary legacy files — do both.

### MAJOR-1 — Locked-state references returned without the lock (Snippet/Transform)
- **Where:** `SnippetStore.h:26-28` + `SnippetStore.cpp:282-292` (`Entries()`, `Warnings()` return `const&`); `TransformStore.h:31-33,37` + `TransformStore.cpp:475-499` (`Entries()`, `Warnings()`, `Loaded()`, `DiffShortcut()`).
- **What:** both classes gained `mutable std::mutex` and lock every mutator/`Expand`/`Validate`, but these accessors hand out references with no lock. A UI thread reading `Entries()` while the engine thread is in `Expand` (locked) or while another edit is in `Upsert` mutating `entries_` is a data race → UB / iterator invalidation. `DictionaryStore`/`NotesStore`/`StatsStore` do this correctly (copy under lock: `DictionaryStore.cpp:408`, `NotesStore.cpp:481`, `StatsStore.cpp:185`).
- **Likely the exact Wave 2 pattern** (page iterates entries while dictation expands). Fix: return copies (consistent with the three correct stores) or snapshot types; at minimum document + return by value for `Loaded/diff_shortcut`.

### MAJOR-2 — `Save` snapshots under lock, writes after unlock — lost update
- **Where:** `SnippetStore.cpp:199-235`, `TransformStore.cpp:421-461`.
- **What:** lock is released after copying entries into JSON, then tmp write + `.bak` copy + rename run unlocked. A concurrent `Upsert`/`SetSlot` that lands during the write is silently overwritten by the rename (its edit is in memory but not in the file; next `Load` loses it). `DictionaryStore::Save` (`DictionaryStore.cpp:367`) holds the lock across the whole write and is correct; `NotesStore` same. Fix: hold the lock across copy+write+rename (as DictionaryStore does), or version/generation-check before rename.

### MAJOR-3 — `PersonalDictionary::Load` throws on malformed legacy replacements; startup crash
- **Where:** `src/core/PersonalDictionary.cpp:142-149` (`element["from"].get<std::string>()`, `element["to"].get<std::string>()` with no `is_string` guard); sole caller `src/main.cpp:370` has no try/catch.
- **What:** `{"replacements":[{"from":42,"to":"x"}]}` (or `from` present but `to` numeric) throws `nlohmann::json::type_error`, uncaught — the "tolerant" loader crashes the app on a user-editable file. The new `DictionaryStore::ParseLegacy` (`DictionaryStore.cpp:124-138`) correctly guards `is_string`; `PersonalDictionary` was left unguarded. 02C flags this as preserved scope; it is still a crash path in a file this wave touches. Fix: guard both fields `is_string` (skip invalid, like the v2 branch at `:121-126`).

### MAJOR-4 — New `Reload` introduces a thread race on `PersonalDictionary` state
- **Where:** `PersonalDictionary.h:21-22,32` (`Reload()`, `path_`), `PersonalDictionary.cpp:90-170` (`Load` writes `words_/replacements_/path_`, no mutex anywhere in the class); `Enforce`/`Words`/`InitialPrompt` read the same vectors on the engine thread (`TextPipeline.cpp:97`, `main.cpp:389,414`).
- **What:** the whole point of `Reload` (D-OP2) is calling it from the UI thread after a page Save while dictation is enforcing — precisely the interleaving that races `std::vector` reallocation. No lock exists to add. 02C discloses this as a Wave 2 risk; as shipped the new public method is unsafe by construction. Fix before Wave 2 wiring: a `mutable std::mutex` + copy-on-write, or serialize through `EngineBridge` (caller's obligation must be stated in the header, not assumed).

### MAJOR-5 — `TransformStore` and `TransformManager` disagree on duplicate names
- **Where:** `TransformStore.cpp:537-607` (no name-uniqueness check in `Validate`/`SetSlot`/`Load`); `TransformManager.cpp:89-94` (throws `duplicate transform name`).
- **What:** two slots set to the same custom name save cleanly through the Store, then `TransformManager(path)` construction throws `runtime_error` at startup. Either the Store must reject duplicate names (matching the Manager) or the Manager must tolerate them (last-wins, matching the Store's own duplicate-slot handling at `TransformStore.cpp:359-366`). Pick one contract; today each layer enforces the opposite.

### MAJOR-6 — `SnippetStore::Upsert` rename/delete collides two entries
- **Where:** `SnippetStore.cpp:411-421` (empty-expansion delete), `:428-432` (existing search).
- **What:** both predicates are `trigger==key OR id==exclude_id`. Editing entry A (id X) and renaming its trigger onto entry B's trigger: (a) empty-expansion path `remove_if` deletes **both** A and B; (b) non-empty path `find_if` updates only the first match, leaving two entries with the same trigger thereafter (invariant broken, `Load`'s last-wins dedup never runs). Fix: when `exclude_id` is given, match by id alone (rename is an edit of that entry); validation already prevents the collision on the non-empty path, but the empty path skips `ValidateLocked` entirely — route it through the same predicate.

### MINOR-1 — `StatsStore::Record` bumps `Generation` even when persistence failed; `PersistLocked` swallows all errors
- **Where:** `StatsStore.cpp:170-172` (`PersistLocked(); generation_.fetch_add(1);` unconditionally), `:246-285` (open/write/rename failures `return` silently; tmp left on flush-failure where `AtomicFile.cpp:18-23` would `remove` it).
- **What:** a UI watching `Generation()` re-reads `Snapshot()` (memory) and believes the data is on disk. At minimum `Record` should not advance generation on a failed persist, and tmp should be cleaned on write failure. (Snapshot itself is always correct — in-memory state is never corrupted.)

### MINOR-2 — `ImportJson` replace-then-`Save` has no `.bak` safety net
- **Where:** `DictionaryStore.cpp:561-596` (`migrated_=false` after import) vs `:388-397` (`.bak` only `if (migrated_)`).
- **What:** import replaces in-memory entries; the next `Save` atomically overwrites the user's existing `dictionary.json` with no `.bak` (import is as destructive as migration, but the `.bak`-once logic only knows about migration). Also `ExportFile` (`:598-607`) writes the export path directly with `ofstream` — torn on crash, unlike every other writer. Fix: set a `dirty`-style flag so the first `Save` after import also `.bak`s; route `ExportFile` through tmp+rename.

### MINOR-3 — Truncation and skip-on-load permanently destroy data on next `Save`
- **Where:** `DictionaryStore.cpp:346-349,579-583` (resize to 2000), `SnippetStore.cpp:169-173` (over-4000 entry skipped), `TransformStore.cpp:287-292` (v1 overflow skipped).
- **What:** each logs a warning and drops the tail, but the next `Save` persists the truncated state — the dropped entries are gone forever, with no `.bak` of the over-long file (`.bak` copies the on-disk file pre-save, so it does contain them — but only if `.bak` semantics fire; for Snippet/Transform the `.bak` is overwritten on every save, so one Save later the original is unrecoverable). At minimum, refuse to `Save` (or refuse to load-as-truncated) when data was dropped, forcing an explicit user decision. The current warning is invisible to the person whose entries vanished.

### MINOR-4 — Notes `Create`/`Restore` "rollback" leaves the body on disk — next `Load` resurrects it
- **Where:** `NotesStore.cpp:354-373` (`Create`: body written, index fails → memory erased, body file remains), `:417-445` (`Restore` same).
- **What:** returns `nullopt`/`false` claiming no change, but the orphan `note-*.md` is adopted on the next `Load`/`Reload`, so the "failed" create reappears. 02E's rollback invariant holds for `List/Get/Size` pre-reload but not across reload. Fix: on index-write failure, also remove the body file (it was created by this call) before returning.

### MINOR-5 — Caps are byte counts, not character counts (4000/2000)
- **Where:** `SnippetStore.cpp:357,406` (`expansion.size() > kMaxExpansionChars`), `TransformStore.cpp:548` (`prompt.size() > kMaxPromptChars`).
- **What:** `std::string::size()` is bytes. 2000 CJK chars (6000 bytes) is rejected as "Snippet too long"/"Prompt too long" despite being within the documented char cap; all tests use ASCII so this never surfaced. Either count code points (decode, as `TextDiff`/`StatsStore::CountWords` already do) or rename the constants/messages to bytes.

### MINOR-6 — `TransformManager` v1 strictness drift (exception type/message changed)
- **Where:** `TransformManager.cpp:68-96` (v1 invalid elements now fall through to `value(..., "")` + `{text}` check instead of `element.at(...).get<...>()`).
- **What:** `no contract change` holds for valid v1 files, but invalid ones now throw `runtime_error("... '{text}'")` instead of a `nlohmann` exception, and the non-array/non-object message at `:62` is retained while the new object-without-slots message at `:58-59` is different. Callers/tests matching on type/message will see a change. Low risk (tests pass), but it should be named, not claimed unchanged.

### MINOR-7 — `.bad` rename failure is ignored everywhere; corrupt file may never move
- **Where:** `DictionaryStore.cpp:324-325,337-338`, `NotesStore.cpp:168-169` (all `rename(..., ec)` with `ec` unchecked; `SnippetStore`/`TransformStore` don't rename at all — corrupt just returns false).
- **What:** if `.bad` already exists (second corruption) and the platform rename won't overwrite, `Load` returns false and clears memory, but the corrupt original stays at `path_` and the next `Load` repeats the cycle indefinitely. The data is never silently overwritten (good), but the recovery is not actually recovering. Check `ec` and fall back (remove/replace `.bad`, or at least log).

### MINOR-8 — Casefolding is ASCII-only, so Unicode dupes slip through every store
- **Where:** `DictionaryStore.cpp:26-38`, `SnippetStore.cpp:15-27`, `NotesStore.cpp:25-37`, `PersonalDictionary.cpp:16-28`, `StatsStore.cpp:20-25` (`u < 0x80` else passthrough).
- **What:** `"Äpfel"` vs `"äpfel"` (or Turkish İ) are distinct keys — `Add` accepts both, `Search` misses cross-case, `SnippetStore` dedup/expand misses. Consistent across the wave (a deliberate, defensible scope cut inherited from `PersonalDictionary`), but the "case-insensitive" claims in 02C/02D reports overpromise for non-ASCII. Worth one shared helper + one pinned test per store, or downgrading the claim to ASCII.

### MINOR-9 — Hash/duplicate work and inconsistent atomic-write copies
- **Where:** `AtomicFile.cpp` has **zero callers** (verified: only its own test references `WriteAtomic`); every store re-implements tmp+rename with small divergences (Stats leaves `.tmp` on flush failure; Dictionary/Transform/Snippet `.bak` semantics differ — once vs always-overwrite; Notes creates dirs, others don't).
- **What:** Wave 1's headline factorization is dead code, and the copies have already diverged (MINOR-1, MINOR-3). Not a correctness bug today, but the exact "three home-grown copies" problem 02A set out to fix is now five. Converge on `WriteAtomic` (extending it with optional `.bak`/mkdir) or delete it and say so.

### MINOR-10 — Snippet trigger text is logged; user text in logs
- **Where:** `SnippetStore.cpp:169-181,188-190` (`Log::Warning("snippets: snippet '" + entry.trigger + "' …")`).
- **What:** triggers are user-authored snippet words, not dictation transcripts, so this is not an R4 transcript leak — but it is user content in a log file, written while the rest of the wave deliberately logs counts only (`DictionaryStore.cpp:347`, `PersonalDictionary.cpp:222`). Either redact to id/index or accept explicitly.

### NIT-1 — `SnippetStore::Load` missing-id generation can collide with file-provided ids
- **Where:** `SnippetStore.cpp:183-185` (`"snippet-" + std::to_string(index+1)`, no collision check vs other entries' ids; contrast `GenerateIdLocked:109-122` which checks). Two entries can end up sharing an id; `RemoveById` then removes both.

### NIT-2 — `TextDiff` has no input-size guard; O(n·m) memory on unbounded input
- **Where:** `TextDiff.cpp:89` (`vector<vector<size_t>>(n+1 × m+1)`). 02A discloses this; a 10k-word paste is ~800 MB transient. A cap (reject/truncate above N words) or a Myers/Hirschberg fallback before Wave 2 feeds it transcripts is cheap insurance.

### NIT-3 — `InsightsModel::ParseDayKey` accepts Feb 31 (serial wraps to Mar 1)
- **Where:** `InsightsModel.cpp:45` (`day_of_month > 31` only). `2026-02-31` parses as the Mar-1 serial, so a hand-edited/malformed day key can alias a real day in streak math. Validate month lengths (or round-trip the serial).

### NIT-4 — `StatsStore::LoadLocked` accepts negative day values verbatim
- **Where:** `StatsStore.cpp:219-223,228-238` (no clamping; `Record` clamps at `:156-158`). A hand-edited `stats.json` with negative `words` counts into `Snapshot()` and `InsightsModel` (which clamps at use time, so impact is cosmetic).

### NIT-5 — `TransformStore` missing-slot fill uses zero timestamps; duplicate-shortcut detection is string-exact, not token-normalized
- **Where:** `TransformStore.cpp:163-190` (`EnsureAllSlotsLocked` defaults `created_at/updated_at = 0` unlike `SeedDefaults:148-155` which stamps `now`); `:219` (`seen_shortcuts` key is `LowerAscii(TrimCopy(shortcut))`, so `"Ctrl+Alt+1"` vs `"Control+Alt+1"` duplicates among slots are both enabled — bindings go through the normalizing `TokenSet`, slots don't).

### NIT-6 — `NotesStore` holds every body in memory; no size/count cap
- **Where:** `NotesStore.h:46` (`notes_` caches bodies), `NotesStore.cpp:147-258` (`LoadLocked` reads all `*.md` fully). No analogue of the 2000/4000/2000 caps elsewhere. Fine for a scratchpad today; a runaway `notes/` dir loads entirely into RAM and every `Search` copies it.

## 4. Optimizations (measurable, not stylistic)

1. **`PersonalDictionary::Enforce` is O(tokens × dict × wordlen²)** (`PersonalDictionary.cpp:285-310` calls `ToLowerUtf8Safe` per comparison and `EditDistance` with fresh DP vectors per candidate). With a 2000-entry dictionary this dominates dictation latency. Index words by lowercased form once (hash map); compute `EditDistance` only on length-±1 candidates. No behavior change, same outputs.
2. **`SnippetStore::Expand` contains-mode** recomputes `ToLowerUtf8Safe(out)` per occurrence (`SnippetStore.cpp:339`) — O(matches × strlen). Lowercase once per entry pass and map positions (lengths are equal under ASCII folding; for non-ASCII the current code is already approximate).
3. **`DictionaryStore::Add/Update` O(N) dup scan** + `Search`/`Entries` full sorts per call — fine at 2000 entries (µs), but `ImportJson` does `ParseAny` then re-scans ids (`DictionaryStore.cpp:585-590`) and `MergeParsed` already built an index — reuse it. All are cheap today; listed only so Wave 2 doesn't build a page on per-keystroke full sorts.
4. **Dead-code convergence (MINOR-9)** is the biggest structural saving: one atomic writer with `.bak`/mkdir policy, adopted by all 5 stores, removes ~150 lines of divergent tmp/rename code and the MINOR-1/MINOR-3 divergence class.

## 5. AGENTS.md compliance

- No code comments: PASS (hygiene OK [RAN]; grep confirms no `//`/`/*` in new sources).
- Additive keys: PASS (category is a defaulted string, unknown preserved at `AppContextResolver.cpp:76-78` and `StatsStore.cpp:241`; v2 JSON omits empty `replacement`).
- No transcript logging: PASS except MINOR-10 (snippet trigger in warning log; needs explicit accept or redaction).
- Gates: hygiene OK, no-network OK [RAN]; G1/G2 = full `lwf_tests` 308/308 [RAN] (G3 zig pending, no zig on box — recorded, not claimed).

## 6. Remaining risks / not verified

- **Windows rename-over-open-handle** is fixed in the three places the FIX reports found (`file.close()` before rename/Save) and I verified no remaining store Saves or renames with its source handle open (`SnippetStore::Load`, `StatsStore::LoadLocked` never rename-while-open). The one untested path is rename-over-existing on MSVC (this box is MinGW) — Wave 2 Windows full job must confirm.
- **No fsync anywhere** (AtomicFile + all stores flush but never fsync/file-sync). Power-loss durability is not provided; only atomicity (no torn file). Acceptable if documented; currently only implied.
- **NOT-RUN:** mutation reverts (read-only mandate) — several survivors in TREV-W1-01 (tie-break order, remainder recipient) remain test-unpinned; this review's findings are static analysis + the passing suite, not empirical mutants.
- **Threading beyond MAJOR-1/2/4** (CRTs, `NotesStore` two-instance same-dir races, `DictionaryStore::Path()` unlocked ref) is reasoned from lock coverage, not executed under TSAN — recommend a TSAN/heaptrack pass before EngineBridge wiring.

## 7. Acceptance criteria for re-review

1. BLOCKER-1 fixed by both validation and single-pass/cursor `Enforce`, with a regression test (`foo → "foo foo"` terminates).
2. MAJOR-1/2: `Entries()` etc. return copies (or the stores document single-threaded use and drop the mutex claim); `Save` holds the lock across the write.
3. MAJOR-3: legacy `from`/`to` guarded (`is_string`), malformed regression test, `PersonalDictionary` never throws.
4. MAJOR-4: `PersonalDictionary` gains a mutex or `Reload` documents EngineBridge serialization as a precondition.
5. MAJOR-5/6: one duplicate-name contract + `Upsert` predicates match on id alone when `exclude_id` is set.
6. MINOR-3: truncated/skip-on-load data cannot be silently `Save`-destroyed.

---
*Reviewer: W1-REV-01 (read-only on source). No source files edited. This report is the sole artifact.*
