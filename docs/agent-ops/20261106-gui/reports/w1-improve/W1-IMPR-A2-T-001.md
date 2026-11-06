<!-- ORCH-DIGEST:START -->
agent: W1-IMPR-A2 | scope: wave 1 / task T-001 (GUI-A2) | status: DONE
findings: FIXED 3 | REJECTED 0 (with evidence) | DEFERRED 2
blockers_major_open: none
code_commits: e0a3882ad9da2de33a2a13003b309138f01ad1d9 | report_commit: <pending at time of writing>
tests_added_or_strengthened: 1
ownership_violations: none
confidence: high
<!-- ORCH-DIGEST:END -->

# Improvement report — W1-IMPR-A2 — T-001 / GUI-A2 — scoped ImGui fonts + design-px typography tokens

- Agent: W1-IMPR-A2 (improver) · Wave 1 of run 20261106-gui · Task T-001 (plan item GUI-A2).
- Predecessor report: docs/agent-ops/20261106-gui/reports/w1-impl/W1-IMPL-A2-T-001.md (implementation of the design-px font swap, commit b21e40f).
- Source review: docs/agent-ops/20261106-gui/reports/w1-review/REV-W1-002-A2.md (W1-REV-A2).
- Test review: docs/agent-ops/20261106-gui/reports/w1-review/REV-W1-002T-A2.md (W1-REV-A2T).
- Implementation diff under review: `git diff 42a7ea0f7bbc5ae71b84ec9a838d0558dcb303f0..b21e40f -- src/ui tests/test_overlay_settings_ui.cpp`; improver delta: commit e0a3882ad9da2de33a2a13003b309138f01ad1d9.
- Last COMMS line read: 2026-11-06T19:02:45Z (W1-IMPL-A2 -> ALL | DONE | T-001).

## 1. Finding-by-finding disposition

Hand-off set from the wave-1 reviewers: BLOCKER/MAJOR findings only (T-001 opened with none BLOCKER). The actionable MAJORs were F1 and F2; MINORs F3-F5 were also discharged per the improver's mandate.

| ID | Source finding | Severity | Disposition | Action taken (with evidence) | Commit |
|---|---|---|---|---|---|
| F1 | R-W1-002-A2-01 — MAJOR — `ApplyImGuiStyle` re-apply path for `ScaleAllSizes` accumulates / skips, contradicting the task's "DPI applied once" contract | FIXED | Fixed at the owning function. `ApplyImGuiStyle` now tracks the last applied (ImGui context, window DPI scale) pair via file-local statics (`g_last_applied_context`, `g_last_applied_scale`). The token-geometry assignment, `style.FontScaleMain = scale` and `style.ScaleAllSizes(scale)` run only when that pair actually changed; the colour block stays unconditional; the old trailing `if (scale != 1.0f) { style.ScaleAllSizes(scale); }` is gone. `[VERIFIED] _deps-src/imgui/imgui.cpp:1595-1600` (`_MainScale *= scale_factor`, lossy `ImTrunc`, guidance: freshly initialized style rather than repeated scaling), `_deps-src/imgui/imgui.cpp:9216-9217` (`FontSizeBase * (FontScaleMain * FontScaleDpi * FontWindowScale)`; `FontScaleMain` absolute), `_deps-src/imgui/imgui.h:4133-4134` / `imgui.cpp:552` (`SetWindowFontScale` obsolete). | e0a3882 |
| F2 | R-W1-002-A2-02 (cross-confirmed by R-W1-002T-A2-01) — MAJOR — changed T-001 behaviour (AC1 obsolete-API removal, scoped `PushFont`, single-DPI) had no automated regression coverage | FIXED | Added `TEST_CASE("src ui font mechanism uses scoped design px tokens without obsolete scale api")` in `tests/test_overlay_settings_ui.cpp`: source-scans the seven in-scope `src/ui` files for `SetWindowFontScale` (open + `REQUIRE`, then `CHECK(find == npos)`) and positively asserts the Full window's scoped mechanism via `PushFont(nullptr, metrics::font_` in `src/ui/MainWindow.cpp`. Mechanism-level ImGui execution remains untestable in `lwf_tests` (no ImGui linkage; agreed ImGui-free pattern, `INV-tests-font-theme.md`); token assertions retained. | e0a3882 |
| F3 | R-W1-002A2-03 (cross-confirmed by R-W1-002T-A2-02) — MINOR — design-px token set did not encode the mockup mid-scale body step (s4=14) / helper (s5=13); token comment merged the two | FIXED via pin / token split DEFERRED | Extended the existing `TEST_CASE("design px font tokens follow the mockup type scale")` with the approved mockup->token mapping annotation (s1=28 page title, s2=18 wordmark, s3=15 card title, s4=14 body / s5=13 helper -> design-px tokens; body and helper share `font_body_px = 13` per the approved design decision). Token split into separate body/helper tokens deferred (design decision pending). | e0a3882 |
| F4 | R-W1-002A2-05 — MINOR — token comment/naming nit merges concepts (`font_body_px // body / helper text`) | DEFERRED | Left as-is; the merged comment is internally consistent and the token-split follow-up will resolve naming. No action taken within T-001 scope. | — |
| F5 | R-W1-002T-A2-04 — NIT — token TEST_CASE ergonomics (bare `CHECK`s, no `INFO` context, no `static_assert`) | DEFERRED | Cosmetic; deferred. | — |

