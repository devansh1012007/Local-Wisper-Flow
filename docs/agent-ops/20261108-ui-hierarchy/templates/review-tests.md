<!-- ORCH-DIGEST:START -->
agent: <ID> | scope: <wave/tasks> | verdict: APPROVE|APPROVE-WITH-CHANGES|REJECT
tests_reviewed: <n> | fail-before/pass-after: <n> / reasoned: <n> / not-possible: <n>
findings: BLOCKER <n> | MAJOR <n> | MINOR <n> | NIT <n>
flaky_suspects: none|<list>
missing_edge_cases: P1 <n> | P2 <n> | P3 <n>
mutation_survivors: <n of m>
changed_behavior_without_tests: none|<list>
confidence: high|med|low
<!-- ORCH-DIGEST:END -->

# Test quality review
## 0. Method / 1. Per-test assessment / 2. Coverage / 3. Mutation thinking / 4. Flakiness / 5. Findings / 6. Missing edge cases P1/P2/P3 / 7. Unverified
