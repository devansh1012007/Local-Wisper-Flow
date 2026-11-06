<!-- ORCH-DIGEST:START -->
agent: W1-REV-DELTA | scope: wave 1 / task T-001 (GUI-A2) | verdict: APPROVE-WITH-CHANGES
findings: BLOCKER 0 | MAJOR 1 | MINOR 1 | NIT 2
claims_audit: 8 / 1
band_aids_found: none
top_3_risks: (1) live-DPI move back to 100% left metrics stuck scaled — fixed by delta, not runtime-re-executed here; (2) open MAJOR: delta-added regression guard does not fail before the delta fix, so the ApplyImGuiStyle gate itself is unguarded; (3) real-TTF / effective default FontSizeBase assumptions pending (Q-1/Q-2).
optimizations: DO-NOW 0 | DO-LATER 1 | NOT-WORTH-IT 0
reversal_check: done
confidence: high (source-level verification of the delta fix complete and internally consistent; live-DPI runtime path not executed on this box)
<!-- ORCH-DIGEST:END -->

# Code review — W1-REV-DELTA — wave 1 / task T-001 / GUI-A2 (delta of W1-IMPR-A2)

- Agent: W1-REV-DELTA (delta reviewer) · Wave 1 of run `20261106-gui` · Task T-001 (plan item GUI-A2)
- Predecessor reviews: `reports/w1-review/REV-W1-002-A2.md` (code MAJOR R-W1-002-A2-01), `reports/w1-review/REV-W1-002T-A2.md` (tests MAJOR R-W1-002T-A2-01)
- Improver report: `reports/w1-improve/W1-IMPR-A2-T-001.md`
- Implementation report: `reports/w1-impl/W1-IMPL-A2-T-001.md`
- Delta commits reviewed: `e0a3882ad9da2de33a2a13003b309138f01ad1d9` (`fix(ui): apply design-px style once per context change, guard font regression`), `be21a2864fcf6f1e3525744dddc5b3bff5a6aae4` (docs-only follow-up)
- Diff range: `git diff e0a3882ad9da2de33a2a13003b309138f01ad1d9^..e0a3882ad9da2de33a2a13003b309138f01ad1d9 -- src tests` (i.e. `42a7ea0f..e0a3882` for `src`/`tests` content; context base `42a7ea0f7bbc5ae71b84ec9a838d0558dcb303f0`)
- Read-only review: no source/test files edited, no commits made.

## 0. Method

1. Required reading followed in order: STANDING_ORDERS → AGENTS.md / `docs/plans/2026-11-06-gui-fix-improvements.md` → investigator + implementer + both reviewer reports.
2. Delta diff first (blind pass): `git show --stat e0a3882`, `git diff e0a3882^..e0a3882 -- src tests`.
3. Files read in context: `src/ui/theme/ThemePalette.{h,cpp}`, `src/ui/MainWindow.cpp`, `src/ui/Overlay.cpp`, `src/ui/theme/ThemeStyle.h`, `tests/test_overlay_settings_ui.cpp`, `CMakeLists.txt` (lwf_tests), vendored `_deps-src/imgui/imgui.{h,cpp}`.
4. Vendored ImGui semantics re-derived independently: `ImGuiStyle::ScaleAllSizes` (`_deps-src/imgui/imgui.cpp:1595-1600`: `_MainScale *= scale_factor`, `ImTrunc` rounding; doc comment `:1596-1597`: "call this over a freshly initialized ImGuiStyle structure rather than scaling multiple times"); `FontScaleMain` absolute (`imgui.h:2313`, `imgui.cpp:9216-9217`); final font size = `FontSizeBase * (FontScaleMain * FontScaleDpi * window->FontWindowScale + legacy)` (`imgui.h:2311-2312`, `imgui.cpp:9180-9217`); `ScaleAllSizes` does not scale fonts; `SetWindowFontScale` obsolete (`imgui.h:4133-4134`, `imgui.cpp:552`, impl `imgui.cpp:8857-8865`).
5. Fix behaviour traced for DPI ∈ {1.0, 1.25, 1.5}: init, live move, move back, context switch, same-scale re-apply.

