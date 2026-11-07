# W1-FIX-DICT — dictionary test failures

## ORCH-DIGEST
Status: FIXED (test-only, 2 lines; no src change needed).
Repro: `cmake --build build --config Release --parallel 4 --target lwf_tests` + doctest filter over all 17 dictionarystore cases [RAN]. Pre-fix: 16 run (excl. max) = 14 passed, 2 failed, 3 assertions failed; `max 2000 boundary` alone [RAN] = passed (2007 assertions).
Root causes: both failures are genuine test bugs, proven below; DictionaryStore.cpp / PersonalDictionary.cpp are correct and were not touched.
Fix: `tests/test_dictionarystore.cpp` only — `REQUIRE(Size()==2)` -> `==3` (seed forgotten); starred-case JSON missing root `}` -> added `}`.
Verify [RAN after fix, rebuild OK]: all 17 dictionarystore cases + 2122 assertions pass; 7 related dictionary/PersonalDictionary cases (26 assertions) also pass.
Files: tests/test_dictionarystore.cpp (this report; no other file touched — COMMS.md dirty state is another agent's, untouched).

## 1. Root cause and context summary
Cluster brief: full ctest failures in tests/test_dictionarystore.cpp (owner W1-IMPL-02C, code 2a2cd70). Reproduction per brief §options: full `ctest --test-dir build --output-on-failure` timed out at 600s with no output [RAN, timeout — pre-existing, not a dictionary hang: every dictionary case completes individually in seconds, incl. max-2000]. So reproduced via the brief's sanctioned fallback — ctest filter / direct doctest binary (`build\lwf_tests.exe --test-case=...`). The 2 failures:

1. `duplicate trimmed case insensitive is rejected` — `REQUIRE(store.Size() == 2)` got `3`. Evidence the test is wrong, not the code: `Load()` on a missing file seeds one entry (`SeedLocked()` in src/core/DictionaryStore.cpp:290-305 pushes `btw`/`by the way`, `next_id_=2`; test asserts `REQUIRE(store.Size() == 1)` at line 144). Then `Add("alpha")` succeeds (size 2), `Add("beta")` succeeds (size 3); both dup re-adds rejected with exact `"Word already exists."` (lines 147-151, all passing). Correct final size is 1+2 = 3. The count `2` would only hold if the seed were removed, which no code does. Fix: `== 3`. Still an exact-size assertion (not weakened).
2. `starred entries sort before newer entries` — `CHECK(store.ImportJson(json, nullptr))` got `false`, so entries stayed at seed size 1. Evidence the test JSON is malformed: concatenated raw strings build `{"schema_version": 2, "entries": [{...},{...},{...}]` — line 183 ended `...200}])"` (entry `}` + array `]` + raw-string terminator `)"`), missing the root object's closing `}`; the full document is therefore invalid JSON and `ImportJson`'s `nlohmann::json::parse` correctly throws/returns false (src/core/DictionaryStore.cpp:562-570). No valid JSON document could import successfully under any store behaviour, so this cannot be a store bug. Fix: `...200}]})"` (content now ends entry `}` + array `]` + root `}`, followed by terminator). Sort expectations (starred `old` first, then `newer` 300, `middle` 200) unchanged and now pass — confirming store sort logic was already correct.

No src change: DictionaryStore sorting, dedup, seed, and ImportJson error paths all behave as specified; changing them would be a band-aid over test data. No assertion weakened, deleted, or loosened (same exact size, same exact order, same error strings).

## 2. Context report (pre-change)
- Write-set per brief: src/core/DictionaryStore.{h,cpp}, src/core/PersonalDictionary.{h,cpp}, tests/test_dictionarystore.cpp. Only the test file was edited (2 lines). Forbidden files untouched.
- Invariants confirmed intact: seed-on-missing-file, trimmed case-insensitive dup rejection with exact message, starred-first/updated-desc sort, cap 2000, .bak/.bad semantics (all 17 cases pass).
- Mid-fix incident (resolved): first JSON edit placed the added `}` before the raw-string terminator parenthesis (`}])}"`), leaving the raw string unterminated and swallowing following test cases at compile time. Caught by the rebuild; corrected to `}]})"` (terminator `)"` after root `}`), rebuild then succeeded.

## 3. Verification evidence
- [RAN `cmake --build build --config Release --parallel 4 --target lwf_tests` pre-fix -> Built target lwf_tests, EXIT 0]
- [RAN doctest 16-case filter pre-fix -> 14 passed, 2 failed, 3 failed assertions (dup `3 == 2`; starred `ImportJson false` then `1 == 3`), EXIT 1]
- [RAN `max 2000 boundary` alone pre-fix -> 1 passed, 2007 assertions, EXIT 0]
- [RAN `ctest --test-dir build --output-on-failure` (full) -> TIMEOUT 600s, no output; not used as gate, documented honestly]
- [RAN rebuild post-fix -> Built target lwf_tests, EXIT 0]
- [RAN all 17 dictionarystore cases post-fix -> 17 passed, 0 failed, 2122 assertions, SUCCESS, EXIT 0]
- [RAN 7 related legacy-dictionary cases post-fix (`dictionary loads legacy and modern formats`, `dictionary words force canonical spelling and case`, 2x fuzzy, `sentinel-expanded...`, `dictionary editor...`, `contains mode...`) -> 7 passed, 26 assertions, EXIT 0]
- [RAN `git diff --stat` -> only tests/test_dictionarystore.cpp changed by this agent; `git branch --show-current` -> feature/studio-pages]
- NOT-RUN: full unfiltered ctest (600s timeout pre-existing, unrelated to this cluster); hygiene script (no src/comment change; test-only 2-line edit).

## 4. Unverified assumptions and remaining risks
- Full-suite green is inferred from filtered runs, not a completed full ctest (timeout). Dictionary cluster is fully covered by the 17-case filter (all present in `lwf_tests`, confirmed via `--list-test-cases`).
- No code risk introduced: zero src diff.

## 5. Git state
Branch: feature/studio-pages [RAN].
Commit: this report + tests/test_dictionarystore.cpp, explicit paths only, trailers below. Working tree otherwise untouched (COMMS.md modification + untracked briefs/ belong to other agents).

Task: T-006
Agent: W1-FIX-DICT
Wave: 1
