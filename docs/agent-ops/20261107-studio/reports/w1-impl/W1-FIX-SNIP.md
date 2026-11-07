# W1-FIX-SNIP — SnippetStore move-constructor fix (T-007)

## ORCH-DIGEST
Status: COMPLETE (fix applied + compiles + snippet tests green). Root cause [VERIFIED]: `StoreWith()` helper in tests/test_snippetstore_v2.cpp returned `SnippetStore` by value (`SnippetStore StoreWith(...) { ...; return store; }`, line 31) and 6 test cases copy-initialized from it (`SnippetStore store = StoreWith(...)`), but `SnippetStore` holds `mutable std::mutex mutex_` (src/core/SnippetStore.h:49) so its move/copy ctors are deleted (HistoryStore precedent — stores intentionally non-movable). Fix: helper now takes `SnippetStore&` out-param (construct-in-place), 6 call sites changed from `SnippetStore store = StoreWith({...});` to `SnippetStore store; StoreWith(store, {...});`. No move semantics added, no assertions relaxed, no src/** touched. tests/test_transformstore.cpp CHECKED: no return-by-value / by-value-init of `TransformStore` exists (all cases construct in place + Load; grep for `TransformStore\s+\w+\s*=` and `StoreWith` → No matches) → NOT modified per conditional write-set. Verification: configure OK, `lwf_tests` builds OK (previously RED on the move deleted-by-mutex error), snippet-case filter 10/10 passed (74 assertions). Full ctest still Red: 308 cases, 302 passed, 6 failed — ALL outside this fix's scope (dictionarystore ×2, errors empty-catch in TransformStore.cpp, textdiff, transformstore Validate/v1-migration behavioral assertions); none touch test_snippetstore_v2.cpp; each needs its owner (W1-IMPL-02C / W1-IMPL-02D src or textdiff owner) and would require either src/** or relaxing assertions, both forbidden to this agent — recorded as blockers, not fixed.

## Context Report (pre-change investigation)
- SnippetStore non-movability [VERIFIED]: src/core/SnippetStore.h includes `<mutex>`, member `mutable std::mutex mutex_` (line 49), no move ctor declared → implicit move/copy deleted. Same pattern in src/core/TransformStore.h:62, src/core/HistoryStore.h:38 (precedent cited in brief).
- Offending pattern [VERIFIED]: tests/test_snippetstore_v2.cpp:31 `SnippetStore StoreWith(...) { ... return store; }` + 6 call sites lines 48, 73, 79, 87, 98, 111 (`SnippetStore store = StoreWith(...)`). Fix rule per brief: change only TEST files; stores correct as-is.
- TransformStore check [VERIFIED clean]: tests/test_transformstore.cpp defines no helper returning the store; every case does `TransformStore store; REQUIRE(store.Load(...))` in place. No move/store-by-value pattern → file left untouched.
- Pre-existing session state: `M docs/agent-ops/20261107-studio/COMMS.md` + untracked `docs/agent-ops/20261107-studio/briefs/` present before this task (not mine; never staged).

## Phased Changes Executed
- Phase 1 — Helper (tests/test_snippetstore_v2.cpp:31): `SnippetStore StoreWith(init_list)` → `void StoreWith(SnippetStore& store, init_list)`; local `SnippetStore store;` declaration and `return store;` removed; `store.SetEntries(items)` now writes into the caller-owned object. Entry-building loop verbatim.
- Phase 2 — Call sites (same file, 6 cases, judge-required context edits): v2 validation messages exact; longest trigger wins in contains mode; expansion casing is preserved; punctuation and extra spaces ignored only when spoken alone; empty expansion removes entry after length check; empty trigger ignored and whole-word matching case-insensitive. Each: `SnippetStore store = StoreWith(X);` → `SnippetStore store; StoreWith(store, X);`. All downstream `store.*` assertions byte-identical.

## Review Audit
- Self-review: grep post-edit for `StoreWith|SnippetStore store\s*=` in test_snippetstore_v2.cpp → signature + 6 call sites only, no by-value init remains. Grep for move pattern in test_transformstore.cpp → No matches. No assertion text changed (git diff shows only declaration/call lines). No new includes required (`<utility>` already present for std::move of Entry). `std::unique_ptr` alternative rejected: out-param construct-in-place is the brief's first-listed option and preserves `store.` call syntax/minimal diff. CRITICAL/MAJOR: none found. Sanitized: test-only change; store mutex/move semantics untouched.
- Forbidden-scope check: `git diff --stat` at verify time listed only `tests/test_snippetstore_v2.cpp` (mine) + pre-existing `COMMS.md` (not staged). No src/**, CMakeLists.txt, scripts/** edits.

## Verification Evidence
Commands run verbatim from `D:\dev work\coding\Projects\Local-Wisper-Flow` on branch `feature/studio-pages`:

1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` → exit 0. Tail verbatim: `-- Configuring done (4.5s)` / `-- Generating done (18.3s)` / `-- Build files have been written to: D:/dev work/coding/Projects/Local-Wisper-Flow/build`. (Deprecation warnings from doctest/whisper/gRPC/CMP0135 only; no errors. ONNX Runtime absent → VAD skipped, informational.)
2. `cmake --build build --config Release --parallel 4 --target lwf_tests` → exit 0:
   ```
   [  0%] Built target miniaudio
   [ 50%] Built target lwf_core
   [ 50%] Building CXX object CMakeFiles/lwf_tests.dir/tests/test_snippetstore_v2.cpp.obj
   [ 50%] Linking CXX executable lwf_tests.exe
   [100%] Built target lwf_tests
   ```
   (The previously failing TU now compiles together with lwf_core — primary gate.)
3. `ctest --test-dir build -R lwf-tests` → exit 1 (scoped full binary):
   ```
   Test project D:/dev work/coding/Projects/Local-Wisper-Flow/build
       Start 1: lwf-tests
   1/1 Test #1: lwf-tests ........................***Failed   21.12 sec
   0% tests passed, 1 tests failed out of 1
   Total Test time (real) =  21.77 sec
   ```
   Diagnose `ctest --test-dir build -R lwf-tests --output-on-failure` tail verbatim: `[doctest] test cases:   308 |   302 passed |  6 failed | 0 skipped` / `[doctest] assertions: 12402 | 12392 passed | 10 failed |`. Failing cases (all outside this fix): `test_dictionarystore.cpp:139 duplicate trimmed case insensitive is rejected (REQUIRE store.Size()==2, got 3)`; `test_dictionarystore.cpp:174 starred entries sort before newer entries (ImportJson false)`; `test_errors.cpp:117 no empty catch blocks (TransformStore.cpp)`; `test_textdiff.cpp:109 non-breaking space separates tokens`; `test_transformstore.cpp:72 Validate(2000)==` got Prompt too long; `test_transformstore.cpp:107 v1 migration missing "version": 2 / "slots"`. Zero failures in `test_snippetstore_v2.cpp`.
4. Scoped proof this fix's file is green: `./build/lwf_tests --test-case="*v2 validation*,*longest trigger*,*expansion casing*,*punctuation and extra*,*empty expansion*,*empty trigger*,*v2 save*,*v1 array still*,*duplicate triggers*"` → `[doctest] test cases: 10 | 10 passed | 0 failed | 298 skipped` / `[doctest] assertions: 74 | 74 passed | 0 failed |` / `[doctest] Status: SUCCESS!`.

## Unverified Assumptions & Remaining Risks
- [VERIFIED GREEN] Compile gate: fixed TU + lwf_core build succeeded. Snippet behavioral correctness assumed unchanged (no src/** delta; assertions byte-identical) — confirmed empirically by the 74/74 scoped run.
- [BLOCKER — not mine] Full ctest RED (6 cases / 10 assertions) as listed above. Root causes are behavioral/src-level (02C DictionaryStore, 02D TransformStore Validate/migration + empty catch, textdiff tokenization), each requiring src/** or assertion changes forbidden to W1-FIX-SNIP. Left for owners: W1-IMPL-02C / W1-IMPL-02D (+ textdiff owner). Explicitly NOT relaxing assertions or touching src/**.
- UNKNOWN: whether orchestrator's gate counts full-ctest-Red as blocking for this repair task — the assigned finding (compile error) is resolved; remaining Reds are distinct findings outside write-set.
- Long-run: Source clean (no move-semantics band-aid on store); pattern removed not worked around; risk if owners' src changes alter Store API — test-style (in-place construction) stays valid.

## Git State
Branch: feature/studio-pages. This agent's changes (explicit-path commits only): code `tests/test_snippetstore_v2.cpp`; report `docs/agent-ops/20261107-studio/reports/w1-impl/W1-FIX-SNIP.md`. NOT staged/never touched by this agent: pre-existing `M docs/agent-ops/20261107-studio/COMMS.md`, untracked `docs/agent-ops/20261107-studio/briefs/`, and all sibling wave-1 files. `tests/test_transformstore.cpp` intentionally unmodified (move pattern absent). Commits: code first, report second; trailers Task: T-007 / Agent: W1-FIX-SNIP / Wave: 1.