## 1. Blind-pass findings (before reading the implementer report)

1. Delta diff shape is minimal and confined to the T-001 write-set: `src/ui/theme/ThemePalette.cpp`, `src/ui/theme/ThemePalette.h`, `tests/test_overlay_settings_ui.cpp` (+ docs commit be21a28 docs-only). `[VERIFIED] git show --stat e0a3882`.
2. The pre-fix defect is confirmed exactly as the code reviewer framed it: trailing `if (scale != 1.0f) { style.ScaleAllSizes(scale); }` (`ThemePalette.cpp:134-136` pre-delta) skipped the apply on a live move back to 100%, leaving style metrics stuck scaled while `FontScaleMain` was set absolute — fonts recovered, layout metrics did not. `[VERIFIED] git diff e0a3882` (pre-image `:134-136`).
3. The delta fix design is sound: design-px geometry is reassigned absolutely inside the gated block before `ScaleAllSizes(scale)` runs, so each actual apply starts from the design base — making re-apply idempotent from design px; combined with the (context, scale) gate this yields: init apply, scale-change apply (design*new), skip for unchanged (context, scale). `[VERIFIED] src/ui/theme/ThemePalette.cpp:42-73` (post-delta).
4. Font semantics hold: `ScaleAllSizes` is metrics-only (does not scale fonts), `FontScaleMain` is absolute, scoped `PushFont(nullptr, design_px)` bases are unscaled → single DPI application, no double scaling. `[VERIFIED] _deps-src/imgui/imgui.cpp:9180-9217, 9272-9293; imgui.h:517-526`.
5. AC1 holds post-delta: no `SetWindowFontScale` anywhere under `src/`; exactly seven files exist under `src/ui/` and all are covered by the new scan. `[RAN] git grep -n "SetWindowFontScale" -- src -> no hits; git ls-files src/ui -> 7 files`.
6. Token fail-before verified independently: base `42a7ea0f` had only legacy `metrics::mini_text_px = 8.5f`; post-delta has the `font_*_px` family. `[VERIFIED] git show 42a7ea0f:src/ui/theme/ThemePalette.h; Select-String on current file]`.

## 2. Claims audit

| # | Claim (source) | Checked | Evidence / finding |
|---|---|---|---|
| 1 | Delta fix: geometry + `FontScaleMain` + `ScaleAllSizes` run only when (context, scale) changed; colors unconditional; trailing `if` gone. | YES | `[VERIFIED] git diff e0a3882 -- src/ui/theme/ThemePalette.cpp` (post-image `:42-73`, `:75-76`, removal of `:134-136` pre-image). |
| 2 | `ScaleAllSizes` cumulative / lossy; must not re-apply same scale; fix tracks last applied pair. | YES | `[VERIFIED] _deps-src/imgui/imgui.cpp:1595-1600, 1596-1597, imgui.h:2394`. |
| 3 | `FontScaleMain` absolute; final font size composition verified. | YES | `[VERIFIED] _deps-src/imgui/imgui.cpp:9180-9217, imgui.h:2311-2314, 526`. |
| 4 | Design-px reassignment before `ScaleAllSizes` makes each apply idempotent from design base. | YES | `[VERIFIED] src/ui/theme/ThemePalette.cpp:49-72` (assignment order precedes the scale call inside one block). |
| 5 | Context-pointer tracking keeps Full/Overlay per-context semantics. | YES | `[VERIFIED] src/ui/MainWindow.cpp:141-152, 235-245, 268-309; src/ui/Overlay.cpp:89-112, 240-248`. |
| 6 | Delta-added source-scan test asserts only invariants that already held at the delta base. | PARTIAL | `[VERIFIED] git diff b21e40f..e0a3882 -- tests/...` + token source state; see Finding R-W1-003-001. |
| 7 | Scoped gates re-run green by this reviewer. | YES | `[RAN] bash scripts/check_hygiene.sh -> hygiene: OK`; `[RAN] ctest --test-dir build -R "lwf-tests" -> 1/1 Passed 16.16 s`; `[RAN] ./build/lwf_tests.exe -> 218/218 cases, 9589/9589 assertions, Status: SUCCESS!, $LASTEXITCODE=0`. |
| 8 | No `SetWindowFontScale` under `src/` post-delta. | YES | `[RAN] git grep -n "SetWindowFontScale" -- src -> no hits.` |

