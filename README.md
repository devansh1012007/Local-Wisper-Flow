# Local Wisper Flow

Local-first voice dictation for Windows: hold a hotkey, speak, and the recognized text is
typed into whatever app you are using. Whisper speech-to-text, an optional LLM cleanup
pass, personal dictionaries, snippets and spoken commands all run entirely on this
machine. The hotkey loop, the VAD gating and the text pipeline are plain C++17 with no
cloud dependency.

![screenshot placeholder: dashboard window and floating dictation pill](docs/screenshot-placeholder.png)

## Features

- Push-to-talk and hands-free dictation (double-tap the hotkey or a dedicated binding)
- Live waveform pill overlay, tray icon with menu, single-instance, optional autostart
- Speech cleanup by a local LLM: punctuation, capitalization, filler removal — with
  strict output guards and cooperative cancel; short utterances skip the LLM
- Spoken commands (period, comma, new line, new paragraph, scratch that, ...) resolved
  before and after the LLM with sentinel round-trip checking
- Personal dictionary (words + replacements, fuzzy enforcement) and text snippets
- Context-aware tone: different cleanup profiles for chat apps, email, code and notes
- Command mode: select text anywhere, hold the command hotkey, speak an edit instruction
- Streaming STT for long dictations (segments leave at natural pauses, tail joined at
  release) with automatic fallback to one-shot transcription
- History with search, retention and export; paste-last-transcript hotkey
- Audio feedback tones, per-app paste chords, clipboard history exclusion

## Requirements

- Windows 10/11 x64 (built and developed with the full pipeline; see below for Linux)
- A microphone; for onboarding, a whisper ggml model; optionally a llama gguf for cleanup
- Build: CMake >= 3.16, MSVC 2022 (Windows) or a C++17 toolchain (Linux core build)

## Privacy

**Offline by default.** No telemetry, no analytics, no accounts, no auto-update. The only
network code in the app is (1) an optional remote-STT gRPC client that is disabled unless
you explicitly set `use_remote=true`, and (2) model downloads in the first-run wizard that
happen only after you click Download. Dictated audio is kept in memory only; it is never
written to disk. Window titles are used locally for app-profile matching and are never
sent anywhere or logged. Automated enforcement: `scripts/check_no_network.sh` fails the
build if network APIs appear outside the two allowlisted files, and a regression test
asserts that no audio files are produced during dictations.

## Building

Linux (core + tests; no GPU, no providers needed):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/lwf-bench          # optional pipeline benchmark
```

Windows (MSVC x64 Native Tools or the VS generator; always the full build):

```sh
scripts\fetch_deps.ps1          # fetches/builds the offline dependency tarball cache
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DLLAMA_URL=file:///C:/absolute/path/_deps-cache/llama-b10603.tar.gz ^
  -DGRPC_URL=file:///C:/absolute/path/_deps-cache/grpc-v1.71.0-full.tar.gz
cmake --build build --config Release
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DLLAMA_URL=file:///C:/absolute/path/_deps-cache/llama-b10603.tar.gz ^
  -DGRPC_URL=file:///C:/absolute/path/_deps-cache/grpc-v1.71.0-full.tar.gz
