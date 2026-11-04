# AGENTS.md — rules for agents working on Local Wisper Flow

Local Wisper Flow is local-first voice dictation for Windows (press-to-talk,
VAD gating, whisper STT, optional local LLM cleanup, text injection) with an
opt-in cloud STT path behind a consent gate. Reconstructed rule sheet: the
original was lost with the dev-environment reset; the paragraphs below are
recovered verbatim from the S9 documentation patch where noted.

## Build and gates

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build
```

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
- Gate ledger: G1 build -j4 zero new warnings, G2 ctest green, G3 zig type-check of
  Windows sources, G4 scripts/check_hygiene.sh, G5 config_v0 backward-compat test.
  This box has no zig and no Windows: G3 stays "pending" and is recorded per task in
  docs/QUESTIONS.md.

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
  pass via `-DGRPC_URL=file://...`. Also needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`
  (CMake 4 vs old dep minimums), `protobuf_INSTALL=OFF`, `utf8_range_ENABLE_INSTALL=OFF`.
  Stub generation uses in-tree `protoc` + `grpc_cpp_plugin` targets into
  `build/generated/`, compiled as `lwf_proto`.

## Config and data

- Config keys (`config.json`): `hotkey_code`, `whisper_model_path`, `vad_threshold` are
  required; `llm_model_path`, `use_remote` (default false) and `remote_endpoint`
  (default `localhost:50051`) optional. The full schema with defaults lives in
  `SettingsStore::BuildSchema()` (src/core/SettingsStore.cpp); keys are additive-only
  across releases and unknown keys are preserved on save. Cloud STT is opt-in:
  `stt_provider` (`local` default; `gnani` | `elevenlabs` | `muse`) needs
  `stt_consent_accepted=true` plus the provider's key from the environment only
  (`GNANI_API_KEY` / `ELEVENLABS_API_KEY` / `META_MODEL_API_KEY`, single source in
  `src/core/ConsentGate.cpp`), never logs transcripts or keys, and only ever falls back
  cloud -> local (see `docs/STT_CLOUD.md`). The in-app consent surface is Settings >
  Cloud (S5); failed attempts keep a redacted error `detail` (lwf::RedactSecret/
  RedactAuthValues, src/core/Redact.h) in the attempt stats. Data files live under
  `%APPDATA%\LocalWisperFlow` (LWF_HOME overrides): config.json, dictionary.json,
  snippets.json, app_profiles.json, history.jsonl, models.json, logs/, models/.
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
- Secrets (API keys) only ever cross the process boundary via the environment; they are
  redacted from every stored log line (lwf::RedactSecret / RedactAuthValues).
