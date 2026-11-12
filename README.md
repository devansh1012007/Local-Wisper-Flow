# Echo flow (repo: Local-Wisper-Flow)

Local-first voice dictation for Windows. Hold a hotkey, speak, and the text
lands in whatever app you are typing in. Whisper speech-to-text and an
optional local LLM run on this machine; a cloud STT provider is available
behind an explicit consent gate (see "Cloud speech-to-text" below).

The app presents itself as "Echo flow" (window title, wordmark, tray); the
repository, data directory and registry identifiers keep the LocalWisperFlow
spelling so existing installs keep their config and history.

- **Press-to-talk dictation** with VAD-gated capture, a mini overlay badge and
  a full window with top-strip navigation (Dear ImGui).
- **Text pipeline**: spoken commands, personal dictionary, snippets, optional
  LLM cleanup, paste-chord handling per app.
- **Streaming mode**: long dictations commit in segments at pauses with a
  rolling context prompt (E1).
- **Resilience and privacy**: model-loss fallbacks, crash reporting, no
  telemetry, offline by default (E2/E3/E4).

## Requirements

- Windows 10/11 (the GUI app) or any Linux for the core build and tests.
- CMake >= 3.16, a C++17 toolchain (MSVC 2022 on Windows for CI; MinGW GCC via
  MSYS2 on the dev box), network access on the first configure for FetchContent
  deps (nlohmann/json, doctest, and - with `LWF_BUILD_FULL=ON` - llama.cpp,
  whisper.cpp, miniaudio, gRPC, GLFW, Dear ImGui).
- Full build extras: whisper `ggml` model, optional Qwen GGUF for cleanup,
  optional ONNX Runtime for the Silero VAD gate.

## Build

