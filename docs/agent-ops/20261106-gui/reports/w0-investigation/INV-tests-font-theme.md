<!-- ORCH-DIGEST:START -->
agent: W0-INV-02 | domain: ui-tests-theme | status: DONE
items: 0 planned items re-verified | proposed_tasks: 1 (SURFACE 1)
top_risks: no ImGui font coverage in lwf_tests
cross_domain_conflicts: none
out_of_plan_discoveries: 0
confidence: high
last_comms_line_read: 2026-11-06T16:00:00Z
<!-- ORCH-DIGEST:END -->

# Investigation report — W0-INV-02 — ui-tests-theme (task A2 prep)

## 1. Question and scope
How the test suite covers the UI font/theme system, and what design-px font tokens the mockup defines.

## 2. Findings
- Only `tests/test_overlay_settings_ui.cpp` touches fonts/theme/metrics: palette hex (171), contrast policy (188-211), WCAG math (213-219). No metrics assertions, no ApplyImGuiStyle test, no font-size assertions.
- No test uses ImGui, no ImGui context, no PushFont/SetWindowFontScale/FontScaleMain; `lwf_tests` does not link ImGui (CMakeLists.txt:145).
- Mockup §5 defines design-px type scale 28/18/15/14/13 px (page title/wordmark/section title/body/helper) plus control heights; mockup fonts are system stand-ins pending approval (mockup.html:171).

## 3. Required additions for A2
- New ImGui-free font/metrics token tests in the existing pattern (inline constexpr tokens in ThemePalette.h).
- Scoped-font mechanism unit tests need ImGui context or a refactored ImGui-free helper.

## 4. Open questions / risks
- ImGui linkage for lwf_tests; token naming convention for font sizes.

# REQUIRED OUTPUT SECTIONS (final form)
## Call-site map
## ImGui font semantics confirmed
## Metrics tokens
## Tests
## Open questions / risks
