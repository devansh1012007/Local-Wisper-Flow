# W3-FIX-SCAN — extend font_sources to the 4 Wave 3 TUs

## ORCH-DIGEST
- Status: DONE. `font_sources` in tests/test_overlay_settings_ui.cpp:254-266 extended with the 4 Wave 3 TUs (StudioPalette.cpp, Buttons.cpp, Display.cpp, Overlays.cpp). All 4 verified ban-clean (grep SetWindowFontScale: zero hits) — no laundering needed.
- Gates: build lwf_tests exit 0; ctest -R lwf-tests 1/1 Passed (20.71s); hygiene OK. Diff = exactly 4 added lines.
- Note: S2 NEED listed 7 files (incl. 3 headers); this repair covers the 4 TUs per brief. Headers StudioPalette.h / Widgets.h / Overlays.h remain unscanned — ORCH decision whether to extend.

## Change
- File: tests/test_overlay_settings_ui.cpp, font_sources block only (+4 lines after ThemeStyle.h):
  - src/ui/theme/StudioPalette.cpp
  - src/ui/widgets/Buttons.cpp
  - src/ui/widgets/Display.cpp
  - src/ui/widgets/Overlays.cpp
- The REQUIRE loop (:263 context) now ASSERTs ban-cleanliness per file, including the 4 new entries.
- Not done (out of scope / FORBIDDEN): nothing else touched; COMMS.md and untracked briefs left unstaged.

## Verification (verbatim)
- cmake --build build --config Release --parallel 4 --target lwf_tests -> [100%] Built target lwf_tests, exit 0.
- ctest --test-dir build -R "lwf-tests" --output-on-failure -> 1/1 Test #1: lwf-tests Passed 20.71 sec; 100% tests passed.
- bash scripts/check_hygiene.sh -> hygiene: OK, exit 0.
- grep SetWindowFontScale on each of the 4 new files -> No matches found (all 4).

## Git state
- Branch feature/studio-pages. Commit follows (code: tests/test_overlay_settings_ui.cpp only; report: this file). Trailers Task: T-015 / Agent: W3-FIX-SCAN / Wave: 3.
