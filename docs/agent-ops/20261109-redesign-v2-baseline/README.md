# Baseline screenshots — pre-redesign (Task 0.5)

Pre-redesign baseline for the UI redesign v2 (Hairline) on branch
`ops/ui-redesign-v2`. Captured **before any token edit**, at 100% OS scale.

| Fact | Value |
|---|---|
| Exe commit | `20be267` ("current HEAD" at capture time) |
| Exe | `build\local-wisper-flow.exe` (MinGW, single-config) |
| Build | `cmake --build build --target local-wisper-flow --parallel 4` → `Built target local-wisper-flow` |
| Generator | MinGW (CMake; `windres` GNU Binutils 2.45) |
| OS | Microsoft Windows 11 Home Single Language, version 26H2, build 26300, 64-bit |
| Display scale | 100% (DPI 96; captured with `GetDpiForSystem` = 96) |
| Capture helper | `scripts/capture_baseline.ps1` (P/Invoke: `EnumWindows`, `GetWindowRect`, `MoveWindow`, `SetForegroundWindow`, `PrintWindow` with `CopyFromScreen` foreground fallback) |
| Isolation | One fresh `LWF_HOME` per capture under the temp dir (`lwf-base-…`); `%APPDATA%\LocalWisperFlow` and the repo-root `config.json` were never touched. App killed between captures (single-instance). Launch configs are saved in `cfg/` (copied **before** launch; the app rewrites `config.json` during the run, e.g. `PersistPage`). Readiness was polled from `$LWF_HOME\logs\lwf.log` (`ready: hold the hotkey`); Full window shown via `Request-OpenSettings` (WM_APP+8 → `LwfTrayWindow`, see note below); sizes set by `MoveWindow` client-size compensation (verified actual client 960×640 / 760×520 reported by the helper). PNGs are **outer-window** captures (OS title bar included); the client area inside is exactly 960×640 resp. 760×520. Mini PNGs are `CopyFromScreen` crops of the 250×70 Mini window at its default bottom-center anchor (715,964 on this machine); the dot area outside is desktop showing through the transparent framebuffer. |

## Captured (18 PNGs)

`full_<page>_960x640.png` and `full_<page>_760x520.png` for page =
`welcome` (first_run=true config), `home`, `settings` (General tab — the
default `SettingsTab`), `insights`, `dictionary`, `snippets`, `transforms`,
`scratchpad` — 16 PNGs, each with its launch config in `cfg/full_<page>.json`.

Mini (`mini_ready.png`, `mini_paused.png`, each with its launch config in
`cfg/mini_<state>.json`):
- `mini_ready.png` — dictation enabled, idle (dot + "Ready").
- `mini_paused.png` — `dictation_enabled=false` (dot + "Paused").

Plus `mini_attention_attempt.png` (see "Failed" below; kept as evidence, it
is **not** an Attention baseline) and its config `cfg/mini_attention.json`
(`whisper_model_path="missing-model.bin"`).

File sizes (bytes) and methods are in the run report table below; all Full
captures used `PrintWindow`, Mini used `CopyFromScreen`.

| File | Bytes | Method |
|---|---|---|
| `full_welcome_960x640.png` | 22917 | PrintWindow |
| `full_welcome_760x520.png` | 20765 | PrintWindow |
| `full_home_960x640.png` | 37520 | PrintWindow |
| `full_home_760x520.png` | 28798 | PrintWindow |
| `full_settings_960x640.png` | 36196 | PrintWindow |
| `full_settings_760x520.png` | 29495 | PrintWindow |
| `full_insights_960x640.png` | 44738 | PrintWindow |
| `full_insights_760x520.png` | 33250 | PrintWindow |
| `full_dictionary_960x640.png` | 32997 | PrintWindow |
| `full_dictionary_760x520.png` | 29812 | PrintWindow |
| `full_snippets_960x640.png` | 27546 | PrintWindow |
| `full_snippets_760x520.png` | 24994 | PrintWindow |
| `full_transforms_960x640.png` | 32922 | PrintWindow |
| `full_transforms_760x520.png` | 30129 | PrintWindow |
| `full_scratchpad_960x640.png` | 36187 | PrintWindow |
| `full_scratchpad_760x520.png` | 31265 | PrintWindow |
| `mini_ready.png` | 1344 | CopyFromScreen |
| `mini_paused.png` | 1418 | CopyFromScreen |

