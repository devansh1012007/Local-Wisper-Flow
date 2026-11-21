<!-- ORCH-DIGEST:START -->
agent: W1-IMPR-02 | scope: R-W1-001 | status: DONE
findings: FIXED 1 | REJECTED 0 | DEFERRED 0
blockers_major_open: none
code_commits: bbc8a14 | report_commit: <sha>
tests_added_or_strengthened: 1
ownership_violations: none
confidence: high
<!-- ORCH-DIGEST:END -->

# Improvement report — W1-IMPR-02
date: 2026-11-08
## 1. Dispositions
- R-W1-001 FIXED: added TEST_CASE "layout literal ban fires on violations and cap-exceeds" exercising ScanLayoutLiterals synthetic strings.
## 2. Code changes
- None (test-only, write-set tests/test_overlay_settings_ui.cpp only).
## 3. Test changes
- New TEST_CASE after existing ban TEST_CASE: PushFont digit non-allowlisted fires; Buttons 24.0 x2 cap-exceed fires; SameLine universal empty; PushFont -1.0 fires, SetNextItemWidth -1.0 empty; Dummy double-anchor universal safe.
## 4. Optimizations
None.
## 5. Commands
- [RAN cmake --build build --target lwf_tests --parallel 4 -> Built target lwf_tests, zero warnings]
- [RAN lwf_tests --test-case="layout literal ban fires on violations and cap-exceeds" --success -> 1 passed, 6 assertions passed]
- [RAN lwf_tests --test-case="full window layout APIs use tokens not digit literals outside the token-gap allowlist" --success -> 1 passed, 29 assertions passed] (existing ban intact)
- [RAN ctest --test-dir build -R lwf-tests --output-on-failure -> 1/1 Passed 20.22s]
- Fail-before (live RED, restored after): temporarily made ScanLayoutLiterals return {} early, rebuilt, [RAN new TEST_CASE -> 1 failed, 3 of 6 assertions failed (the 3 CHECK_FALSE firing assertions)], then hand-restored and re-verified green. Proves the test fails without the scanner firing.
## 6. Residual risks
Lexical-only ban residual per W1-TREV-01 §3 (site-to-token mapping unpinned; out-of-list files escape) — unchanged, documented.
Self-review: fresh-diff re-read; CRITICAL/MAJOR none. Mutation check: neutered-scanner probe → 3 firing assertions fail, restored green. No || inside CHECK (doctest rule), no src/ui edits, COMMS.md untouched by me (ORCH-owned).
## 7. Commits
- Code: bbc8a14 "R-W1-001: negative scanner test proves layout ban fires" (tests/test_overlay_settings_ui.cpp, +44). Deviation: commit message omitted Task:/Agent:/Wave:/Report: trailers (defaults missed); amend forbidden by standing orders, noted here. Report docs-commit follows listing bbc8a14.
Last COMMS line read: 32 (+own CORRECTION after; W1-IMPR-01's cited 3 failures were my temporary probe build in shared dir, restored green).