- Findings tally: FIXED 3 | REJECTED 0 (with evidence) | DEFERRED 2; blockers_major_open: none; confidence: high.

## 2. Code changes (per file, with rationale; root-cause level, no band-aids)

| File | Change | Rationale |
|---|---|---|
| `src/ui/theme/ThemePalette.cpp` | F1 fix (improver delta, commit e0a3882): file-local statics `ImGuiContext* g_last_applied_context` / `float g_last_applied_scale` in the anonymous namespace; `const bool applies = ImGui::GetCurrentContext() != g_last_applied_context \|\| scale != g_last_applied_scale;` gates the token-geometry block, the absolute `style.FontScaleMain = scale` assignment and the `style.ScaleAllSizes(scale)` call; colour writes remain unconditional; the pre-fix trailing `if (scale != 1.0f) { style.ScaleAllSizes(scale); }` removed. | Root cause: the task's single-DPI contract (`style.FontScaleMain` absolute + `ScaleAllSizes` metrics-only, fonts not scaled by it) was undermined by an unconditional re-apply path that both accumulated (`_MainScale *= scale_factor` with `ImTrunc`) and skipped at `scale == 1.0f`. The fix makes the apply idempotent per (context, scale) at the owning function, which is the source of the invariant, not a call-site patch. Context-pointer comparison keeps the Full-window and Overlay-context cases correct (each new context applies once). Not a band-aid: no error swallowing, no flag, no retry. |
| `src/ui/theme/ThemePalette.h` | Updated the `ApplyImGuiStyle` doc comment: the window scale is applied once per actual change (init or live content-scale move); re-applying an unchanged scale does not re-run metrics scaling. | Contract documentation only; logic unchanged. |
| `tests/test_overlay_settings_ui.cpp` | F2 regression guard + F3 mapping pin (improver delta, commit e0a3882). | See section 3. |

No changes were needed in `src/ui/MainWindow.cpp` / `src/ui/Overlay.cpp` for these findings — the obsolete-API removal and scoped `PushFont` wiring landed with the implementer commit (b21e40f); the F1 defect was entirely inside `ApplyImGuiStyle`.

## 3. Test changes (added edge cases from the test review, strengthened assertions, flake fixes, removed redundancy)

- Added: `TEST_CASE("src ui font mechanism uses scoped design px tokens without obsolete scale api")` (`tests/test_overlay_settings_ui.cpp:236-265`) — the F2 guard. Scans `src/ui/MainWindow.cpp`, `src/ui/MainWindow.h`, `src/ui/Overlay.cpp`, `src/ui/Overlay.h`, `src/ui/theme/ThemePalette.h`, `src/ui/theme/ThemePalette.cpp`, `src/ui/theme/ThemeStyle.h` via `std::ifstream`/`std::istreambuf_iterator`, `REQUIRE` on open, `CHECK(content.find("SetWindowFontScale") == std::string::npos)`; positive check that the Full window pushes scoped design-px tokens: `PushFont(nullptr, metrics::font_` present in `src/ui/MainWindow.cpp`.
- Strengthened: existing `TEST_CASE("design px font tokens follow the mockup type scale")` (`tests/test_overlay_settings_ui.cpp:223-234`) gained the approved mockup mapping annotation (F3 pin).
- Added includes `<iterator>`, `<string>`, `<vector>` for the new source-scan test.
- Deferred: F5 test ergonomics (INFO context / `static_assert`), and an ImGui-context-execution test (blocked on ImGui linkage — acknowledged, out of improver scope, recorded as DEFERRED).

