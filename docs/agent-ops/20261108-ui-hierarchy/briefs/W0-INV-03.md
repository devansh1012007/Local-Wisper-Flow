# Brief — W0-INV-03 (INVESTIGATOR) · wave 0 · run 20261108-ui-hierarchy
## 1. Mission
Audit tests + blast radius for Full pages. Why: P-005 source-scan strategy and Wave 1 disjoint write-sets.
## 2. Required reading
STANDING-ORDERS, AGENTS.md (Build gates), 00-mission, 01-work-ledger, report-investigation, COMMS, tests/test_overlay_settings_ui.cpp, tests/test_dictionary_page.cpp, test_insights_page.cpp, test_transforms_page.cpp, test_snippets_page.cpp, test_scratchpad_page.cpp, CMakeLists.txt:138-199,366-378, scripts/check_hygiene.sh, check_no_network.sh
## 3. Task
What tests guarantee/not about layout/density (contrast, tokens pinned, source-scan bans, logic vs pixel), how run (ctest), flaky/slow candidates, blast radius callers of MainWindow/pages/widgets/theme (grep counts), screenshot harness existence (none expected), propose P-005 test additions that fail-before/pass-after, classify SURFACE/DEEP, acceptance measurable.
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w0-investigation/W0-INV-03.md
## 5. Siblings: 01,02,04
## 6. Traps: lwf_tests does not link ImGui (lexical only); relative ifstream scans fail if CWD != repo root; SettingsTab is internal not Page (PageForRoute fallback to Home).
## 7. Env: read/grep only, no builds/tests (orchestrator runs baseline).
## 8. Deliverables: report-investigation.md, docs-only commit trailers Agent: W0-INV-03 Wave: 0.
