# Brief — W1-REV-01 (CODE REVIEWER) · wave 1 · run 20261108-ui-hierarchy
## 1. Mission
Independent code review (read-only) of Wave 1 diffs. Try to break the change: bugs, regressions, flawed assumptions, security, edge cases, conventions, scope creep.
## 2. Required reading (blind-first)
1. STANDING-ORDERS.md 2. AGENTS.md UI + Build 3. 00-mission.md 4. Plan discussion (hierarchy & density, keep tokens, src/ui only) 5. Diff: git diff 2c06061..HEAD -- src/ui/ 6. Code in context (MainWindow.cpp, pages, widgets). THEN read implementer reports: reports/w1-impl/W1-IMPL-01-T-001.md, W1-IMPL-02-T-002.md (test report optional for code lens) 7. COMMS.md
WAVE_BASE_SHA: 2c06061
## 3. Task
Blind pass first, then claims audit (each implementer claim vs code), long-run test independent answers (Source/Load/Hostile/Operability/Reversal), attempt breaks (wrong same-value mapping e.g. 13→15, 15→13, 16→24, allowlist explosion, dual-palette leak, context/style regressions), optimization table DO NOW/DO LATER/NOT WORTH IT, verdict APPROVE/APPROVE-WITH-CHANGES/REJECT with severity BLOCKER/MAJOR/MINOR/NIT file:line evidence + fix.
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w1-review/W1-REV-01.md (report only)
READ-ONLY: all source. Never edit source.
## 5. Siblings
W1-TREV-01 test reviewer runs in parallel (tests lens). You see only each other's reports after.
## 6. Traps
Tokens frozen — any pixel-value change is BLOCKER unless documented as intentional hierarchy fix. Metrics namespace used inside studio pages — check same-value correctness, not vocabulary. Launcher: ScratchpadWindow shares widgets — check editor window unaffected visually (note as residual risk, not BLOCKER without screenshot).
## 7. Env
Scoped: grep, git show, scoped builds/tests if needed (no full suite unless isolated). Never write source.
## 8. Deliverables
Report template review-code.md, digest APPROVE/APPROVE-WITH-CHANGES/REJECT, docs-only commit trailers Task: R-W1 Agent: W1-REV-01 Wave: 1 Report: <path>
Final ≤8 lines.
