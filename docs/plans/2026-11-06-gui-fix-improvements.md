# Plan — Local-Wisper-Flow GUI fix/improvement stream (2026-11-06)

- **Date:** 2026-11-06
- **Branch:** `fix/gui-typography-dpi`
- **Status:** draft → in progress (wave 1 underway)
- **Run:** `docs/agent-ops/20261106-gui`

## 1. Problem statement and goal

**Problem:** The Full window's typography/DPI handling is built on `ImGui::SetWindowFontScale`, which Dear ImGui v1.92.9b marks obsolete, and the design-px sizes (wordmark 18, page title 28, card title 15, body 13, mini label 8.5) are expressed as magic ratios against the built-in font (`kBaseFontPx = 13`) instead of design-px tokens.

**Goal:** Replace every obsolete font-scale call site with the sanctioned scoped mechanism (`ImGui::PushFont(NULL, design_px)` + `PopFont`, or design-px metrics tokens) so typography is expressed in design px, DPI-scaled once via `style.FontScaleMain`, with no stale per-window scales.

**Non-goals:** Real TTF font loading (font approval pending, audit D6); engine behavior; injection/STT/LLM logic.

## 2. Acceptance criteria (measurable)

- AC1: `src/ui/` contains zero `SetWindowFontScale` references (code + comments updated).
- AC2: Wordmark renders at design px 18 × DPI, page titles at 28 × DPI, card titles at 15 × DPI, body at 13 × DPI, mini label at 8.5 × DPI (via scoped fonts / tokens; verified by code path + tests).
- AC3: `lwf_tests` (or scoped new target) cover the new font tokens/mechanism; failing-before/pass-after evidence.
- AC4: Full build green (G1), scoped tests green, hygiene green (G4).

## 3. Facts vs assumptions

- **Facts:** `SetWindowFontScale` obsolete in ImGui 1.92.9b (`_deps-src/imgui/imgui.h:4134`, changelog `imgui.cpp:552`); final size = `FontSizeBase * FontScaleMain * FontScaleDpi * window->FontWindowScale` (`imgui.cpp:9216-9217`); `PushFont(NULL, base)` keeps font / sets unscaled size; `ScaleAllSizes` does not scale fonts.
- **Assumptions:** design-px values map 1:1 to token values; the Full-window default `FontSizeBase` is the built-in font's `LegacySize` (13) until a real font lands.

## 4. Candidate approaches

- **A (chosen): scoped `PushFont(NULL, design_px)` + design-px tokens in `metrics::`, reset via PushFont/PopFont pairing; `ApplyImGuiStyle` sets `FontScaleMain = dpi` only.**
- **B: keep `style.FontScaleMain` for DPI and add per-scope `window->FontWindowScale` via a new helper.**
- **C: load real TTFs (deferred, out of scope).**

A vs B: A is the ImGui-sanctioned scoped path and removes stale per-window scale state; B reuses global scaling but keeps obsolete-pattern residue. A chosen.

## 5. Tasks (atomic phases)

| Task | Title | Depends on | Write-set | Tests | Done criteria |
|---|---|---|---|---|---|
| T-001 | A2: scoped fonts + design-px tokens | — | src/ui/MainWindow.cpp, src/ui/Overlay.cpp, src/ui/theme/ThemePalette.{h,cpp}, tests/test_overlay_settings_ui.cpp | token/font tests | green scoped checks |

## 6. Blast radius / risks
- Font sizing must not double-apply DPI (`FontScaleMain` + scoped design px). Risk: HIGH (wrong factor). Mitigation: token tests + build.
- `lwf_tests` has no ImGui linkage — token tests may stay ImGui-free (preferred).

## 7. Open questions
- Q-1: real font approval (deferred).
- Q-2: `FontSizeBase` effective value (investigated; default path).
