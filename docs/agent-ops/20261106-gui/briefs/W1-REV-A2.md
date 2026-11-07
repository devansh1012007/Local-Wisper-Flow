# Brief — W1-REV-A2 (code reviewer) · wave 1 · run 20261106-gui

## 1. Role and mission
Mission: independently review task T-001 (GUI-A2) implementation (commits b21e40f..a6aea40, code at b21e40f) for correctness, root-cause depth, regression risk, and band-aid absence. Failure looks like: scoped font change introduces double scaling, unbalanced PushFont/PopFont, token drift from mockup, or scope creep.

## 2. Required reading (in this order)
1. docs/agent-ops/20261106-gui/templates/STANDING_ORDERS.md (binding)
2. AGENTS.md, docs/UI_AUDIT.md D6, docs/design/mockup.html §5, docs/plans/2026-11-06-gui-fix-improvements.md
3. docs/agent-ops/20261106-gui/00-mission.md, docs/agent-ops/20261106-gui/01-work-ledger.md
4. Investigation: docs/agent-ops/20261106-gui/reports/w0-investigation/INV-font-dpi-map.md, INV-tests-font-theme.md
5. Implementation: docs/agent-ops/20261106-gui/reports/w1-impl/W1-IMPL-A2-T-001.md (read AFTER your blind pass)
6. Code: git diff 42a7ea0f..b21e40f -- src/ui tests (read in context), the full changed regions of src/ui/MainWindow.cpp, src/ui/Overlay.cpp, src/ui/theme/ThemePalette.{h,cpp}.

## 3. Task and acceptance criteria
Task T-001 (GUI-A2). Acceptance: AC1-AC4 (see brief W1-IMPL-A2.md).

## 4. Ownership
WRITE-SET: none (read-only). FORBIDDEN: any edit to source. You write only your report.

## 5. Parallel siblings this wave
None (solo wave-1 task).

## 6. Skip list and known traps
Blind-first: form findings before reading the implementer report.

## 7. Environment and tests
Read-only source review; no source edits. Do NOT run the full suite; you may run scoped checks if you wish.

## 8. Deliverables
Report: docs/agent-ops/20261106-gui/reports/w1-review/REV-W1-002-A2.md (code) — ORCH-DIGEST block first, see docs/agent-ops/20261106-gui/templates/review-code.md.

## 9. Role-specific notes
- Code reviewer: READ-ONLY. Blind pass first, then claims audit. Diff: `git diff 42a7ea0f..b21e40f -- src/ui tests`. Implementer report: docs/agent-ops/20261106-gui/reports/w1-impl/W1-IMPL-A2-T-001.md (read after blind pass).
- Try to break it: construct adversarial font-scale scenarios (100%/125%/150% DPI, nested card scopes, rail vs page scopes, mini badge).
- Digest: verdict APPROVE|APPROVE-WITH-CHANGES|REJECT; findings BLOCKER/MAJOR/MINOR/NIT; claims audit; top-3 risks; optimizations DO-NOW/DO-LATER/NOT-WORTH-IT.
