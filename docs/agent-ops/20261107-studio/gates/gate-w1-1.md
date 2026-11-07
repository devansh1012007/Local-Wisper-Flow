# Gate record — wave 1 integration (gate-w1-1)

Run: 20261107-studio | Wave: 1 | Agent: W1-IMPL-INT (integrator)
UTC: 2026-11-07T08:36:53Z | Gate status: RED (one new test TU; owner W1-IMPL-02D)
Tree state: branch feature/studio-pages; HEAD 2a765e98399a3d295c847422bb1d7437c0b32a43
(pre-integration) + uncommitted INT wiring (CMakeLists.txt, scripts/check_no_network.sh,
this file, report) at gate time; COMMS.md uncommitted (peers' appends, ORCH-owned).
Baseline: BASE 0e655ec5e997915ec5db351ba83e98a690912394; CI run 42310536369
(windows 128210561461, linux 128210561573) both success at 1d95fb2 (00-mission.md).
Template: skill Appendix J gate format (tree state, HEAD, step table, regressions).

## Step table (verbatim commands)

| # | Step | Command | Result |
|---|---|---|---|
| 1 | Configure | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` | exit 0 (`CONFIG_EXIT=0`); third-party deprecation warnings only, none from lwf sources. ONNX skipped (expected, AGENTS.md). |
| 2 | Build (tests) | `cmake --build build -j4 --target lwf_tests` | exit 2 (`BUILD_TESTS_EXIT=2`); sole failure `tests/test_snippetstore_v2.cpp:42` — use of deleted move ctor (see §Blocker). All 7 new core sources compiled; all other new test TUs compiled. |
| 3 | Build (inventory) | `cmake --build build -j4 --target lwf_tests -- -k` | exit 2 (`KEEPGOING_EXIT=2`); confirms exactly ONE failing TU (no other new-code errors hidden). |
| 4 | Build (app) | `cmake --build build -j4 --target local-wisper-flow` | exit 0 (`APPBUILD_EXIT=0`); `local-wisper-flow.exe` linked (grpc `slice.h` warnings are third-party/pre-existing). |
| 5 | Build (all) | `cmake --build build -j4` | exit 2 (`FULLBUILD_EXIT=2`); same single TU failure; benches linked (`lwf-bench.exe`, `lwf-cloud-bench.exe`). |
| 6 | ctest | `ctest --test-dir build --output-on-failure` | NOT-RUN — `lwf_tests.exe` not linkable; on-disk binary is stale (11/7 04:37, pre-wave) and predates the new suites, so running it proves nothing about this wave. All 8 new suites NOT-RUN (see §Per-suite). |
| 7 | Hygiene (G4) | `bash scripts/check_hygiene.sh` | `hygiene: OK`, exit 0 (`HYGIENE_EXIT=0`). |
| 8 | Privacy (G4) | `bash scripts/check_no_network.sh` | `no-network: OK`, exit 0 (`NONET_EXIT=0`). Allowlist now `(CrashReporter\|DictionaryStore\|HistoryStore\|Log\|NotesStore\|SettingsStore\|SnippetStore\|StatsStore\|TransformStore)\.cpp` + `AtomicFile.(cpp\|h)`. |

G3 (zig type-check of Windows sources): pending — no zig on dev box (AGENTS.md records
G3 as pending per task). Never claimed as verified.

## Blocker (owner W1-IMPL-02D, NEED posted)

`tests/test_snippetstore_v2.cpp:42:12: error: use of deleted function
'SnippetStore::SnippetStore(SnippetStore&&)'` — helper `StoreWith` (lines 31-43) returns a
local `SnippetStore` by value; `SnippetStore` gained `mutable std::mutex mutex_;`
(SnippetStore.h:49, W1-IMPL-02D commit 530555f) which deletes the implicit move ctor,
and `std::mutex` is neither copyable nor movable, so no header-side default can restore it.
Fix belongs in the test (owner 02D / ORCH-reassigned): restructure the helper to fill a
caller-provided `SnippetStore&` and update all callers (first caller at line 48,
`SnippetStore store = StoreWith({{"alpha", "bravo"}});`). INT did not touch either file
(FORBIDDEN src/**, tests/**). Not pre-existing: the test file does not exist at BASE
(`git cat-file -e 0e655ec...:tests/test_snippetstore_v2.cpp` → exit 128).

Minor NEW-code warning (owner W1-IMPL-02A, FINDING posted, not build-blocking):
`tests/test_textdiff.cpp:110:46: warning: hex escape sequence out of range`
(`"a\xC2\xA0b"`). Gate ledger G1 wants zero *new* warnings; recommend `\u00A0` escape.

## Regressions vs baseline CI 42310536369

- New failure: `lwf_tests` build red on one wave-new TU (530555f). Baseline build/ctest/
  hygiene were green at BASE; this is a wave-local defect, not a pre-existing failure.
- No regression in app/link/hygiene/privacy: configure 0, app build 0, hygiene OK,
  no-network OK.
- Wave-local addition: one new compiler warning (test_textdiff.cpp:110) vs pre-existing
  third-party warnings (parakeet `_USE_MATH_DEFINES`, grpc `slice.h` `empty_bases`).

## Per-suite results (new suites)

All NOT-RUN by INT (link blocked). Suites registered: test_atomicfile, test_textdiff,
test_insights, test_statsstore, test_dictionarystore, test_snippetstore_v2,
test_transformstore, test_notesstore. For reference only (not RAN by INT, REPORTED by
owners): 02A behavioural mirror verifier passed 30+ asserts; 02E standalone doctest build
11 cases 272 asserts PASS. Authoritative counts come from the re-gate after the 02D fix.

## Follow-up required

1. W1-IMPL-02D (or ORCH-reassigned agent): fix `tests/test_snippetstore_v2.cpp` `StoreWith`
   (out-param form), re-run this gate end-to-end (G1-G5) and record new counts.
2. W1-IMPL-02A: fix `tests/test_textdiff.cpp:110` hex-escape warning (teardown clean).
3. Independent review: recommended for DictionaryStore migration + SnippetStore/TransformStore
   transforms parsing per implementer reports (not an INT blocker for build; required by
   wave rules before Wave 2 wiring).
