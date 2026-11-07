# Brief — W0-INV-04 (INVESTIGATOR) · wave 0 · run 20261108-ui-hierarchy
## 1. Mission
History/intent + mockup verification: does code match docs/design/mockup.html intent for density/hierarchy? Why: P-001..P-004 must not contradict mockup; rail drift and dual-palette need intent.
## 2. Required reading
STANDING-ORDERS, AGENTS.md, 00-mission, 01-work-ledger, report-investigation, COMMS, docs/design/mockup.html sections 3,5, docs/UI_AUDIT.md (D-register, R1 status), docs/PROGRESS.md (R1, Studio, GUI wave), docs/STUDIO_PLAN.md P17-P24 if exists, docs/MANUAL_TEST_WINDOWS.md section R, git log --oneline for src/ui/theme, MainWindow
## 3. Task
Mockup token/density claims vs code, history why kRailWidth 220 exists (COMMS W3-IMPL-S1), why dual palette persists (Wave 4 deferred), prior failed attempts (WB4 lightened palette reverted), D-rail recommendation with evidence, classify SURFACE/DEEP, test plan for screenshot gate.
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w0-investigation/W0-INV-04.md
## 5. Siblings: 01,02,03
## 6. Traps: Do NOT lighten colors to fix contrast (WB4 reverted); ThemePalette must not change without mockup regen.
## 7. Env: read/git log only, no builds.
## 8. Deliverables: report-investigation.md, docs-only commit trailers Agent: W0-INV-04 Wave: 0.
