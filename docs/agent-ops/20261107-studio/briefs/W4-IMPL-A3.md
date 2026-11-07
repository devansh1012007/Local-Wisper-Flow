# Brief — W4-IMPL-A3 (implementer) · wave 4 · run 20261107-studio

## 1. Mission
In-app hotkey recorders (plan A3/T-024): reuse the existing WindowsHotkeyListener learn mode (WH_KEYBOARD_LL hook → HotkeyBinding::Serialize → StartLearnHotkey → hotkey_ptt today) for ALL 5 config hotkeys (ptt, handsfree, cancel, command, paste_last — D10) + studio shortcuts (transform slots 1-10 recorder, Win+Alt+O diff viewer, Win+Alt+S scratchpad). Requires a new target-key parameter (W0 finding: recorder today only targets hotkey_ptt). Root cause: only ptt is recordable, only via onboarding/Home.
Role: implementer.

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; windows.h ONLY in src/os/ — hygiene; never break dictation hotkey behavior (R6 exception: hotkey CHANGE is user-originated, Wave 2 already respects opt-in; conflicts must disable + warn); ADDING a hotkey may never steal dictation's own capture)
3. 00-mission.md, 01-work-ledger.md; plan §§E(A3),H (T-conflict),I; D10.
4. INV-ui-shell (learn wiring: src/os/WindowsHotkeyListener.cpp:157-174, main.cpp:559-585, StartLearnHotkey, Home chips :623-634, HotkeyBinding::LoadBindings :177-222) and INV-datastore (HotkeyBinding registry shape).
5. Read src/os/WindowsHotkeyListener.{h,cpp}, src/core/HotkeyBinding.{h,cpp}, src/main.cpp learn-mode section fully before coding.
6. Wave 2 E2 report (TransformStore shortcut model + conflict-warning field) and Wave 3 S1 report (General tab layout — recorder controls belong there + Transform/Scratchpad pages' own recorders in Wave 4 pages (transforms edit by page, not here)).

## 3. Acceptance (T-024)
- Generalized recorder: LearnHotkey(target_key, current_chord) → chord string out (extend UiBus for target payload — main.cpp consumer updates in YOUR pass if Armed? main.cpp is not in your write-set — post NEED with exact lines for integration; your recorder must work through the extended UiCommandType).
- All 5 config hotkeys recordable from Settings > General (rows with Keycap + record/clear); studio 3 categories recordable where owned (transform slots: transform page recorder consumes TransformStore; your General tab may host diff/scratchpad recorders; avoid duplicating page recorders — host per-page, share the mechanism).
- Conflict check: every recorder checks the full HotkeyBinding registry (config + studio); conflict disables the shortcut + warns (exact plan copy).
- Modifier-required: chord without a modifier rejected.
- Tests: conflict detection (ptt vs custom shortcut), modifier requirement, Serialize/LoadBindings round-trip for all 5 keys, registry-wide uniqueness, dictation behavior unchanged (engine tests); manual checklist listed in report (record+persist+survive restart; conflict disables).

## 4. Ownership
WRITE-SET: src/os/WindowsHotkeyListener.{h,cpp}, src/core/HotkeyBinding.{h,cpp}, src/core/UiBus.{h,cpp} (target-key additive), src/ui/MainWindow.{h,cpp} (General tab recorder rows ONLY)
READ-ONLY: else. FORBIDDEN: CMakeLists.txt (NEED), scripts/**, src/ui/pages/**, src/core/SettingsStore.* (unless chord format needs it — NEED), docs/agent-ops (report only).
Siblings: A4 also edits MainWindow — CONFLICT RISK: post CONFLICT + declare hunks; Insights parallel. Windows-only (type-checked; never "works").
NEEDs for integration: main.cpp StartLearnHotkey consumer (exact target-key threading), engine wiring if bindings re-load is needed after a General-tab record (ApplyAndSave path), ProcessTransformShortcut registration for studio chords.

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; ctest filtered; bash scripts/check_hygiene.sh. Never full ctest. One build at a time.

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-A3-T-024.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-024 / Agent: W4-IMPL-A3 / Wave: 4. Final <=8 lines.
