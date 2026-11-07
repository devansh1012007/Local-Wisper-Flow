# Mission — 20261107-studio (orchestrator-owned)

## Scope
Execute approved plan `C:\Users\devan\.opencode\plan\2026-11-07-studio-pages-and-settings-gui.md` (P01-P24, A1-A4, C1-C2, decisions D1-D15 + D-OP1..D-OP4).
Workstreams: B studio pages (F0-F9), A settings GUI (all 49+ keys, api_keys.json, pickers, recorders), C OpenRouter (last).
Out of scope: accounts/sync/team/mobile, PNG export, binary assets beyond approved OFL fonts, snapshot-UI retrofit (D-OP1).

## Branch / baseline
- Branch: feature/studio-pages (per plan D1; skill convention ops/<run-id> adapted — plan branch name takes precedence)
- BASE_SHA: 0e655ec5e997915ec5db351ba83e98a690912394 (1d95fb2 merge + docs cherry-pick d7595d1 → 0e655ec)
- Baseline gates at 1d95fb2: CI run 42310536369 both jobs success (windows 128210561461, linux 128210561573); local configure 0, app build 0, ctest lwf-tests 1/1, hygiene OK.
- Commands (AGENTS.md): cmake -S . -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build -j4; ctest --test-dir build; scripts/check_hygiene.sh; scripts/check_no_network.sh. G3 zig: pending (no zig on box).

## Hotspots (single-writer)
scripts/check_no_network.sh (P02/A1/C2 same-commit rule), CMakeLists.txt (every phase adding sources/tests), tests/test_overlay_settings_ui.cpp (source-scan sync P17/P18 + per-page), src/core/SettingsStore.cpp BuildSchema (A4/D-OP4), AGENTS.md (A1 rule amendment w/ P01).

## Assumptions (A-nnn)
- A-001: local MinGW MSYS2 build is representative; MSVC compile errors surface on full CI (as in prior stream).
- A-002: pixel-measured palette and Scratchpad screenshot (R-01) are delivered at F0/P01 in docs/screenshots/source/; until then token table is estimated.
- A-003: IHttpTransport fits OpenRouter (R-02) — verified first task of C1 with fake-transport test.

## Pre-existing uncommitted changes
None (git status clean at preflight).

## Failure rule (R10)
Same gate fails twice → stop, write docs/FAILURES.md, ask Devansh. No band-aids.
