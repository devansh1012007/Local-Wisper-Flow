# W1-TREV-01 — Wave 1 test review (read-only on source)

## ORCH-DIGEST

**Verdict: PASS-WITH-GAPS — no P0 test bug remains after W1-FIX-{DICT,DIFF,TRANS,SNIP}; 4 P1 coverage gaps + 5 mutation classes survive.** 8 files, 83 TEST_CASEs reviewed [VERIFIED `Select-String TEST_CASE` count: atomicfile 5, textdiff 10, insights 14, statsstore 10, dictionarystore 17, snippetstore_v2 9, transformstore 7, notesstore 11 = 83]. [RAN `./build/lwf_tests.exe` filtered, 3x -> 39 cases / 2310 assertions PASS x3, exit 0 each; full Wave-1 filter 58/58 + 29/29 PASS, 2727 assertions] — no flake observed in-session, but hermeticity is **not** parallel-safe (snippet/transform share one fixed temp dir; stats only removes the main file). Fail-before evidence exists for exactly the 4 fixed clusters [VERIFIED W1-FIX-*.md]; the other 4 files have **no recorded fail-before/mutation evidence**. Strongest suites: notesstore (hostile/orphan/corrupt/concurrent), statsstore (730-cap, threads), snippetstore_v2 (exact messages + boundary). Weakest: atomicfile (1 vacuous case), insights (remainder/tie-break/order unpinned), textdiff (no Equal==LCS invariant, no tie-break order pin).

**P1 missing (prioritized, §7):** (1) no `Equal==LCS` / Delete-vs-Insert order assertion in textdiff; (2) CategoryShares remainder recipient + sort/tie-break unpinned in insights; (3) DictionaryStore Add/Remove/Update error paths + ImportJson-corrupt/.bad untested; (4) TransformStore Validate slot-range/name/shortcut + per-slot enable + DiffShortcut persistence untested. **Mutation survivors (§6): 9 named, 5 classes survive at least one plausible mutant.**

---

## 1. Method & evidence base

- Scope diff [RAN `git diff --stat 0e655ec..HEAD -- tests/` -> 8 files, 1711 insertions, all new files]: `test_atomicfile.cpp, test_textdiff.cpp, test_insights.cpp, test_statsstore.cpp, test_dictionarystore.cpp, test_snippetstore_v2.cpp, test_transformstore.cpp, test_notesstore.cpp`.
- Read every test file in full, plus the units under test (`src/core/{AtomicFile,InsightsModel,TextDiff,StatsStore,DictionaryStore,PersonalDictionary,SnippetStore,TransformStore,NotesStore}.cpp`) to judge whether assertions pin behavior or merely restate implementation.
- [RAN `git log --oneline 0e655ec..HEAD`] confirms the 4 fix commits (`a92ac4a` dict, `5aac095` textdiff, `e7b2014` transform, `aa382b6` snippet) — used as fail-before evidence where present; not re-derived by reverting (read-only mandate).
- [RAN build binary present: `build/lwf_tests.exe`, 5,086,196 bytes, 2026-11-07 14:24] — no rebuild performed (would be a source-adjacent write; binary is post-fix per timestamps). All runs below are filtered doctest invocations of that binary.

## 2. Per-file review

### 2.1 `test_atomicfile.cpp` (5 cases) — WEAK

- **Behavior quality: MIXED.** Roundtrip, overwrite, stale-tmp-ignore, missing-dir-fail are real behavior assertions (content byte-equality via `ReadFile`, `!exists(.tmp)`, `!exists(path)`). Good.
- **Vacuous case (test bug, LOW):** `"a tmp write without the rename leaves the old content intact"` never calls `WriteAtomic` at all — it hand-writes `path` + `path.tmp` then reads `path`. It passes under *any* `WriteAtomic` implementation, including a stub returning `true` without writing. It tests the filesystem, not the unit. Structurally identical class to the fixed hex-escape bug: a test that cannot fail for the reason its name claims.
- **Fail-before: NONE recorded.** No report shows this file failing pre-fix. The missing-directory case is the only error path; it does pin `return false` + no artifacts, which is good but unverified as fail-first.
- **Hermeticity: GOOD** — per-case suffix (`TempDir(name)` + `remove_all` + `create_directories`). No cleanup after (leaves temp dirs; acceptable, matches house pattern).
- **Over-mocking: NONE** — tests the real FS function. Correct choice.
- **Missing:** binary/NUL content, content larger than a page, `path` is an existing directory (rename must fail, tmp cleaned), read-only directory, rename-failure cleanup of tmp (hard to trigger portably — at minimum the directory case).

