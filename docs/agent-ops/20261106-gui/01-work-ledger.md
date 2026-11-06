# Work ledger — 20261106-gui

## Plan coverage

| Plan item | Summary | Disposition (task / skip-reason / deferred-reason) |
|---|---|---|
| GUI-A2 | Replace obsolete ImGui::SetWindowFontScale with scoped PushFont / design-px metrics tokens | TASK (wave 1) |
| GUI-A1 | Keyboard navigation for rail/pages | TASK (wave 2) |
| GUI-A3 | Copy/state UX polish | TASK (wave 2) |
| GUI-A5 | Idle cost / render hygiene | TASK (wave 2) |
| GUI-A6 | Mini badge contrast | TASK (wave 2) |
| ENG-B1 | Streaming segment double-commit race | TASK (wave 3) |
| ENG-B2 | G3 zig type-check of Windows sources | DEFERRED (needs zig) |
| CI-C1 | CI watch | TASK (wave 3) |

## Tasks

| Task | Plan items | Domain | Class | Root cause (1 line) | Write-set | Contract-set | Env-set | Depends on | Conflicts (C#) | Wave | Agent | Status | Code commits | Report |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| T-001 | GUI-A2 | ui | SURFACE | Obsolete global-per-window font scale via SetWindowFontScale; design-px sizes expressed as ratios to built-in font | src/ui/MainWindow.cpp, src/ui/Overlay.cpp, src/ui/theme/ThemePalette.{h,cpp}, tests/test_overlay_settings_ui.cpp | none (internal) | lwf_tests | — | — | 1 | W1-IMPL-A2 / W1-IMPR-A2 / W1-IMPR-A2-followup | DONE (2 review cycles) | b21e40f, e0a3882, fda449e | reports/w1-impl/W1-IMPL-A2-T-001.md |

## Waves

### Wave 1 — Task A2 (GUI typography/DPI)
- WAVE_BASE_SHA: 42a7ea0f7bbc5ae71b84ec9a838d0558dcb303f0
- Agents: W1-INV-01 (done), W1-IMPL-A2 (dispatched), W1-REV-A2 (code), W1-REV-A2T (tests), W1-IMPR-A2
- Ownership file: ownership/W1.tsv
- Pre-mortem: font mechanism swap could double-scale at DPI; mitigated by using design-px unscaled base + FontScaleMain for DPI.
- Status: WAVE-1 DONE. Review: APPROVE-WITH-CHANGES x2 (2+1 MAJOR) + delta APPROVE-WITH-CHANGES (1 MAJOR R-W1-003-001 CLOSED by fda449e/c9a28fb). Gate re-run green. Cycle 2/2 used.

## Deep dives
(none yet)

## Deferred / open items
- ENG-B2 (G3 zig): blocked on tool availability.

## Out-of-plan discoveries
- None yet.

## Temporary mitigations (user-approved)
- none.

## Assumptions → see 00-mission.md
