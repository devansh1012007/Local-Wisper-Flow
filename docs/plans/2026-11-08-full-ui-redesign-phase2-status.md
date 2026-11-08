# Phase 2 Status — Token Unify (2026-11-08)

Branch: `ops/ui-full-redesign`
Commit: `5736431` feat(ui): Phase 2 token unify Ivory primary + tests
Base: `511afee` docs(design): Phase 1 mockup full unify Ivory light primary

## Saved RED/GREEN (per-AC, evidence)
- AC1 Contrast ≥4.5 text incl accent/ok/warn/err/brass/muted/on_accent; ≥3 meaningful non-text; 2 hairlines exempt: GREEN — ctest 1/1 Passed 21.92s (tests/test_overlay_settings_ui.cpp contrast loop); err_fill asserted at ≥3.0 fill-only, never ≥4.5 as text; ThemePalette.h header documents this.
- AC2 Single accent per view (accent==brass==focus 0x7A5C25 Ivory): GREEN — test pins accent/focus 0x1F3A5F→0x7A5C25 + invariant CHECK; StudioPalette shim header maps teal_700/500→accent.
- AC3 Tokens (rail 220, font_body 14, fade_in 0.15, radius_card 10 single): GREEN — ThemePalette.h values + test pins updated; studio radius_card 12→10; studio_metrics::font_helper_px stays 13 documented.
- AC5 Gates/build: GREEN — `cmake --build build --target lwf_tests` Built; `ctest -R lwf-tests` 1/1 Passed 21.92s; `check_hygiene.sh` OK; `check_no_network.sh` OK; G3 zig pending (no zig on dev box).
- Studio de-teal compilation: GREEN — kStudioPalette remapped to Ivory (page_bg #F6F1E4, ink #1C2B26, teal_700/500 #7A5C25, teal_300 #A28C64 ramp-3, teal_100/heat_empty #D9CFBA); HeatColor brass ramp #D9CFBA/#E4DDCD/#C3B599/#A28C64/#7A5C25; pages/widgets still compile via deprecated shim; full migration deferred to Ph3/Ph5.
- Full app exe build / screenshots / MainWindow-Mini migration: RED (not in Phase 2 scope) — attempt #2 angle: Phase 3 migrates Widgets+shared controls to Palette accessor, Phase 4 shell, Phase 5 pages, Phase 7 Windows screenshots 960x640/760x520 DPI 1.0/1.5.

## Build recovery note
Initial `cmake --build` reconfigure failed (build/_deps/grpc-src lacked cares submodule; GRPC_URL pointed at GitHub tarball without submodules). Repaired by reconfigure with cached full tarball: `-DGRPC_URL=file://C:/Users/devan/AppData/Local/Temp/grpc-full.tar.gz` (copy of `_deps-cache/grpc-v1.71.0-full.tar.gz`, file://C: form to avoid %20/`/C:` bug). Configure succeeded (157.3s), target built, ctest green. No source changes needed for this repair.

## What changed
- `src/ui/theme/ThemePalette.h`: Ivory primary comment, kLightPalette accent/focus→brass, rail 220, font_body 14, fade_in 0.15.
- `src/ui/theme/StudioPalette.h`: deprecated shim header, Ivory remap, radius 10, HeatColor brass ramp.
- `tests/test_overlay_settings_ui.cpp`: hex pins, contrast asserts (err_fill 3.0), rail/font/fade/radius pins, studio pins, HeatColor ramp pins.
