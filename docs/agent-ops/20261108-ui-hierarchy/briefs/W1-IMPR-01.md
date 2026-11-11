# Brief — W1-IMPR-01 (IMPROVER general) · wave 1 · run 20261108-ui-hierarchy
## 1. Mission
Fix CODE R-W1-001 MINOR (Transforms header measurement/layout split). Why: Same-value token alignment prevents future silent misalignment; zero pixel delta today.
Reviewer report: reports/w1-review/W1-REV-01.md section 4 R-W1-001.

## 2. Required reading
STANDING-ORDERS, AGENTS.md UI, 00-mission, W1-REV-01 section 4 R-W1-001, templates/report-improvement.md, COMMS from last WAVE-START. Implementer report W1-IMPL-02 context.

## 3. Task + acceptance
In src/ui/pages/TransformsPage.cpp ONLY:
- :105 `8.0f * scale` → `metrics::space_s * scale`
- :189-190 both `8.0f * scale` → `metrics::space_s * scale`
- :107 `metrics::font_body_px` → `studio_metrics::font_helper_px`
Leave 10/12 raw (allowlisted until Wave 2). Verify values: space_s=8, font 13==13, so zero pixel delta.
Acceptance: grep shows no 8.0f*scale in those 3 sites, font matches Keycap widget (Buttons.cpp:154,166); `cmake --build build --target lwf_tests --parallel 4` + `ctest -R lwf-tests` pass (tokens unchanged, ban unaffected).

## 4. Ownership
WRITE-SET: src/ui/pages/TransformsPage.cpp (only)
READ-ONLY: everything else
FORBIDDEN: tests/*, theme/*, other pages/widgets.

## 5. Siblings
W1-IMPR-02 (coder) owns tests/test_overlay_settings_ui.cpp — disjoint, you cannot see their work.

## 6. Deliverables
Report reports/w1-improve/W1-IMPR-01.md template report-improvement.md (digest FIXED 1). Commits code then report docs-only trailers Task: R-W1-001 Agent: W1-IMPR-01 Wave: 1 Report: <path>
Final ≤8 lines.
