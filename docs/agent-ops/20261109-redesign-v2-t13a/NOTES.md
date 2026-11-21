# T13a capture notes (2026-11-09)

Fixture: `build/t13a-lwf-home` (isolated `LWF_HOME`, copied from the T7
fixture; `whisper_model_path=ggml-base.en.bin` and `last_page=Settings`
so the app opens on Settings > General). Launch: `build/local-wisper-flow.exe`
with `WorkingDirectory` = repo root. Helper script: `t13a-capture.ps1`.

Capture method note: `PrintWindow` returned blanked frames for the
OpenGL container on this box during T13a (the app itself rendered fine,
verified by the log and by the foreground `Screen` crop). All PNGs here
use `Capture-WindowToFile -Method Screen` (CopyFromScreen crop).

Captured (100% DPI, 960x640 and 760x520):
- `t13a_general_960x640.png`, `t13a_general_760x520.png`:
  Settings > General — regression check, renders as expected.
- `t13a_home_960x640.png`: Home top — regression check.

Owed (input blocked in this subagent session; T5-T8 pattern):
- Settings > Speech with the Download button visible (incl. the
  empty-url unavailable state), the Downloading progress bar, and the
  3-part error copy.
- Live download end-to-end (impossible here: `models.json` url is empty
  until Q2; no URL was invented and no real download was attempted).
- Cancel mid-transfer and quit-mid-download (real hardware checks).

Also owed: G3 zig type-check (no zig on this box; recorded in QUESTIONS).
