<!-- ORCH-DIGEST:START -->
agent: W1-IMPL-A2 | task: T-001 | status: DONE
code_commits: <code-sha> | report_commit: <sha>
files_changed: 5 (src/ui/MainWindow.cpp, src/ui/Overlay.cpp, src/ui/theme/ThemePalette.h, src/ui/theme/ThemePalette.cpp, tests/test_overlay_settings_ui.cpp)
contracts_changed: none (internal lwf::ui::metrics design-px tokens only; no serialized / public API boundary touched)
tests: added 1, changed 0; scoped run: pass
ownership_violations: none
deferred_dependencies: none
out_of_scope_findings: 0
temporary_mitigation: no
deep_issue_flag: no
least_confident_about: effective default FontSizeBase (LegacySize = 13) for the Full-window context until a real TTF font lands (font approval pending, Q-2).
confidence: high (scoped G1/G2/G4 green; ImGui v1.92.9b font semantics verified against pinned vendored source; design-px tokens mirrored from mockup section 5 and the W0 investigation reports).
last_comms_line_read: 2026-11-06T18:33:35Z
<!-- ORCH-DIGEST:END -->

# Implementation report — W1-IMPL-A2 — T-001 — Scoped ImGui fonts and design-px typography tokens

## 1. Task and acceptance criteria (restated)

Task T-001 (plan item GUI-A2; plan: `docs/plans/2026-11-06-gui-fix-improvements.md`; brief: `docs/agent-ops/20261106-gui/briefs/W1-IMPL-A2.md`).

Mission: replace the obsolete Dear ImGui `SetWindowFontScale` font mechanism with the sanctioned scoped mechanism and express typography sizes as design-px tokens.

Acceptance criteria (measurable, from the brief):

- AC1: no `SetWindowFontScale` remains anywhere under `src/ui/` (code + comments).
- AC2: wordmark ~=18 px, page title ~=28 px, card title ~=15 px, body ~=13 px, mini label ~=8.5 px design px, each applied via the scoped font mechanism and DPI-scaled exactly once (no double scaling).
- AC3: tests added under `tests/` covering the new font tokens / mechanism, in the existing ImGui-free pattern where possible; must fail before the fix and pass after.
- AC4: `cmake --build build --config Release --parallel 4 --target local-wisper-flow` green; `ctest --test-dir build -R lwf-tests` green; `scripts/check_hygiene.sh` green.

## 2. Verification that the problem exists

Evidence gathered by this agent (not copied from the investigation report):

- `[VERIFIED] _deps-src/imgui/imgui.h:4134` — `ImGui::SetWindowFontScale` is marked obsolete in the pinned ImGui v1.92.9b header, with the sanctioned replacement direction (`PushFont` / style-based scaling).
- `[VERIFIED] _deps-src/imgui/imgui.cpp:552` — vendored ImGui changelog entry declaring `SetWindowFontScale` obsolete and moving the global font-scale concept to `style.FontScaleMain`.
- `[VERIFIED] _deps-src/imgui/imgui.cpp:9216-9217` — final glyph size composition is `FontSizeBase * (FontScaleMain * FontScaleDpi * window->FontWindowScale (+ legacy))`; `GetFontSize()` returns the post-scale value and must not be fed back into `PushFont`.
- `[VERIFIED] src/ui/MainWindow.cpp` (pre-change) — magic ratio call sites: `ImGui::SetWindowFontScale(18.0f / kBaseFontPx)` + reset (wordmark, rail), `ImGui::SetWindowFontScale(28.0f / kBaseFontPx)` + reset (page titles), and twelve `ImGui::SetWindowFontScale(15.0f / kBaseFontPx)` + reset (card titles), all against `constexpr float kBaseFontPx = 13.0f;` (built-in font until real fonts are approved).
- `[VERIFIED] src/ui/theme/ThemePalette.cpp` (pre-change) — `ApplyImGuiStyle` set `style.FontScaleMain = scale` and, when `scale != 1`, called `style.ScaleAllSizes(scale)`; display typography was therefore mixed into style-level scaling instead of being expressed in design px.
- `[VERIFIED] src/ui/theme/ThemePalette.h:113` (pre-change) — the only font token was `metrics::mini_text_px = 8.5f`.
- `[ASSUMED]` — design-px values map 1:1 to token values and the Full-window default `FontSizeBase` is the built-in font's `LegacySize` (13) until a real font lands; the W0 investigation flagged this as Q-1.