### 2.2 `test_textdiff.cpp` (10 cases) — GOOD, one gap

- **Behavior quality: GOOD on counts, WEAK on order/invariant.** Cases pin `replaced/inserted/deleted` + `CountOp(Equal)` + `HasInsert/HasDelete` presence — real behavior, not implementation restatement. `"token order is preserved"` checks only tokens[0] and tokens[3], not the full op sequence.
- **Whitespace/NBSP coverage: GOOD after fix** — tab/newline, NBSP (C2 A0), CJK single-token all pin `Tokenize` via `Diff`. The NBSP case is the fixed hex-escape bug (§5.1).
- **Fail-before: STRONG** [VERIFIED W1-FIX-DIFF.md]: pre-fix `replaced 1==0, inserted 1==0, Equal 0==2` with byte-level root cause; post-fix 11/11.
- **Hermeticity: PERFECT** — pure functions, no FS/clock.
- **Over-mocking: NONE.**
- **Critical gap (§6 M-1, §7 P1-1):** no assertion of the plan's §H invariant `Equal == LCS length`, and no Delete-vs-Insert ordering pin for the tie-break (`TextDiff.cpp:108` strict `>` -> Insert-first). A mutant flipping `>` to `>=` (Delete-first) keeps `replaced/inserted/deleted` identical on every current case and survives. The 02A report notes the tie-break was deliberately chosen; no test pins it.

### 2.3 `test_insights.cpp` (14 cases) — GOOD, tie-breaks unpinned

- **Behavior quality: STRONG on formulas** — `Wpm` floor (59s empty / 60s value / 0-words-with-time), `FixesMadeByFlow` (negative ignored, empty=0), streaks (today-no-words=0, month boundary, gap), `CalendarLevel` quartiles including clamp (`150,100 -> 4`, `1,100 -> 1`). These are genuine boundary pins.
- **CategoryShares: WEAK on recipient.** `"rounds thirds so the shares still sum to 100"` asserts only `ShareSum==100` for 1/1/1 — **any** remainder recipient passes. Implementation (`InsightsModel.cpp:122-130`) distributes remainder in sorted order (desc share, asc name). A mutant distributing to the *last* share, or largest-remainder-first, survives. The first CategoryShares case pins values (40/30/30) but not the +1 recipient on a true remainder case, nor sort order/tie-break (`a.first < b.first`).
- **Fail-before: NONE recorded** for this file.
- **Hermeticity: PERFECT** — pure, no clock/FS. `CurrentStreak` takes `today_key` as a parameter (good design, tested deterministically).
- **Missing:** invalid/malformed `today_key` -> 0; duplicate day records aggregation in streaks (two records same day, one zero-words); unsorted input to `LongestStreak` (impl uses `std::set` so order-free, but no test proves it); `Wpm` with negative `speaking_ms` / negative words; `DayKey` with single-digit year or month 13 (impl `snprintf %04d` handles, untested); `CalendarLevel(24,100)` (quarter floors to 0 -> clamped 1 — boundary just below the first tested 25).

### 2.4 `test_statsstore.cpp` (10 cases) — STRONGEST on cap/threads, two hermeticity nits