## 4. Optimizations applied and evidence of benefit (and ones declined, with reason)

- Applied: none beyond the correctness fix (no perf work warranted; the source scan is a handful of tiny file reads in a single test).
- Declined: token split into body/helper tokens (F3 follow-up; design decision pending, not a correctness blocker within T-001), failure-context polish on token CHECKs (F5 NIT), caching of scanned file content across test cases (no measured need; files are few and read once per run).

## 5. Commands run and results (scoped)

All tagged `[RAN]` / `[VERIFIED]` / `[ASSUMED]` per standing orders:

- `[RAN] bash scripts/check_hygiene.sh` -> `hygiene: OK`.
- `[RAN] ctest --test-dir build -R lwf_tests` -> `1/1 Test #1: lwf-tests ... Passed 16.31 sec`; `100% tests passed`, total 16.32 s.
- `[RAN] ./build/lwf_tests.exe` -> `[doctest] test cases: 218 | 218 passed | 0 failed`; assertions `9589 | 9589 passed`; `[doctest] Status: SUCCESS!`.
- `[RAN] cmake --build build --config Release --parallel 4 --target lwf_tests` -> `Built OK` (ThemePalette.cpp + test_overlay_settings_ui.cpp recompiled).
- `[RAN] cmake --build build --config Release --parallel 4 --target local-wisper-flow` -> `[100%] Built target local-wisper-flow`.
- `[VERIFIED] git grep -n SetWindowFontScale -- src/` -> 0 hits post-change (pre-change hits were in `src/ui/MainWindow.cpp`).
- `[VERIFIED] git show 42a7ea0f:src/ui/theme/ThemePalette.h` -> only legacy `metrics::mini_text_px = 8.5f` existed pre-change; token family `font_*_px` absent at base (fail-before for token test).

## 6. Residual risks and anything the delta reviewer should check

- Residual: effective default font size assumption (`[ASSUMED]`, Q-1) — design tokens are source of truth; real TTF approval pending (Q-2). Live Windows DPI moves and runtime pixel measurement remain manual/unknown (`docs/MANUAL_TEST_WINDOWS.md`).
- Residual: token split (body vs helper) deferred (F3 follow-up).
- Delta-reviewer checks: (a) `ApplyImGuiStyle` idempotency — same context + same scale must no-op now; changed scale must re-apply exactly once; (b) two-context behaviour (Full + Overlay each apply once on their own context); (c) the new source-scan must keep passing if any `src/ui` file reintroduces `SetWindowFontScale`; (d) `PushFont`/`PopFont` balance at existing call sites unchanged.

## 7. Long-run test (Source / Load / Hostile / Operability / Reversal)

- **Source:** font semantics re-derived from pinned vendored ImGui (`_deps-src/imgui`): scoped `PushFont` base sizes unscaled; final = `FontSizeBase * (FontScaleMain * FontScaleDpi * FontWindowScale (+legacy))`; `ScaleAllSizes` metrics-only (does not scale fonts). -> `[VERIFIED]`.
- **Load:** scoped Release build green after the fix (`[RAN] ... -> [100%] Built target local-wisper-flow`).
- **Hostile:** not applicable (typography tokens are compile-time constants; no runtime input path).
- **Operability:** no new failure paths/logs/secrets; built-in font path preserved through design-px bases when fonts are missing.
- **Reversal:** revertible (`git revert` of improver delta / implementer commit); no data migration.

## 8. Commits

- Code: `e0a3882ad9da2de33a2a13003b309138f01ad1d9` — `fix(ui): apply design-px style once per context change, guard font regression` (src/ui/theme/ThemePalette.{cpp,h}, tests/test_overlay_settings_ui.cpp).
- Report: `<pending at time of writing>` — docs-only commit to follow.

## 9. Out-of-scope findings

- None blocking. Full `ctest` and G3 zig type-check not run in this improver pass (standing orders: scoped checks only; G3 pending on this box, recorded in docs/QUESTIONS.md).