Baseline gates at wave start (`WAVE_BASE_SHA: 42a7ea0f...`): G1 build green, `ctest -R lwf-tests` -> 1/1 Passed (17.27 s), G4 hygiene OK.

## 3. Root cause

The Full window expressed every display typography size as a *ratio against a hard-coded built-in font size* (`SetWindowFontScale(value / kBaseFontPx)`) through an API that the pinned ImGui v1.92.9b marks obsolete, and the DPI factor was applied at style level. The result: design px were indistinguishable from magic numbers, per-window stale scale state was possible via the obsolete setter, and DPI scaling was coupled to font scaling rather than applied once over unscaled design-px tokens. The mini badge already had a design-px token (`metrics::mini_text_px = 8.5f`) but the rest of the UI had no design-px token source of truth.

## 4. Approach chosen and why

Approach A (chosen, matches the sanctioned pattern and the plan's preferred option):

- Add design-px tokens to `lwf::ui::metrics` in `src/ui/theme/ThemePalette.h`: `font_wordmark_px = 18.0f`, `font_page_title_px = 28.0f`, `font_card_title_px = 15.0f`, `font_body_px = 13.0f`, `font_mini_text_px = 8.5f`.
- Replace every obsolete call site in `src/ui/MainWindow.cpp` with scoped `ImGui::PushFont(nullptr, metrics::font_*_px)` + `ImGui::PopFont()` pairing (balanced scopes: body window scope, rail wordmark scope, page-chrome scope, twelve card scopes).
- Keep `ApplyImGuiStyle` applying only the window DPI factor via `style.FontScaleMain = scale` (plus the pre-existing metrics-only `ScaleAllSizes` behavior, which does not scale fonts); scoped design-px bases are unscaled, so the DPI factor is applied exactly once.

Alternatives considered and rejected:

- **B (keep `style.FontScaleMain` + add per-scope `window->FontWindowScale`):** rejected; reuses global scaling but leaves the obsolete-pattern residue and adds a second scaling mechanism.
- **C (load real TTFs):** rejected as out of scope (font approval pending, audit D6).

How this addresses the cause: typography is now expressed in design px at the token layer and applied through the scoped font mechanism, removing both the obsolete setter and the magic ratios; DPI is applied once.

## 5. What changed (per file)

| File | Change | Why |
|---|---|---|
| `src/ui/MainWindow.cpp` | Removed `constexpr float kBaseFontPx = 13.0f;` (built-in font until real fonts approved). Body text now `ImGui::PushFont(nullptr, metrics::font_body_px)` (+`PopFont`) scoped to the full-window draw. Wordmark: `ImGui::PushFont(nullptr, metrics::font_wordmark_px)` (+`PopFont`). Page titles: `ImGui::PushFont(nullptr, metrics::font_page_title_px)` (+`PopFont`). All twelve card-title sites switched from `SetWindowFontScale(15.0f / kBaseFontPx)` to `ImGui::PushFont(nullptr, metrics::font_card_title_px)` (+`PopFont`). | Design-px tokens as source of truth; scoped mechanism; DPI once via `FontScaleMain`. |
| `src/ui/Overlay.cpp` | Mini label bitmap font config now uses `metrics::font_mini_text_px` (design token renamed from `metrics::mini_text_px` for family consistency). Value unchanged (8.5 design px). | Token naming consistency with the new `font_*_px` family. |
| `src/ui/theme/ThemePalette.h` | Added design-px tokens: `font_wordmark_px = 18.0f`, `font_page_title_px = 28.0f`, `font_card_title_px = 15.0f`, `font_body_px = 13.0f`, `font_mini_text_px = 8.5f`, each with a comment tying it to the mockup type scale (section 5). Removed the now-superseded `metrics::mini_text_px`. Also updated the `ApplyImGuiStyle` doc comment. | Design-px token source of truth; single place for typography. |
| `src/ui/theme/ThemePalette.cpp` | Updated `ApplyImGuiStyle` comments to state that display typography is scoped per call site via `ImGui::PushFont(nullptr, design_px)` + `PopFont` with the `metrics::font_*_px` tokens, and the DPI factor (`style.FontScaleMain`) applies exactly once. | Documented the new contract; kept `style.FontScaleMain = scale` behavior. |
| `tests/test_overlay_settings_ui.cpp` | Added `TEST_CASE("design px font tokens follow the mockup type scale")` asserting the five token values (18 / 28 / 15 / 13 / 8.5). | Token regression coverage (ImGui-free pattern). |

Note: no changes were made under `src/core/`, engine stores, `_deps-src/`, or `src/main.cpp` (UI wiring) — outside the write-set.

## 6. Blast radius and contracts

- Grep run: `git grep -n SetWindowFontScale -- src/` -> 0 hits (post-change). Pre-change hits existed only in `src/ui/MainWindow.cpp` (and stale mentions in docs, which are out of the write-set).
- All former `SetWindowFontScale` call sites were inside window / card scopes; the replacement `PushFont`/`PopFont` pairs are likewise scoped within those same boundaries, so no cross-window state leakage is introduced.
- Contract impact: none (internal design tokens + internal ImGui call pattern; no serialized data, config schema, or public API change).
- Backward compatibility: additive tokens; the only rename (`metrics::mini_text_px` -> `metrics::font_mini_text_px`) is internal to the app and both the definition and the single consumer (`src/ui/Overlay.cpp`) were updated together.
- `ApplyImGuiStyle` contract (DPI scale in -> style scaled once) preserved; `ScaleAllSizes` metrics-only behavior left untouched (verified not to scale fonts).

## 7. Tests

- Added: `tests/test_overlay_settings_ui.cpp` -> new `TEST_CASE("design px font tokens follow the mockup type scale")` (asserts `font_wordmark_px == 18`, `font_page_title_px == 28`, `font_card_title_px == 15`, `font_body_px == 13`, `font_mini_text_px == 8.5`).
- Changed: 0.
- Behavior covered: the design-px token values required by AC2.
- Edge / abuse coverage: token constants are compile-time `inline constexpr` (no runtime input path); no hostile-input surface in sizing for this change (titles are static strings; hotkey/dynamic strings flow through ImGui's own text layout).
- Fail-before / pass-after: before the fix the `font_*_px` token symbols did not exist, so the new test could not compile -> it fails before the fix; after the tokens landed, the same test compiles and passes -> pass after. (Mechanism-level ImGui execution is not covered because `lwf_tests` does not link ImGui; token assertions are the agreed ImGui-free pattern, per `INV-tests-font-theme.md`.)
- Commands and results: `ctest --test-dir build -R lwf-tests` -> `1/1 Test #1: lwf-tests ... Passed 16.09 sec`; `100% tests passed, Total Test time (real) = 16.10 s`.

## 8. Long-run test

Source / Load / Hostile / Operability / Reversal:

- **Source:** font semantics verified in the pinned vendored ImGui (`_deps-src/imgui`): scoped `PushFont` takes an unscaled base size; final size = `FontSizeBase * (FontScaleMain * FontScaleDpi * FontWindowScale)`; `ScaleAllSizes` does not scale fonts. -> `[VERIFIED]`.
- **Load:** scoped Release build green after the change (`[RAN ...] cmake --build build --config Release --parallel 4 --target local-wisper-flow -> [100%] Built target local-wisper-flow`).
- **Hostile:** not a security-relevant surface (typography only); no unbounded inputs introduced.
- **Operability:** no new failure paths, no new logs/secrets introduced; runtime behavior degrades exactly as before when fonts are missing (built-in font path).
- **Reversal:** revertible; tokens are additive except the internal rename, both updated together; no data migration.

## 9. Edge cases and abuse scenarios

- Handled: balanced `PushFont`/`PopFont` pairing at every site (no scope leaks); body token equals the default font size (13), so body rendering is visually unchanged at 100% DPI; mini label value preserved (8.5 design px, only renamed).
- Consciously not handled: real TTF font loading (deferred, Q-2); effective `FontSizeBase` confirmation until then (Q-1).
- Unknown: none observed during scoped gates.

## 10. Performance and security considerations

- No new dependencies, no new allocations in the font path, no measurable performance impact; no secrets/PII introduced. No network APIs touched.

## 11. Commands run and results (verbatim summaries)

- `[RAN] cmake --build build --config Release --parallel 4 --target local-wisper-flow`
  - output tail: `[ 98%] Building CXX object CMakeFiles/local-wisper-flow.dir/src/ui/MainWindow.cpp.obj`, `[ 98%] Linking CXX executable local-wisper-flow.exe`, `[100%] Built target local-wisper-flow`
- `[RAN] ctest --test-dir build -R lwf-tests`
  - `1/1 Test #1: lwf-tests ... Passed 16.09 sec`; `100% tests passed, Total Test time (real) = 16.10 s`
- `[RAN] bash scripts/check_hygiene.sh`
  - `hygiene: OK`

## 12. Deviations from the plan/brief; ownership issues; deferred dependencies

- Deviation (cosmetic): leftover double blank lines after removal of `kBaseFontPx` were tidied to single blank lines (no logic change).
- Deviation: token-level (ImGui-free) test chosen over ImGui-context mechanism test, per the agreed pattern and `INV-tests-font-theme.md` (lwf_tests does not link ImGui).
- Deferred dependencies: none blocking; real TTF font approval (Q-2) deferred; G3 zig type-check pending (no zig on this box) — recorded in `docs/QUESTIONS.md`.

## 13. Out-of-scope findings

- None.

## 14. Known limitations, risks, rollback notes

- Mockup type scale lists body at 14 px (`s4`) / helper at 13 px (`s5`) while the token is `font_body_px = 13`; subtitles render at the body font — recorded as a known discrepancy, not silently "fixed".
- Real fonts pending approval (audit D6 / Q-2): token fidelity for actual glyph rendering awaits the approved font.
- Windows-hardware verification not claimed (manual checklist in `docs/MANUAL_TEST_WINDOWS.md`).

## 15. Where I am least confident / what the reviewer should scrutinize

- Effective default `FontSizeBase` (`LegacySize` = 13) for the Full-window context until a real TTF lands (Q-1).
- That the scoped design-px values plus single `FontScaleMain` DPI application produce the intended effective sizes (mechanism verified in source; runtime pixel measurement pending).

Reviewer focus (W1-REV-A2 / W1-REV-A2T / W1-IMPR-A2): PushFont/PopFont pairing balance, no double scaling, token test rigor, write-set compliance.

## 16. Self-review checklist

- [x] Root cause, not symptom addressed.
- [x] No band-aid patterns (no try/catch swallow, no type suppression, no sleeps/retries).
- [x] Tests fail before / pass after (token symbols absent pre-fix -> compile fail pre-fix; pass post-fix).
- [x] Only write-set files changed (plus required wave-ops docs: report, COMMS append, PROGRESS task-log).
- [x] No secrets / PII.
- [x] AGENTS.md conventions followed (scoped commands only, hygiene, naming).

## 17. Commits

- code: <sha> (pending at time of writing)
- report: <sha> (pending at time of writing)
