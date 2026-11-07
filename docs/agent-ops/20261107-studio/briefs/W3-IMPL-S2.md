# Brief — W3-IMPL-S2 (implementer) · wave 3 · run 20261107-studio

## 1. Role and mission
Mission: the visual system (plan P18) — StudioPalette light tokens + reusable widget kit (PrimaryButton, LightButton, OutlinePill, TabRow, IconButton, ToggleSwitch restyle, Keycap, BadgeBeta, Chip, RowList, StatCard, RadialGauge, ProgressPill, ActivityGrid, Banner) + modal/sheet/toast infra. Root cause: no light theme applied, no reusable widgets, zero modal infra for the upcoming 5 pages.
Role: implementer.

## 2. Required reading (in order)
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; do NOT change the old palette used by the mini overlay; 3:1/4.5:1 contrast policy; tests/test_overlay_settings_ui.cpp enforces exact tokens — extend carefully; no binary assets (fonts — ImGui default keeps the scale until F0 font files land, one-line swap later))
3. docs/agent-ops/20261107-studio/00-mission.md, 01-work-ledger.md
4. Approved plan: §§C,D,E (P18), §H, §I; spec §2 token table (page_bg #FAFAF8, rail_bg #F4F2EE, card_bg #F4F2EE, card_bg_white #FFFFFF, border #E7E4DE, ink #1C1C1C, muted #7A776F, teal_700 #1E5E5A, teal_500 #2F8077, teal_300 #7DBFB7, teal_100 #CDEBE7, heat_empty #CDC6BB, banner_cream #F1ECE4 — VALUES ARE VISUAL ESTIMATES; F0(a) will pixel-confirm; put exactly these in the palette and mark provisional in the report), typography scale (page title 28 semibold … helper 13 … caps 12 ls 0.08em), shape (radii 12/16/10/10/round/6/8; page padding 32; gaps 24; rows 52; hit 32), banner headline 40 serif (default font — serif is F0-approved but NOT bundled yet — use PushFont(nullptr, 40px) with the default font; note the swap point).
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-ui-shell.md (kLightPalette exists unused; ToggleSwitch file-local at MainWindow.cpp:63-84 — copy semantics, don't move it (S1 owns MainWindow); PushFont scoped with FontScaleMain via ApplyImGuiStyle(kDarkPalette) — StudioPalette must NOT change the applied palette until pages opt in; metrics::space_*/radius_* tokens).
6. Screenshots (4 in conversation): gauge/activity/tab-underline fidelity targets for Insights (P21 uses your widgets Wave 4).

## 3. Task / acceptance (T-015)
- src/ui/theme/StudioPalette.{h,cpp}: the §2 tokens as `StudioPalette` struct (portable, tested — no windows.h, no ImGui dep in the header if separable). Old palette untouched.
- src/ui/widgets/: PrimaryButton, LightButton, OutlinePill (+disabled), TabRow (Active ink + 2px underline, inactive muted, 1px row border; tabs All/Personal/Shared-with-team), IconButton (24px glyph, 32px hit), ToggleSwitch (52x28, ink track on/border off/white knob — restyle semantics from MainWindow's, as a shared implementation), Keycap, BadgeBeta, Chip (18% white glass), RowList (bordered, 12px, 1px separators, 15px text), StatCard, RadialGauge (180° arc, 14px, round caps, teal_700), ProgressPill (28px, label inside fill, badge when narrow), ActivityGrid (7 rows Sun-Sat, week columns, 14px/4px/3px, heat levels by quartile of month max), Banner (16px radius, close X, placeholder gradient).
- Modal/sheet/toast infra (new): modal popup helper (centered, backdrop, Esc), sheet (right-side form sheet), toast (bottom, 5000ms dwell used for undo). No existing infra — build it minimal, ImGui-idiomatic, keyboard-navigable (NavEnableKeyboard respected).
- Contrast: every text pair ≥4.5:1, shapes/borders ≥3:1, per palette tokens — extend tests/test_overlay_settings_ui.cpp token checks to StudioPalette (same-commit update; the existing exact-token tests pin the OLD palette — yours must not break them; add StudioPalette to the tested set with the provisional values).
- Pages import widgets — Wave 4 compiles them. Your widgets must already compile against vendored ImGui 1.92.9b + the repo's ApplyImGuiStyle/PushFont system.
Acceptance: widgets compile + scoped test green for tokens/contrast; ApplyImGuiStyle(kDarkPalette) still the only applied palette (report says so); hygiene OK.

## 4. Ownership
WRITE-SET: src/ui/theme/StudioPalette.{h,cpp}, src/ui/widgets/* (new dir), tests/test_overlay_settings_ui.cpp (StudioPalette token/contrast additions ONLY — coordinate with S1's rail edits via COMMS; if you collide, S1 wins the file this wave and you post NEED)
READ-ONLY: everything else. FORBIDDEN: CMakeLists.txt (NEED for integrator), src/ui/MainWindow.*, src/ui/Overlay.*, scripts/**, docs/agent-ops (report only).

## 5. Siblings
W3-IMPL-S1 owns MainWindow.* + last_page this wave (read-only for you). Do not edit MainWindow to wire your widgets — S1 leaves page stubs Wave 4 will wire.

## 6. Traps
Never apply StudioPalette globally (mini-overlay palette is frozen). Never invent banner marketing copy (original placeholder in Wave 4 pages). No code comments. Snapshot-vs-liveref: pages read main-thread state directly (D-OP1); widgets are pure renderers.

## 7. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; ctest -R lwf-tests filtered; bash scripts/check_hygiene.sh. Never full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w3-impl/W3-IMPL-S003-T-015.md (ORCH-DIGEST first). Commits explicit paths only; trailers Task: T-015 / Agent: W3-IMPL-S2 / Wave: 3 / Report: <path>. Final <=8 lines.
