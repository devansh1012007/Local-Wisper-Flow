# Brief — W2-REV-01 (CODE REVIEWER) · wave 2 · run 20261108-ui-hierarchy
## 1. Mission
Independent code review (read-only) of Wave 2 tranche-2 diff (9 tokens, stage-2 migration, file-local hoists). Try to break: wrong same-value mapping, palette freeze violation, dual-palette recolor, structural literal mishandling, test couplings.
## 2. Required reading blind-first
STANDING-ORDERS, AGENTS.md UI, 00-mission, design W2-DEEP-01 (approved full 9), critique W2-PLAN-01 + decisions D-W2-01..04, diff git diff ea04e1a..HEAD -- src/ui/, code context headers + .cpp edits. THEN implementer reports w2-deep/W2-IMPL-01-report.md + W2-IMPL-02-report.md, gate gate-w2-1.md.
Wave 2 base: ea04e1a.
## 3. Task
Blind pass first, then claims audit (each implementer claim vs code), long-run test (Source/Load/Hostile/Operability/Reversal), break attempts (12↔10/24↔28, metrics reappear, ApplyStyle touch), optimization table, verdict APPROVE/APPROVE-WITH-CHANGES/REJECT with BLOCKER/MAJOR/MINOR/NIT file:line + fix.
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w2-deep/W2-REV-01-review.md
READ-ONLY: source/tests.
## 5. Siblings
W2-TREV-01 test reviewer parallel.
## 6. Traps
Tokens additive only; ThemePalette.h frozen; Apply*Style untouched; structural 40/rail/widths must stay; 14 hoist stays file-local; space_xs documented reuse (1 site only, D-W2-01 escape hatch) — check not inflated.
## 7. Env
Scoped grep/diff/tests (only build/lwf_tests targets). Never full suite beyond ctest.
## 8. Deliverables
Report template review-code.md, digest verdict. Docs-only commit trailers Task: R-W2 Agent: W2-REV-01 Wave: 2 Report: <path>
Final ≤8 lines.
