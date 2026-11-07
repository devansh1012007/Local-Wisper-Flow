# W1-IMPL-02C T-006 DictionaryStore — report

## ORCH-DIGEST
Status: REVIEW_REQUIRED (code complete; DEFERRED-DEPENDENCY on scripts/check_no_network.sh allowlist; NOT-RUN on build/tests per branch brief §7 shared-build contention; hygiene OK).
Files created: src/core/DictionaryStore.h, src/core/DictionaryStore.cpp, tests/test_dictionarystore.cpp.
Files modified: src/core/PersonalDictionary.h, src/core/PersonalDictionary.cpp (fix-count default-param, v2-aware Load, Reload hook).
Behaviour: 4-shape migration to v2 (schema_version 2), .bak-once from pre-migration file, first-run seed btw/by the way, trimmed case-insensitive dup rejection "Word already exists.", 2000-entry cap, case-insensitive substring search over phrase+replacement, starred-first then newer-first display sort, atomic tmp+rename Save, Import/Export JSON v2, corrupt file renamed to .bad (empty state, Load false), full mutex guard (HistoryStore pattern).
PersonalDictionary: Enforce(text, fuzzy, fix_count=nullptr) additive; counts each whole-word replacement occurrence plus each word-case/fuzzy token correction; Reload() re-runs Load(path_); Load now parses v2 entries (phrases to Words, phrase->replacement to Replacements) so pipeline keeps working after migration; matching order unchanged.
Need: scripts/check_no_network.sh:30 allowlist must add DictionaryStore.cpp in P02 same-commit edit (posted to COMMS). Verified [RAN check_no_network -> FAIL listing DictionaryStore.cpp:378,600 plus siblings Stats/Transform/Snippet pending same edit].
Verification: [RAN hygiene -> OK]. Build/ctest NOT-RUN (CMakeLists integrator-owned, brief forbids shared-build run). Self-review completed below; independent fresh-context review recommended before Wave 2 wiring (highest-risk data task).

## 1. Root cause and context summary
Root cause [VERIFIED src/core/PersonalDictionary.cpp:89-139, src/core/SnippetStore.cpp:76-128, grep Save]: dictionary.json was load-only via PersonalDictionary, with no Save path, no UI, no fix counts. Main loads once at startup [VERIFIED src/main.cpp:369-370] and pipeline + cloud keyterms reuse the same object [VERIFIED src/main.cpp:389,414,425,433], so page edits could never reach the pipeline without restart (plan D-OP2 dual-source staleness). The migration surface had 4 shapes ever written [VERIFIED PersonalDictionary.cpp:104-128]: bare string array, {words:[]}, {words:[], replacements:[{from,to}]}, plus the new v2. Investigation INV-datastore confirms both stores were Load-only and neither had a mutex; HistoryStore/SettingsStore provide the locking and atomic-write precedents [VERIFIED src/core/HistoryStore.h:38, src/core/SettingsStore.cpp:217-228]. Approach: introduce DictionaryStore as the file/CRUD owner (v2 + migration + Save), keep PersonalDictionary as the pipeline-facing projection, and bridge via shared-instance Reload semantics so Wave 2 can wire a single instance (pipeline + page + cloud keyterms) with reload-on-save instead of restart. PersonalDictionary Load was extended to understand v2 because otherwise the first migration would silently empty the pipeline (R5). Enforce order was not altered.

## 2. Context report (pre-change)
- Architectural entry points: PersonalDictionary::Load/Enforce/InitialPrompt (pipeline TextPipeline.cpp:97, main.cpp:389), cloud keyterms via Words() (main.cpp:414+), SettingsUiModel::DictionaryEditorModel (unwired, not adopted — DictionaryStore Entry superset was cleaner).
- Invariants preserved: Enforce whole-word case-insensitive matching, no reordering/sorting, old 2-arg Enforce call sites (TextPipeline) compile unchanged via default param; old dictionary.json files never overwritten without .bak/.bad; additive JSON (v2 omits empty replacement field, HistoryStore-style optional omission).
- Risks: migration is one-way (mitigated by .bak), First Save overwrites real file (mitigated by tmp+rename never truncating in place), PersonalDictionary thread safety (pre-existing no-mutex, see risks), Windows rename of open file (fixed by close-before-rename).
- Expected tests: T-mig x4, T-dup, T-count, T-reload from plan §H, plus search/sort/max/bak/seed/corrupt covered in tests/test_dictionarystore.cpp.

