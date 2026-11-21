# Ubuntu manual test matrix (Linux port, plan 2026-11-21, change C10)

Automated coverage: `ctest --test-dir build` (keysym tables, subprocess
runner, session detection, clipboard tool choice, notify argv hygiene, TUI
model navigation/editing/clamping, config schema round-trips) plus CI
(linux-ubuntu-24: full build + tests + smoke).

The rows below are the human checklist from the plan (H, T01-T03) — they need
a real desktop session and are NOT claimed as verified by the agent.

| ID | Setup | Steps | Expected | Status |
|----|-------|-------|----------|--------|
| T01 | X11 session, full build, model present | `./build/local-wisper-flow --daemon`; focus gedit; hold RightAlt; speak; release | toast "Listening…"; transcript typed into gedit; history.jsonl entry; log lines under `~/.local/share/LocalWisperFlow/logs/lwf.log` | pending manual |
| T02 | daemon running | `./build/local-wisper-flow`; edit any value; F2 | "saved; daemon reloaded" in the TUI; daemon log shows `config reloaded`; no daemon restart (same pid) | pending manual |
| T03 | Wayland session, wl-clipboard installed | dictate with the evdev hotkey (opt-in, see UBUNTU_DEPS.md) | clipboard holds the transcript; "Press Ctrl+V to paste" toast; paste lands in the focused app | pending manual |
| T04 | X11, another app owns the hotkey (e.g. a second daemon) | start two daemons | second exits with "already running"; a conflicting grabber logs the BadAccess error and the daemon keeps running degraded | agent-verified (Xvfb) |
| T05 | headless (no DISPLAY/WAYLAND_DISPLAY) | `--daemon` | starts, logs "session headless" + no-hotkey reason, stays up, SIGTERM exits 0 | agent-verified |
| T06 | smoke contract | `--smoke` | prints `Local Wisper Flow core build: …` and exits 0 (L0 unchanged) | agent-verified |
| T07 | SIGHUP reload | `kill -HUP <pid>` | log line `config reloaded`; hot-apply keys take effect without restart | agent-verified (Xvfb) |
| T08 | crash marker | `kill -SEGV <pid>` | `crash-<ts>.txt` in the logs dir; next start surfaces "stopped unexpectedly - see logs" once | agent-verified |
