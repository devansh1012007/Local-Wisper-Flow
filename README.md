# Local Wisper Flow

A fully local, Wispr Flow-style push-to-talk dictation app for Windows. Hold a hotkey,
speak, release — your speech is transcribed with Whisper, optionally cleaned up by a
local LLM, and pasted into whatever app has focus. Nothing leaves your machine.

```
hold Right Alt -> speak -> release
      |                |
   microphone      Silero VAD gate (576-sample streaming input, per-capture reset)
      |                |
      v                v
   Whisper.cpp  ->  LLM cleanup (llama.cpp, guarded output)  ->  clipboard paste
                                                                      |
                                                          your previous clipboard
                                                          contents are restored
```

## Features

- Push-to-talk dictation with a configurable hotkey (default Right Alt)
- AltGr-safe hotkey handling: on AltGr layouts (US-International, English India) Right
  Alt + Ctrl passes through to type characters like `₹` instead of being swallowed
- Silero VAD speech gating with head/tail trimming, hands-free auto-stop
  (`hands_free_silence_ms`), and a maximum recording duration
- Whisper transcription with personal dictionary (initial prompt + post-cleanup
  replacement enforcement) and hallucination filtering (`[BLANK_AUDIO]`, `(music)`, ...)
- LLM cleanup through a chat-templated llama.cpp provider with a strict output guard:
  answered questions, preambles, translations, and truncated generations are rejected
  and the raw transcript is used instead
- Clipboard injection that saves and restores **all** clipboard formats (text, images,
  file lists) and is excluded from clipboard history and cloud sync
- Warm capture with a 300 ms pre-roll ring: the first words are not clipped
- Live dashboard window with state, audio level, and error surfacing; the engine
  survives mic failures and recovers automatically

## Requirements

- Windows 10/11 for the app (builds and runs its test suite on Linux)
- CMake >= 3.16, a C++17 compiler (MSVC on Windows)
- Models (not included, see below): Whisper GGML, optional Qwen/Phi GGUF for cleanup,
  Silero VAD ONNX, ONNX Runtime

## Build

```sh
# Linux (runs the portable test suite)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4        # phase 1 builds llama.cpp, link of the app fails - expected
cmake -S . -B build            # phase 2 picks up the installed llama
cmake --build build -j4
ctest --test-dir build --output-on-failure

# Windows (MSVC)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Notes:

- llama.cpp (`b10603`) builds via ExternalProject during phase 1; the two-phase
  configure is required so whisper links llama's ggml (single copy). See AGENTS.md.
- gRPC remote inference is **optional** and off by default (`-DLWF_BUILD_GRPC=ON`).
  The bundled tarballs are not in git anymore; run `sh scripts/fetch_deps.sh` to
  rebuild `_deps-cache/` if you need an offline or remote build.
- ONNX Runtime is expected under `third_party/onnxruntime/{include,lib}`
  (override with `-DONNXRUNTIME_DIR=<path>`). Without it the app builds but runs
  without VAD gating.

## Model setup

Put models next to the executable or under the data directory
(`%APPDATA%\LocalWisperFlow`, or the `LWF_HOME` environment variable):

- `ggml-base.en.bin` or any whisper.cpp GGML model (`whisper_model_path`)
- `silero_vad.onnx` (`vad_model_path`)
- optional LLM GGUF such as `Qwen2.5-1.5B-Instruct Q4_K_M` (`llm_model_path`)

Relative paths in `config.json` resolve against the working directory, the
executable directory, and the data directory (in that order), so launching from a
shortcut works.

## Configuration (`config.json`)

| Key | Default | Meaning |
| --- | --- | --- |
| `hotkey_code` | required | Virtual-key code of the push-to-talk key (165 = Right Alt) |
| `whisper_model_path` | required | Path to the Whisper GGML model |
| `vad_threshold` | required | Silero speech threshold (0.01-0.99) |
| `llm_model_path` | `""` | Optional cleanup LLM (empty disables cleanup) |
| `use_remote` | `false` | Prefer the gRPC remote server (falls back to local) |
| `remote_endpoint` | `localhost:50051` | gRPC server address |
| `cleanup_enabled` | `true` | LLM cleanup toggle |
| `cleanup_min_words` | `4` | Skip cleanup below this many words |
| `cleanup_timeout_ms` | `4000` | Cleanup wall-clock budget (falls back to raw) |
| `min_hold_ms` | `100` | Shorter presses are ignored |
| `hands_free_silence_ms` | `0` | >0 enables auto-stop after this much silence |
| `max_recording_ms` | `30000` | Hard recording cap |
| `llm_context_size` | `2048` | llama context size |
| `llm_max_predict_tokens` | `1024` | Cleanup generation cap (truncated output is rejected) |
| `llm_gpu_layers` | `0` | GPU layers to offload (0 = CPU) |
| `vad_model_path` | `models/silero_vad.onnx` | Silero ONNX model |
| `inject_restore_delay_ms` | `300` | Clipboard restore delay for slow paste targets |
| `whisper_language` | `auto` | ISO code or `auto` |
| `whisper_threads` | `4` | CPU threads |
| `whisper_beam_size` | `1` | >1 switches to beam search |
| `whisper_disable_temp_fallback` | `false` | true kills temperature-fallback latency spikes |
| `avoid_altgr` | `true` | Pass AltGr (Right Alt + Ctrl) through untouched |
| `dictionary_path` | `dictionary.json` | Personal dictionary (words + replacements) |
| `stt_provider` | `local` | `local` keeps Whisper on-device; `gnani` routes through cloud STT |
| `stt_consent_accepted` | `false` | Consent gate for cloud STT; cloud stays off until accepted |
| `stt_language` | `indic-auto` | Gnani language code (`indic-auto` or a comma list) |
| `stt_timeout_ms` | `15000` | Per-request cloud timeout (failures fall back to local) |
| `stt_max_clips` | `40` | Cloud request budget per dictation (30 s clips) |

Out-of-range values are clamped with a warning; wrong-typed optional keys fall back
to their defaults instead of crashing.

`dictionary.json`:

```json
{
  "words": ["Wispr", "Qwen"],
  "replacements": [{"from": "teh", "to": "the"}]
}
```

Words feed Whisper's initial prompt (capped at 600 characters, oldest dropped first);
replacements are enforced after cleanup as whole-word, case-insensitive substitutions.
A bare `["word", ...]` array is still accepted.

## Troubleshooting

- **Microphone blocked**: Windows privacy settings can deny mic access; the app
  reports `MicUnavailable` and retries on the next press.
- **Hotkey does nothing in elevated apps**: a non-elevated app cannot inject input
  into elevated windows.
- **Transcription is slow**: use a smaller Whisper model, more threads, or enable
  `llm_gpu_layers`; consider `whisper_disable_temp_fallback: true`.
- **Wrong characters when using Right Alt on non-US layouts**: keep `avoid_altgr`
  enabled (default) so AltGr combinations reach the app.
- **Paste happens before the target reads the clipboard**: raise
  `inject_restore_delay_ms` (Word/Electron/remote desktop can be slow).

## Privacy

Everything runs locally. The remote gRPC mode is opt-in (`use_remote`), off by
default, and its reference server binds to loopback. No telemetry of any kind. The
dictated audio is never written to disk.

## License notes

Model weights are subject to their own licenses (Whisper: MIT; Silero VAD: MIT;
Qwen/Phi GGUFs: check the model card). The app code follows the repository license.