cmake --build build --config Release
```

The two configure passes are required: the first build compiles and installs llama.cpp;
the second re-configure switches whisper to that system ggml so a single copy is linked.
`scripts/fetch_deps.sh` (bash) does the same on Unix-like systems. VAD support needs
ONNX Runtime prebuilt under `third_party/onnxruntime/{include,lib}`; without it the
configure succeeds and the VAD targets are skipped (the app then runs without silence
gating).

## Model setup

Place files under the data directory `%APPDATA%\LocalWisperFlow\models\` (or next to the
exe; relative paths in `config.json` resolve against the data dir first, then the exe
dir):

| File | Purpose | Notes |
|------|---------|-------|
| `silero_vad.onnx` | silence gating, streaming segment commits | skip to run without VAD |
| `ggml-base.en.bin` | speech-to-text | any whisper.cpp ggml; `.en` models force `whisper_language=en` |
| `qwen2.5-1.5b-instruct-q4_k_m.gguf` | cleanup / command mode | optional; cleanup auto-disables without it |

The first-run wizard (shown when `first_run=true` or models are missing) walks through
microphone test, hotkey detection and model download; downloads start only after an
explicit click and are SHA-256 verified when a hash is listed in `models.json`.

## First run

1. Start the exe. A single instance runs; a second launch just opens Settings.
2. The onboarding wizard checks the microphone (live level meter), the hotkey
   (press your chord; it is captured and saved) and the models.
3. Hold the push-to-talk key, speak, release: text is typed at the cursor.
4. Settings live in the tray menu; every control writes through the atomic settings
   store; entries marked "restart needed" take effect after relaunch.

## Hotkeys

| Action | Default | Config key |
|--------|---------|------------|
| Push to talk | `Ctrl+Win` (new installs; existing configs keep their value) | `hotkey_ptt` |
| Hands-free toggle | unset | `hotkey_handsfree` |
| Cancel dictation | `Escape` (only while capturing/transcribing) | `hotkey_cancel` |
| Command mode | unset | `hotkey_command` |
| Paste last transcript | unset | `hotkey_paste_last` |
| Double-tap PTT = hands-free | off | `double_tap_handsfree` |

Bindings accept chords like `Ctrl+Win`, `RightAlt`, `F13`, `Ctrl+Shift+Space`. With
`avoid_altgr_conflict=true` (default), Right Alt is not bound when the layout uses AltGr
so international characters keep working. Keys are swallowed only while a binding is
actually matched; injected events are never swallowed.

## Settings reference

All keys live in `%APPDATA%\LocalWisperFlow\config.json`; unknown keys are preserved,
out-of-range values are clamped with a log line, and a corrupt file is moved aside as
`config.json.bad` with defaults loaded.

| Key | Default | Meaning |
|-----|---------|---------|
| `min_hold_ms` | 250 | taps shorter than this are ignored (unless completing a double-tap) |
| `max_recording_ms` | 30000 | hard stop for one dictation |
| `hands_free_silence_ms` | 1500 | silence that ends a hands-free dictation (0 = off) |
| `double_tap_window_ms` | 400 | window for double-tap detection |
| `avoid_altgr_conflict` | true | keep Right Alt free on AltGr layouts |
| `whisper_model_path` | `ggml-base.en.bin` | STT model file |
| `whisper_language` | `auto` | ISO code or `auto` |
| `vad_threshold` | 0.6 | Silero speech probability threshold |
| `llm_model_path` | empty | cleanup/command-mode gguf |
| `cleanup_enabled` | true | LLM cleanup pass on/off |
| `cleanup_min_words` | 4 | utterances shorter than this skip the LLM |
| `cleanup_timeout_ms` | 2000 | cleanup budget; timeout falls back to raw text |
| `cleanup_language_policy` | `keep` | cleanup must keep the input language |
| `spoken_commands_aggressive` | false | convert punctuation phrases even mid-sentence |
| `dictionary_fuzzy` | true | fix single-edit-distance typos against the dictionary |
| `snippet_mode` | `whole` | `whole` utterance or `contains` matching |
| `command_timeout_ms` | 8000 | budget for command-mode LLM edits |
| `use_remote` | false | route STT/cleanup to the gRPC endpoint instead of local |
| `remote_endpoint` | `localhost:50051` | gRPC target when `use_remote=true` |
| `inject_delay_ms` | 0 | extra delay before paste for slow apps |
| `streaming_stt` | true | commit long dictations in segments at pauses |
| `streaming_min_segment_ms` | 3000 | minimum audio before a segment may commit |
| `streaming_pause_ms` | 700 | pause length that triggers a commit |
| `mic_device` | empty | input device (empty = system default) |
| `sound_enabled` | true | start/stop/error tones |
| `sound_volume` | 0.3 | tone volume |
| `reduce_motion` | false | disable overlay animation |
| `history_enabled` | true | keep a local history of dictated text |
| `keep_raw_in_history` | false | also store the pre-cleanup text |
| `history_max_entries` | 1000 | retention cap |
| `history_max_age_days` | 90 | retention window |
| `debug_log_transcripts` | false | write dictated text to the log (warned in the UI) |
| `first_run` | true | show the onboarding wizard |

App-specific behavior (paste chord, tone profile) lives in `app_profiles.json` in the data
dir; dictionary and snippets in `dictionary.json` / `snippets.json`; history in
`history.jsonl`.

## Troubleshooting

- **Microphone blocked**: if the level meter stays empty, open
  `ms-settings:privacy-microphone` and allow desktop apps to use the mic; the onboarding
  step links this directly.
- **Hotkey not working in admin apps**: an unelevated app cannot inject into an elevated
  window (UIPI). The app detects this and reports a clear error instead of failing
  silently; run elevated only if you accept the tradeoff. If the low-level keyboard hook
  dies after a session lock/resume it is reinstalled automatically.
- **Slow transcription**: use a smaller whisper model (`ggml-base.en.bin` instead of
  `ggml-small.bin`), or a CUDA build of whisper.cpp; keep `streaming_stt` on so long
  dictations overlap capture and inference.
- **AltGr layouts**: with Right Alt bound, typing `@`, `\`, `{` and friends breaks. Leave
  `avoid_altgr_conflict=true` or bind a chord that does not use Right Alt.
- **Text appears with wrong quotes/tildes**: some apps intercept `Ctrl+V`; per-app paste
  chords are configurable in `app_profiles.json` (e.g. `shift+insert` for terminals).

## License notes

Code: see repository license. Bundled/reference models follow their own licenses —
whisper ggml models (MIT, OpenAI Whisper), Qwen2.5 Instruct gguf (Apache-2.0), Silero VAD
(MIT). The installer bundles `silero_vad.onnx`; check its license before redistribution.
No model weights are committed to this repository.
