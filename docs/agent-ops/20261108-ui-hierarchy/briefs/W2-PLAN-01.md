# Brief — W2-PLAN-01 (PLANNER REVIEWER) · wave 2 · run 20261108-ui-hierarchy
## 1. Mission
Critique W2-DEEP-01 design against actual code. Try to break root-cause analysis and proposed design (missed sites, token conflicts, test blind spots, wrong values).
## 2. Required reading
STANDING-ORDERS, AGENTS.md UI frozen, reports/w2-deep/W2-DEEP-01-design.md full, W0-INV-02 section 5 T-003, W0-INV-04 mockup mapping, src/ui/theme/* headers, BanAllowance + scanner tests/test_overlay_settings_ui.cpp:1136-1372, COMMS tail.
## 3. Task
Return objections with [VERIFIED path:line] evidence. Answer: (1) all 26+ sites inventory complete? (2) token values same-value? (3) palette freeze respected? (4) test plan complete (pins, flip, negative rewrite)? (5) rollback/screenshot adequate? (6) what would you change? Verdict APPROVE/REJECT with severity per objection.
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w2-deep/W2-PLAN-01-review.md
READ-ONLY: source/tests/design.
## 5. Deliverables
Report template review-code.md style (digest verdict). Commit docs-only Agent: W2-PLAN-01 Wave: 2. Final ≤8 lines.
