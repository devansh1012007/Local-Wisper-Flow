# Brief — W0-INV-02 (INVESTIGATOR) · wave 0 · run 20261108-ui-hierarchy
## 1. Mission
Map hierarchy/density patterns: how spacing, cards, typography, page layout already solved. Why: P-001..P-003 must follow existing conventions (PushFont design-px, metrics vs studio_metrics, BeginCard, widgets).
## 2. Required reading
1. STANDING-ORDERS.md 2. AGENTS.md UI 3. 00-mission.md 4. 01-work-ledger.md 5. report-investigation.md 6. COMMS.md 7. src/ui/theme/ThemePalette.h, StudioPalette.h, src/ui/widgets/Widgets.h, Buttons.cpp, Display.cpp, src/ui/pages/DictionaryPage.cpp:115-237, InsightsPage.cpp:85-107
## 3. Task
For P-001..P-003 list every spacing/typography literal bypassing tokens (grep for PushFont(, Dummy(, SameLine(, 40.0f etc.), dual-palette split proof, classify SURFACE vs DEEP, propose 2+ fix options (normalize both vs unify), long-run test, WRITE-SET/CONTRACT-SET/test plan.
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w0-investigation/W0-INV-02.md
READ-ONLY: source
## 5. Siblings: W0-INV-01 arch, W0-INV-03 tests, W0-INV-04 mockup/history.
## 6. Traps: ThemePalette values frozen (mockup-exact); ApplyStudioStyle call sites — grep yourself; do not propose palette rebrand.
## 7. Env: grep/read only, no builds.
## 8. Deliverables: report-investigation.md + digest, docs-only commit trailers Task: T-000 Agent: W0-INV-02 Wave: 0.
## 9. Long-run test required. Start early append.
