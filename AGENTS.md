# AGENTS.md — rules for agents working on Local Wisper Flow

Local Wisper Flow is local-first voice dictation for Windows (press-to-talk,
VAD gating, whisper STT, optional local LLM cleanup, text injection) with an
opt-in cloud STT path behind a consent gate. Reconstructed rule sheet: the
original was lost with the dev-environment reset; the paragraphs below are
recovered verbatim from the S9 documentation patch where noted.

- Branding (followups-2): the app's user-visible name is "Echo flow" (wordmark,
  window title, tray, About, crash/log headers). Internal identifiers - repo
  name, data dir `%APPDATA%\LocalWisperFlow`, registry run value, window class
  names, HTTP user agent - deliberately keep the LocalWisperFlow spelling so
  existing installs keep their config and data. The installer is not renamed
  yet (user decision).
- Default transforms (followups-2): nothing is seeded. TransformStore starts
  with ten empty slots, no entry is `built_in`, and Load drops legacy
  built_in-flagged entries (the retired Polish / Prompt Engineer) from
  existing transforms.json files. All slots are user slots (Create New fills
  the first empty one, Delete clears any). v1 arrays convert from slot 1.

## Build and gates

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build
```

Local dev box (Windows, MSYS2 MinGW) - the VS generator is not installed here, so the
build dir must be configured with the MinGW Makefiles generator (a VS-era cache, e.g.
`CMAKE_GENERATOR=Visual Studio 18 2026`, makes every build fail with "could not find any
instance of Visual Studio"):

```sh
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DGRPC_URL=file://C:/Users/devan/AppData/Local/Temp/grpc-full.tar.gz
cmake --build build -j4
```

- If `build/CMakeCache.txt` + `build/CMakeFiles` hold a foreign generator, delete both and
  re-run the configure above (CMake refuses to switch generators in place).
- gRPC full-tarball: `-DGRPC_URL=file://C:/Users/devan/AppData/Local/Temp/grpc-full.tar.gz` (the
  re-tarred full tarball, copy of `_deps-cache/grpc-v1.71.0-full.tar.gz`). If
  `build/_deps/grpc-src` is stale/partial, clear `build/_deps/grpc-src`,
  `build/_deps/grpc-build` and `build/_deps/grpc-subbuild`, then re-run configure before
  building.

- `LWF_BUILD_FULL=ON` (forced on Windows) builds whisper/llama/miniaudio
  and gRPC and links the real providers. Windows always builds full.
- Windows-only sources (`src/os/*`, `src/ui/*`, `_WIN32` main) are type-checked with
  `zig c++ -fsyntax-only -target x86_64-windows-gnu -Isrc <file>` where zig exists.
