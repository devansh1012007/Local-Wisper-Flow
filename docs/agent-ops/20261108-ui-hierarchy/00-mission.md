# Mission — 20261108-ui-hierarchy

Branch: ops/20261108-ui-hierarchy
BASE_SHA: 8a6ea2255dd8b54d42371987456c5f53e784575f
Date: 2026-11-08

## Scope
Improve hierarchy & density in Full window pages only.
- Keep mockup-exact theme values (Hunter/Brass dark, Ivory/Navy light) — no palette change [FACT src/ui/theme/ThemePalette.h:45-79]
- Keep src/ui/ only — no engine, schema, UiBus, main.cpp changes (except docs/agent-ops)
- Normalize literal spacing/typography to metrics::* / studio_metrics::*; order Primary→Secondary→Helper via structure only
- Proof: before/after screenshots on Windows at 960×640 (default) and 760×520 (min), DPI 1.0 + 1.5; plus source-scan tests
- Out of scope: Mini overlay, Onboarding/Welcome flow logic, palette unification, density toggle, font loading (real TTFs), engine work

## Plan items (from 2026-11-08 discussion)
| ID | Summary |
| P-001 | MainWindow native pages (Home/Settings/Welcome) spacing/typography literals → metrics::* |
| P-002 | Studio pages (Dictionary/Insights/Snippets/Transforms/Scratchpad) spacing/typography → studio_metrics::* |
| P-003 | Widgets density consistency (cards/rows/chips/toggles) |
| P-004 | kRailWidth 220 vs metrics::rail_width 208 drift — default: leave 220 local, document dead token |
| P-005 | Tests — expand source-scan for literal bypasses, fail-before/pass-after |
| P-006 | Screenshots before/after (manual, Windows) |
| P-007 | Gates G1/G2/G4/G5 + hygiene/no-network; record in docs/PROGRESS.md |

## Decisions locked (2026-11-08)
- D1 Primary surface = Full window pages
- D2 Main complaint = Hierarchy & density
- D3 Visual direction = Keep tokens
- D4 Scope guardrail = src/ui/ only
- D5 Success signal = Screenshots review
- D-rail default (reversible): leave kRailWidth=220 local, document token drift, do NOT retoken
- D-baseline: capture before screenshots first (cheap learning), then after
- D-ui-skills: frontend-ui-engineering primary, frontend-design guardrail-only; npx ui-skills CLI pending user permission (may fetch package)

## Commands (from AGENTS.md)
Build: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build -j4
Tests: ctest --test-dir build (doctest lwf_tests, no ImGui link)
Hygiene: scripts/check_hygiene.sh ; Privacy: scripts/check_no_network.sh
Windows sources type-check: zig c++ -fsyntax-only -target x86_64-windows-gnu -Isrc <file> (G3 pending — no zig on dev box, record in docs/QUESTIONS.md per AGENTS)
Single test: .\build\lwf_tests.exe --test-case="*name*" (Windows) or ./build/lwf_tests (Linux)

## Hotspots (single-writer)
- src/ui/theme/ThemePalette.h — frozen mockup values; value changes require mockup regen + contrast tables; test-pinned
- src/ui/theme/StudioPalette.h — test-pinned
- tests/test_overlay_settings_ui.cpp — source-scan pins; renames break CI
- src/ui/MainWindow.cpp — shared by all native pages; one writer in Wave 1
- docs/PROGRESS.md — one row per task (orchestrator appends)

## Pre-existing uncommitted changes
None — git status --porcelain clean at BASE_SHA 8a6ea22 [RAN git status --porcelain -> clean]

## Baseline (captured Phase 0, 2026-11-08, HEAD 8a6ea22)
- Build (lwf_tests): [RAN cmake --build build --target lwf_tests --parallel 4 -> Built target lwf_tests, miniaudio, lwf_core]
- Full build (cmake --build build --parallel 4): timed out at 120s on first attempt (no output) — likely slow link, not failure. Retry with longer timeout or target-scoped before Wave 1 gate.
- ctest: [RAN ctest --test-dir build -R lwf-tests -> 1/1 Passed 23.98s]
- Hygiene: [RAN bash scripts/check_hygiene.sh -> hygiene: OK]
- No-network: [RAN bash scripts/check_no_network.sh -> no-network: OK]
- G3 zig: pending (no zig on dev box)
- G5 config_v0: inside suite (suite green); dedicated run pending full gate

## Assumptions
- A-001 [ASSUMPTION] Dev box is Windows MSYS2 MinGW, build dir exists and is incremental-safe (Test-Path build True). Full rebuild may need deps (grpc tarballs) — verify in baseline.
- A-002 [ASSUMPTION] docs/design/mockup.html is source of truth for tokens; not inspected in plan mode — Wave 0 will verify.
- A-003 [ASSUMPTION] Screenshots require Windows GUI run; Linux CI cannot render ImGui. Manual check in docs/MANUAL_TEST_WINDOWS.md section R.
