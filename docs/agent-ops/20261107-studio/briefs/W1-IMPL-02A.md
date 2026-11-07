# Brief — W1-IMPL-02A (implementer) · wave 1 · run 20261107-studio

## 1. Role and mission
Mission: deliver three small, fully-tested portable building blocks: AtomicFile (shared tmp+rename writer), TextDiff (word-level LCS diff), InsightsModel (pure stats formulas). Root cause: three home-grown copies of atomic write + zero diff/formula code.
Role: implementer. Files are NEW except scripts/check_no_network.sh (hotspot — same-commit rule).

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; additive keys; hermetic tests)
3. docs/agent-ops/20261107-studio/00-mission.md
4. Approved plan: C:\Users\devan\.opencode\plan\2026-11-07-studio-pages-and-settings-gui.md §§C,D,E (P02-P04), §H, §I.
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-datastore.md (atomic pattern locations: SettingsStore.cpp:217-228, HistoryStore.cpp:81-108)
6. Templates: report-implementation (your report); scripts/check_no_network.sh (exact allowlist: line 30 area).

## 3. Task / acceptance
- src/core/AtomicFile.{h,cpp}: `lwf::WriteAtomic(path, content) -> bool` (tmp + flush + filesystem rename; cleans stale .tmp; parent-dir create not required). Portable (no windows.h).
- src/core/TextDiff.{h,cpp}: word-token LCS → {replaced, inserted, deleted} counts + inline diff tokens (insertions/deletions). Whitespace-tokenized (Unicode whitespace; CJK unsegmented — document in header). Pure, no I/O.
- src/core/InsightsModel.{h,cpp}: pure functions: Wpm (total_words, speaking_ms; "—" sentinel below 60000ms → return optional), FixesMadeByFlow, CategoryShares (whole %, 0 when empty), TotalAppsUsed, CurrentStreak (today must have words>0 else 0), LongestStreak, CalendarLevel (0-4 quartile of month max), DayKey format.
- Same-commit gate edit: add src/core/AtomicFile.cpp (and .h) to the file-write allowlist in scripts/check_no_network.sh (exact `grep -vE` line — do NOT touch the network-API allowlist).
- Tests (doctest, tests/test_atomicfile.cpp, tests/test_textdiff.cpp, tests/test_insights.cpp — new files): T-atomic (stale tmp ignored, tmp written then rename skipped → old content intact), T-diff (counts incl. empty/identical/all-replaced), T-formulas (wpm 0/59/60s floors, midnight streak boundary in local time, gap longest-streak, shares sum to 100 or 0, quartile levels incl. max=0). Hermetic temp_directory_path fixtures only.
- Do NOT edit CMakeLists.txt (hotspot — integrator wires after the wave; note NEED in COMMS). Mark scoped tests NOT-RUN if build dir contention prevents compiling; orchestrator gate runs them.
Acceptance: files exist, tests added, scripts/ allowlist updated in same commit as AtomicFile.cpp, scoped compile attempted (single-file zig/msvc check or cmake if safe), hygiene OK.

## 4. Ownership
WRITE-SET: src/core/AtomicFile.{h,cpp}, src/core/TextDiff.{h,cpp}, src/core/InsightsModel.{h,cpp}, tests/test_atomicfile.cpp, tests/test_textdiff.cpp, tests/test_insights.cpp, scripts/check_no_network.sh
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt, src/core/StatsStore.*, SnippetStore.*, DictionaryStore.*, TransformStore.*, NotesStore.*, src/ui/**, src/main.cpp, AGENTS.md.

## 5. Siblings (same wave)
W1-IMPL-01 docs; W1-IMPL-02B StatsStore+app_profiles; W1-IMPL-02C DictionaryStore; W1-IMPL-02D SnippetStore+TransformStore; W1-IMPL-02E NotesStore. Disjoint write-sets (all new files). Nobody edits CMakeLists.txt this wave.

## 6. Skip list / traps
Do not refactor SettingsStore/HistoryStore to use AtomicFile (out of scope, touching green code). No code comments (header docs via §/expression only — AGENTS.md bans comments; keep header minimal). No feature flags. No tweaks to existing tests.

## 7. Environment
Scoped only: bash scripts/check_hygiene.sh; scoped cmake --build --target lwf_tests is FORBIDDEN (shared build dir) — try `cmake --build build --target lwf_core` only if safe, else mark NOT-RUN. Never run full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w1-impl/W1-IMPL-02A-T-00234.md (ORCH-DIGEST first). Commits: code first, report second (trailers Task: T-002,T-003,T-004 / Agent: W1-IMPL-02A / Wave: 1 / Report: <path>).
Final message <=8 lines.
