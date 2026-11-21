# Ubuntu/Linux decisions (plan 2026-11-21, section G)

Record of the agreed decisions for the Linux/Ubuntu port. Full plan:
`docs/plans/2026-11-21-ubuntu-linux-port-tui.md` (operator-approved scope).

| ID | Decision |
|----|----------|
| D1 | **No GUI windows.** The control plane is an ncurses TUI + `notify-send` toasts (desktop integration, not windows). A GLFW/ImGui port stays out of scope. |
| D2 | **Single binary, two modes:** `--daemon` (background supervisor) and default (interactive TUI); `--smoke` preserves the L0 banner contract. |
| D3 | **Hotkey:** X11 `XGrabKey` primary (unprivileged); Wayland via evdev opt-in (`hotkey_evdev_enabled`, needs the `input` group — keylogger-adjacent, ships disabled); headless = no hotkey, documented limit. |
| D4 | **Injection:** XTest direct typing on X11; clipboard copy + "press Ctrl+V" toast on Wayland. X11 clipboard path synthesizes Ctrl+V via XTest when available. |
| D5 | **VAD:** ONNX Runtime + silero_vad.onnx fetched by `scripts/install_ubuntu_deps.sh`; both optional and degrade to no-silence-gating. |
| D6 | **LLM cleanup:** llama.cpp builds on Linux under `LWF_BUILD_FULL=ON` (same ExternalProject as Windows). |
| D7 | **`--smoke` flag** keeps the xvfb smoke contract (banner + exit 0) after replacing the stub main. |

Deferred (non-goals): `.deb`/AppImage packaging (L8), cloud STT remote
transport on Linux (post-MVP; the router runs the local chain and a cloud
`stt_provider` degrades to local with a logged reason), tray/StatusNotifierItem.

Naming note: this repository builds `local-wisper-flow` and stores data under
`~/.local/share/LocalWisperFlow` (`LWF_HOME` overrides); the plan's "EchoFlow"
name maps onto the same binary.
