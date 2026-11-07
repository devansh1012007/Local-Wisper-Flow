# Brief — W1-REV-A2T (test reviewer) · wave 1 · run 20261106-gui

## 1. Role and mission
Mission: independently audit the tests added/changed for task T-001 (GUI-A2) — tests/test_overlay_settings_ui.cpp design-px token test. Failure looks like: test asserts implementation detail rather than behavior, over-mocks, doesn't fail before the fix, or misses token drift.

## 2. Required reading (in this order)
1. docs/agent-ops/20261106-gui/templates/STANDING_ORDERS.md (binding)
2. AGENTS.md, docs/plans/2026-11-06-gui-fix-improvements.md, docs/agent-ops/20261106-gui/00-mission.md
3. Investigation: docs/agent-ops/20261106-gui/reports/w0-investigation/INV-tests-font-theme.md
4. Implementation: docs/agent-ops/20261106-gui/reports/w1-impl/W1-IMPL-A2-T-001.md (after blind pass)
5. Tests: tests/test_overlay_settings_ui.cpp (L171-219 region), the diff `git diff 42a7ea0f..b21e40f -- tests/test_overlay_settings_ui.cpp`.

## 3. Task and acceptance criteria
Task T-001. AC3: tests cover new font tokens/mechanism, fail before fix, pass after; AC4 green.

## 4. Ownership
WRITE-SET: none (read-only). FORBIDDEN: source edits, test weakening.

## 5. Parallel siblings: none.

## 6. Skip list and known traps
Blind-first; mutation thinking on the new token test.

## 7. Environment and tests
Run scoped: `ctest --test-dir build -R lwf-tests`. No source edits.

## 8. Deliverables
Report: docs/agent-ops/20261106-gui/reports/w1-review/REV-W1-002T-A2.md (tests) — ORCH-DIGEST first, see docs/agent-ops/20261106-gui/templates/review-tests.md.

## 9. Role-specific notes
- Test reviewer: does each test fail without the fix; assert behavior/values not "doesn't throw"; deterministic; no over-mocking; mutation coverage for off-by-one/inverted condition/wrong default on token assertions.
