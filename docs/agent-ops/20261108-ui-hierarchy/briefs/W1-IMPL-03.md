# Brief — W1-IMPL-03 (IMPLEMENTER) · wave 1 · run 20261108-ui-hierarchy
## 1. Role and mission
Mission: Add literal-bypass bans + flip pinned-literal CHECKs (P-005/T-004). Why: current tests BLESS 37 literals [VERIFIED tests/test_scratchpad_page.cpp:343-344 pins 17/15; test_snippets_page.cpp:315-316]; ban is the only guard against re-drift.
Role: implementer (test author).

## 2. Required reading
1. STANDING-ORDERS.md 2. AGENTS.md gates 3. 00-mission.md 4. Plan discussion 5. W0-INV-03.md section 5 (exact helper + ban design + acceptance A1-A4 + Writer C scope) + W0-INV-02.md section 4 (long-run test design) 6. report-implementation.md 7. COMMS from WAVE-START

## 3. Task and acceptance criteria
Task T-004 (P-005): In tests/test_overlay_settings_ui.cpp, tests/test_scratchpad_page.cpp, tests/test_snippets_page.cpp ONLY:
- Helper ReadSource(path) LWF_SOURCE_DIR-first + relative fallback [VERIFIED tests/test_transforms_page.cpp:30-44 pattern]
- Ban digit literals in layout APIs across MainWindow.cpp, 5 pages .cpp, widgets Buttons/Display/Overlays.cpp: PushFont(nullptr, <digit>, Dummy(ImVec2(0.0f, <digit>, SameLine(0.0f, <digit>, SetNextItemWidth(<digit>, ImVec2(<digit>.0f * scale where digit not in {0,0.5,1,2} and line lacks metrics::/studio_metrics::. Exclude ThemePalette.cpp/StudioPalette.cpp/Overlay.cpp/ScratchpadWindow.cpp (frozen/Mini/separate).
- ALLOWLIST (Wave 1): 24/17/10 font headings + 12px gap sites that W1-IMPL-02 documented as token-gap (must include line refs or value+file allowlist with comment pointing to Wave 2 T-003 — do NOT force zero yet, otherwise T-002 same-value rule violated). PushFont ban must be zero for non-allowlisted files; Dummy ban zero for non-allowlisted.
- Flip pins: test_scratchpad_page 17/15 → require studio_metrics::/metrics::; test_snippets PushFont(nullptr, + ban PushFont(metrics → require studio_metrics::; add metrics::font_ requirement for Dictionary/Insights/Snippets/Transforms (fail-before on literals).
Acceptance (all must hold at WAVE gate, not standalone):
- A1: ctest FAILS on HEAD before T-001/T-002 swaps (fail-before proof), PASSES after wave (allowlist-carved). Report fail-before output.
- A2: three pin flips present, no other TEST_CASE in touched files changed
- A3: hygiene OK, full ctest green after wave
- A4: helper LWF_SOURCE_DIR-first (run exe from repo root AND build/ both green — or document CWD constraint)
Scoped run now: expect RED (by design) until T-001/T-002 land — report RED as intended with evidence, not as BLOCKED.

## 4. Ownership
WRITE-SET: tests/test_overlay_settings_ui.cpp, tests/test_scratchpad_page.cpp, tests/test_snippets_page.cpp
READ-ONLY: everything else
FORBIDDEN: src/ui/*, theme/*, other tests/* (test_dictionary etc read-only)

## 5. Siblings
W1-IMPL-01 MainWindow, W1-IMPL-02 pages+widgets. You cannot see their work. Final gate after wave decides green/red.

## 6. Skip/traps
- Tests are lexical only (no ImGui link) — do NOT add pixel/render tests
- Do NOT weaken existing token/contrast CHECKs
- Relative-ifstream CWD trap: Dictionary/Insights/Snippets use bare paths [VERIFIED W0-INV-03 section 3] — your helper must not worsen it
- Glob vs hardcoded list: prefer glob src/ui/**/*.cpp per W0-INV-02 hostile case, but match existing 27-file idiom today (note as Wave 2 follow-up)

## 7. Env
Scoped: cmake --build build --target lwf_tests, ctest -R lwf-tests (expect RED), grep counts. Never full suite beyond ctest lwf-tests.

## 8. Deliverables
Report: reports/w1-impl/W1-IMPL-03-T-004.md digest first, template report-implementation.md. Include fail-before output, allowlist rationale, long-run test reversal proof.
Commits: code then report docs-only, trailers Task: T-004 Agent: W1-IMPL-03 Wave: 1 Report: <path>
Final ≤8 lines.

## 9. Tests-first honesty: failing-before is success for this task — document it.
