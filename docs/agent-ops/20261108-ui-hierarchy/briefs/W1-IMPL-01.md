# Brief — W1-IMPL-01 (IMPLEMENTER) · wave 1 · run 20261108-ui-hierarchy
## 1. Role and mission
Mission: Normalize MainWindow native spacing/typography literals to metrics::* (same-value only, no visual change). Why: P-001 hierarchy/density — every literal is drift; main window contains zero PushFont-digit literals today [VERIFIED W0-INV-03: MainWindow PushFonts tokenized], so effort is geometry literals (40, 19/10/8/4/32/52, 14px Dummy) + ToggleSwitch geometry.
Role: implementer, root-cause only, tests are part of work.

## 2. Required reading (in order)
1. docs/agent-ops/20261108-ui-hierarchy/templates/STANDING-ORDERS.md (binding)
2. AGENTS.md (UI section + Build gates)
3. docs/agent-ops/20261108-ui-hierarchy/00-mission.md (scope, D-rail, hotspots)
4. Plan discussion (2026-11-08): hierarchy & density, keep tokens, src/ui only
5. Investigation: reports/w0-investigation/W0-INV-01.md (arch + invariants) + W0-INV-02.md sections P-001, section 5 T-001, section 9 risks
6. Template: templates/report-implementation.md
7. COMMS.md from last WAVE-START (read before edit)

## 3. Task and acceptance criteria
Task T-001 (P-001): In src/ui/MainWindow.cpp/.h ONLY, replace digit literals in spacing/typography calls with metrics::* same-value tokens (space_xs/s/m/l/xl, radius_*, font_*). Allowlist: 0.0/0.5/1.0/2.0 factors, structural geometry (kRailWidth, item_h=40, ToggleSwitch 52/32) ONLY if no token exists — document each as allowlisted in report. Do NOT change any pixel value. Do NOT touch widgets/ (W1-IMPL-02 owns), tests/ (W1-IMPL-03), ThemePalette (frozen), *Palette.cpp, Apply* call sites.
Acceptance:
- G1 scoped: cmake --build build --target lwf_tests --parallel 4 passes (orchestrator will full-gate)
- ctest -R lwf-tests still passes (token values unchanged)
- Grep: PushFont(nullptr, [0-9] in MainWindow.cpp count = 0 (already 0, keep 0); Dummy/SameLine digit literals with metrics equivalent replaced, remaining documented allowlist
- Screenshots: no pixel delta (values frozen) — note for ORCH gate
Tests that must fail-before: none in MainWindow (already tokenized for fonts); the literal-bypass ban is W1-IMPL-03's job — do not touch tests.

## 4. Ownership
WRITE-SET: src/ui/MainWindow.cpp, src/ui/MainWindow.h
READ-ONLY: everything else
FORBIDDEN: src/ui/widgets/*, src/ui/pages/*, tests/*, src/ui/theme/*, src/ui/Overlay.*, src/ui/ScratchpadWindow.*, any Palette.cpp
Need outside? Post NEED, report DEFERRED-DEPENDENCY.

## 5. Siblings
W1-IMPL-02 pages+widgets, W1-IMPL-03 tests. Disjoint — you cannot see their uncommitted work.

## 6. Skip/traps
- kRailWidth 220 vs metrics::rail_width 208: LEAVE local 220, add comment noting dead token + mission D-rail (do NOT retoken, do NOT change to 208)
- Dual palette: do NOT call ApplyStudioStyle, do NOT recolor — Full window single dark context [VERIFIED W0-INV-02 section 6]
- Live-apply/PersistPage/focus-restore invariants must hold [VERIFIED W0-INV-01: ApplyAndSave, PersistPage last_page]
- Never lighten colors for contrast (WB4 reverted)

## 7. Env
Scoped only: cmake --build build --target lwf_tests, ctest -R lwf-tests, grep counts. Never full suite, container, ports.

## 8. Deliverables
Report: docs/agent-ops/20261108-ui-hierarchy/reports/w1-impl/W1-IMPL-01-T-001.md (ORCH-DIGEST first, template report-implementation.md) — include long-run test, per-file table, grep counts, least-confident line.
Commits: code first, then report docs-only, trailers Task: T-001 Agent: W1-IMPL-01 Wave: 1 Report: <path>
Final ≤8 lines.

## 9. Role notes
Implementer: root cause only, no band-aids. If proper fix needs couple/out-of-scope, ESCALATE.