- **Behavior quality: STRONG.** Roundtrip pins words/dictations/speaking/corrected/fixes + category + app attribution with exact values; toggle-off no-op pins `Snapshot().empty()` **and** `Generation()==0`; empty/whitespace-only text ignored; corrupt-tolerate + fresh-start; category precedence (`process-first, title-second`: `slack+Gmail-title -> Work messages`, `chrome+ChatGPT -> AI prompts`) — excellent precedence pins; unknown-value preservation (`Custom Thing` category survives load).
- **730-day cap: GOOD boundary** — plants 731 distinct days, expects exactly 730, oldest absent, today present [VERIFIED test_statsstore.cpp:107-136]. Off-by-one in either direction fails. (Caveat §4: `DateKey(now - i*86400)` is not calendar-day arithmetic — see DST flake note.)
- **Threads: GOOD** — 8x50 records, exact 400/800 totals. Real lock contention, not mocked.
- **Fail-before: NONE recorded** for this file (no FIX report covers statsstore).
- **Hermeticity: WEAK in two ways (MEDIUM).** (a) `TempPath(name)` returns a *file* path; cases call `std::filesystem::remove(path)` before/after but never remove the `.tmp` sidecar that `PersistLocked` creates (`StatsStore.cpp:271`) — a leftover `.tmp` from a crashed prior run is never read back (Load only opens `path_`), so impact is low, but the fixture is not fully self-cleaning. (b) No per-case directory isolation — all cases share `temp_directory_path()/lwf_stats_02b_<name>.json` by name; two concurrent `lwf_tests` processes running the same filter would collide. Matches house pattern, but not parallel-safe.
- **CountWords case is mislabeled:** `"splits on unicode whitespace"` only exercises ASCII space/tab/newline — no NBSP/ideographic-space input despite the name (contrast textdiff, which does test NBSP/CJK). Test-name overclaims coverage. LOW.
- **Missing:** negative `duration_ms`/`words_corrected`/`dictionary_fixes` clamping (impl clamps at StatsStore.cpp:156-158, untested); empty `process_name` -> `"unknown"` app key; `Record` title path (impl hardcodes `CategoryFor(process, "")` at :159, so title-based attribution through `Record` is *untestable* — the resolver title cases pass only because they call `CategoryFor` directly; this coupling deserves a comment/test pinning the current limitation); `DateKey(ts<=0)` fallback-to-now.

### 2.5 `test_dictionarystore.cpp` (17 cases) — STRONG after fix, error paths thin