Note: the 19th file, mini_attention_attempt.png, is a failed Attention attempt showing Ready — see below.

## Failed / owed

1. **Mini "Needs attention" — FAILED, kept as `mini_attention_attempt.png`.**
   Missing-model config works (log: `ModelMissing: speech model is missing;
   open Settings > Speech` + tray notify), but the Mini shows **Ready**, not
   Attention. Root cause is app logic, not capture: `main.cpp:788-811` moves
   the one-shot `AttentionBox` message into `overlay.SetState(Attention)` and
   then, on the **next frame**, takes the `else if (…now >= done_flash_until)`
   branch (which is true from the epoch-clean `done_flash_until`) and calls
   `overlay.SetState(Ready/Paused)`. `OverlayModel::VisualFor(Attention)`
   carries a 6000 ms dwell (`src/core/OverlayModel.cpp:25-28`), but the main
   loop overrides it after one frame (~16 ms), so the state is not
   scriptably captureable at poll speed. No `src/` edit was made (out of
   scope). A future fix could hold Attention until its dwell elapses; the
   capture should then be re-done.
2. **Listening / Thinking Mini states — OWED (manual).** Require a human
   hotkey hold (recording / STT). Not scriptable on this HEAD.
3. **150% / 200% DPI matrix — OWED.** This run is 100% only (DPI 96). The
   helper declares per-monitor DPI awareness, but nothing at >100% was
   captured.

## Notes noticed (out of scope, no fix applied)

- **Second-instance show does not work on this HEAD.** The brief's
  "launch the exe again — the 2nd instance signals OpenSettings and exits"
  was tried: the 2nd process does exit 0 (`SingleInstance::SignalOpenSettings`
  sets the `Local\LwfOpenSettingsEvent` event) but nothing waits on that
  event anywhere (`grep` over `src/` finds no `CreateEvent`/`WaitForSingleObject`
  for it), and `WindowsTray` never posts the `kWmOpenSettingsEvent`
  (WM_APP+8) it handles. The Full window consequently stays hidden.
  The working scriptable path is `Request-OpenSettings` in
  `scripts/capture_baseline.ps1` (WM_APP+8 → `LwfTrayWindow`, which
  `WindowsTray::Impl::WindowProc` already maps to `OpenSettings →
  MainWindow::SetVisible(true)`). One transient hang was seen in the
  Home run (a 2nd instance did not exit within 10 s; later runs exited
  promptly); the robust kill loop (wait up to 15 s) covers it.
- **Full-window bottom clipping is real.** Home "Quick settings", Settings
  (hotkey rows), and especially Transforms/Scratchpad headers overflow at
  760×520 (and Home/Try at 960×640): the min-size state truncates cards
  rather than reflowing. Captured as-is (`full_*_760x520.png`).
- **Kill side effects.** `Stop-Process` (TerminateProcess) left no crash
  file in any `$LWF_HOME\logs` (crash file is wired to
  `SetUnhandledExceptionFilter`, not to hard kills); no `.seen.txt` notice
  appeared on next launch. Tray-only quit cannot be scripted.

## Reproduce

```powershell
cmake --build build --target local-wisper-flow --parallel 4
. .\scripts\capture_baseline.ps1
# per page: fresh LWF_HOME + config (see cfg/), launch exe, poll logs\lwf.log
# for "ready: hold the hotkey", then Request-OpenSettings (except Welcome,
# which is first_run=true and shows by itself), settle, Set-WindowClientSize,
# Capture-WindowToFile (PrintWindow for Full, PreferScreen for Mini).
```

### Runbook

Orchestration was external to `scripts/capture_baseline.ps1` (which only captures). The capture used, in order:

1. Build exe (`cmake --build build --target local-wisper-flow --parallel 4`).
2. Per capture: set `$env:LWF_HOME` to the cfg dir, `Start-Process` the exe, poll `$LWF_HOME\logs\lwf.log` for "ready: hold the hotkey" (max ~60s), open the Full window (first_run auto-show or `PostMessage` WM_APP+8 0x8008 to `LwfTrayWindow`), settle ~2s, run helper capture at 960x640 then resize+capture at 760x520, `Stop-Process -Name local-wisper-flow` between captures (wait until process gone before next launch; a crash file may appear next launch — noted).