Claims checked: **8** / unsupported or false: **1** (claim 6 is partially satisfied — the delta's fail-before regression guarantee does not hold for the delta's own change).

## 3. Long-run test (independent answers: Source / Load / Hostile / Operability / Reversal)

- **Source:** Font semantics re-derived and consistent with the delta fix; live-DPI matrix traced by hand for scale transitions 1.0 ↔ {1.25, 1.5}. Post-fix: all transitions produce design-base * new_scale. Pre-fix: transition into 1.0 left metrics stuck. `[VERIFIED]` (by read).
- **Load:** `[RAN] cmake --build build --config Release --parallel 4 --target lwf_tests` was run by the improver (`[RAN] -> Built OK`); this delta reviewer did **not** re-run the Release build. Tests compiled and run green by this reviewer via existing artifacts.
- **Hostile:** Not applicable — typography tokens are compile-time constants; no runtime input path in sizing.
- **Operability:** No new failure paths, logs, or secrets; built-in font path preserved.
- **Reversal:** Mechanically revertible; no data migration.

## 4. Findings

### R-W1-003-001 — MAJOR — Delta-added regression guard does not fail before the delta fix (coverage gap for the actual change)

- Location: `tests/test_overlay_settings_ui.cpp:236-265` (added by `e0a3882`).
- Evidence: The delta's test diff (`git diff b21e40f..e0a3882 -- tests/test_overlay_settings_ui.cpp`) adds only `<iterator>`/`<vector>` includes, a design-mapping comment, and the new `TEST_CASE("src ui font mechanism uses scoped design px tokens without obsolete scale api")`. The asserted invariants — no `SetWindowFontScale` in the seven `src/ui` files, and `PushFont(nullptr, metrics::font_` in `MainWindow.cpp` — already held at `b21e40f` (the implementer commit removed the obsolete API and wired the scoped fonts). The fail-before evidence cited by the improver (token symbols absent at `42a7ea0f`) belongs to the implementer commit, not to this delta.
- Why it matters: The behaviour this delta changed (last-applied `(ImGuiContext*, float)` gate in `ApplyImGuiStyle`) therefore has no automated fail-before regression test; the task-level AC3 fail-before guarantee is met only via the token test from the implementer commit. The single-DPI half of the test-reviewer MAJOR (R-W1-002T-A2-01) is acknowledged as ImGui-linkage-blocked, but the delta also did not source-scan for the gate markers.
- Recommended fix: extend the source-scan to assert the gate markers (`g_last_applied_context`, `g_last_applied_scale`, `applies`) in `src/ui/theme/ThemePalette.{h,cpp}`, and/or add an ImGui-linkable context test as a follow-on; document that the delta-added test is a mechanism guard, not a fail-before test for the gate.

### R-W1-003-002 — MINOR — `_MainScale` internal reference scale still accumulates across live scale changes

