# Ubuntu dependencies (Linux port, plan 2026-11-21)

One-shot setup: `sh scripts/install_ubuntu_deps.sh` (no root needed; it
elevates only for `apt-get`). Everything below is what it installs and why.

## Build tools

| Package | Why |
|---|---|
| `build-essential`, `cmake` (>= 3.16), `ninja-build` | Toolchain; C++17 |
| `git` | FetchContent deps + the offline vendor-patch flow (AGENTS.md) |

## Platform libraries

| Package | Why | Missing at build time |
|---|---|---|
| `libx11-dev` | Global hotkey via `XGrabKey` (X11 sessions) | No X11 hotkey; daemon logs the reason |
| `libxtst-dev` (+ `libxi-dev`, pulled in) | Text injection via the XTest extension | Clipboard-only injection (copy + Ctrl+V) |
| `libncurses-dev` | The settings TUI (`./local-wisper-flow`, no flags) | Banner mode only |
| `libnotify-bin` | `notify-send` toasts (runtime, not build-time) | Toasts degrade to log lines |
| `wl-clipboard`, `xclip` | Clipboard handoff (runtime) | Injection fails with "install wl-clipboard or xclip" |

All X11/ncurses pieces are probed at configure time; a build on a minimal box
succeeds and the daemon reports the reduced capability set at startup
(`capabilities: x11_hotkey=… evdev_hotkey=… xtest_inject=… tui=… toasts=…`).

## Models and runtimes

- **ONNX Runtime** (not fetched by CMake, per AGENTS.md): the script drops the
  prebuilt `linux-x64` release into `third_party/onnxruntime/`. Missing =>
  configure still succeeds; VAD targets are skipped and the daemon runs
  without silence gating.
- **silero_vad.onnx**: fetched into `<data dir>/models/` (`~/.local/share/
  LocalWisperFlow` unless `LWF_HOME` is set). Missing => same graceful
  degradation.
- **whisper model** (e.g. `ggml-base.en.bin`): place next to the binary or in
  `<data dir>/`; point `whisper_model_path` at it in the TUI.

## Session behavior (decision D3/D4)

| Session | Hotkey | Injection |
|---|---|---|
| X11 | `XGrabKey` chords, unprivileged | XTest direct typing; clipboard+Ctrl+V fallback |
| Wayland | evdev opt-in (`hotkey_evdev_enabled=true` + `input` group: `sudo usermod -aG input $USER`, then re-login) | clipboard copy + "press Ctrl+V" toast |
| Headless | none (documented limit) | disabled (fails with a clear error) |

The evdev backend is **opt-in and keylogger-adjacent by construction**: any
reader of `/dev/input` sees all keys. It ships disabled.

## Full build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLWF_BUILD_FULL=ON
cmake --build build -j$(nproc)
ctest --test-dir build
```

`LWF_BUILD_FULL=ON` pulls whisper.cpp + llama.cpp + miniaudio + gRPC via
FetchContent (see AGENTS.md for the offline tarball recipes and the
`patches/whisper-v1.9.3-ggml-alias.patch` vendor patch). Without it the daemon
still runs and exercises the hotkey/injector/notifier/TUI planes with a clear
"built with LWF_BUILD_FULL=OFF" notice.