## 3. Phased changes executed
### Phase 1 — DictionaryStore header + implementation (new)
Files: src/core/DictionaryStore.h (62 lines), src/core/DictionaryStore.cpp (~630 lines). Conventions: #pragma once, no code comments anywhere (AGENTS.md + brief §6), HistoryStore mutex pattern, SettingsStore tmp+flush+rename+bak-copy ordering (copy happens before rename, so .bak holds the pre-save bytes). Key engineering decisions:
- DECISION — v2 JSON shape: {"schema_version":2, "entries":[{"id","phrase","replacement?","starred","source":"manual|imported","usage_count","created_at","updated_at"}]}. Replacement key omitted when empty (matches HistoryStore optional-field omission, keeps diffs readable). Timestamps are int64 seconds (HistoryStore NowSeconds pattern).
- DECISION — Migration order and merge: file order preserved; legacy words first then replacements; sequential ids 1..N for legacy; v2 ids preserved with repair of missing/duplicate ids. MergeParsed layer dedupes trimmed case-insensitively, first-wins, but upgrades a bare word entry when a later replacement for the same phrase arrives (otherwise a word and replacement naming the same phrase would silently drop the replacement — verified edge, e.g. words:["gihub"], replacements:[{from:"gihub",to:"GitHub"}]).
- DECISION — .bak once semantics: Save copies current on-disk file to path+".bak" only when this load migrated from legacy AND .bak does not yet exist; flag cleared after first successful Save. Later v2 saves never overwrite .bak. Corrupt files go to .bad (not .bak) per plan §I corruption row.
- DECISION — Load contract: missing file -> seed btw + Load true (usable first-run); corrupt/unsupported JSON -> close handle, rename to .bad, empty entries, Load false (distinguishes corrupt from first-run, matches SnippetStore/PersonalDictionary false-on-bad precedent while still never silently overwriting); legacy                      -> migrate + Load true + migrated flag. Reload() delegates to Load().
- DECISION — Sorting: Entries()/Search() return starred-first, then updated_at desc, created_at desc, id desc. Internal vector keeps insertion/file order; Save/ExportJson write internal order (no surprise file reorder on save).
- DECISION — RecordUsage increments usage_count only, does not bump updated_at (usage is not an edit, must not churn newer-first order). SetStarred does bump updated_at (starring is an edit; starred band already dominates).
- DECISION — Import tolerant vs Load lenient vs Add strict: Add/Update enforce exact duplicate message and 2000 cap as errors; Load/Import truncate+dedup silently with warning log (user file/import must never fail wholesale). Importing legacy JSON marks entries as Imported source; v2 source preserved; migration marks Manual.
- DECISION — No dependency on sibling 02A AtomicFile header (brief forbidden): tmp+rename implemented locally, mirroring StatsStore sister per brief.
- INVARIANT — All public ops hold mutex_; Path() returns reference to immutable-after-ctor string; Save refuses to write if in-memory size exceeds cap (defensive).
### Phase 2 — PersonalDictionary fix-count + Reload + v2 Load
Files: src/core/PersonalDictionary.h (added Reload(), defaulted fix_count param, path_ member), src/core/PersonalDictionary.cpp.
- Enforce: signature Enforce(text, fuzzy, fix_count=nullptr) (plan P13 additive out-param with default, exactly). Counting increments once per successful replacement substitution loop iteration and once per word-phase token that actually changes (case or fuzzy). Null pointer tolerated; old TextPipeline call unchanged.
- Load: now stores path_ on entry (so Reload retries same path even after a failed load), and parses v2 before legacy branches: entries -> Words = phrases (then legacy 60-char/empty filter still applies), Replacements = (phrase, replacement) when replacement non-empty. Degenerate v2 without entries array returns false.
- Reload(): bool, false when no path ever set, else Load(path_).
- WHY — write-set note: brief §4 labels PersonalDictionary as "(count overload only)" but §3 explicitly orders Reload hook + Keep Load() working + "replacing PersonalDictionary's file handling, keeping its public functions working". Without v2-aware Load the first Save would empty the pipeline; without Reload D-OP2 cannot be wired in Wave 2. Edits were confined to those three additive changes; no other PersonalDictionary logic was refactored. No Enforce order change (whole-word case-insensitive, no sorting added).
### Phase 3 — Tests (new)
File: tests/test_dictionarystore.cpp (17 TEST_CASEs, NOT-RUN). Hermetic temp_directory_path fixtures, never touches repo dictionary.json. Covers: T-mig x4 (array, words, words+replacements, v2 round-trip with starred/usage/timestamps), seed, dup rejection exact string + trim, search (phrase+replacement, case-insensitive, empty query), starred sort via ImportJson-controlled timestamps, max 2000 boundary (2000 ok loop + 2001st rejected), fix-count (replacements incl. repeats, word corrections, null-param legacy path), .bak once (legacy bytes preserved across second Save), Reload external change, PersonalDictionary v2 integration + Reload after Store Save, export/import round-trip, corrupt -> .bad + empty + false.
- Mutation mental check: deleting ParseLegacy replacement branch fails mig_modern; removing dup check fails dup test ("Word already exists." never produced); removing fix-count increment fails T-count; removing migrated flag fails bak test; removing v2 branch in PersonalDictionary fails pdv2 test (Enforce would return input unchanged). Tests are behaviour-focused, no mocking of unit under test.

