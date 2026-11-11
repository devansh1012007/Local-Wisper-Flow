<!-- ORCH-DIGEST:START -->
agent: W1-IMPR-01 | scope: R-W1-001 | status: DONE
findings: FIXED 1 | REJECTED 0 | DEFERRED 0
blockers_major_open: none (ctest gate depends on sibling W1-IMPR-02 scanner landing; FINDING posted)
code_commits: 1f7a71d | report_commit: pending
tests_added_or_strengthened: 0
ownership_violations: none
confidence: high
<!-- ORCH-DIGEST:END -->

# Improvement report — W1-IMPR-01 (R-W1-001)

## 1. Dispositions
- R-W1-001 [MINOR] Paired measurement arithmetic left as raw literals (Transforms header) — **FIXED** (1 finding, 3 sites).

## 2. Code changes
`src/ui/pages/TransformsPage.cpp` only (4 lines changed):
- `KeycapRowWidth` line 105: `width += 8.0f * scale;` → `width += metrics::space_s * scale;` — now spelled the same as the layout side `DrawKeycapRow` SameLine at line 95 (`metrics::space_s * scale`) [VERIFIED TransformsPage.cpp:95,105].
- `KeycapRowWidth` line 107: `ImGui::PushFont(nullptr, metrics::font_body_px);` → `ImGui::PushFont(nullptr, studio_metrics::font_helper_px);` — matches the Keycap widget it measures [VERIFIED Buttons.cpp:154,166 `studio_metrics::font_helper_px`].
- `DrawHeader` `cluster_w` lines 189–190: both `8.0f * scale` → `metrics::space_s * scale` — matches the rendered SameLines (line 203 `metrics::space_s`, line 210 `metrics::space_s`) [VERIFIED TransformsPage.cpp:203,210].

Same-value proof (zero pixel delta): `metrics::space_s = 8.0f` [VERIFIED src/ui/theme/ThemePalette.h:86], `metrics::font_body_px = 13.0f` [VERIFIED ThemePalette.h:106], `studio_metrics::font_helper_px = 13.0f` [VERIFIED src/ui/theme/StudioPalette.h:58]. So `8.0f * scale == space_s * scale` and `13.0f == 13.0f` for all `scale`; the layout is byte-identical, only future token edits now stay aligned on both sides.
- 10/12 literals left raw as instructed (allowlisted; Wave 2 T-003 adds real tokens) — cluster_w still carries `10.0f * scale`/`12.0f * scale` [VERIFIED TransformsPage.cpp:189-190 post-edit]. Line 284 (`8.0f * scale` button offset) also left: outside the brief's 3 sites, no change (scope discipline).
- No palette/theme/header/test edits. `studio_metrics::` already resolves in this file [VERIFIED used at TransformsPage.cpp:146,180,283]; `metrics::` already used throughout.
- Alternatives considered: retokenizing 10/12 to nearest spacing tokens (rejected — shifts pixels, violates frozen-token rule); adding new tokens to headers (rejected — forbidden, Wave 2 owns them).

## 3. Test changes
None. tests/* are forbidden for this agent and owned by sibling W1-IMPR-02. Tokens unchanged, so the existing lexical ban is unaffected — proven: the ban test that scans real source files passes 29/29 with this change [RAN `.\build\lwf_tests.exe --test-case="full window layout APIs use tokens not digit literals outside the token-gap allowlist"` → 1 passed, 29 assertions].

## 4. Optimizations
Only the 3 mandated sites were aligned; no other literals touched. No runtime cost delta (inline constexpr float constants). No further optimization is in scope.

## 5. Commands
- [RAN `git branch --show-current` → ops/20261108-ui-hierarchy; in uncommitted leftover `?? docs/plans/2026-11-08-dict-rows-imgui-dummy.md` (not mine, untouched).]
- [RAN `cmake --build build --target lwf_tests --parallel 4` → Built target lwf_tests, miniaudio, lwf_core.]
- [RAN `ctest --test-dir build -R lwf-tests --output-on-failure` → 1/1 Failed: 519/520 cases, 27775/27778 assertions. Sole failing case: sibling W1-IMPR-02's new uncommitted test `layout literal ban fires on violations and cap-exceeds` (tests/test_overlay_settings_ui.cpp:1338,1345,1360 — `ScanLayoutLiterals` returns empty for all synthetic inputs, i.e. the scanner's firing logic is not yet implemented by its owner).]
- [RAN `.\build\lwf_tests.exe --test-case="layout literal ban fires on violations and cap-exceeds"` (retry per standing orders §18) → same 3 CHECKs fail. The test feeds synthetic strings directly to `ScanLayoutLiterals` and never reads TransformsPage.cpp, so it provably cannot be affected by this change.]
- [RAN `.\build\lwf_tests.exe --test-case="full window layout APIs use tokens not digit literals outside the token-gap allowlist"` → 1 passed, 29 assertions passed (the test that DOES scan my edited file — no regression).]
- [RAN grep `8.0f * scale|font_body_px|font_helper_px|space_s` in TransformsPage.cpp → lines 105/189/190 now `metrics::space_s`, line 107 now `studio_metrics::font_helper_px`; remaining 10/12 literals and other `metrics::font_body_px` sites unchanged.]
- G3 zig: NOT-RUN (no zig on dev box, per AGENTS.md). Screenshots P-006: NOT-RUN (manual Windows, values provably identical, expected null diff).
- COMMS last line read before commit: appended FINDING `2026-11-07T21:00:30Z W1-IMPR-01 -> ORCH | FINDING | R-W1-001` (their START line at :31 preceded it).
- Commits: code `1f7a71d` (explicit path `git add -- src/ui/pages/TransformsPage.cpp`, trailers `Task: R-W1-001 Agent: W1-IMPR-01 Wave: 1 Report: <path>`); report commit follows (docs-only, lists code SHA). Sibling's uncommitted tests/test_overlay_settings_ui.cpp + COMMS.md edits deliberately NOT committed (not my files; COMMS belongs to ORCH per rule 8).

## 6. Residual risks
- **ctest gate**: full suite is red solely due to W1-IMPR-02's in-flight negative-scanner test (their scanner implementation is pending — their START line is uncommitted work). FINDING posted to COMMS; not fixed here (tests/* forbidden, standing orders §18: don't fix others' files). The wave gate should re-run ctest after the sibling lands; the ban test that covers this change is already green.
- [ASSUMED] `scale` is a positive DPI factor; `scale=0` collapses both forms equally (0 == 0), no divergence.
- Long-run test: Source — reintroducing a digit gap at these 3 sites is now visible as `8.0f` beside `metrics::space_s` and trips review/ban discipline. Load/hostile — N/A (compile-time constants, no input surface). Operability — rollback is `git revert 1f7a71d`; no migration/data. Reversal — reverting line 105 to `8.0f * scale` restores the raw form but the existing 29-assertion ban test still passes (arithmetic lines are outside the 5-API scan, disclosed limit shared with sibling's test).
