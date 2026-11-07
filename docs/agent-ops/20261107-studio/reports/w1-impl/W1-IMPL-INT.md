# W1-IMPL-INT — integrator: CMake + gate wiring for Wave 1 (T-002..T-010 integration)

## ORCH-DIGEST

Status: BLOCKED (integration complete; gate RED on one non-owned test TU).
Mission done: `CMakeLists.txt` registers the 7 new `lwf_core` sources
(AtomicFile, TextDiff, InsightsModel, StatsStore, DictionaryStore, TransformStore,
NotesStore) and the 8 new `lwf_tests` files (test_atomicfile, test_textdiff,
test_insights, test_statsstore, test_dictionarystore, test_snippetstore_v2,
test_transformstore, test_notesstore); `scripts/check_no_network.sh:30` allowlist
extended with exactly the five `ofstream`-bearing stores from the wave
(DictionaryStore, NotesStore, SnippetStore, StatsStore, TransformStore, `.cpp` only —
headers verified write-API-free). Gate: configure exit 0, app/lwf_core/bench builds
exit 0, `lwf_tests` build exit 2, hygiene OK, no-network OK, ctest NOT-RUN (link
blocked). Blocker: `tests/test_snippetstore_v2.cpp:42` — `StoreWith` returns a local
`SnippetStore` by value while the new `mutable std::mutex mutex_` (SnippetStore.h:49)
deletes the move ctor; owner W1-IMPL-02D (commit 530555f); NEED posted to COMMS; files
are FORBIDDEN src/**+tests/** for INT so no fix was applied. Minor: new-code warning
`tests/test_textdiff.cpp:110` hex escape (owner 02A). Gate record:
`docs/agent-ops/20261107-studio/gates/gate-w1-1.md`.

---

# Body

## 1. Role and mission (brief §1)

Wire Wave 1 into the build: register all new sources + tests in CMakeLists.txt,
extend check_no_network.sh with the DEFERRED-DEPENDENCY lines, run the wave gate.
INT is the only CMakeLists/scripts writer this wave (hotspot single-writer, mission
§Hotspots). Wiring is additive-only (7 + 8 lines + 1 allowlist line); no target
restructure, no flag changes, no warning-level changes.

## 2. Required reading (brief §2 — completed in order before any edit)

1. `briefs/W1-IMPL-INT.md` (full).
2. `templates/STANDING-ORDERS.md` (binding; INT single-writer on CMakeLists/scripts).
3. `AGENTS.md` — gates (`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`;
   `cmake --build build -j4`; `ctest --test-dir build`; `check_hygiene.sh`;
   `check_no_network.sh`; G3 zig pending, G1 zero-new-warnings, G2 ctest green,
   G5 config_v0 backward-compat).
4. `00-mission.md` + `01-work-ledger.md` — BASE `0e655ec`, baseline CI 42310536369
   green at 1d95fb2, P02-P11 rows, plan hotspots.
5. Approved plan §§E (P02-P11: AtomicFile same-commit, TextDiff LCS, InsightsModel
   formulas, StatsStore rollup, DictionaryStore v2 migration, SnippetStore v2,
   TransformStore 10 slots, NotesStore md-first, app_profiles category), §H (test IDs
   T-atomic, T-diff, T-formulas, T-stats incl. cap, T-mig x4, T-val x4, T-slots,
   T-conflict, T-notes incl. orphan reconcile, T-cat-mapping, T-count, T-reload,
   T-transform-v1) and §I (corrupt→`.bad` never-silent-overwrite; stale `.tmp`
   ignored; Record on engine worker thread mutex; same-commit gate rule).
6. Wave-1 implementer reports W1-IMPL-01 + 02A..02E + COMMS.md tail (last line read
   at start: `[2026-11-07T08:30:16Z] W1-IMPL-02E -> ORCH | DONE | T-009 | ...`).
   NEED lines extracted: 02B StatsStore.cpp allowlist (COMMS 09:10Z), 02C
   DictionaryStore.cpp:378,600 `ofstream` (COMMS 08:03:56Z), 02D SnippetStore +
   TransformStore allowlist (COMMS 13:36:35Z), 02E NotesStore.cpp:273,305 (COMMS
   08:30:16Z), 02A AtomicFile already landed in 02A's commit a7dbd93.

## 3. Root Cause & Context Summary (brief §1 root cause, verified)

- Root cause [VERIFIED]: 5 parallel coders landed new `.cpp/.h` + doctest files
  without CMake registration (their briefs forbid CMakeLists.txt), so the sources
  were dead code to the build (`grep` of CMakeLists.txt pre-edit found only the
  legacy `tests/test_dictionary.cpp:155` — none of the 7 new sources or 8 new test
  files were listed). G1/G2 could not see them; G4 (`check_no_network.sh`) flagged
  the five `ofstream`-bearing stores pending same-commit allowlists (plan §D/D4,
  §E ordering rule).
- Write-API inventory [VERIFIED by grep this run, src/core *.cpp/*.h]: write-API
  hits in `AtomicFile.cpp:12` (allowlisted), `CrashReporter.cpp:39`,
  `HistoryStore.cpp:83,123,169,174`, `Log.cpp:113` (fopen), `SettingsStore.cpp:219`
  (existing allowlist), plus the five wave files `DictionaryStore.cpp:378,600`,
  `NotesStore.cpp:274,306`, `SnippetStore.cpp:219`, `StatsStore.cpp:273`,
  `TransformStore.cpp:442`. **Zero hits in TextDiff, InsightsModel,
  PersonalDictionary, AppContextResolver, TransformManager** and in all `*.h` —
  so the allowlist extension is exactly those five `.cpp` names, `.cpp`-only.
  This matches all four independent NEEDs (consistency check passed).
- `SnippetStore.cpp` is a *modified*, not new, file — 02D's `Save` added the
  `ofstream`. It still needs the allowlist entry (same-commit rule is about the
  file that first introduces the write API, not about newness).
- Environment [VERIFIED]: branch `feature/studio-pages`; build tree pre-configured
  with `FETCHCONTENT_SOURCE_DIR_*` overrides into `D:/lwf/_deps-src` (CMakeCache.txt)
  so reconfigure needs no network; cmake 4.4.4, MinGW g++ (msys64), no zig → G3
  pending. `cmake` exit codes captured via explicit `CONFIG_EXIT`/`BUILD_*_EXIT`
  variables because PowerShell pipes surface native stderr as error records.
- Concurrent-landing note (brief §4): `NotesStore` was already committed (894675a,
  02E) before INT started editing → wired in the same edit; no FOLLOW-UP needed.
  No polling was done.

## 4. Phased Changes Executed (atomic phases)

### Phase 1 — CMakeLists.txt wiring (write-set: CMakeLists.txt)

DECISION — alphabetical insertion into existing lists (no new target, no flag or
option change; reversible by `git revert` of 15 added lines):
- `LWF_CORE_SOURCES` (+7): `src/core/AtomicFile.cpp`, `src/core/DictionaryStore.cpp`,
  `src/core/InsightsModel.cpp`, `src/core/NotesStore.cpp`, `src/core/StatsStore.cpp`,
  `src/core/TextDiff.cpp`, `src/core/TransformStore.cpp` (exact list per brief §3).
  `SnippetStore.cpp`, `PersonalDictionary.cpp`, `AppContextResolver.cpp`,
  `TransformManager.cpp` were already listed (verified lines 62/94/105/108 pre-edit)
  → no duplicate entries. INVARIANT: `lwf_core` already links `nlohmann_json` +
  `miniaudio` publicly; the new stores need nothing else (their test files pull only
  their own headers + doctest + stdlib).
- `lwf_tests` target (+8): `tests/test_atomicfile.cpp`, `tests/test_textdiff.cpp`,
  `tests/test_insights.cpp`, `tests/test_statsstore.cpp`,
  `tests/test_dictionarystore.cpp`, `tests/test_snippetstore_v2.cpp`,
  `tests/test_transformstore.cpp`, `tests/test_notesstore.cpp`.

### Phase 2 — check_no_network.sh allowlist (write-set: scripts/check_no_network.sh)

Single line 30 changed (line 15-18 network allowlist and 20-23 endpoint regex
untouched, per brief §3): `.cpp` group alphabetised to
`(CrashReporter|DictionaryStore|HistoryStore|Log|NotesStore|SettingsStore|SnippetStore|StatsStore|TransformStore)\.cpp`,
`AtomicFile.(cpp|h)` branch kept as 02A landed it. Verified against COMMS NEEDs:
02B/02C/02D/02E proposals all merge exactly into this line.

## 5. Review Audit

### Phase 1 self-review (stranger view of `git diff -- CMakeLists.txt scripts/check_no_network.sh`)

- Phase 1 (CMake): checked every inserted line against disk inventory
  (`src/core/*.cpp` 43 files, `tests/*.cpp` incl. many dead files — confirmed the 8
  targeted test files exist and no dead files were accidentally registered).
  [MINOR found+fixed during editing] one edit failed to match
  (`test_log.cpp/test_onboarding.cpp` not adjacent — `test_dictionary.cpp` sits
  between); redone as `dictionarystore + notesstore` block between `test_dictionary`
  and `test_onboarding`, verified by re-reading lines 136-182. No duplicate
  registrations (final grep shows each new path exactly once). [MAJOR:] none.
- Phase 2 (script): line 30 only; verified the regex still anchors
  `^src/core/((AtomicFile\.(cpp|h))|(…)\.cpp):` and that headers (`*.h`) have zero
  write-API hits, so `.cpp`-only grouping is sufficient and minimal.
- Security/edge (allowlist design): widening is confined to the **file-write**
  group for six already-audited text stores; the network-API pattern (15-18) and
  endpoint regex (20-23) are byte-identical post-edit. The privacy invariant
  ("offline unless opt-in cloud") is unaffected — these are local tmp+rename
  JSON/MD writers, same category as the pre-existing four.
- Mutation mental check: reverting (git revert of the CMake hunk) → `lwf_core.a`
  no longer contains the stores, but `git status` + build prove nothing else depends
  on their compile (no callers yet — Wave 2 wiring doesn't exist). Reverting the
  allowlist hunk → `check_no_network.sh` fails on the five files (gates/gate-w1-1.md
  step 8 would not print `no-network: OK`). Fix correct, not masking.
- Residual from Phase 1 audit: none open.

### Gate-blocker analysis (why this is BLOCKED, not fixed by INT)

- Cause [VERIFIED]: `src/core/SnippetStore.h:49` `mutable std::mutex mutex_`
  (W1-IMPL-02D, 530555f) → implicitly-deleted move ctor; `tests/test_snippetstore_v2.cpp:31-43`
  helper `StoreWith` returns a local `SnippetStore` by value → `error: use of deleted
  function 'SnippetStore::SnippetStore(SnippetStore&&)'` at line 42. `std::mutex` is
  neither copyable nor movable, so **no** header-side default can restore return-by-value;
  the only root-cause fix is test-side (out-param/caller-provided instance +
  rewiring all call sites, first at line 48). Removing the mutex would break the
  HistoryStore-pattern thread-safety contract 02D deliberately mirrored — worse fix.
- Both files are FORBIDDEN to INT (src/**, tests/**). Editing either would violate
  brief §4 + standing orders §1/§2 (out-of-write-set edits are NEEDs, not fixes).
- Not pre-existing [VERIFIED]: `git cat-file -e 0e655ec:tests/test_snippetstore_v2.cpp`
  → exit 128 (absent at BASE); `git log -- tests/test_snippetstore_v2.cpp` → single
  wave commit 530555f. R10 analysis: R10 (`FAILURES.md` + ask Devansh on a gate
  failing twice) is a mission rule, but `docs/FAILURES.md` is outside INT's
  WRITE-SET and the failure is a single deterministic compile error with a known
  owner + a known fix shape — not an unexplained twice-failing gate. Escalated via
  NEED to the owner + BLOCKED verdict instead of writing outside the write-set.
- Independent review: NOT obtained; recommend fresh-context review of (a) the 02D
  fix commit, (b) DictionaryStore migration/Save, (c) TransformManager v2 parsing
  before Wave-2 UI wiring (implementer-flagged MEDIUM/HIGH risk).

### Final review (whole task vs brief)

- §4 write-set respected: only `CMakeLists.txt`, `scripts/check_no_network.sh`,
  `gates/gate-w1-1.md`, this report (+ COMMS append, ORCH-committed per its rule 5).
  `git diff --stat` shows no other touched files (COMMS delta is peers' appends).
- Acceptance vs brief §3: wiring ✓, allowlist ✓, gate run + gate file ✓; green
  acceptance ✗ — BLOCKED on the 02D TU (evidence in gate file, not hidden).
- No debug artifacts, no stray processes (build processes reaped by shell), COMMS
  lines ≤6 (3 posted), report started early and completed.

## 6. Verification Evidence (verbatim)

- `[RAN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -> CONFIG_EXIT=0]` (warnings:
  doctest/re2/whisper/protobuf `cmake_minimum_required` deprecation, FetchContent
  imgui author warning, ONNX skipped — all pre-existing/third-party, recorded in
  gate file). First attempt's "Exited with code 1" was PowerShell stderr-as-error
  noise; explicit exit capture = 0.
- `[RAN cmake --build build -j4 --target lwf_tests -> BUILD_TESTS_EXIT=2]` — sole
  error `tests/test_snippetstore_v2.cpp:42:12: error: use of deleted function
  'SnippetStore::SnippetStore(SnippetStore&&)'` (+ mutex notes). Warning in
  `tests/test_textdiff.cpp:110:46: hex escape sequence out of range`.
- `[RAN keep-going (-k) build -> KEEPGOING_EXIT=2]` — exactly one failing TU; every
  other new source/test TU compiled (lwf_core.a relinked with all 7).
- `[RAN cmake --build build -j4 --target local-wisper-flow -> APPBUILD_EXIT=0]`
  (`local-wisper-flow.exe` linked; grpc slice.h warnings pre-existing).
- `[RAN cmake --build build -j4 (all) -> FULLBUILD_EXIT=2]` — same single TU;
  `lwf-bench.exe` + `lwf-cloud-bench.exe` linked.
- `[RAN ctest --test-dir build --output-on-failure -> NOT-RUN]` — on-disk binary
  stale (04:37, pre-wave); running it would prove nothing. All 8 new suites NOT-RUN.
- `[RAN bash scripts/check_hygiene.sh -> hygiene: OK, HYGIENE_EXIT=0]`
- `[RAN bash scripts/check_no_network.sh -> no-network: OK, NONET_EXIT=0]`
- `[RAN grep write APIs src/core/*.cpp+*.h -> 15 hits listed in §3]` (policy evidence).
- Per-suite counts (AtomicFile 5 cases per 02A, TextDiff 10, Insights 13, StatsStore
  10 per 02B, DictionaryStore 17 per 02C, SnippetStore-v2 TBD, TransformStore TBD,
  NotesStore 11/272): **NOT-RUN by INT**; 02A's assert-verifier and 02E's standalone
  doctest run are REPORTED (not RAN) evidence — authoritative counts arrive after
  the 02D fix + re-gate.

## 7. Unverified Assumptions & Remaining Risks

- [UNKNOWN] Whether 02D's fix will be a clean out-param refactor (any additional
  compile breaks in sibling paths of that TU; the keep-going build showed this TU
  fails at the helper only, but other errors could lurk behind it — e.g. callers
  with the same return-by-value pattern elsewhere in the file cannot compile until
  the helper is fixed, so a *second* error class could surface post-fix).
- [ASSUMED] Windows/MSVC compile behaviour mirrors MinGW here (A-001); re-gate on
  the Windows full job will confirm. NotesStore/DictionaryStore Windows rename
  paths already behave per 02C/02E reviews.
- [UNKNOWN] `tests/test_snippetstore_v2.cpp` and `tests/test_transformstore.cpp`
  suite sizes (not compiled/listed — only presence verified).
- Risk: the integration could be re-gated while sibling code changes — re-run this
  exact gate sequence after the 02D fix; if a NEW failure appears in files INT
  doesn't own, route per the same rule (NEED, don't fix).
- G3 zig remains pending (no zig on box) — recorded, not claimed.

## 8. Long-run test (standing orders §8)

- Source: root-cause fix is the test-helper restructure (mutex is unmovable by
  design). No band-aid exists in INT's diff (pure registration + allowlist).
- Load: allowlist/CMake have no runtime effect beyond compile-time registration;
  the new stores' runtime load paths are Wave-2-unverified (no callers yet).
- Hostile: `.cpp`-only allowlist means a *header* introducing `ofstream` later
  would fail the gate (fail-closed); network/endpoint regex untouched (fail-closed).
- Operability: one-commit revert restores BASE build shape; allowlist same-commit
  rule (plan §D/D4) is now satisfied for the whole wave (02A AtomicFile + these 5).
- Reversal: reverting INT's CMake hunk breaks the build visibly (missing symbols
  once Wave-2 UI calls the stores); that's the correct signal, not a silent pass.

## 9. Git State

- Branch: `feature/studio-pages` [VERIFIED `git branch --show-current` throughout].
- Commits (INT only, explicit paths, no `git stash/reset/checkout/clean`):
  1. `1532266` — `CMakeLists.txt` + `scripts/check_no_network.sh` (code/integration
     first). Disclosure: this commit's message omitted the required trailers
     (`Task:`, `Agent:`, `Wave:`, `Report:`) — a self-review catch after the fact;
     `git commit --amend` is forbidden, so left as-is and disclosed here rather
     than rewriting history.
  2. docs-only — `docs/agent-ops/20261107-studio/gates/gate-w1-1.md` + this report,
     with full trailers: `Task: T-002..T-010-integration / Agent: W1-IMPL-INT /
     Wave: 1 / Report: docs/agent-ops/20261107-studio/reports/w1-impl/W1-IMPL-INT.md`.
- Uncommitted at handoff: `COMMS.md` (peers' appends + INT's 3 lines — ORCH-owned,
  not committed by INT per its rule 5); `docs/agent-ops/20261107-studio/briefs/`
  (untracked, pre-existing peer/ORCH artifact — not INT's to commit).
- No commits on protected branches; no destructive git operations performed.