Linux core (engine, pipeline, tests, bench - no GUI):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build
```

Windows full app (two-phase configure: the first configure builds llama.cpp
via ExternalProject; re-run configure afterwards so the installed llama/ggml
package is picked up). The dev box builds with the MinGW Makefiles generator
(MSYS2 MinGW); the gRPC FetchContent URL must point at the full, submodule
complete tarball:

```sh
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DGRPC_URL=file://C:/Users/devan/AppData/Local/Temp/grpc-full.tar.gz
cmake --build build -j4
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DGRPC_URL=file://C:/Users/devan/AppData/Local/Temp/grpc-full.tar.gz
cmake --build build -j4
```

If `build/` was previously configured with a different generator (for example the
Visual Studio generator used by CI), delete `build/CMakeCache.txt` and
`build/CMakeFiles` before reconfiguring - CMake refuses to switch generators in
place. If `build/_deps/grpc-src` is stale or partial, clear `build/_deps/grpc-src`,
`build/_deps/grpc-build` and `build/_deps/grpc-subbuild` and re-run the configure
before building.

## Models

| Model | Used for | Where configured |
|---|---|---|
| Whisper `ggml` (e.g. ggml-base.en.bin) | speech-to-text | `whisper_model_path` |
| Qwen2.5 1.5B Instruct GGUF | cleanup LLM | `llm_model_path` |
| silero_vad.onnx | silence gating | data dir `models/` |

Model files are never shipped by the installer; the onboarding wizard
downloads them on first run with a SHA-256 check when the entry carries a
known hash (unverified entries warn in the UI).

## First run

The wizard walks through the hotkey, the whisper model and the cleanup LLM.
Everything can be changed later in the Full window (top strip: Home, History,
Insights, Dictionary, Snippets, Transforms, Scratchpad, Profiles, plus the
Settings tabs General, Speech, Cloud, Cleanup, Privacy, About). Dictation
starts immediately with local Whisper; cloud STT stays off until you opt in.

## Hotkeys (config keys)

| Key | Default | Purpose |
|---|---|---|
| `hotkey_ptt` | (wizard-set) | press-to-talk |
| `hotkey_handsfree` | (empty) | toggle hands-free mode |
| `hotkey_cancel` | `Escape` | cancel the running dictation |
| `hotkey_command` | (empty) | command mode on selected text |
| `hotkey_paste_last` | (empty) | paste the last transcript |

## Settings reference

The full schema with defaults lives in `SettingsStore::BuildSchema()`
(src/core/SettingsStore.cpp); keys are additive-only across releases and
unknown keys are preserved on save. The important rows:

| Key | Default | Meaning |
|---|---|---|
| `hotkey_code` | 165 | default hotkey virtual key |
| `double_tap_window_ms` | 400 | double-tap detection window |
| `min_hold_ms` | 250 | shortest hold treated as dictation |
| `max_recording_ms` | 30000 | hard recording cap |
| `hands_free_silence_ms` | 1500 | silence that ends a hands-free utterance |
| `avoid_altgr_conflict` | true | keep shortcuts off AltGr chords |
| `whisper_model_path` | `ggml-base.en.bin` | local STT model |
| `whisper_language` | `auto` | local whisper language |
| `vad_threshold` | 0.6 | Silero VAD speech threshold |
| `llm_model_path` | (empty) | cleanup LLM model; empty disables cleanup |
| `cleanup_enabled` | true | LLM cleanup of dictated text |
| `cleanup_min_words` | 4 | skip cleanup below this word count |
| `cleanup_timeout_ms` | 2000 | give up on cleanup after this long |
| `spoken_commands_aggressive` | false | rewrite commands inside sentences |
| `snippet_mode` | `whole` | snippet expansion mode |
| `command_timeout_ms` | 8000 | budget for command-mode LLM edits |
| `use_remote` | false | route STT/cleanup to the gRPC endpoint instead of local |
| `remote_endpoint` | `localhost:50051` | gRPC target when `use_remote=true` |
| `stt_provider` | `local` | `local` keeps Whisper on-device; `gnani`, `elevenlabs`, or `muse` route through cloud STT |
| `stt_consent_accepted` | false | consent gate for cloud STT; cloud stays off until accepted (Settings > Cloud) |
| `stt_language` | `indic-auto` | provider-neutral language hint: ISO code or comma list; per-provider mapping in docs/STT_CLOUD.md |
| `stt_timeout_ms` | 15000 | per-request cloud timeout (failures fall back to local) |
| `stt_max_clips` | 40 | cloud request budget per dictation (30 s clips, Gnani/ElevenLabs; Muse uses one request up to 10 min) |
| `inject_delay_ms` | 0 | extra delay before paste for slow apps |
| `streaming_stt` | true | commit long dictations in segments at pauses |
| `streaming_min_segment_ms` | 3000 | minimum audio before a segment may commit |
| `streaming_pause_ms` | 700 | pause length that commits a segment |
| `sound_enabled` | true | tone feedback |
| `sound_volume` | 0.3 | tone volume |
| `history_enabled` | true | keep a local history of transcripts |
| `debug_log_transcripts` | false | WARNING: write dictated text to the local log |

App-specific behavior (paste chord, tone profile) lives in `app_profiles.json` in
the data dir; dictionary and snippets in `dictionary.json` / `snippets.json`; history in
`history.jsonl`.

## Cloud speech-to-text (opt-in)

Dictation stays on-device by default. Three opt-in cloud providers are available behind
Settings > Cloud, each gated by an explicit consent flag plus an API key that is entered
masked in Settings > Cloud (stored in `api_keys.json` next to `config.json`) or provided
via the provider's environment variable - never in config.json, and never written to logs:

| Provider | `stt_provider` | Key environment variable | Contract |
|---|---|---|---|
| Gnani Prisma | `gnani` | `GNANI_API_KEY` | `api.vachana.ai` STT v3 |
| ElevenLabs Scribe | `elevenlabs` | `ELEVENLABS_API_KEY` | `api.elevenlabs.io/v1/speech-to-text` |
| Meta Muse Voice | `muse` | `META_MODEL_API_KEY` | `api.meta.ai/v1/asr/transcribe` |

When a cloud request fails - auth, rate limit, timeout, network, malformed response -
that utterance falls back to local Whisper; local is never upgraded to cloud mid-utterance.
Audio is encoded in memory and never touches disk; logs carry provider, request id,
status, error category, byte counts and latency only. Before trusting a cloud path, run
the S0 smoke check on a machine with a real key (docs/STT_CLOUD.md) - the request shapes
are vendor-verified from documentation but not yet exercised against the live endpoints.

## Troubleshooting

- **Microphone blocked**: if the level meter stays empty, the app may lack mic
  permission, or the device is exclusive-mode capture. Pick another input in
  Settings > Speech and restart.
- **Admin (elevated) apps ignore injection**: UIPI blocks synthetic input into
  higher-integrity windows. Run the app elevated to dictate into elevated apps.
- **Slow STT**: pick a smaller whisper model; keep `cleanup` off on weak
  machines; check `debug_log_transcripts` stays off.
- **AltGr characters**: enable `avoid_altgr_conflict` so the dictation hotkey
  never swallows AltGr chords.
- **Paste chords**: some apps need a custom paste chord - set it per app in
  `app_profiles.json`.

## Privacy

No telemetry. No network calls except explicit model downloads and, if and
only if you enable a cloud provider, that provider's transcription requests
(consent + API key required). Transcripts stay on disk only if history is
enabled; `debug_log_transcripts` writes text to the log and the UI warns when
it is on.

## Model license notes

whisper.cpp models are distributed under their own licenses (check the model
page before bundling); Qwen2.5 GGUF follows the Qwen license; silero_vad.onnx
is MIT. The installer bundles no model weights.