- Location: `_deps-src/imgui/imgui.cpp:1587, 1598-1600` (`ImGuiStyle::_MainScale`), `_deps-src/imgui/imgui.h:2394`; applied via `src/ui/theme/ThemePalette.cpp:44-73`.
- Evidence: `ScaleAllSizes` does `_MainScale *= scale_factor`; ImGui documents resetting via a freshly initialized style rather than scaling repeatedly. The delta's design-px reassignment before the scale call keeps rendered floats correct, and `_MainScale` appears write-only in the vendored source (no read site found), so no rendering impact is demonstrable today. Fields not reassigned by the app (e.g. `MouseCursorScale`) could still accumulate under Full/Overlay context alternation.
- Why it matters: residual design smell; not a confirmed defect. Non-blocking.

### R-W1-003-003 — NIT — Geometry assignments moved inside the `applies` gate

- Location: `src/ui/theme/ThemePalette.cpp:42-73`.
- Note: Behavior-preserving today (geometry derives from compile-time tokens; colors handle palette). Minor coupling of geometry apply to scale apply.

### R-W1-003-004 — NIT — Context-pointer-only last-applied tracking

- Location: `src/ui/theme/ThemePalette.cpp:30-43`.
- Note: Theoretical recycled-`ImGuiContext*` pointer edge on context teardown/recreation; not reproducible in the current lifecycle.

## 5. Attempts to break it

- Live-DPI matrix by reasoning: init scale 1.0 → apply; live → 1.25; live → 1.0 (previously stuck, now recovers via gate). Init 1.5 → live → 1.0 → live → 1.5 (all design-base based). `[VERIFIED]` by read of pre/post images.
- Same-scale re-apply: gate now skips; design reassignment makes even an apply idempotent. `[VERIFIED]`
- Context alternation (Full ↔ Overlay): re-apply per frame is harmless because design values are reassigned before scaling. `[VERIFIED]`
- Obsolete API: `git grep -n "SetWindowFontScale" -- src` → no hits. `[RAN]`
- Fail-before: base token state source-verified. `[VERIFIED]`

## 6. Dimension checklist

| Dimension | Verdict | Notes |
|---|---|---|
| Correctness | PASS (code) / open (coverage) | Fix logic verified correct; see R-W1-003-001. |
| Root cause vs symptom | OK | Root cause addressed at owning function, no band-aid. |
| Regression / compat | OK | Internal change; revertible. |
| Concurrency / idempotency | OK | Single-threaded ImGui loop; gate adds no shared mutable state beyond file-local statics. |
| Error handling | OK | No new paths. |
| Security / abuse | OK | Typography only. |
| Performance / scalability | OK | No I/O/allocation added in font path. |
| Migration / rollback | OK | Revertible. |
| Observability | OK | No new logs. |
| Maintainability / conventions | OK | No drive-by refactor. |
| Docs impact | OK | be21a28 docs-only follow-up consistent with fix. |

## 7. Optimization and simplification opportunities

| # | Location | Evidence | Impact | Effort | Risk | Verdict |
|---|---|---|---|---|---|---|
| 1 | `src/ui/theme/ThemePalette.cpp:42-73` | `_MainScale` drift under scale changes (write-only) | LOW | LOW | LOW | DO-LATER |

Optimizations: DO-NOW 0 | DO-LATER 1 | NOT-WORTH-IT 0.

## 8. Cross-agent / cross-task interactions observed

- W1-REV-A2's code MAJOR (R-W1-002-A2-01) and W1-REV-A2T's test MAJOR (R-W1-002T-A2-01) were convergent in spirit; the improver discharged both in the delta. The delta reviewer confirms the code half is correctly discharged; the test half is discharged only partially at the delta level (Finding R-W1-003-001).

## 9. What I could not verify

- Re-run of the Release build by this reviewer (improver reported green; not re-run here).
- Runtime live-DPI moves / pixel measurement (manual checklist: `docs/MANUAL_TEST_WINDOWS.md` R/E1).
- G3 zig type-check (no zig on box).
- Full `ctest` (standing orders: scoped checks only).
- Executing pre-fix behavior to observe the stuck-metrics symptom (no checkout/restore per standing orders).
