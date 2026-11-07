# Brief — W0-INV-01 (INVESTIGATOR) · wave 0 · run 20261108-ui-hierarchy

## 1. Role and mission
Mission: Map Full window architecture for hierarchy & density work. Why: implementers need exact call flow to avoid breaking rail/persist/live-apply.
Role: investigator, read-only on source, write only own report.

## 2. Required reading (in order)
1. docs/agent-ops/20261108-ui-hierarchy/templates/STANDING-ORDERS.md (binding)
2. AGENTS.md (UI section lines 135-161, Build 9-45)
3. docs/agent-ops/20261108-ui-hierarchy/00-mission.md
4. docs/agent-ops/20261108-ui-hierarchy/01-work-ledger.md
5. docs/agent-ops/20261108-ui-hierarchy/templates/report-investigation.md
6. COMMS.md from last WAVE-START
7. src/ui/MainWindow.h, src/main.cpp:570-810, docs/UI_AUDIT.md sections 0-2, docs/MANUAL_TEST_WINDOWS.md section R

## 3. Task and acceptance criteria
For P-001..P-004 answer: modules/layers/entry points/boundaries of Full window; execution flow RunOnce→DrawRail→RenderPages→Draw*Page; data flow SettingsStore/UiBus/FSM read-only snapshot; invariants (live-apply, PersistPage last_page, keyboard focus restore, hide-on-close).
Acceptance: each claim [VERIFIED path:line]; blast radius lists callers of MainWindow/Page/RouteName/PageForRoute.

## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w0-investigation/W0-INV-01.md
READ-ONLY: all source
FORBIDDEN: no source edits, no other reports.

## 5. Siblings
W0-INV-02 patterns/tokens, W0-INV-03 tests/blast, W0-INV-04 history/mockup. You cannot see their uncommitted reports.

## 6. Skip/traps
Do not propose palette changes; keep tokens frozen. Do not read mockup.html in depth — W0-INV-04 owns it. Note PLAN-MODE recon already found kRailWidth 220 vs 208 — verify it yourself.

## 7. Env
Scoped grep/read only. No builds, no full suite.

## 8. Deliverables
Report at WRITE-SET path, ORCH-DIGEST first, template report-investigation.md. Commits: docs-only with trailers Task: T-000 Agent: W0-INV-01 Wave: 0 Report: <path>
Final ≤8 lines: STATUS | agent | task | report | SHAs | blockers

## 9. Notes
Start report early, append as you work. Read COMMS before final.
