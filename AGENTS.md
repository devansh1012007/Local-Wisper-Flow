# AGENTS.md

C++17 local voice dictation app (push-to-talk -> Whisper STT -> optional LLM transform -> text injection). Windows-first; builds and self-tests on Linux.

## Build & verify

```sh
# Linux core + tests (fast path; no whisper/llama/grpc needed)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4          # laptop: never use bare --parallel (12 cores swap-thrash)
ctest --test-dir build --output-on-failure
./build/local-wisper-flow        # loads config, prints one line, exits 0

# Linux full provider build (whisper + llama + grpc + onnx, needed for vad-selftest)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLWF_BUILD_FULL=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure

# VAD inference self-test (needs ONNX Runtime + models, see below; requires LWF_BUILD_FULL=ON)
cmake --build build --target vad-selftest && ./build/vad-selftest

# Windows (MSVC; always the full build)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

- `LWF_BUILD_FULL=OFF` (Linux default) builds only `lwf_core` + `lwf_tests` (doctest
  v2.4.12 via FetchContent, ctest) + a config-printing `local-wisper-flow` + `lwf-bench`
  (portable pipeline benchmark, `./build/lwf-bench [runs] [seconds] [workload_units]`,
  numbers go to docs/BENCHMARKS.md). All portable logic lives in `src/core/` and is
  unit-tested here with fakes. `ctest` runs lwf-tests with WORKING_DIRECTORY at the repo
  root (tests read tests/data/config_v0.json via relative paths — running the binary
  from elsewhere fails some cases).
- `LWF_BUILD_FULL=ON` additionally fetches/builds miniaudio, whisper.cpp, llama.cpp
  and gRPC and links the real providers. Windows always builds full.
- Windows-only sources (`src/os/*`, `src/ui/*`, `_WIN32` main) are type-checked with
  `zig c++ -fsyntax-only -target x86_64-windows-gnu -Isrc <file>` where zig exists.
- Gates beyond the build: `scripts/check_hygiene.sh` (comment/OS-header/TODO lint) and
  `scripts/check_no_network.sh` (network APIs only in src/stt/GrpcInferenceProvider.*
  and src/os/WindowsDownloader.*; file-write APIs banned in src/audio|stt|vad|llm and
  most of core). CI (.github/workflows/ci.yml) runs the Linux core job + hygiene +
  privacy + bench smoke, and the Windows full job with cached tarballs.
- Offline dependency tarballs are NOT in git anymore (`_deps-cache/` ignored).
  `scripts/fetch_deps.sh` / `fetch_deps.ps1` rebuild them: llama + grpc tag tarballs,
  six submodule tarballs at pinned SHAs injected into grpc's third_party/, re-tarred as
  grpc-v1.71.0-full.tar.gz (flat ./ layout — verified identical listing to the original
  artifact). Pass the file:// URLs to cmake for offline/full builds.


## Dependencies — non-obvious facts

- FetchContent pins live in `CMakeLists.txt`: miniaudio `0.11.25`, whisper.cpp `v1.9.3`, nlohmann/json `v3.12.0`. `BUILD_SHARED_LIBS=OFF` is force-set globally.
- **llama.cpp (`b10603`) is NOT FetchContent** — it builds via ExternalProject into its own CMake invocation (install prefix `build/llama-install`, jobs capped by `LLAMA_JOBS=4`). Reason: whisper and llama both vendor conflicting `ggml`s; one shared ggml must win. Source comes from a tag tarball (`LLAMA_URL`, override with `-DLLAMA_URL=file://...` for offline).
- **Two-phase configure is required**: phase 1 stubs llama as an IMPORTED target while `llama_ext` builds+installs during the build; **re-run cmake configure afterwards** — phase 2 does `find_package(ggml/llama)` from the install tree and sets `WHISPER_USE_SYSTEM_GGML=ON` so whisper links llama's ggml (single copy). Forgetting phase 2 => link errors (undefined ggml symbols or duplicate definitions).
- **whisper needs a patch**: `patches/whisper-v1.9.3-ggml-alias.patch` guards its unconditional `ggml::ggml` ALIAS so llama's installed package can define that namespace first. Applied by idempotent `patches/apply_vendor_patch.cmake` (-DPATCH_FILE=...); with FETCHCONTENT_SOURCE_DIR overrides the patch step is skipped, so apply it manually to the cached source once.
- **ONNX Runtime is NOT fetched**. It expects prebuilt binaries at `third_party/onnxruntime/{include,lib}` (override with `-DONNXRUNTIME_DIR=<path>`). Missing dir => configure still succeeds; VAD targets are just skipped with a status message.
- **gRPC `v1.71.0` (FetchContent, hard-required)** builds abseil/protobuf/re2/cares/zlib/boringssl from IN-TREE `third_party/` dirs — the GitHub auto-archive omits those submodules and dies in `cmake/cares.cmake`. Recipe: download tag tarball + inject submodule sources at their pinned SHAs (see `.gitmodules` / git tree API) into `third_party/{cares/cares,boringssl-with-bazel,abseil-cpp,protobuf,re2,zlib}`, re-tar, pass via `-DGRPC_URL=file://...` (`_deps-cache/grpc-v1.71.0-full.tar.gz` is that artifact). Also needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (CMake 4 vs old dep minimums), `protobuf_INSTALL=OFF`, `utf8_range_ENABLE_INSTALL=OFF`. Stub generation uses in-tree `protoc` + `grpc_cpp_plugin` targets into `build/generated/`, compiled as `lwf_proto`.
- Config keys (`config.json`): `hotkey_code`, `whisper_model_path`, `vad_threshold` are required; `llm_model_path`, `use_remote` (default false) and `remote_endpoint` (default `localhost:50051`) optional. The full schema with defaults lives in `SettingsStore::BuildSchema()` (src/core/SettingsStore.cpp); keys are additive-only across releases and unknown keys are preserved on save. Data files live under `%APPDATA%\LocalWisperFlow` (LWF_HOME overrides): config.json, dictionary.json, snippets.json, app_profiles.json, history.jsonl, models.json, logs/, models/.
- Streaming STT (E1): the engine commits a committed-samples slice on VAD pauses (>= `streaming_pause_ms` after >= `streaming_min_segment_ms`) into `StreamingDictator` (single worker, strict FIFO, rolling 200-char context passed via the additive `IInferenceProvider::TranscribeWithPrompt`); any segment failure falls back to one-shot full-buffer transcription; cancel discards in-flight work. LocalWhisperProvider merges the dictionary prompt prefix with the rolling context under a 600-char cap and honors `RequestCancel()` via whisper's ggml abort callback (verified against vendored whisper.h v1.9.3).
- Crash handling (E3): `os/CrashHandler` installs SetUnhandledExceptionFilter and writes `crash-<ts>.txt` through the portable `lwf::CrashReporter` (version from `LWF_APP_VERSION` compile definition set from project VERSION; FSM state mirrored from the FSM transition listener into an atomic; log tail via `Log::RecentLinesTry`, a try-lock ring buffer of the last 512 lines). Next launch surfaces a one-time notice and renames the file to `.seen.txt`.
- Resilience (E2): model load failures at startup are caught in main (bad_alloc -> ModelLoadFailed with smaller-model advice; other -> ModelMissing + UnavailableStt placeholder, cleanup auto-disabled without an LLM, VAD failure degrades to no gating); notices surface through EngineBridge after wiring. Mic loss is detected via miniaudio's stopCallback (only while capturing) -> `IAudioCapture::DeviceLost()` -> engine discards with MicLost.

## Architecture map

- `src/core/` pure-virtual interfaces only (`I*Provider.h`, `DictationFSM`, `ConfigManager`, `TransformManager`, `ProviderRouter`). Never include OS headers here; `<windows.h>` must stay confined to `src/os/*.cpp` (GLFW's own header is fine inside `src/ui/*`).
- Layers: `src/audio` (miniaudio capture) -> `src/stt` (whisper.cpp or `GrpcInferenceProvider`) routed by `src/core/ProviderRouter` (remote-first with seamless local fallback; FSM never knows which) -> `src/core/TransformManager` (async LLM via router adapter) -> `src/os` (hotkey hook + SendInput) -> `src/ui` (ImGui dashboard).
- `DictationFSM::CurrentState()` / `MiniaudioCapture::AudioLevel()` are lock-free relaxed atomics written by engine/audio threads and polled by the UI render loop. Do not replace with mutexed getters.
- Main thread owns the GLFW loop (`MainWindow::Run`). Background threads must never touch UI; UI must never call providers directly (future buttons go through a queue consumed by the engine thread).
- UI targets are built ONLY under `if(WIN32)` (headless Linux has no GL/X11 dev pkgs); imgui is fetched via `FetchContent_Populate` because upstream ships no CMakeLists — expect one deprecation warning.
- Remote layer: gRPC `InferenceService` (`proto/inference.proto`) -> Python reference server under `server/` (mock transcriber, optional faster-whisper; `docker-compose up` from repo root). `server/inference.proto` is a COPY of `proto/inference.proto` — keep them in sync.
- `DictationFSM` owns all flow: states IDLE..EMITTING; engine code may only move it via `TriggerEvent`. Mutex is held ONLY for lookup/state swap — side effects run after unlock via `SetTransitionListener`.
- `src/main.cpp` compiles in two modes: `_WIN32` full app (tray, overlay, hotkey tracker wiring, engine thread, UiCommand loop, onboarding); non-Windows = minimal harness (load config, print one line, return 0, <= 15 lines). Keep both branches working when editing main.
- Config keys (`config.json`): `hotkey_code`, `whisper_model_path`, `vad_threshold` are required; `llm_model_path`, `use_remote`, `remote_endpoint` optional. `transforms.json` is an arbitrary-length array; every prompt must contain `{text}`. Phi-4-mini-*reasoning* emits `<think>` blocks — stripped in `LocalLlamaProvider::Process`.

## Conventions & landmines

- **No comments in code** (project owner's rule). Self-explanatory names only.
- Error style: throw `std::runtime_error` at C-API boundaries; async completion callbacks receive an empty string instead of throwing.
- ORT sessions are thread-safe for concurrent Run(), but `SileroVadProvider` keeps recurrent state between calls — it serializes under a mutex; do not remove.
- Silero v5 scores pure tones ~0.2 (not speech): never gate tests on sine->true; use noise==0 + jfk.wav density >=25% like the existing selftest.
- ONNX Runtime 1.29 removed `SessionOptions::DisableTelemetryEvents()` — don't re-add.
- whisper.cpp v1.9.3 API: segment accessors are `whisper_full_n_segments` / `whisper_full_get_segment_text` (no `whisper_n_segments`).
- `.github/workflows/ci.yml` is this repo's CI since F1 (a stale vision_sst Python workflow was replaced). `.github/` is no longer git-ignored.

## Environment quirks (Linux dev box)

- No sudo; no cmake/mingw preinstalled. A user-space cmake works fine (`python3 -m venv` + `pip install cmake`; `/tmp` gets purged mid-session — keep toolchains under `~/.local/share/` or the repo).
- First configure clones whisper.cpp+llama.cpp shallowly and can take 10-20 min on slow networks; subsequent reconfigures are fast. Build dirs: `build/`, plus ad-hoc `build-win*` used for llvm-mingw cross-checks of the WIN32 sources.
