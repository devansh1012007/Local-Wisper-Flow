# Architecture

C++17 local dictation app. All portable logic lives in `src/core/` behind interfaces and
is unit-tested on Linux with fakes; `src/os/` holds thin Windows adapters; `src/ui/` the
GLFW/ImGui windows (built only under WIN32). `<windows.h>` never appears outside
`src/os/*.cpp`.

## Pipeline (fixed order)

```
 hold PTT
    |
    v
 HotkeyTracker (core)  <-- WindowsHotkeyListener (os, WH_KEYBOARD_LL)
    |  EngineEvent{PttPress|PttRelease|Toggle|Cancel|CommandMode|PasteLast|Quit}
    v
 DictationEngine (core, engine thread)
    |  BeginCapture -> MiniaudioCapture (audio thread -> OnAudioChunk)
    |      |  AudioLevel (atomic) -> StatusSnapshot -> overlay
    |      v
    |  SileroVadProvider (speech gating, 512-sample chunks)
    |      |  pause >= streaming_pause_ms after >= streaming_min_segment_ms
    |      v
    |  StreamingDictator (core, worker thread, strict FIFO)
    |      |  TranscribeWithPrompt(pcm, rolling 200-char context)
    |      v
    |  LocalWhisperProvider (stt) ---- GrpcInferenceProvider (optional, use_remote)
    |      |  at release: Finish(tail) or one-shot full-buffer fallback
    |      v
    |  TextPipeline (core, fixed order):
    |      normalize whitespace
    |      -> SpokenCommandProcessor sentinels
    |      -> DictationCleanup (LLM via TransformManager worker, guards, cancel)
    |      -> sentinel count check (discard LLM output on mismatch)
    |      -> sentinel expansion
    |      -> PersonalDictionary enforcement (exact + fuzzy)
    |      -> SnippetStore expansion
    |      v
    v  ITextInjector -> WindowsTextInjector (clipboard save/restore, per-app chord,
       typing fallback, elevated detection)
    |
    v
 HistoryStore (history.jsonl), last_text_ for paste-last
```

Command mode branches before transcription: selection capture (Ctrl+C), instruction
recording, one LLM edit via CommandModeController, validation, paste replaces the
selection, clipboard restored on every branch.

## Threads

| Thread | Owns | Notes |
|--------|------|-------|
| UI (GLFW main) | MainWindow, SettingsWindow, Overlay, wizard | reads StatusSnapshot only; posts UiCommands |
| Engine | DictationEngine, FSM transitions, pipeline, injection calls | consumes EngineEvent queue; Tick() every ~20 ms |
| Audio capture | miniaudio data callback | feeds chunks + level; never blocks on engine |
| STT worker | StreamingDictator | one segment at a time, FIFO |
| LLM worker | TransformManager | single worker + task queue, cooperative cancel |
| Hook | WH_KEYBOARD_LL message pump | tracker only, no allocation, reinstall on session events |
| Tray | message-only window pump | Shell_NotifyIconW, menus, rate-limited toasts |

Shared state is lock-free where it crosses threads: `DictationFSM` exposes an atomic
state snapshot for diagnostics and the UI; `AudioLevel` is a relaxed atomic; the UI
command queue and engine event queue are mutex+condition_variable SPSC-style queues;
`StatusSnapshot` is a seqlock.

## Data layout (`%APPDATA%\LocalWisperFlow`, override with `LWF_HOME`)

```
config.json          settings (SettingsStore: schema, atomic save, .bak, .bad recovery)
dictionary.json      personal words + replacements
snippets.json        text expansions
app_profiles.json    per-app tone profiles and paste chords
history.jsonl        append-only history with retention compaction
models.json          onboarding download catalog (url/sha256, unverified -> amber warning)
logs/                rotating lwf.log (5 x 1 MB) + crash-<ts>.txt reports
models/              silero_vad.onnx, whisper ggml, optional llama gguf
```

## Error and crash paths

Every failure maps to an `ErrorCode` with a user-facing message: engine emits
`IEngineSink::OnError` -> UiBus snapshot -> overlay error state + rate-limited tray
notification + log. Startup notices (missing/OOM models, corrupt config, leftover crash
report) surface through the same bridge. `os/CrashHandler` wraps
`SetUnhandledExceptionFilter` and writes `crash-<ts>.txt` (version, exception code, FSM
diagnostic state, last 200 log lines) via the portable `CrashReporter`.

## Build variants

- Linux default: `lwf_core` + `lwf_tests` (doctest) + `local-wisper-flow` stub + `lwf-bench`.
- `LWF_BUILD_FULL=ON` (forced on Windows): adds miniaudio, whisper.cpp, llama.cpp
  (ExternalProject, two-phase configure, single shared ggml), gRPC (vendored submodule
  sources via `scripts/fetch_deps.*`), ONNX Runtime (preinstalled under
  `third_party/onnxruntime`, optional), VAD selftest.
- CI (`.github/workflows/ci.yml`): Linux core job (build + ctest + hygiene + privacy +
  bench smoke) and Windows full job (cached tarballs, two-phase configure, MSVC, ctest).
