# T9 capture notes (2026-11-10)

Fixture: `build/t9-lwf-home` (isolated `LWF_HOME`, copied from the T13a
fixture; `first_run=true`, `last_page=Welcome` so the app auto-opens the
Welcome page at step 1). Launch: `build/local-wisper-flow.exe` with
`WorkingDirectory` = repo root. Helper scripts: `t9-capture.ps1` (Welcome)
and `t9-home-capture.ps1` (Home regression, non-first-run
`build/t9-lwf-home2` with `first_run=false`, `last_page=Home`, opened via
`Request-OpenSettings` tray message `WM_APP+8`).

Capture method note: `PrintWindow` returned blanked frames for the
OpenGL container on this box during T13a; all PNGs here use
`Capture-WindowToFile -Method Screen` (CopyFromScreen crop), same as T13a.

Captured (100% DPI, 960x640 and 760x520):
- `t9_welcome_960x640.png`, `t9_welcome_760x520.png`: Welcome step 1
  (microphone test) — "Four small steps and you are dictating." +
  "Step 1 of 4 - microphone test." + "Speak now; the bar should move." +
  0% meter (quiet, so "Microphone works" is gated off — correct).
- `t9_home_960x640.png`: Home top — regression check, renders as T8
  (Right now, Hotkey, Trust, Quick settings, Last dictation).

Owed (input blocked in this subagent session; T5-T8 pattern):
- Welcome steps 2–4 click-through (hotkey accept, models Browse/Download,
  Finish) and Tab-walk. Step 3's Download button renders disabled
  (inert, `models.json` url empty until Q2 — no URL invented).
- Live download end-to-end (impossible here: `models.json` url is empty
  until Q2), cancel mid-transfer, quit-mid-download (real hardware checks).
- 150%/200% DPI matrix.

Owed (pre-existing, preserved verbatim by T9): with a quiet mic, the
"Open microphone settings" button shares the progress-bar line via
`SameLine` and renders off the right edge — same positions as the
pre-T9 card layout (out of scope for the T9 re-skin).

Also owed: G3 zig type-check (no zig on this box; recorded in QUESTIONS).

Build state for captures: exe built from the T9 commit (base 889414e +
uncommitted T9 MainWindow/test edits); `lwf_tests` green at 565/565 cases,
31966 assertions (new T9 pin case, 64 assertions).
