# Brief — W1-IMPL-A2 (implementer) · wave 1 · run 20261106-gui

## 1. Role and mission
Mission: Replace the obsolete Dear ImGui `SetWindowFontScale` font mechanism with the sanctioned scoped font mechanism and express typography sizes as design-px tokens. Task: T-001 (plan item GUI-A2; plan: docs/plans/2026-11-06-gui-fix-improvements.md).
Why this task matters / failure looks like: the pinned ImGui v1.92.9b marks `SetWindowFontScale` obsolete (prefer `PushFont(NULL, style.FontSizeBase * factor)`); design px are currently magic ratios against the built-in font; stale per-window scales can accumulate DPI bugs.

## 2. Required reading (in this order)
1. docs/agent-ops/20261106-gui/templates/STANDING_ORDERS.md (binding)
2. AGENTS.md, docs/UI_AUDIT.md §5.1 (D6), docs/design/mockup.html §5 (type scale)
3. docs/agent-ops/20261106-gui/00-mission.md
4. Plan: docs/plans/2026-11-06-gui-fix-improvements.md — item GUI-A2
5. Investigation reports: docs/agent-ops/20261106-gui/reports/w0-investigation/INV-font-dpi-map.md, INV-tests-font-theme.md
6. Template: docs/agent-ops/20261106-gui/templates/report-implementation.md
7. COMMS.md (last WAVE-START line)
8. Code: src/ui/MainWindow.cpp (font sites ~L39, L154, L247, L265, L299, L330-440, L500-560, L570-840), src/ui/Overlay.cpp (font ~L80-130, L240-260, L400-540), src/ui/theme/ThemePalette.h/.cpp, src/ui/theme/ThemeStyle.h, tests/test_overlay_settings_ui.cpp (L171-219), _deps-src/imgui/imgui.h font API, docs/design/mockup.html §5.

## 3. Task and acceptance criteria
Task T-001 (GUI-A2): scoped fonts / design-px typography tokens; zero obsolete `SetWindowFontScale` in src/ui.

Acceptance criteria (measurable):
- AC1: no `SetWindowFontScale` remains anywhere under src/ui (code + comments).
- AC2: wordmark ≈18 px, page title ≈28 px, card title ≈15 px, body ≈13 px, mini label ≈8.5 px design px, each applied via scoped font mechanism and DPI-scaled exactly once (no double scaling).
- AC3: tests added under tests/ covering the new font tokens/mechanism, in the existing ImGui-free pattern where possible; must fail before the fix and pass after.
- AC4: `cmake --build build --config Release --parallel 4 --target local-wisper-flow` green; `ctest --test-dir build -R lwf-tests` green; `scripts/check_hygiene.sh` green.

Tests that must exist and fail without the fix: new token/mechanism assertions (design-px values) in tests/test_overlay_settings_ui.cpp (or a new ImGui-free header token set + tests).

Commands that must pass (scoped): `cmake --build build --config Release --parallel 4 --target local-wisper-flow`, `ctest --test-dir build -R lwf-tests`, `scripts/check_hygiene.sh`.

## 4. Ownership
WRITE-SET (may edit): src/ui/MainWindow.cpp, src/ui/Overlay.cpp, src/ui/theme/ThemePalette.h, src/ui/theme/ThemePalette.cpp, tests/test_overlay_settings_ui.cpp.
READ-ONLY: all other src/, tests/ (except the listed), docs/, _deps-src/.
FORBIDDEN (hotspots / other agents' files): src/core/DictationEngine.cpp (task B1 later), engine/STT/LLM stores, _deps-src vendored sources (read-only), src/main.cpp UI wiring (unless a font call must be moved there — do not without NEED).

## 5. Parallel siblings this wave
None (wave 1 is single-agent: coder → reviewer → improver). No other writer active.

## 6. Skip list and known traps
- Real TTF font loading: OUT OF SCOPE (font approval pending; audit D6).
- Do NOT feed scaled values (`GetFontSize()`) into `PushFont`; use unscaled base design px.
- Do NOT call `ScaleAllSizes` repeatedly (cumulative `_MainScale`).
- Keep `metrics::` design-px tokens as source of truth; prefer adding `font_*` tokens rather than magic numbers.
- Existing baseline: build green at HEAD 42a7ea0; `ctest -R lwf-tests` 1/1 Passed (17.27 s); tree clean.

## 7. Environment and tests
Scoped commands: `cmake --build build --config Release --parallel 4 --target local-wisper-flow`; `ctest --test-dir build -R lwf-tests`; `scripts/check_hygiene.sh`.
Do NOT run full ctest, zig, full rebuild, container restarts, or touch shared env.

## 8. Deliverables
Report: docs/agent-ops/20261106-gui/reports/w1-impl/W1-IMPL-A2-T-001.md
Commits: code first, then report (docs-only), trailers: Task: T-001, Agent: W1-IMPL-A2, Wave: 1, Report: docs/agent-ops/20261106-gui/reports/w1-impl/W1-IMPL-A2-T-001.md.

## 9. Role-specific notes
- Implementer: root cause only; tests first-class; answer long-run test (Source/Load/Hostile/Operability/Reversal).
- If the proper fix exceeds scope: ESCALATE with analysis; no stopgaps.