## 4. Review audit
### Phase 1 self-review (stranger view of diff)
- CORRECTNESS: off-by-one on cap — Add checks size>=2000 before insert, so 2000 succeeds, 2001st rejected [line 443]. Sort comparator is strict weak ordering (starred, updated desc, created desc, id desc) — deterministic. Rename targets: tmp rename overwrites destination (SettingsStore precedent); .bak copy uses overwrite_existing but only when .bak absent (dead flag check, harmless). [MINOR] ImportJson unused migrated local (passed by ref, no warning) — found, left as intentional out-param.
- SECURITY/EDGE: empty file -> parse throws -> .bad path (never silently truncates); unsupported schema_version -> .bad; whitespace-only phrase -> rejected/skipped; over-long legacy phrase kept in store (PersonalDictionary still filters >60 for prompt — truncation only at projection, not data loss); negative usage_count clamped at parse; relative paths handled (no path construction, caller-supplied).
- MAINTAINABILITY: helpers local to anonymous namespace, no duplication of PersonalDictionary logic beyond Trim/ToLower (small, matches codebase duplication instead of shared utility — acceptable, no premature indirection); no comments per AGENTS.md; no TODO.
- [CRITICAL] Windows rename of still-open ifstream in Load corrupt branches (SettingsStore closes before rename) — FOUND IN REVIEW, FIXED: added file.close() before both rename calls.
### Phase 2 self-review
- CORRECTNESS: v2 Load branch placed first (avoids legacy object branch swallowing v2 since v2 object has no words key — ordering safe even without, but explicit is clearer). Count increments placed after result==out guard so identity replacements do not inflate count. Fuzzy/case token counts coincide with actual rebuilt change. Old 2-arg callers unaffected (default nullptr).
- [MAJOR] PersonalDictionary thread safety: Enforce (const, engine thread) vs Reload (UI thread) races on words_/replacements_/path_ (pre-existing no-mutex design). Not introduced by this task; adding mutex would exceed "count overload" scope and change class layout contract. Documented as remaining risk for Wave 2 wiring (EngineBridge serialization or Store-level lock must be decided at integration; DictionaryStore itself is mutex-guarded). Not fixed here by scope discipline — surfaced explicitly per Zero-Shortcut Mandate.
### Phase 3 self-review
- Two test bugs caught in review-and-fix: dup test originally asserted beta fresh-add fails (fixed to succeed); pdreload test contained a convoluted ContainsPhrase workaround (rewritten to direct Words loop). Export test starred call removed.
- Determinism: sort test avoids wall-clock dependence by importing explicit timestamps; max test is 2000 adds with O(N^2) dup scan (~2M comparisons, fast, hermetic). Temp dirs use remove_all + create_directories (test_settings TempDir pattern).
### Final review (cross-phase)
- Cross-phase: Store Save(v2) -> PersonalDictionary Load(v2) -> Enforce/Search/prompt all coherent; Reload chain: Page edits -> Store Save -> dict.Reload() (D-OP2) works without restart. Backwards compat: existing test_dictionary.cpp expectations (legacy array/words/modern loads, Enforce strings, AddWord order) untouched by header default param. No CMakeLists edit (integrator-owned): new sources/tests intentionally unregistered this wave.
- No debug artifacts, no secrets/PII, no TODO/lint strings. Header free of code comments and of non-public identifiers per presentation rules.
- Independent review still required: Directory high-risk (first-ever Save + one-way migration). Request fresh-context reviewer for DictionaryStore.cpp migration/merge + Save .bak logic before wiring to main.cpp in Wave 2.

