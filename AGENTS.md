# AGENTS.md

C++17 local voice dictation app (push-to-talk -> Whisper STT -> optional LLM transform -> text injection). Windows-first; builds and self-tests on Linux.

## Build & verify

```sh
# Linux (portable core is covered by the doctest suite; exit code = pass/fail)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLLAMA_URL=file://$PWD/_deps-cache/llama-b10603.tar.gz
cmake --build build -j4          # laptop: never use bare --parallel (12 cores swap-thrash)
ctest --test-dir build --output-on-failure
./build/local-wisper-flow

# VAD inference self-test (needs ONNX Runtime + models, see below)
cmake --build build --target vad-selftest && ./build/vad-selftest

# Windows (MSVC)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The unit-test framework is doctest (pinned `v2.4.12`, FetchContent). `ctest` runs `lwf-tests` (portable core: FSM table, router, TransformManager, DictationCleanup guard, DictationEngine with fakes, VadGate, ConfigManager). `vad-selftest` (target) is the only model-dependent test. Windows-only sources are type-checked with `zig c++ -target x86_64-windows-gnu -fsyntax-only` (ziglang pip package).

## Dependencies — non-obvious facts

- FetchContent pins live in `CMakeLists.txt`: miniaudio `0.11.25`, whisper.cpp `v1.9.3`, nlohmann/json `v3.12.0`. `BUILD_SHARED_LIBS=OFF` is force-set globally.
- **llama.cpp (`b10603`) is NOT FetchContent** — it builds via ExternalProject into its own CMake invocation (install prefix `build/llama-install`, jobs capped by `LLAMA_JOBS=4`). Reason: whisper and llama both vendor conflicting `ggml`s; one shared ggml must win. Source comes from a tag tarball (`LLAMA_URL`, override with `-DLLAMA_URL=file://...` for offline).
- **Two-phase configure is required**: phase 1 stubs llama as an IMPORTED target while `llama_ext` builds+installs during the build; **re-run cmake configure afterwards** — phase 2 does `find_package(ggml/llama)` from the install tree and sets `WHISPER_USE_SYSTEM_GGML=ON` so whisper links llama's ggml (single copy). Forgetting phase 2 => link errors (undefined ggml symbols or duplicate definitions).
- **whisper needs a patch**: `patches/whisper-v1.9.3-ggml-alias.patch` guards its unconditional `ggml::ggml` ALIAS so llama's installed package can define that namespace first. Applied by idempotent `patches/apply_vendor_patch.cmake` (-DPATCH_FILE=...); with FETCHCONTENT_SOURCE_DIR overrides the patch step is skipped, so apply it manually to the cached source once.
- **ONNX Runtime is NOT fetched**. It expects prebuilt binaries at `third_party/onnxruntime/{include,lib}` (override with `-DONNXRUNTIME_DIR=<path>`). Missing dir => configure still succeeds; VAD targets are just skipped with a status message.
- **gRPC `v1.71.0` is optional** (`-DLWF_BUILD_GRPC=ON`, default OFF — it is a very long build for a feature that is off by default). When OFF, `GrpcInferenceProvider.cpp` is not compiled and `main.cpp` stays local even if `use_remote=true` (warning printed). When ON it builds abseil/protobuf/re2/cares/zlib/boringssl from IN-TREE `third_party/` dirs — the GitHub auto-archive omits those submodules and dies in `cmake/cares.cmake`. Recipe: download tag tarball + inject submodule sources at their pinned SHAs (see `.gitmodules` / git tree API) into `third_party/{cares/cares,boringssl-with-bazel,abseil-cpp,protobuf,re2,zlib}`, re-tar, pass via `-DGRPC_URL=file://...` (`_deps-cache/grpc-v1.71.0-full.tar.gz` is that artifact). Also needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (CMake 4 vs old dep minimums), `protobuf_INSTALL=OFF`, `utf8_range_ENABLE_INSTALL=OFF`. Stub generation uses in-tree `protoc` + `grpc_cpp_plugin` targets into `build/generated/`, compiled as `lwf_proto`.
- Config keys (`config.json`): `hotkey_code`, `whisper_model_path`, `vad_threshold` are required; `llm_model_path`, `use_remote` (default false) and `remote_endpoint` (default `localhost:50051`) optional.

## Architecture map

