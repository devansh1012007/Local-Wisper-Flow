<!-- ORCH-DIGEST:START -->
agent: W4-FIX-RESEED | finding: R-W4-DELTA-001 (MAJOR) | status: FIXED
files: src/core/DictionaryStore.cpp (Clear tombstone), tests/test_dictionarystore.cpp (3 tests) | main.cpp/MainWindow untouched (delete-all lives in MainWindow::DeleteLocalData; Clear fix covers it)
fix: Clear persists intentionally-empty EntriesToJson({}) under lock (tombstone) so lazy Load reads empty array, not missing-file seed; genuine first-run still seeds btw
verify: lwf_tests build OK; 4/4 targeted (36 asserts) + 36/805 dictionary-set + full ctest lwf-tests 1/1 PASS; hygiene OK
NEED: MainWindow.cpp:1351 Privacy copy "restart to reseed empty files" is now stale for dictionary (stays empty across restart); docs/PROGRESS.md row (outside write-set)
<!-- ORCH-DIGEST:END -->

# W4-FIX-RESEED — Privacy delete-all resurrection (R-W4-DELTA-001)

## Fix
`DictionaryStore::Clear()` (src/core/DictionaryStore.cpp:408) now persists the intentionally-empty state: clears RAM (`entries_`, `next_id_=1`, `loaded_=true`), then atomically writes `EntriesToJson({})` (`schema_version: 2`, `entries: []`) to `path_` via `.tmp` + rename under the same lock (mirrors `StatsStore::Clear` persisting `"{}"`). A later lazy `Load()` (DictionaryPage first-`Draw`, DictionaryPage.cpp:116) therefore reads an existing empty array instead of the missing-file `SeedLocked()` `"btw"` path; a subsequent `Save()` dumps the same empty RAM, so neither Load nor Save resurrects entries. `Load()`'s seed branch is untouched — genuine first-run (file never existed, `Clear` never called) still seeds `"btw"`. No format change; no new member.

## Why this seam
Delete-all (`MainWindow::DeleteLocalData`, src/ui/MainWindow.cpp:700) removes `dictionary.json` then calls `Clear()`. Writing the empty file inside `Clear` recreates the tombstone after the removal, in the same call, with no caller change. Gating the lazy `Load` on `Loaded()` was rejected (would also skip the legitimate first-ever load, per REV-W4-DELTA-001 §R-W4-DELTA-001); `DictionaryPage.cpp` is outside the write-set in any case. `main.cpp` holds only the `DataResetStores` wiring (main.cpp:545) — no delete-all logic there — so no `main.cpp`/`MainWindow` edit was needed. Default-lifetime store (`path_` empty) skips the file write and still clears RAM.

## Tests (tests/test_dictionarystore.cpp)
- `delete-all clear before first visit does not reseed on next load` — pre-visit order (no file, `Clear()` → `Load()`). Asserts file exists, no `"btw"`, `Size()==0` after Load.
- `clear leaves an empty file so reload and save stay empty` — post-visit order (`Load`+`Save`, file removed, `Clear()` → `Reload()`/`Save()`/fresh `Load`). Asserts stays 0, no `"btw"`.
- `genuine first-run still seeds btw without a prior clear` — guards the seed (existing `first run seeds btw` also pins it).

## Verification (verbatim)
- `cmake --build build --config Release --parallel 4 --target lwf_tests`:
```
[  0%] Building CXX object CMakeFiles/lwf_core.dir/src/core/DictionaryStore.cpp.obj
[  0%] Linking CXX static library liblwf_core.a
[ 50%] Built target lwf_core
[ 50%] Building CXX object CMakeFiles/lwf_tests.dir/tests/test_dictionarystore.cpp.obj
[ 50%] Linking CXX executable lwf_tests.exe
[100%] Built target lwf_tests
```
- `.\build\lwf_tests.exe --test-case="delete-all clear before first visit does not reseed on next load,clear leaves an empty file so reload and save stay empty,genuine first-run still seeds btw without a prior clear,first run seeds btw"`:
```
[doctest] test cases:  4 |  4 passed | 0 failed | 490 skipped
[doctest] assertions: 36 | 36 passed | 0 failed |
[doctest] Status: SUCCESS!
```
- `.\build\lwf_tests.exe -tc="*dictionary*,*first run*,*clear*,*delete-all*,*btw*"`:
```
[doctest] test cases:  36 |  36 passed | 0 failed | 458 skipped
[doctest] assertions: 805 | 805 passed | 0 failed |
[doctest] Status: SUCCESS!
```
- `ctest --test-dir build -R lwf-tests --output-on-failure`:
```
1/1 Test #1: lwf-tests ........................   Passed   19.86 sec
100% tests passed out of 1
```
(`ctest -R dictionarystore` matches no ctest name — the suite is the single `lwf-tests` entry; filtered doctest run above is the dictionarystore filter.)
- `bash scripts/check_hygiene.sh`:
```
hygiene: OK
```

## NEEDs (out of write-set, not done)
- `src/ui/MainWindow.cpp:1351` Privacy copy ("restart to reseed empty files") no longer describes the dictionary (stays empty across restart; no `"btw"` reseed). Confirm-copy update belongs to the MainWindow owner.
- `docs/PROGRESS.md` task row (AGENTS.md working rules) — outside this agent's write-set; orchestrator to add.
