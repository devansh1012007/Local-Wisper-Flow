# Brief — W1-IMPR-02 (IMPROVER coder) · wave 1 · run 20261108-ui-hierarchy
## 1. Mission
Fix TEST R-W1-001 MAJOR (no negative scanner test). Why: Ban must prove it fires; otherwise regression to always-empty scanner passes forever.
Reviewer: reports/w1-review/W1-TREV-01.md section 5 R-W1-001 + section 6 P1 sketch.

## 2. Required reading
STANDING-ORDERS, AGENTS.md gates, 00-mission, W1-TREV-01, W1-IMPL-03 report, templates/report-improvement.md, COMMS last WAVE-START. Scanner in tests/test_overlay_settings_ui.cpp:1136-1283 (BanAllowance, ParseNumberAt, ScanLayoutLiterals).

## 3. Task + acceptance
In tests/test_overlay_settings_ui.cpp ONLY, add TEST_CASE("layout literal ban fires on violations and cap-exceeds") using synthetic strings passed to ScanLayoutLiterals (no real file edits):
- PushFont digit violation: "ImGui::PushFont(nullptr, 13.0f);\n" → non-empty
- Cap-exceed: repeat Buttons PushFont 24.0 x2 in Buttons content → cap violation (cap=1)
- Universal: SameLine(0.0f, 0.5f) → empty (no violation)
- Sentinel: PushFont -1.0f → violation (SetNextItemWidth -1.0f → empty)
- DummyWidth/Height double-anchor safe (existing 29 assertions keep passing)
Acceptance: new test passes on landed tree; `ctest -R lwf-tests` 1/1 Passed; report fail-before proof (revert token→digit fires). Do NOT touch src/ui, no other test files.

## 4. Ownership
WRITE-SET: tests/test_overlay_settings_ui.cpp only
FORBIDDEN: src/*, thickness other tests.

## 5. Siblings
W1-IMPR-01 owns TransformsPage.cpp — disjoint.

## 6. Deliverables
Report reports/w1-improve/W1-IMPR-02.md template report-improvement.md (digest FIXED 1). Commits code then report docs-only trailers Task: R-W1-001 Agent: W1-IMPR-02 Wave: 1 Report: <path>
Final ≤8 lines.