- `src/core/` pure-virtual interfaces only (`I*Provider.h`, `DictationFSM`, `ConfigManager`, `TransformManager`, `ProviderRouter`) plus the portable engine layer (`DictationEngine`, `DictationCleanup`, `VadGate`, `EngineEvent`, `AudioPipe`). Never include OS headers here; `<windows.h>` must stay confined to `src/os/*.cpp` (GLFW's own header is fine inside `src/ui/*`).
- `DictationEngine` consumes `EngineEvent`s (PttPress/PttRelease/Cancel/Quit) from a queue, drives the FSM, feeds captured chunks through `VadGate` (512-sample chunks, head/tail speech-interval trimming), runs STT, then `DictationCleanup` (LLM with output guard; never throws), then the injector. Mic start/stop failures are caught: the FSM returns to IDLE and the engine reports a typed `EngineErrorCode` through `IEngineSink` — the engine thread never dies on mic errors.
- `DictationCleanup` runs the LLM on the `TransformManager` single worker and waits with a timeout; on timeout it calls `ILlmProvider::RequestCancel()` (cooperative cancel via `llama_set_abort_callback`). Its output guard rejects answered questions, preambles, translations/script flips, overlong hallucinations, and truncated generations (`ILlmProvider::LastGenerationTruncated`), always falling back to the raw transcript.
- Layers: `src/audio` (miniaudio capture) -> `src/stt` (whisper.cpp or `GrpcInferenceProvider`) routed by `src/core/ProviderRouter` (remote-first with seamless local fallback; FSM never knows which) -> `src/core/TransformManager` (single worker thread + task queue, joined in destructor) -> `src/os` (hotkey hook + SendInput) -> `src/ui` (ImGui dashboard).
- `DictationFSM::CurrentState()` / `MiniaudioCapture::AudioLevel()` are lock-free relaxed atomics written by engine/audio threads and polled by the UI render loop. Do not replace with mutexed getters.
- Main thread owns the GLFW loop (`MainWindow::Run`). Background threads must never touch UI; UI must never call providers directly (future buttons go through a queue consumed by the engine thread).
- UI targets are built ONLY under `if(WIN32)` (headless Linux has no GL/X11 dev pkgs); imgui is fetched via `FetchContent_Populate` because upstream ships no CMakeLists — expect one deprecation warning.
- Remote layer: gRPC `InferenceService` (`proto/inference.proto`) -> Python reference server under `server/` (optional faster-whisper; `docker-compose up` from repo root). The server binds 127.0.0.1 by default (`LWF_BIND`/`LWF_PORT` env) and raises gRPC UNAVAILABLE instead of returning placeholder text, so the client's ProviderRouter falls back to local. Both sides accept messages up to 256 MB (float32 PCM at 16 kHz is 64 KB/s, so the old 4 MB default rejected recordings over about 65 s). `server/inference.proto` is a COPY of `proto/inference.proto` — keep them in sync. The C++ client creates its channel lazily and only when `use_remote=true`.
- `DictationFSM` owns all flow: states IDLE..EMITTING; engine code may only move it via `TriggerEvent`. Mutex is held ONLY for lookup/state swap — side effects run after unlock via `SetTransitionListener`.
- `src/main.cpp` compiles in two modes: `_WIN32` full app (engine thread + hotkey + GLFW dashboard; the UI loop exits if the engine thread dies so the app can never hang showing CAPTURING); non-Windows = config load + one line (real tests live in `tests/`). Keep both branches working when editing main.
- `_deps-cache/` tarballs are NOT committed anymore (they were 179 MB of blobs). `scripts/fetch_deps.sh` rebuilds them from the pinned tag/SHA recipe; `scripts/check_hygiene.sh` enforces the no-comments/no-stray-windows.h rules and runs in CI. CI (`.github/workflows/ci.yml`) does a two-phase Linux build + ctest and a Windows MSVC build + ctest; gRPC stays OFF in CI.
- Config keys (`config.json`): `hotkey_code`, `whisper_model_path`, `vad_threshold` are required; optional keys (`llm_model_path`, `use_remote`, `remote_endpoint`, `cleanup_enabled`, `cleanup_min_words`, `cleanup_timeout_ms`, `min_hold_ms`, `hands_free_silence_ms`, `max_recording_ms`, `llm_context_size`, `llm_max_predict_tokens`, `llm_gpu_layers`, `vad_model_path`) are additive with defaults and type-tolerant (wrong type -> warning + default, never a crash). `transforms.json` is an arbitrary-length array; every prompt must contain `{text}`; a missing file is tolerated with a warning. Phi-4-mini-*reasoning* emits `<think>` blocks — stripped in `LocalLlamaProvider::Process`.

## Conventions & landmines

- **No comments in code** (project owner's rule). Self-explanatory names only.
- Error style: throw `std::runtime_error` at C-API boundaries; async completion callbacks receive an empty string instead of throwing.
- ORT sessions are thread-safe for concurrent Run(), but `SileroVadProvider` keeps recurrent state between calls — it serializes under a mutex; do not remove.
- Silero v5 scores pure tones ~0.2 (not speech): never gate tests on sine->true; use noise==0 + jfk.wav density >=25% like the existing selftest. The model input is **[1, 576]** = 64-sample left context + the 512-sample chunk (official `utils_vad.py` semantics: `context` starts zeroed and is updated to the last 64 samples of each chunk); `Reset()` zeroes both LSTM state and context and must be called at the start of every capture.
- ONNX Runtime 1.29 removed `SessionOptions::DisableTelemetryEvents()` — don't re-add.
- whisper.cpp v1.9.3 API: segment accessors are `whisper_full_n_segments` / `whisper_full_get_segment_text` (no `whisper_n_segments`).
- `.github/workflows/ci.yml` belongs to an unrelated Python project (`vision_sst`) — stale leftover, safe to ignore/delete.

## Environment quirks (Linux dev box)

- No sudo; no cmake/mingw preinstalled. A user-space cmake works fine (`python3 -m venv` + `pip install cmake`; `/tmp` gets purged mid-session — keep toolchains under `~/.local/share/` or the repo).
- First configure clones whisper.cpp+llama.cpp shallowly and can take 10-20 min on slow networks; subsequent reconfigures are fast. Build dirs: `build/`, plus ad-hoc `build-win*` used for llvm-mingw cross-checks of the WIN32 sources.
