# Brief — W1-IMPL-02 (IMPLEMENTER) · wave 1 · run 20261108-ui-hierarchy
## 1. Role and mission
Mission: Normalize Studio pages + widgets literals to studio_metrics::* / metrics::* where same-value exists (P-002+P-003 tranche 1). Why: 37 PushFont-digit + ~30 Dummy-digit literals across 5 pages + 3 widget files [VERIFIED W0-INV-02/03]; tests currently bless them.
Role: implementer, root-cause only.

## 2. Required reading
1. STANDING-ORDERS.md 2. AGENTS.md UI 3. 00-mission.md 4. Plan discussion 5. W0-INV-02.md section 5 (T-002 scope: existing-token sites only) + section 9 risks + W0-INV-03.md section 5 (Writer B scope) 6. report-implementation.md 7. COMMS from WAVE-START

## 3. Task and acceptance criteria
Task T-002 (P-002+P-003 tranche 1): In src/ui/pages/* (Dictionary/Insights/Snippets/Transforms/ScratchpadPage.cpp) + src/ui/widgets/{Buttons,Display,Overlays}.cpp ONLY, replace PushFont/Dummy/SameLine digit literals with same-value tokens:
- 13.0f → studio_metrics::font_helper_px (13) or metrics::font_body_px (13) per file's palette (studio pages use studio, native widgets follow existing palette); 15.0f → font_card_title_px (15) / equivalent
- 12px gap in Dictionary (3 sites) → gap_s? None exists — ALLOWLIST with comment, do NOT map to 16 (visual shift)
- 8px Dummy → space_s (8) / studio 8 gap? Map same-value only; 16px → space_m (16) / page_padding? 16→space_m same-value; 10px → allowlist (no token)
- 24/17 headings (Insights 24, Transforms 24?, Scratchpad 17) → ALLOWLIST until Wave 2 token decision; do NOT map to 28 (visual shift)
- 10px font (Insights) → ALLOWLIST
Do NOT change any pixel value. Do NOT touch StudioPalette.h/ThemePalette.h (additive tokens are Wave 2 T-003), *Palette.cpp, MainWindow, tests, ScratchpadWindow (separate OS window).
Acceptance:
- grep PushFont(nullptr, [0-9] in your files: only allowlisted 24/17/10/12-gap sites remain (target <10, all allowlisted documented); Dummy-digit: only allowlisted remain
- ctest -R lwf-tests still passes (token values unchanged — note: test you run will still bless old literals; W1-IMPL-03 flips bans, so ctest WILL go red after their flip until your tranche lands — expect gate after wave to be green when both land; report scoped ctest result with caveat)
Actually: for your scoped run, note that W1-IMPL-03's new ban will fail on current tree — after your tranche, ban should pass except allowlisted. Report both counts.
- No pixel delta for mapped sites

## 4. Ownership
WRITE-SET: src/ui/pages/DictionaryPage.cpp, InsightsPage.cpp, SnippetsPage.cpp, TransformsPage.cpp, ScratchpadPage.cpp, src/ui/widgets/Buttons.cpp, Display.cpp, Overlays.cpp
READ-ONLY: everything else
FORBIDDEN: MainWindow.*, theme/*, tests/*, ScratchpadWindow.*, Overlay.*, *Palette.cpp

## 5. Siblings
W1-IMPL-01 MainWindow, W1-IMPL-03 tests. Widgets shared with ScratchpadWindow editor (same widgets) — screenshot must include editor window (note in report).

## 6. Skip/traps
- Same-value only [W0-INV-02 risk 1]: 15→13 would shift density silently, no test catches it — forbid
- Dual palette: do NOT call ApplyStudioStyle; do NOT recolor studio pages inside dark Full context [VERIFIED W0-INV-02 section 6, risk 3]
- TransformsPage already tokenized (35/36) — control case, minimal change
- 172px/440px/220px design constants in Transforms/Snippets: leave, unknown intent [W0-INV-02 section 10]

## 7. Env
Scoped: cmake --build build --target lwf_tests, ctest -R lwf-tests (expect red after W1-IMPL-03 flip if they land first — note timing), grep counts.

## 8. Deliverables
Report: reports/w1-impl/W1-IMPL-02-T-002.md, digest first, template report-implementation.md. Long-run test required.
Commits: code then report docs-only, trailers Task: T-002 Agent: W1-IMPL-02 Wave: 1 Report: <path>
Final ≤8 lines.

## 9. If token gap needs new token: do NOT add header change — report DEFERRED-DEPENDENCY for Wave 2 T-003.
