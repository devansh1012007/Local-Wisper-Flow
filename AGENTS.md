# AGENTS.md

C++17 local voice dictation app (push-to-talk -> Whisper STT -> optional LLM transform -> text injection). Windows-first; builds and self-tests on Linux.

## Build & verify

```sh
# Linux (also runs FSM + LLM-passthrough smoke tests, exit code = pass/fail)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4          # laptop: never use bare --parallel (12 cores swap-thrash)
./build/local-wisper-flow

# VAD inference self-test (needs ONNX Runtime + models, see below)
cmake --build build --target vad-selftest && ./build/vad-selftest

# Windows (MSVC)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

There is no unit-test framework. `vad-selftest` (target) is the only standalone test; other verification is the smoke tests embedded in the `#else` branch of `src/main.cpp`.

## Dependencies — non-obvious facts

- FetchContent pins live in `CMakeLists.txt`: miniaudio `0.11.25`, whisper.cpp `v1.9.3`, nlohmann/json `v3.12.0`. `BUILD_SHARED_LIBS=OFF` is force-set globally.
- **llama.cpp (`b10603`) is NOT FetchContent** — it builds via ExternalProject into its own CMake invocation (install prefix `build/llama-install`, jobs capped by `LLAMA_JOBS=4`). Reason: whisper and llama both vendor conflicting `ggml`s; one shared ggml must win. Source comes from a tag tarball (`LLAMA_URL`, override with `-DLLAMA_URL=file://...` for offline).
- **Two-phase configure is required**: phase 1 stubs llama as an IMPORTED target while `llama_ext` builds+installs during the build; **re-run cmake configure afterwards** — phase 2 does `find_package(ggml/llama)` from the install tree and sets `WHISPER_USE_SYSTEM_GGML=ON` so whisper links llama's ggml (single copy). Forgetting phase 2 => link errors (undefined ggml symbols or duplicate definitions).
- **whisper needs a patch**: `patches/whisper-v1.9.3-ggml-alias.patch` guards its unconditional `ggml::ggml` ALIAS so llama's installed package can define that namespace first. Applied by idempotent `patches/apply_vendor_patch.cmake` (-DPATCH_FILE=...); with FETCHCONTENT_SOURCE_DIR overrides the patch step is skipped, so apply it manually to the cached source once.
- **ONNX Runtime is NOT fetched**. It expects prebuilt binaries at `third_party/onnxruntime/{include,lib}` (override with `-DONNXRUNTIME_DIR=<path>`). Missing dir => configure still succeeds; VAD targets are just skipped with a status message.
- **gRPC `v1.71.0` (FetchContent, hard-required)** builds abseil/protobuf/re2/cares/zlib/boringssl from IN-TREE `third_party/` dirs — the GitHub auto-archive omits those submodules and dies in `cmake/cares.cmake`. Recipe: download tag tarball + inject submodule sources at their pinned SHAs (see `.gitmodules` / git tree API) into `third_party/{cares/cares,boringssl-with-bazel,abseil-cpp,protobuf,re2,zlib}`, re-tar, pass via `-DGRPC_URL=file://...` (`_deps-cache/grpc-v1.71.0-full.tar.gz` is that artifact). Also needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (CMake 4 vs old dep minimums), `protobuf_INSTALL=OFF`, `utf8_range_ENABLE_INSTALL=OFF`. Stub generation uses in-tree `protoc` + `grpc_cpp_plugin` targets into `build/generated/`, compiled as `lwf_proto`.
- Config keys (`config.json`): `hotkey_code`, `whisper_model_path`, `vad_threshold` are required; `llm_model_path`, `use_remote` (default false) and `remote_endpoint` (default `localhost:50051`) optional.

## Architecture map

- `src/core/` pure-virtual interfaces only (`I*Provider.h`, `DictationFSM`, `ConfigManager`, `TransformManager`, `ProviderRouter`). Never include OS headers here; `<windows.h>` must stay confined to `src/os/*.cpp` (GLFW's own header is fine inside `src/ui/*`).
- Layers: `src/audio` (miniaudio capture) -> `src/stt` (whisper.cpp or `GrpcInferenceProvider`) routed by `src/core/ProviderRouter` (remote-first with seamless local fallback; FSM never knows which) -> `src/core/TransformManager` (async LLM via router adapter) -> `src/os` (hotkey hook + SendInput) -> `src/ui` (ImGui dashboard).
- `DictationFSM::CurrentState()` / `MiniaudioCapture::AudioLevel()` are lock-free relaxed atomics written by engine/audio threads and polled by the UI render loop. Do not replace with mutexed getters.
- Main thread owns the GLFW loop (`MainWindow::Run`). Background threads must never touch UI; UI must never call providers directly (future buttons go through a queue consumed by the engine thread).
- UI targets are built ONLY under `if(WIN32)` (headless Linux has no GL/X11 dev pkgs); imgui is fetched via `FetchContent_Populate` because upstream ships no CMakeLists — expect one deprecation warning.
- Remote layer: gRPC `InferenceService` (`proto/inference.proto`) -> Python reference server under `server/` (mock transcriber, optional faster-whisper; `docker-compose up` from repo root). `server/inference.proto` is a COPY of `proto/inference.proto` — keep them in sync.
- `DictationFSM` owns all flow: states IDLE..EMITTING; engine code may only move it via `TriggerEvent`. Mutex is held ONLY for lookup/state swap — side effects run after unlock via `SetTransitionListener`.
- `src/main.cpp` compiles in two modes: `_WIN32` full app (hotkey loop, stdin commands `t`=Fix Grammar, `q`=quit); non-Windows = smoke-test harness (FSM walk + async-transform test + ProviderRouter fallback test). Keep both branches working when editing main.
- Config keys (`config.json`): `hotkey_code`, `whisper_model_path`, `vad_threshold` are required; `llm_model_path`, `use_remote`, `remote_endpoint` optional. `transforms.json` is an arbitrary-length array; every prompt must contain `{text}`. Phi-4-mini-*reasoning* emits `<think>` blocks — stripped in `LocalLlamaProvider::Process`.

## Conventions & landmines

- **No comments in code** (project owner's rule). Self-explanatory names only.
- Error style: throw `std::runtime_error` at C-API boundaries; async completion callbacks receive an empty string instead of throwing.
- ORT sessions are thread-safe for concurrent Run(), but `SileroVadProvider` keeps recurrent state between calls — it serializes under a mutex; do not remove.
- Silero v5 scores pure tones ~0.2 (not speech): never gate tests on sine->true; use noise==0 + jfk.wav density >=25% like the existing selftest.
- ONNX Runtime 1.29 removed `SessionOptions::DisableTelemetryEvents()` — don't re-add.
- whisper.cpp v1.9.3 API: segment accessors are `whisper_full_n_segments` / `whisper_full_get_segment_text` (no `whisper_n_segments`).
- `.github/workflows/ci.yml` belongs to an unrelated Python project (`vision_sst`) — stale leftover, safe to ignore/delete.

## Environment quirks (Linux dev box)

- No sudo; no cmake/mingw preinstalled. A user-space cmake works fine (`python3 -m venv` + `pip install cmake`; `/tmp` gets purged mid-session — keep toolchains under `~/.local/share/` or the repo).
- First configure clones whisper.cpp+llama.cpp shallowly and can take 10-20 min on slow networks; subsequent reconfigures are fast. Build dirs: `build/`, plus ad-hoc `build-win*` used for llvm-mingw cross-checks of the WIN32 sources.
