<!-- ORCH-DIGEST:START -->
agent: <ID> | domain: <domain> | status: DONE|PARTIAL|BLOCKED
items: <n> verified EXISTS / ALREADY-FIXED / NOT-REPRODUCIBLE / PARTIAL / MISCLASSIFIED
proposed_tasks: <n> (SURFACE <n>, DEEP <n>)
top_risks: <≤3>
cross_domain_conflicts: none|<list>
out_of_plan_discoveries: <n>
confidence: high|med|low (<why>)
last_comms_line_read: <timestamp>
<!-- ORCH-DIGEST:END -->

# Investigation report — <ID> — <domain>
## 0. Scope and method
## 1. Per plan item (P-xxx)
- Verdict / Evidence [VERIFIED path:line] [RAN cmd->result] / Root cause / Blast radius (grep counts) / Fix options / Long-run test / Classification SURFACE|DEEP / WRITE-SET, CONTRACT-SET, DEPENDS-ON, ENV-SET / Test plan / Conflicts / Acceptance criteria
## 2. Duplicates / shared root causes
## 3. Out-of-plan discoveries
## 4. Assumptions and unknowns
## 5. Recommended task breakdown and ordering
