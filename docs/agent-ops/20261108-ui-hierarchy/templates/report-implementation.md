<!-- ORCH-DIGEST:START -->
agent: <ID> | task: <T-xxx> | status: DONE|PARTIAL|BLOCKED|ESCALATE
code_commits: <sha> | report_commit: <sha>
files_changed: <n> (<paths>)
contracts_changed: none|<list>
tests: added <n>, changed <n>; scoped run: pass|fail|NOT-RUN(<why>)
ownership_violations: none|<list>
deferred_dependencies: none|<list>
out_of_scope_findings: <n>
temporary_mitigation: no|yes
deep_issue_flag: yes|no
confidence: high|med|low (<why>)
last_comms_line_read: <timestamp>
<!-- ORCH-DIGEST:END -->

# Implementation report
## 1. Acceptance criteria / 2. Verified problem / 3. Root cause / 4. Approach + alternatives / 5. Changes per file / 6. Blast radius / 7. Tests (fail-before/pass-after) / 8. Long-run test (Source/Load/Hostile/Operability/Reversal) / 9. Edge cases / 10. Performance/security / 11. Commands / 12. Deviations / 13. Out-of-scope / 14. Risks/rollback / 15. Least confident / 16. Self-review / 17. Commits
