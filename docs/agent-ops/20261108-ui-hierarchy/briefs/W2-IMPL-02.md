# Brief — W2-IMPL-02 (IMPROVER coder) · wave 2.5 · run 20261108-ui-hierarchy
## 1. Mission
Close DEFERRED-DEPENDENCY from W2-IMPL-01: flip two stale test pins that still require native fonts after approved studio migration, plus document the single space_xs reuse site. Why: tranche 2 migrated all studio-page PushFont to studio_tokens (grep→0), so the 2 pin checks contradict the approved design and block ctest (518/520).
## 2. Required reading
STANDING-ORDERS, AGENTS.md gates, 00-mission, design W2-DEEP-01 (approved full 9-token tranche), W2-PLAN-01 (D-W2-01 zero metrics), W2-IMPL-01 report (DEFERRED-DEPENDENCY detail), COMMS tail, plus files tests/test_snippets_page.cpp, tests/test_transforms_page.cpp, src/ui/pages/TransformsPage.cpp:576 context.

## 3. Task
WRITE-SET ONLY:
- tests/test_snippets_page.cpp lines 338-341: flip native metrics::font_ requirement to studio_metrics::font_ (mirror approved migration; verify exact lines with grep before editing)
- tests/test_transforms_page.cpp line ~380: flip same requirement to studio fonts
- src/ui/pages/TransformsPage.cpp:576: add one-line comment above metrics::space_xs usage documenting intentional native grid reuse (no 4px studio twin; keep value 4, do not add 10th token, do not change pixels)
Acceptance:
- G1 target lwf_tests build passes; G2 ctest -R lwf-tests 1/1 Passed (expect 520/520 total green)
- Never weaken: flips must assert studio token presence positively, not delete assertions; keep all other checks intact.
- No other files, no value changes.

## 4. Ownership
WRITE-SET: tests/test_snippets_page.cpp, tests/test_transforms_page.cpp, src/ui/pages/TransformsPage.cpp
READ-ONLY: everything else. If a line differs from description, trust grep, document in report, do not expand scope.

## 5. Siblings
None (single small follow-up, disjoint from any parallel work).

## 6. Deliverables
Report reports/w2-deep/W2-IMPL-02-report.md digest first, template report-improvement.md (FIXED count, dispositions, fail-before evidence: ctest 518/520 → 520/520). Commits code then report docs-only trailers Task: T-003 Agent: W2-IMPL-02 Wave: 2 Report: <path>. Final ≤8 lines.
