# Brief — W2-TREV-01 (TEST REVIEWER) · wave 2 · run 20261108-ui-hierarchy
## 1. Mission
Independent test-quality review (read-only) of Wave 2 (empty BanAllowance, 9 pins, has_studio flip + MainWindow proof, negative rewrites, 2 stale pin flips).
## 2. Required reading blind-first
STANDING-ORDERS, AGENTS.md gates, 00-mission, diff git diff ea04e1a..HEAD -- tests/, tests context scan/flip/negative. THEN implementer test sections + gate gate-w2-1.md, COMMS tail. READ-ONLY.
## 3. Task
Per-test assessment (ban 29→? assertions, pins 9 values, flip broadened + MainWindow, negative 6 CHECKs), mutation (wrong-value token retaining), fail-before/pass-after for empty-table, flakiness (run ctest 1-2 times), missing edges P1/P2/P3, verdict.
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w2-deep/W2-TREV-01-review.md
READ-ONLY: source/tests.
## 5. Siblings
W2-REV-01 code reviewer parallel.
## 6. Traps
Empty table is strictly stronger than capped — verify not just re-label cap-exceed→digit_violation hides real violation. Lexical residuals (wrong-value swap) systemic — record as residual, not wave defect. Pre-existing CWD trap unchanged (not regression).
## 7. Env
Scoped: lwf_tests build + ctest (repeat). No source edits.
## 8. Deliverables
Report template review-tests.md, digest verdict. Docs-only commit trailers Agent: W2-TREV-01 Wave: 2. Final ≤8 lines.
