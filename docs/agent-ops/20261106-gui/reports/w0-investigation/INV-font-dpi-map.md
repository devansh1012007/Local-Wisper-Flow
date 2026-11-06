<!-- ORCH-DIGEST:START -->
agent: W0-INV-01 | domain: ui-font | status: DONE
items: 0 planned items re-verified | proposed_tasks: 1 (SURFACE 1)
top_risks: obsolete SetWindowFontScale in active use; stale per-window scales
cross_domain_conflicts: none
out_of_plan_discoveries: 0
confidence: high
last_comms_line_read: 2026-11-06T16:00:00Z
<!-- ORCH-DIGEST:END -->

# Investigation report — W0-INV-01 — ui-font (task A2 prep)

## 1. Question and scope
Complete map of font-size / DPI-scale decisions in src/ui for planning task A2: replace obsolete `ImGui::SetWindowFontScale` with scoped `PushFont` / design-px tokens.

## 2. Call-site map
- Full window / `src/ui/MainWindow.cpp`: `SetWindowFontScale(18.0f/kBaseFontPx)` + reset (351/353, wordmark, ≈18 px); `SetWindowFontScale(28.0f/kBaseFontPx)` + reset (432/434, page titles, ≈28 px); `SetWindowFontScale(15.0f/kBaseFontPx)` + reset at card-title sites (542/544, 582/584, 595/597, 607/609, 631/633, 654/656, 681/683, 703/705, 723/725, 750/752, 779/781, 826/828) ≈15 px; `CalcTextSize` at 299, 515; `GetFontSize` at 371; `AddText` at 389. All inside `Begin("##full")/End` — no PushFont/PopFont scope today.
- Overlay / `src/ui/Overlay.cpp`: `AddFontDefaultBitmap(&label_font)` with `SizePixels = metrics::mini_text_px` (99); `CalcTextSize` 431, 522; `AddText(GetFont(), GetFontSize(), …)` 529. No SetWindowFontScale/PushFont in Overlay.
- Theme / `src/ui/theme/`: `ApplyImGuiStyle` sets `style.FontScaleMain = scale` (ThemePalette.cpp:51) and, if scale != 1, `style.ScaleAllSizes(scale)` (114-116); no `FontSizeBase` set; no font reset.
- Tests / `tests/`: only `tests/test_overlay_settings_ui.cpp` touches fonts/theme/metrics (palette hex + WCAG contrast; no metrics, no ApplyImGuiStyle, no ImGui context).

## 3. ImGui font semantics confirmed (pinned v1.92.9b, `_deps-src/imgui/`)
- `PushFont(font, font_size_base_unscaled)`: `PushFont(NULL, size)` keeps font changes size; use unscaled base values (`style.FontSizeBase * factor`), never scaled `GetFontSize()`.
- Final size = `FontSizeBase * (FontScaleMain * FontScaleDpi * window->FontWindowScale (+ legacy))`; `GetFontSize()` returns post-scale; `ScaleAllSizes` does NOT scale fonts.
- `SetWindowFontScale` obsolete (imgui.h:4134; imgui.cpp:552 changelog): prefer `PushFont(NULL, style.FontSizeBase * factor)` or `style.FontScaleMain`.

## 4. Metrics tokens
Only font token today: `metrics::mini_text_px = 8.5f` (ThemePalette.h:113). No `font_wordmark`/`title_px`/`section_px` tokens exist.

## 5. Tests
`lwf_tests` does not link ImGui; no ImGui font execution in tests.

## 6. Open questions / risks
- Effective `FontSizeBase` for Full-window context (default font `LegacySize`).
- Mockup px (14 body/13 helper/13 mini) vs code px (13 base/8.5 mini) mapping.

# REQUIRED OUTPUT SECTIONS (final form)

## Call-site map
(→ section 2 above)

## ImGui font semantics confirmed
(→ section 3 above)

## Metrics tokens
(→ section 4 above)

## Tests
(→ section 5 above)

## Open questions / risks
(→ section 6 above)