- **Behavior quality: STRONG on migration.** Four legacy shapes pinned (bare array, `{words}`, `{words+replacements}`, v2 roundtrip with all fields incl. `id/starred/usage_count/created_at/updated_at`), seed-on-first-run, `.bak` created-once-from-pre-migration-bytes-and-never-overwritten (byte-equality `ReadFile(.bak)==legacy` + second `Save` doesn't touch it — genuinely good), reload-picks-external, PersonalDictionary v2-load + reload, export/import roundtrip, corrupt->`.bad` + `Size()==0` + `Load()==false`.
- **Validation messages: GOOD** — `"Word already exists."` exact, `"maximum 2000 entries reached"` exact.
- **Fix-count: GOOD** — replacements and word-corrections both counted, multi-occurrence (`gihub and gihub -> 2`), zero case.
- **Fail-before: STRONG** [VERIFIED W1-FIX-DICT.md]: 16-case filter pre-fix = 14 pass / 2 fail with both root causes proven (seed forgotten; malformed JSON). Post-fix 17/17, 2122 assertions.
- **Hermeticity: GOOD** — per-case suffix + `remove_all`. Note the `max 2000` case issues 2000 `CHECK(Add(...))` — slow-ish but bounded; [RAN in filter: included in 2310-assertion runs].
- **Gaps (P1-3, §7):** `Add` with empty/whitespace-only phrase (message?); `Remove`/`Update`/`SetStarred`/`BumpUsage` paths have **zero** direct tests (only `Remove` used once to clear the seed in the max case); `ImportJson` corrupt/unsupported input (error string + store unchanged) untested — only `Load` corrupt is tested; `Search` ordering (starred-first) untested though `Entries()` sort is; sort tie-break when `starred` and `updated_at` both tie (impl falls to `created_at` then ?) unpinned. Legacy-format variants beyond the 4 shapes (e.g. `{"entries":[...]}` without `schema_version`, v1 object with unknown extra keys preserved) untested.

### 2.6 `test_snippetstore_v2.cpp` (9 cases) — STRONGEST on exactness

- **Behavior quality: EXCELLENT on §H exactness.** `"v2 validation messages are exact"` pins every message string, the 4000/4001 boundary in *both* directions, the precedence case `4001 spaces -> "Snippet too long"` (length checked before empty-trim — matches `SnippetStore.cpp:357-361`), case-insensitive duplicate detection in both directions (trigger==other expansion, expansion==other trigger), self-edit allowed (`Validate("alpha","updated")==""`). Empty-expansion-removes-after-length-check is pinned (`4001 spaces` rejected, entry survives). Longest-trigger-wins, casing-preserved, punctuation-alone-vs-contains, whole-word boundaries (`emails`/`mymailbox` not matched) — all real behavior.
- **Fail-before: STRONG (compile-level)** [VERIFIED W1-FIX-SNIP.md]: pre-fix TU RED (deleted move ctor via `mutex_`); post-fix 10-filter/74 assertions green. Behavioral fail-before (a wrong message string failing) is not separately recorded, but exact-string assertions are maximally fail-sensitive by construction.
- **Hermeticity: WEAK (MEDIUM).** `TestDir()` returns **one fixed path** `lwf_snippetstore_v2` for all 3 file cases, with `remove_all` at entry. Cases run sequentially under doctest so this passes, but (a) parallel `lwf_tests` processes collide; (b) a crashed run leaves state the next run silently wipes — masking a persistence bug *between* cases (e.g. v2-save's `.bak` assertion could pass against a stale `.bak` from the previous case if `remove_all` silently failed). Per-case suffix (as in dict/atomic/notes) is the established pattern in this same wave — this file deviates. Same deviation in transformstore (§2.7).
- **Missing (P2):** contains-mode tie-break for equal-length triggers (impl sorts by size desc, then `trigger <` asc at SnippetStore.cpp:323-329 — unpinned); overlapping-trigger replacement order in whole mode (impl picks longest *raw* trigger size, not normalized — unpinned); `Upsert` with `exclude_id` (edit path) untested despite `Validate(exclude_id)` existing; `RemoveByTrigger` untested; non-ASCII trigger casing (`ToLowerUtf8Safe`) untested.

### 2.7 `test_transformstore.cpp` (7 cases) — STRONG after fix

- **Behavior quality: STRONG.** Seed shape pinned field-by-field (10 slots, Polish/PromptEngineer prompts with `<selection>{text}</selection>` + `data, not instructions`, shortcut `Win+Alt+1..0`, `DiffShortcut()=="Win+Alt+O"`, `Enabled()==false`); `{text}` auto-append + 2000/2001 cap (the fixed bug — pins the *editor-copy* semantics: 2000 raw chars accepted even though stored becomes 2008); opt-in persists; v1->v2 migration (slots 3,4, `.bak` contains legacy bytes, file rewritten to v2 — the open-handle bug); v2-load backfills `{text}`; shortcut conflict disables slot + warning contains `"conflicts"` + subset-chord collision (`Win+Alt` subset of `Win+Alt+1`); corrupt keeps seeded built-ins + file untouched.
- **Fail-before: STRONG** [VERIFIED W1-FIX-TRANS.md]: 3 failures with 3 distinct root causes, each mapped to the exact reverted hunk. Best fail-before documentation in the wave.
- **Hermeticity: WEAK (MEDIUM)** — same fixed-dir deviation as snippetstore (`lwf_transformstore`, 3 file cases share it).
- **Gaps (P1-4, §7):** `Validate`/`SetSlot` reject slot 0 and slot 11 (impl bounds `1..10` — completely untested); empty/whitespace name message; duplicate custom name across slots; `SetSlot` on a built-in slot (1/2) — allowed or rejected? unpinned; `IsSlotEnabled(n)` for an *empty* slot (should be false even when globally enabled — untested); custom `diff_shortcut` persisted across Save/Load (only default is pinned); v1 file with >8 entries (impl truncates with warning at TransformStore.cpp:287-292 — untested); v1 entry with empty name skipped (untested); `Validate` empty-prompt message path (empty prompt is allowed? `SetSlot(3,"Shorten","Make it short")` doesn't cover the empty case).

### 2.8 `test_notesstore.cpp` (11 cases) — STRONGEST overall

- **Behavior quality: EXCELLENT.** Full CRUD + persistence roundtrip (incl. index *not* containing the deleted id — negative pin); pinned-first ordering + `Pin` does **not** touch `modified_at` (pinned via `Unpin` comparing `modified_at` — subtle, correct); search title+body case-insensitive + empty/blank query returns all; blank-title fallback chain (first-body-line, padded first line, `Untitled` for empty/whitespace bodies) incl. `Update`-driven re-derivation and persistence; **orphan reconcile** (two `.md` adopted, `.txt`/`.tmp` ignored, adoption persisted — the §H orphan case directly); dangling index dropped + persisted; **hostile id** (`../evil` in index never escapes `notes/`, `Get("../evil")==nullopt`, outside `evil.md` never read — security behavior pinned); corrupt index -> `.bad` byte-preserved + orphan sweep recovers bodies; delete-returns-entry-for-undo + `Restore` roundtrip + double-restore rejected + invasive restore rejected; reload-picks-late-file; 4x25 concurrent creates with exact count, unique ids, every body file present, reloaded count exact.
- **Fail-before: NONE recorded** for this file (no FIX report). The hostile/orphan cases are high-value; a fail-before (e.g. reverting `IsValidId`) would be cheap to record but was not.
- **Hermeticity: GOOD** — per-case `TestDir(name)` + `remove_all`; concurrent case is the only thread user. Clock dependence is mild (`created_at > 0`, `modified_at >= created_at` — not exact-equality, correctly non-brittle).
- **Over-mocking: NONE** — real FS throughout, which is right for a two-file reconcile store.
- **Minor brittleness (LOW):** `CHECK(Search("o").size() == 2)` — single-char substring match over both notes; correct today, silently breaks if either fixture body changes. Prefer a distinct token.
- **Missing (P2):** duplicate-id entries in a hand-written index (first/last wins?); orphan `.md` that *is* pinned in a surviving index (orphan can't be pinned — shape untestable, fine); `Restore` with an id that already exists (tested via double-restore only); body containing invalid UTF-8 / extremely long single line; `Create` with title longer than any cap (none exists — unpinned).

## 3. Fail-before evidence — summary

| File | Fail-before recorded? | Evidence |
|---|---|---|
| test_textdiff | YES | W1-FIX-DIFF: exact pre-fix assertion values + byte-level cause |
| test_dictionarystore | YES | W1-FIX-DICT: 14/16 pass pre-fix, both causes proven |
| test_snippetstore_v2 | YES (compile) | W1-FIX-SNIP: TU RED pre-fix, 74 assertions green post-fix |
| test_transformstore | YES | W1-FIX-TRANS: 302/308 pre-fix, 3 causes each hunk-mapped |
| test_atomicfile | NO | — |
| test_insights | NO | — |
| test_statsstore | NO | — |
| test_notesstore | NO | — |

For the 4 YES files, fail-before is genuinely demonstrated (not asserted). For the 4 NO files, correctness rests on post-fix green only — acceptable for new-feature suites, but a one-line mutation spot-check (e.g. revert `IsValidId`, flip streak `+1`) would have closed the loop. Standing order 10 ("must fail without the fix") is evidenced in half the wave.

## 4. Determinism / isolation / flakiness

- [RAN 3x filtered subset (39 cases, incl. stats-threads, notes-concurrent, dict-max-2000, migration, cap) -> 3/3 PASS, 2310 assertions each, exit 0]. [RAN full Wave-1 filter -> 58/58 (2628 assertions) + 29/29 (99 assertions) PASS]. No flake observed in-session.
- **Hermetic fixtures:** atomicfile, dictionarystore, notesstore use per-case suffix dirs + `remove_all` — GOOD. Snippet/transform use one fixed dir for all cases — passes sequentially, not parallel-safe (MEDIUM, §2.6/2.7). Statsstore uses per-case *file* names but never cleans `.tmp` sidecars and shares the temp root — LOW.
- **Clock dependence:** statsstore uses real `NowSeconds()` + `DateKey`. The evict case plants keys via `DateKey(now - i*86400)` — near a DST transition or if the test runs across local midnight between planting and construction, distinct keys can collapse (two `i` values map to one date) making `snap.size()==730` fail spuriously. Probability is low (one hour per year per timezone) but the test is not midnight-safe. Recommend pinning via fixed timestamps far from "now" is impossible (evict is relative to now) — at minimum, assert `<= 730` plus oldest-absent, or retry-once. LOW.
- **No sleeps, no retries, no network, no wall-clock exact-equality** anywhere in the wave. Thread tests assert exact totals (correct — no timing-dependent counts).

## 5. Test bugs found (structural patterns)

All 4 pre-existing test bugs were found and fixed by the FIX agents; recorded here structurally because the *patterns* recur:

1. **`a\xA0b` hex-escape greed (test_textdiff.cpp:110, FIXED `5aac095`).** Pattern: `\xHH` greedily consumes following hex digits; `b` is a hex digit so `\xA0b == 0xA0B`. Structural signature: any `\x` escape immediately followed by `[0-9a-fA-F]`. Fix idiom: split the literal (`"a\xC2\xA0" "b"`). Grep-able: `\\x[0-9A-Fa-f]+[0-9A-Fa-f]` adjacent to a hex-digit char. **Residual risk: LOW** — no other `\x` escapes in the 8 files [VERIFIED by inspection of the diff].
2. **Missing root brace in concatenated raw string (test_dictionarystore.cpp:174, FIXED `a92ac4a`).** Pattern: multi-part raw-string JSON where the final part ends `...}])"` (entry-close + array-close + terminator) but omits object-close `}`. Structural signature: a JSON fixture that *no* correct implementation could parse — the test cannot distinguish "store bug" from "fixture bug" without external proof, which the FIX report supplied. Lesson: fixtures should be validated by an independent parse (e.g. `nlohmann::json::parse` in a comment, or a `.json` file) before blaming the store.
3. **Seed-count-off-by-one (test_dictionarystore.cpp:139, FIXED `a92ac4a`).** Pattern: `Load()` on a missing file *seeds* one entry, so `Size()==1` at setup, then two `Add`s make 3 — the test asserted 2, forgetting the seed it had itself asserted two lines earlier. Structural signature: setup assertion and later count assertion in the same case disagree about initial state. Fix was `==3`, preserving exactness (correctly not loosened to `>=`).
4. **Return-by-value of a non-movable store (test_snippetstore_v2.cpp:31, FIXED `aa382b6`).** Pattern: helper `StoreWith(...) -> SnippetStore` + copy-init, but `SnippetStore` holds `mutable std::mutex` so move/copy are deleted. Structural signature: any `StoreType x = Helper(...)` where the store has a mutex member. This one was caught at compile time (best possible outcome) — the wave's other stores (Transform, Notes) were checked and are clean [VERIFIED W1-FIX-SNIP.md].

**New test bugs found by THIS review (not fixed — read-only):**

5. **Vacuous atomic-crash case (test_atomicfile.cpp:57-66, LOW).** `"a tmp write without the rename leaves the old content intact"` never calls the unit under test. Mutant-proof recommendation: either delete it or make it call `WriteAtomic` and then simulate the crash by checking the committed file (which is already covered by the stale-tmp case). As written it inflates the case count by one.
6. **Mislabeled CountWords case (test_statsstore.cpp:185, LOW).** Name claims "unicode whitespace"; body tests only ASCII. Either add an NBSP/ideographic-space CHECK (mirroring textdiff) or rename to "ascii whitespace".

## 6. Mutation thinking — 9 plausible bugs, caught or surviving

| # | Mutant | Caught? | Why / why not |
|---|---|---|---|
| M-1 | TextDiff tie-break `>` -> `>=` (Delete-first instead of Insert-first) | **SURVIVES** | All counts identical; no test pins token *order* for a tie case. The plan's core invariant `Equal==LCS` is also unasserted. |
| M-2 | CategoryShares remainder distributed to last share instead of first | **SURVIVES** | Only `ShareSum==100` is asserted for remainder cases. |
| M-3 | CategoryShares sort tie-break `a.first<b.first` flipped to `>` | **SURVIVES** | No equal-share case asserts order. |
| M-4 | Snippet contains-mode equal-length tie-break flipped (alpha asc -> desc) | **SURVIVES** | Only distinct-length longest-match is tested. |
| M-5 | TransformStore `Validate` off-by-one `> 2000` -> `>= 2000` | CAUGHT | `Validate(2000)==""` + `Validate(2001)=="Prompt too long"` pin both sides. (This is the fixed bug class — now pinned.) |
| M-6 | StatsStore evict cutoff `729` -> `728` or `730` | CAUGHT | 731 planted, exactly 730 kept, oldest absent — both directions fail. |
| M-7 | Snippet length-check order swapped (empty-trim before length) | CAUGHT | `Validate("word", 4001 spaces)=="Snippet too long"` pins length-first. |
| M-8 | DictionaryStore cap `2000` -> `1999`/`2001` | CAUGHT | Loop fills to exactly 2000 then next `Add` must fail with exact message. |
| M-9 | NotesStore `IsValidId` slash check removed (`../evil` accepted) | CAUGHT | Hostile case pins `Get("../evil")==nullopt` + outside file unread. |
| M-10 | Insights `CalendarLevel` quarter `day*4/max` -> `day*3/max` | CAUGHT | 25/50/75/100 quartiles all pinned. |
| M-11 | Insights `CurrentStreak` counts back through a gap (ignores missing day) | CAUGHT | Gap case (`LongestStreak` 3 across gap) + `CurrentStreak(today+1)==0`. |
| M-12 | StatsStore `Record` stops clamping negatives (duration/corrected/fixes) | **SURVIVES** | No negative-input case exists. |
| M-13 | Dict `Add("")` accepts empty phrase | **SURVIVES** | No empty-phrase Add test. |
| M-14 | Transform `SetSlot(0 or 11, ...)` accepted | **SURVIVES** | No out-of-range slot test. |
| M-15 | AtomicFile `WriteAtomic` leaves `.tmp` behind on success | CAUGHT | Three cases assert `!exists(.tmp)`. |
| M-16 | Notes `Pin` touches `modified_at` | CAUGHT | `modified_at == before` after Unpin pin. |

**Survivors: M-1, M-2, M-3, M-4, M-12, M-13, M-14 (7 of 16).** Classes: tie-break order (M-1/M-3/M-4), remainder recipient (M-2), unclamped negatives (M-12), unvalidated inputs (M-13/M-14). Note M-1/M-2 are the highest-value survivors because the *plan* (§H) explicitly calls out tie-break and formulas — the tests verify the formulas' sums but not their ordering decisions.

## 7. Missing edge cases — prioritized, with sketches

### P1
1. **TextDiff LCS invariant + tie-break order.** Sketch:
   ```cpp
   // for a battery of (a,b) pairs:
   CHECK(CountOp(r, Equal) == LcsLength(Tokenize(a), Tokenize(b))); // independent LCS oracle in test
   // tie case: Diff("a x", "x a") — pin full op sequence, not just counts
   ```
2. **CategoryShares recipient + order.** Sketch: records 1/1/1 words in categories `c,b,a` (insertion order scrambled); assert exact vector `{("a",34),("b",33),("c",33)}` (desc-share, asc-name, remainder to first). This single case kills M-2 and M-3.
3. **DictionaryStore mutation error paths.** Sketch: `Add("",...)` / `Add("   ",...)` -> false + message; `Remove(missing)==false`; `ImportJson("{bad")` -> false + entries unchanged; `ImportJson` of valid JSON into a store that already has entries (replace vs merge — pin current behavior).
4. **TransformStore slot/name validation.** Sketch: `Validate(0,...)`, `Validate(11,...)` non-empty; `SetSlot(3,"  ",...)` rejected; `SetSlot` duplicate custom name rejected (or pinned allowed); `IsSlotEnabled(3)==false` for empty slot even when globally enabled.

### P2
5. Snippet contains-mode equal-length tie-break + `Upsert(...,exclude_id)` edit + `RemoveByTrigger`.
6. Notes duplicate-id index entries; `CurrentStreak` malformed `today_key`; `LongestStreak` unsorted/duplicate input; `Wpm` negatives; `DayKey(999,1,1)` padding.
7. Stats negative clamping + empty-`process_name -> "unknown"` + `CountWords` with NBSP/ideographic space (fix the mislabeled case).
8. Transform v1 >8 entries, empty-name v1 entry, custom `diff_shortcut` persistence, `SetSlot` on built-in slots 1/2.

### P3
9. AtomicFile binary/NUL content; `path` is a directory; Transform `Validate` empty-prompt path; Dict legacy `{"entries":[...]}` shape.

## 8. Over-mocking / abstraction assessment

**No over-mocking anywhere in the wave.** Every suite tests the real unit against the real filesystem (stores), real containers (insights), or pure functions (textdiff/atomic). No mocks, no fakes, no test doubles in any of the 8 files. This is the correct choice for persistence/reconcile stores whose contract *is* the filesystem layout (`.bak`/`.bad`/`.tmp` sidecars, orphan sweep). The one abstraction concern is inverse: `StoreWith` in snippetstore and the hand-rolled `TempDir/WriteFile/ReadFile` helpers are copy-pasted across 5 files with slight variations (fixed-dir vs suffixed-dir) — the hermeticity deviation in §2.6/2.7 comes from exactly this duplication. A shared `tests/StoreFixture.h` (or at minimum copying the suffixed pattern) would remove the class. P3, not a defect.

## 9. Remaining risks / NOT-RUN

- **NOT-RUN:** full unfiltered `ctest` (exceeds session timeout per W1-FIX-DICT/DIFF reports; this review ran filtered Wave-1 slices only). Full-suite green is inferred from the filters, not observed.
- **NOT-RUN:** mutation reverts to prove survivors empirically (read-only mandate) — M-1..M-16 are static analysis of assertions vs implementation, not executed mutants. Each survivor claim is falsifiable by running the sketched test against the mutant.
- **Build freshness:** binary is 2026-11-07 14:24, post-fix; no rebuild was performed, so a source edit after that timestamp would invalidate the runs above. [RAN `git status --short` shows only `M COMMS.md` + untracked `briefs/` — no source drift.]
- The statsstore midnight/DST flake (§4) and snippet/transform fixed-dir parallel collision (§2.6/2.7) are the only two determinism risks; both are LOW-probability but real.

## 10. Long-run test (standing order 8)

- **Source:** the 4 fixed bug *patterns* (hex-escape greed, unparseable fixture, forgotten seed, non-movable return-by-value) are all grep-able classes; this review greps them clean in the remaining files. The 2 new findings (vacuous case, mislabeled case) are the same "test cannot fail" family — recommend a wave-wide rule: every test name must be falsifiable by reverting the unit's primary behavior.
- **Load:** dict-max (2000 CHECKs) and notes-concurrent (100 creates + reload) are the heaviest; both complete in seconds [RAN]. No unbounded loops in tests.
- **Hostile:** notesstore is the model (path traversal, corrupt, dangling, orphan). Dict/stats/transform corrupt cases are thinner (no hostile *content* beyond "not json" — e.g. valid JSON of wrong shape, huge JSON, deeply nested). P2.
- **Operability:** no test writes outside `temp_directory_path()`; no secrets; no network. Clean.
- **Reversal:** 4 files have proven reversal (fail-before). 4 do not — recommend recording one mutation per file before wave close, as the cheapest remaining evidence.

---
*Reviewer: W1-TREV-01 (read-only on source). No source files edited. This report is the sole artifact.*
