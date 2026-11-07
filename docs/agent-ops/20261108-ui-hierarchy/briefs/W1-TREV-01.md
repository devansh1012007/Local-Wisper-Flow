# Brief — W1-TREV-01 (TEST REVIEWER) · wave 1 · run 20261108-ui-hierarchy
## 1. Mission
Independent test-quality review (read-only) of Wave 1 test changes. Judge tests as work product: fail-before, determinism, mutation survival, missing edges.
## 2. Required reading (blind-first)
1. STANDING-ORDERS 2. AGENTS.md gates 3. 00-mission 4. Diff: git diff 2c06061..HEAD -- tests/ 5. Tests in context (test_overlay_settings_ui.cpp new ban, test_scratchpad_page, test_snippets_page). THEN implementer test report: W1-IMPL-03-T-004.md 7. COMMS.md
WAVE_BASE_SHA: 2c06061
## 3. Task
Per-test assessment, coverage of changed lines/branches/boundaries, mutation thinking (5+ bugs: mapping 15→13, 16→8, allowlist abuse, file glob escape, CWD trap), flakiness (run new tests multiple times if env allows; else reason), missing edge cases P1/P2/P3 with sketches, findings R-W1 with severity. Try to break: introduce literal bypass in unlisted file, in allowlisted file, in widget file — would ban catch it?
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w1-review/W1-TREV-01.md
READ-ONLY: source + tests (never edit).
## 5. Siblings: W1-REV-01 code reviewer parallel.
## 6. Traps
lwf_tests no ImGui link (lexical only) — pixel regression invisible is systemic, note as residual. Existing tests blessed literals — pin flips must not weaken token/contrast CHECKs. CWD trap: Dictionary/Insights/Snippets bare relative paths.
## 7. Env
Scoped: cmake --build build --target lwf_tests, ctest -R lwf-tests (can run multiple times), grep. No full suite beyond ctest.
## 8. Deliverables
Template review-tests.md, digest verdict + counts. Docs-only commit trailers Task: R-W1 Agent: W1-TREV-01 Wave: 1 Report: <path>
Final ≤8 lines.
