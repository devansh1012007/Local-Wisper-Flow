# Brief — W2-IMPL-A1A3 · wave 2 · run 20261106-gui

Mission: GUI-A1 (keyboard nav) + GUI-A3 (copy/state polish) in src/ui/MainWindow.{h,cpp} only. Read STANDING_ORDERS, AGENTS.md, mockup §5, and the wave-2 investigation digest (command log + summary above).

AC (measurable):
- NavEnableKeyboard set on Full window IO; rail InvisibleButton + ToggleSwitch pass ImGuiButtonFlags_EnableNav and draw 2px focus ring (kDarkPalette.focus) when focused.
- Page-switch focus restore via SetKeyboardFocusHere gated on IO.NavActive (no mouse-focus stealing); re-show refocus.
- Copy button shows "Copied" for saved_dwell after click; copied_until_ reset on SetLastTranscript.
- saved_dwell 1.6->1.5; Welcome saves route through ApplyAndSave/ShowSaved; sliders commit on IsItemDeactivatedAfterEdit, not per-tick.
- validation_error_ cleared on page switch and on successful edit.
- Trust card branches on stt_provider!=local + consent to cloud-variant copy.
- Build green, hygiene green.

WRITE-SET: src/ui/MainWindow.{h,cpp}. FORBIDDEN: everything else incl. tests/, Overlay.*, ThemePalette.*, src/core/*. Do NOT add tests (sibling owns tests). Verify scoped: cmake build local-wisper-flow + check_hygiene.sh only.

Report: docs/agent-ops/20261106-gui/reports/w2-impl/W2-IMPL-A1A3-T-002.md + commits (code then docs, trailers Task: T-002/T-003, Agent: W2-IMPL-A1A3, Wave: 2).