## 5. Verification evidence
- [RAN & "C:\Program Files\Git\bin\sh.exe" scripts/check_hygiene.sh -> hygiene: OK, EXIT 0]
- [RAN & "C:\Program Files\Git\bin\sh.exe" scripts/check_no_network.sh -> FAIL (expected DEFERRED-DEPENDENCY): file-write APIs outside allowlist at src/core/DictionaryStore.cpp:378,600 (+ siblings StatsStore.cpp:273, TransformStore.cpp:442, SnippetStore.cpp:219 — same P02 same-commit edit pending). Exact line needed: check_no_network.sh:30 allowlist regex must include DictionaryStore.cpp (see below).]
- [RAN git status/diff review -> only write-set files changed by this agent (plus COMMS NEED append by orchestrator-owned file, sibling AGENTS.md/COMMS at Wave 1 contention). No forbidden file touched.]
- NOT-RUN: cmake --build / ctest / lwf_tests compile (brief §7: scoped hygiene only, shared build dir + CMakeLists integrator-owned; new files not yet registered). All 17 new cases marked NOT-RUN (needs shared env) — will fail to compile/run until CMakeLists adds DictionaryStore.cpp to lwf_core and test_dictionarystore.cpp to lwf_tests.
- No secrets in code/reports/commits. No fixed ports/containers touched. No processes started.

## 6. Unverified assumptions and remaining risks
- [ASSUMED] v2 field names (id/phrase/replacement/starred/source/usage_count/created_at/updated_at/schema_version) will match Wave 2 DictionaryPage expectations — page not yet written; any divergence is isolated to Store/Load parse and test fixtures.
- [ASSUMED] Words() projection = phrases (triggers) is the right keyterm/prompt source; if Wave 2 wants canonical replacement targets as keyterms, PersonalDictionary v2 branch needs a one-line augmentation.
- Remaining risk (MAJOR, Wave 2): shared-instance threading — PersonalDictionary Enforce/Reload unsynchronized (pre-existing). Wave 2 wiring must serialize pipeline Enforce vs dict.Reload() (or add lock with migration review). DictionaryStore is safe.
- Remaining risk (MEDIUM): .bak only on migration leaves v2-to-v2 first save without backup; R-03 first-save-overwrite is covered for legacy (the dangerous case), but a manual v2 edit flow has no .bak safety net beyond git. Acceptable per brief "(. once)" wording.
- Build risk (MINOR): nlohmann/json is_object/contains/get chains type-guarded (no exception escape except file parse, caught); legacy PersonalDictionary branch retains pre-existing unchecked element["from"].get<string>() (unchanged original code) — a numeric from would throw type_error uncaught; preserved as-is to avoid scope expansion, flagged here.
- Long-run test: Source — new parses/merge are total (skip-invalid, never throw out of Load/Import except bad_alloc); Load — hostile JSON/entirely-valid-legacy paths exercised in tests; Hostile — corrupt/oversized/duplicate fixtures covered, .bad never silently overwrites; Operability — G4 same-commit allowlist is the brittle point (NEED posted, DEFERRED-DEPENDENCY reported); Reversal — git revert + .bak restore per plan §K.

## 7. Git state
Branch: feature/studio-pages [RAN git branch --show-current].
Commit hashes: code commit + report commit (explicit paths only, trailers required). COMMS.md NEED append left uncommitted for ORCH (standing order 5: only ORCH commits COMMS).
Working tree at handoff: this agent's 5 files (2 new src, 2 modified PersonalDictionary, 1 new test) + report file; sibling dirty files (AGENTS.md edit, COMMS append from other agents, 02A/02B/02D untracked sources) observed but untouched.

## 8. Need for orchestrator (DEFERRED-DEPENDENCY)
Posted to COMMS.md: [W1-IMPL-02C -> ORCH | NEED | T-006 | FACT scripts/check_no_network.sh:30 allowlist must add DictionaryStore.cpp ...].
Exact edit owed (file owned by 02A/P02 same-commit): scripts/check_no_network.sh:30
  from: (CrashReporter|HistoryStore|Log|SettingsStore)\.cpp
  to:   (CrashReporter|HistoryStore|Log|SettingsStore|DictionaryStore)\.cpp
Same-commit rule (plan §D/D4, brief §3): land with DictionaryStore.cpp, not before. Until then G4 privacy gate fails on this task (expected, reported honestly). No other outside-write-set change needed; Wave 2 wiring into main.cpp is explicitly out of this brief.

## 9. Gate allowlist copy (for 02A integrator)
The regex on check_no_network.sh line 30 must additionally match the two ofstream sites:
  src/core/DictionaryStore.cpp:378 (Save tmp write)
  src/core/DictionaryStore.cpp:600 (ExportFile write)
Both are text-store writes (allowed category per script header). No fopen/freopen/CreateFile, no network API, no endpoint strings in our files — verified by hygiene OK + no-network hits limited to those two ofstream lines.

