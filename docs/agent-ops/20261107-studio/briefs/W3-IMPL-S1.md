# Brief — W3-IMPL-S1 (implementer) · wave 3 · run 20261107-studio

## 1. Role and mission
Mission: navigation shell restructure (plan P17/F3) — new rail, Settings sub-tabs incl. General (D11), page routing, last-page memory. Root cause: 6-item rail (Home/Speech/Cloud/Cleanup/Privacy/About) must become Home/Insights/Dictionary/Snippets/Transforms/Scratchpad/Settings.
Role: implementer (you OWN MainWindow.* this wave).

## 2. Required reading (in order)
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; never break dictation behavior; tests-only-if-incorrect — the rail source-scan tests become incorrect at this task; UI reads snapshots or live refs per D-OP1: keep live-ref pattern, main-thread only; no BeginPopupModal exists — modal infra is W3-IMPL-S2's job, you leave the seams (page function pointers) for it)
3. docs/agent-ops/20261107-studio/00-mission.md, 01-work-ledger.md
4. Approved plan: §§C,D,E (P17), §H (T-route), §I; spec §3, §5.1-5.5 headers.
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-ui-shell.md (Page enum MainWindow.h:73-85, NavItem[] at MainWindow.cpp:351-434, source-scan REQUIREs exact list, live refs, ApplyAndSave, UiBus no-default-case, last_page key absent).
6. Wave 1/2 reports for context (Overlay note button already in; Settings tabs stand alone).
7. Screenshots: 4 received — rail not visible in any; rail colors per spec estimates (§2 token table), F3 checkpoint is Devansh's.

## 3. Task / acceptance (T-014)
- Page enum: Home, Insights, Dictionary, Snippets, Transforms, Scratchpad, Settings (+Welcome retained for onboarding; Welcome overlays as today). Rail (220px, rail_bg, 1px right border): Home, Insights, Dictionary, Snippets, Transforms, Scratchpad, divider, Settings. Active row = card_bg_white + ink; inactive muted; 40px rows, 8px radius, 15px text.
- Settings sub-page with own tab row: General (D11, NEW) | Speech | Cloud | Cleanup | Privacy | About — existing Draw*Page bodies MOVE behind tabs unchanged (General starts with a subset: keep at minimum the hotkey chips (StartLearnHotkey for ptt), mic/sound toggles, reduce_motion, and window position readout — A4 in Wave 4 completes the all-key surface; your General tab must not regress Home's working controls).
- Remember last opened page across restarts: additive `last_page` string key in SettingsStore::BuildSchema (clamped/sane default Home; invalid → Home fallback); write on every successful nav, read at construction. Do NOT touch ImGui ini (io.IniFilename=nullptr stays).
- Unknown route → Home (round-trip test).
- tests/test_overlay_settings_ui.cpp source-scans: update the rail/page-name REQUIREs in the SAME commit (they encode pre-change structure = incorrect after change; justify in commit msg; extend `font_sources` only with files that exist — pages land Wave 4).
- Draw stub pages for Insights/Dictionary/Snippets/Transforms/Scratchpad: ONE line each with the page title + an implementation TODO-free empty state ("Not available yet." — never placeholder numbers, R3) so the app builds standalone; Wave 4 replaces outlines with real pages (do NOT build pages here).
- UiBus: no new default case remains (assert no silent drop for existing commands — OpenScratchpad/StartTransform already consumed in main.cpp from Wave 2; verify and report).
- Never break dictation (engine/STT/injection) — route only.
Acceptance: route round-trip + unknown→Home tests; updated source-scans green; app builds; hygiene OK; F3 checkpoint note in report (rail colors vs estimated tokens; what Devansh checks).

## 4. Ownership
WRITE-SET: src/ui/MainWindow.{h,cpp} (+ optional src/ui/pages/ stubs? NO — pages are Wave 4: keep stubs as free functions in MainWindow.cpp this wave), src/core/SettingsStore.{h,cpp} (last_page key ONLY), tests/test_overlay_settings_ui.cpp (rail/page scans ONLY)
READ-ONLY: everything else. FORBIDDEN: CMakeLists.txt (NEED if a test file must be registered — the existing scan test file keeps its slot), scripts/**, src/os/**, src/core/DictationEngine.*, src/main.cpp (read only), docs/agent-ops (report only).

## 5. Siblings
W3-IMPL-S2 (StudioPalette + widgets + modal/toast infra — src/ui/theme + src/ui/widgets NEW files, no MainWindow overlap; it depends on your rail for first paint but works on its slice read-only). You cannot see their uncommitted work; coordinate via COMMS CLAIM on the palette tokens you consume.

## 6. Traps
No new key may break config additive-only rule. last_page default Home. Rail focus/keyboard nav (NavEnableKeyboard, 2px ring, NavActive-gated restore) must keep working on the new items — don't regress A1's wave-1 polish. No code comments.

## 7. Environment
Scoped: cmake --build build --config Release --parallel 4 --target local-wisper-flow + lwf_tests; ctest -R lwf-tests filtered to overlay/settings; bash scripts/check_hygiene.sh. Never full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w3-impl/W3-IMPL-S1-T-014.md (ORCH-DIGEST first). Commits explicit paths only; trailers Task: T-014 / Agent: W3-IMPL-S1 / Wave: 3 / Report: <path>. Final <=8 lines.