- Gates beyond the build: `scripts/check_hygiene.sh` (comment/OS-header/TODO lint;
  `--self-test` plants offenders and verifies they are caught) and
  `scripts/check_no_network.sh` (network APIs only in src/stt/GrpcInferenceProvider.*,
  src/os/WindowsDownloader.* and src/os/WindowsHttpClient.*; file-write APIs banned in
  src/audio|stt|vad|llm and most of core; cloud endpoint names only in src/stt/cloud/*
  and scripts/gnani_smoke.py). CI (.github/workflows/ci.yml) runs the Linux core job +
  hygiene + privacy, and the Windows full job with cached tarballs.
- CI (ci.yml) — two things that bite:
  - Hygiene runs before the build; any `windows.h` outside src/os/* and the cloud
    HTTP client fails the Linux job (it caught src/ui/Overlay.cpp's work-area query;
    move OS-API use into src/os/WindowStyles).
  - `ctest` runs the doctest suite; `lwf_tests` must compile against the current
    tests/Fakes.h. Keep test call sites in sync with the fakes; the suite is the
    only thing standing between the PR and a red CI.
  - The whisper FetchContent PATCH_COMMAND runs `patches/apply_vendor_patch.cmake`
    via `cmake -P`; MSBuild's custom-build rule flags any line matching `error`
    (even git's expected reverse-check output) and fails with MSB8066. The script
    captures git output instead of letting it reach the log; do not remove that
    capture, and do not let other patch/vendor steps print unchecked git stderr.
  - Windows job uses `windows-2022` (the `windows-2025-vs2026` image no longer ships
    the VS2022 generator).
- Gate ledger: G1 build -j4 zero new warnings, G2 ctest green, G3 zig type-check of
  Windows sources, G4 scripts/check_hygiene.sh, G5 config_v0 backward-compat test.
  The dev box is Windows (MSYS2 MinGW) but has no zig: G3 stays "pending" and is
  recorded per task in docs/QUESTIONS.md.

## Dependencies (offline recipe)

- Offline dependency tarballs are NOT in git anymore (`_deps-cache/` ignored).
  `scripts/fetch_deps.sh` / `fetch_deps.ps1` rebuild them: llama + grpc tag tarballs,
  six submodule tarballs at pinned SHAs injected into grpc's third_party/, re-tarred as
  one artifact, passed via `-DGRPC_URL=file://...` (same pattern for the llama tarball).
- **whisper needs a patch**: `patches/whisper-v1.9.3-ggml-alias.patch` guards its
  unconditional `ggml::ggml` ALIAS so llama's installed package can define that
  namespace first. Applied by idempotent `patches/apply_vendor_patch.cmake`
  (-DPATCH_FILE=...); with FETCHCONTENT_SOURCE_DIR overrides the patch step is skipped,
  so apply it manually to the cached source once.
- **ONNX Runtime is NOT fetched**. It expects prebuilt binaries at
  `third_party/onnxruntime/{include,lib}` (override with `-DONNXRUNTIME_DIR=<path>`).
  Missing dir => configure still succeeds; VAD targets are just skipped with a status
  message.
- **gRPC `v1.71.0` (FetchContent, hard-required)** builds abseil/protobuf/re2/cares/zlib/
  boringssl from IN-TREE `third_party/` dirs — the GitHub auto-archive omits those
  submodules and dies in `cmake/cares.cmake`. Recipe: download tag tarball + inject
  submodule sources at their pinned SHAs into
  `third_party/{cares/cares,boringssl-with-bazel,abseil-cpp,protobuf,re2,zlib}`, re-tar,
  pass via `-DGRPC_URL=file://...` (locally: `-DGRPC_URL=file://C:/Users/devan/AppData/Local/Temp/grpc-full.tar.gz`,
  the re-tarred full tarball copied from `_deps-cache/grpc-v1.71.0-full.tar.gz`). Also needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`
  (CMake 4 vs old dep minimums), `protobuf_INSTALL=OFF`, `utf8_range_ENABLE_INSTALL=OFF`.
  Stub generation uses in-tree `protoc` + `grpc_cpp_plugin` targets into
  `build/generated/`, compiled as `lwf_proto`.

## Config and data

- Config keys (`config.json`): `hotkey_code` (schema key + LoadBindings fallback
  retained; its Settings UI row was removed in T14 — do not re-add a row),
  `whisper_model_path`, `vad_threshold` are required; `llm_model_path`, `use_remote` (default false) and `remote_endpoint`
  (default `localhost:50051`) optional. The full schema with defaults lives in
  `SettingsStore::BuildSchema()` (src/core/SettingsStore.cpp); keys are additive-only
  across releases and unknown keys are preserved on save. Cloud STT is opt-in:
  `stt_provider` (`local` default; `gnani` | `elevenlabs` | `muse`) needs
  `stt_consent_accepted=true` plus the provider's key (store→env: an optional
  `api_keys.json` entry next to `config.json`, never stored in `config.json`,
  redacted and masked in the GUI, falling back to the environment
  `GNANI_API_KEY` / `ELEVENLABS_API_KEY` / `META_MODEL_API_KEY`, single source in
  `src/core/ConsentGate.cpp`), never logs transcripts or keys, and only ever falls back
  cloud -> local (see `docs/STT_CLOUD.md`). The in-app consent surface is Settings >
  Cloud (S5); failed attempts keep a redacted error `detail` (lwf::RedactSecret/
  RedactAuthValues, src/core/Redact.h) in the attempt stats. Data files live under
  `%APPDATA%\LocalWisperFlow` (LWF_HOME overrides): config.json, dictionary.json,
  snippets.json, app_profiles.json, history.jsonl, logs/, models/. The model
  catalog `models.json` is NOT a data-dir file: it is read from the exe
  directory (the T13a/T9 GetExeDir() sites), so keep it next to the exe.
- `whisper_model_path` (same for `llm_model_path`) is resolved by `ResolveModelPath`
  (src/os/DataDir.cpp → `ResolveDataPath`, src/core/PathResolver.cpp): an absolute path
  is used as-is; otherwise it is probed in order: `%APPDATA%\LocalWisperFlow\<path>`,
  the exe directory, the exe directory's **parent** (repo root when the exe lives in
  `build/`), then the current working directory; if all fail, the bare path is handed
  to the loader (whisper logs a clear "failed to open" and the app continues in
  ModelMissing mode). Run the exe from the repo root (where ggml-base.en.bin lives) or
  keep the model next to the exe.
- Streaming STT (E1): the engine commits a committed-samples slice on VAD pauses
  (>= `streaming_pause_ms` after >= `streaming_min_segment_ms`) into `StreamingDictator`
  (single worker, strict FIFO, rolling 200-char context passed via the additive
  `IInferenceProvider::TranscribeWithPrompt`); any segment failure falls back to one-shot
  full-buffer transcription; cancel discards in-flight work. LocalWhisperProvider merges
  the dictionary prompt prefix with the rolling context under a 600-char cap and honors
  `RequestCancel()` via whisper's ggml abort callback (verified against vendored
  whisper.h v1.9.3).

## Cloud STT stack (S0-S8)

`SttRouter` (core) picks cloud vs local via `ConsentGate` and owns the one-way fallback;
providers live in `src/stt/cloud/` behind `ISttProvider` and share
WavEncoder/MultipartBuilder/ClipSplitter (`MultipartBuilder::AddJsonField` renders a
content-typed, filename-less part for Muse's `request` part). Every provider loop aborts
on first failure, honors the cancel check between clips, and redacts
`SttAttemptStats::detail`. `stt_language` is provider-neutral:
`sttlang::ElevenLabsLanguageCode` picks the first valid ISO token,
`sttlang::MuseLanguageBias` maps codes to the vendor's language names (indic-auto -> the
Indic subset of Muse's 25 documented languages). `lwf-cloud-bench` benchmarks the offline
cloud path with a loopback transport (docs/BENCHMARKS.md); contracts were verified from
vendor docs on 2026-11-04, NOT yet against the live APIs.

## Crash handling and resilience

- Crash handling (E3): `os/CrashHandler` installs SetUnhandledExceptionFilter and writes
  `crash-<ts>.txt` through the portable `lwf::CrashReporter` (version from
  `LWF_APP_VERSION` compile definition set from project VERSION; FSM state mirrored from
  the FSM transition listener into an atomic; log tail via the Log ring buffer). Next
  launch surfaces a one-time notice and renames the file to `.seen.txt`.
- Resilience (E2): model load failures at startup are caught in main (bad_alloc ->
  ModelLoadFailed with smaller-model advice; other -> ModelMissing + UnavailableStt
  placeholder, cleanup auto-disabled without an LLM, VAD failure degrades to no gating);
  notices surface through EngineBridge after wiring. Mic loss is detected via miniaudio's
  stopCallback (only while capturing) -> `IAudioCapture::DeviceLost()` -> engine discards
  with MicLost.

## UI (GLFW + Dear ImGui, three windows)

- Three OS windows, each with its own Dear ImGui context AND its own GL
  context: `MainWindow` (Full window: Hairline top-strip navigation + pages —
  Welcome, Home, History, Insights, Dictionary, Snippets, Transforms,
  Scratchpad, Profiles, Settings; the left rail was replaced by the top strip
  in T4a), `Overlay` (Mini badge, draggable, click-through unless hovered) and
  `ScratchpadWindow` (the floating scratchpad editor, T10e — owns its own
  window/context while the Scratchpad page renders inside Full). Never share
  a context between windows.
- **`ImGui::CreateContext()` restores the PREVIOUS context after initializing the new
  one** (vendored v1.92.9b behaviour). Do NOT do
  `imgui_context_ = ImGui::GetCurrentContext()` after `CreateContext()` — it returns
  the other window's context. Keep the pointer the function returns and
  `ImGui::SetCurrentContext()` it. With both windows this way, the Full window
  rendered with `DisplaySize = 140x70` (the Mini badge's size), its GL viewport
  anchored at the bottom-left (GL origin) and the GUI appeared as a tiny pill on a
  black background (the long-standing "black screen"). Fixed in 1a8304f.
- The main loop (src/main.cpp) calls `full.RunOnce()`, then
  `overlay.RenderFrame()` and `scratchpad_editor.RenderFrame()` every tick;
  all three re-assert `glfwMakeContextCurrent()` and `ImGui::SetCurrentContext()`
  before their backend NewFrame calls because they run back-to-back on one thread.
- Theme tokens are the Hairline spec values from docs/design/DESIGN.md (the
  design source of truth; docs/design/mockup.html is superseded legacy) in
  src/ui/theme/ThemePalette.h, and tests/test_overlay_settings_ui.cpp enforces
  them (and the 3:1/4.5:1 contrast policy off those exact tokens). Two
  palettes, one accent per theme: Graphite (kDarkPalette, runtime default) and
  Paper (kLightPalette, config `theme` = "graphite"/"paper", live swap, T2b).
  Do NOT lighten colors to "fix" perceived contrast — the dark-screen bug was
  a viewport/context bug, not a color bug; the earlier lightened-palette
  change was reverted.
- The Full window is the only visible main window; it hides on close (the tray owns
  Quit) and opens from the tray (Open Settings…). `first_run` (schema default true)
  and an empty `whisper_model_path` control whether it appears at startup.
- `docs/UI_AUDIT.md` section 1 is the audit of this area (D1: cross-cutting shared
  context); `docs/MANUAL_TEST_WINDOWS.md` section R/E1 lists the real-hardware
  verification steps.

## Working rules for agents

- Read docs/PROGRESS.md before starting; record what you did there (one row per task
  with gates run). Honest limits: Windows-only behavior (hook, tray, overlay, injector,
  downloader, installer, crash filter) is adapter code with manual checklists in
  docs/MANUAL_TEST_WINDOWS.md — not claimed as verified.
- Tests first when touching portable logic; the suite is doctest (tests/), hermetic via
  tests/V0ConfigFixture.h (seeds the untracked v0 config data).
- Never invent model URLs, SHA-256 values, vendor prices or API shapes: contracts come
  from vendor documentation pages and are recorded in src/stt/cloud/*Contract.h and
  docs/STT_CLOUD.md with the date they were checked.
- Secrets (API keys) only ever cross the process boundary via `api_keys.json`
  (store→env: file entry first, environment fallback, separate file, never `config.json`)
  or the environment; masked in the GUI and redacted from every stored log line
  (lwf::RedactSecret / RedactAuthValues). `llm_provider` defaults to `local`; the
  only networked exception to the offline default is opt-in OpenRouter
  (`llm_provider=openrouter`, D2) — it must be stated on the Privacy page + About.
