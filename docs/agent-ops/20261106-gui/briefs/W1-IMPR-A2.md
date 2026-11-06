# Brief — W1-IMPR-A2 (improver) · wave 1 · run 20261106-gui

## 1. Role and mission
Mission: address every BLOCKER/MAJOR finding from W1-REV-A2 (code) and W1-REV-A2T (tests); minor findings to general agent. Task T-001.

## 2. Required reading (in this order)
1. STANDING_ORDERS (binding)
2. AGENTS.md, docs/plans/2026-11-06-gui-fix-improvements.md
3. Investigator reports, implementation report, BOTH reviewer reports (read fully).

## 3. Task and acceptance criteria
Fix review findings; no new behavior beyond T-001 scope.

## 4. Ownership
WRITE-SET: src/ui/MainWindow.cpp, src/ui/Overlay.cpp, src/ui/theme/ThemePalette.h, src/ui/theme/ThemePalette.cpp, tests/test_overlay_settings_ui.cpp, plus report/docs if needed. FORBIDDEN: engine files.

## 5. Parallel siblings: none.

## 6. Deliverables
Report: docs/agent-ops/20261106-gui/reports/w1-improve/W1-IMPR-A2-T-001.md — ORCH-DIGEST first, see docs/agent-ops/20261106-gui/templates/report-improvement.md. Commit code (if source changed) then report docs-only.

## 7. Environment and tests
Scoped: build/ctest/hygiene as needed.

## 8. Role-specific notes
- Improver: address every BLOCKER/MAJOR (FIXED/REJECTED-with-evidence/DEFERRED-with-reason); improve code AND tests; reviewer findings to address.
