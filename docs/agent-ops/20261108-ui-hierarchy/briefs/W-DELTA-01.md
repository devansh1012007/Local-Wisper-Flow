# Brief — W-DELTA-01 (FINAL INTEGRATION REVIEWER) · final Review · run 20261108-ui-hierarchy
## 1. Mission
Fresh final review of combined branch diff vs requirements/plan/architecture. You have no prior context — review from scratch.
Combined diff: git diff 8a6ea225..HEAD excluding docs/agent-ops. Requirements: Full window hierarchy & density, keep mockup-exact tokens, src/ui only, screenshots proof. Plan: docs/agent-ops/20261108-ui-hierarchy/00-mission.md + 01-work-ledger.md.
## 2. Required reading
AGENTS.md UI + Build, 00-mission.md, 01-work-ledger.md Wave 1+2, combined diff + surrounding code, per-wave reviews (W1-REV-01, W1-TREV-01, W2-REV-01, W2-TREV-01) read AFTER independent audit. Docs: docs/PROGRESS.md (check if rows added), docs/MANUAL_TEST_WINDOWS.md R section (screenshot gate), docs/UI_AUDIT.md.
## 3. Task
Check cross-wave interactions, merge artifacts, regressions, debug code, docs accuracy, migrations (none expected), coverage, and whether tests prove correctness. Run full relevant suite (lwf_tests + hygiene) and compare to baseline in 00-mission. Findings severity + file:line + command output. Verdict APPROVE/APPROVE-WITH-CHANGES/REJECT.
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/final/W-DELTA-01-review.md
READ-ONLY: source/docs outside own report.
## 5. Deliverables
Report template review-code.md (digest verdict + counts). Docs-only commit trailers Agent: W-DELTA-01 Wave: final Report: <path>. Final ≤8 lines.
