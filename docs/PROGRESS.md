# PROGRESS

| id | title | status | commit | gates G1-G5 | manual-check | notes |
|----|-------|--------|--------|-------------|--------------|-------|
| A0 | Scaffolding | DONE | 4a36132 G1-G4 green, G3 zig pending | | no | |
| A1 | Test framework | DONE | a45eb7a G1-G4 green, G3 zig pending | | no | |
| A2 | Extract the engine | DONE | 4f80c5c G1-G4 green, G3 zig pending | | no | |
| A3 | Harden cleanup | DONE | ad8a493 G1-G4 green, G3 zig pending | | no | |
| A4 | Settings store | DONE | ec0a1fe G1-G4 green, G3 zig pending | | no | |
| A5 | Logging and error codes | DONE | deb8fa6 G1-G4 green, G3 zig pending | | no | |
| B1 | Hotkey bindings and chord tracker | DONE | def2d3b G1-G4 green, G3 zig pending | | no | |
| B2 | Windows hook adapter | DONE | 3a81f7e G1-G4 green, G3 zig pending | | yes | zig unavailable in this environment; type-check pending |
| B3 | Modes and cancel | DONE | 89ea811 G1-G4 green, G3 zig pending | | no | |
| C1 | Injection hardening | DONE | | | yes | |
| C2 | Spoken commands | DONE | | | no | |
| C3 | Dictionary 2.0 | DONE | | | no | |
| C4 | Snippets | DONE | | | no | |
| C5 | Context-aware tone | DONE | | | yes | |
| C6 | Languages | DONE | | | no | |
| C7 | Command mode | DONE | | | yes | |
| C8 | Paste last transcript | DONE | | | no | |
| C9 | Whisper low-volume mode | DONE | | | no | |
| D1 | UI bus and status snapshot | DONE | a7b5a59 G1-G4 green, G3 zig pending | | no | |
| D2 | Overlay pill | DONE | 94872bd G1-G4 green, G3 zig pending | | yes | |
| D3 | Tray, single instance, autostart | DONE | 94872bd G1-G4 green, G3 zig pending | | yes | |
| D4 | Settings window | DONE | 94872bd G1-G4 green, G3 zig pending | | yes | |
| D5 | History | DONE | 0906542 G1-G4 green, G3 zig pending | | no | |
| D6 | Audio feedback | DONE | 014a166 G1-G4 green, G3 zig pending | | yes | |
| D7 | Error surfacing | DONE | 21ba5e0 G1-G4 green, G3 zig pending | | no | |
| D8 | First-run onboarding | DONE | 94872bd G1-G4 green, G3 zig pending | | yes | |
| E1 | Chunked streaming transcription | DONE | (this commit) G1-G5 green, G3 zig pending | | yes | StreamingDictator strict-FIFO worker, pause>=700ms after >=3s commits, 200-char rolling context, failure falls back to one-shot full buffer, cancel discards in-flight; lwf-bench target + docs/BENCHMARKS.md (synthetic proxy numbers measured) |
| E2 | Resilience | DONE | (this commit) G1-G5 green, G3 zig pending | | yes | DeviceLost on IAudioCapture + miniaudio stopCallback; MicLost graceful stop; startup_errors surface ModelMissing/ModelLoadFailed(OOM advice)/ConfigCorrupt via bridge; cleanup auto-disabled when llm missing; vad load failure degrades to no-gating; chaos test 1000 dictations all IDLE |
| E3 | Crash handling | DONE | (this commit) G1-G5 green, G3 zig pending | | yes | portable CrashReporter writes crash-<ts>.txt (version, code, FSM state, 200-line log tail); os/CrashHandler wraps SetUnhandledExceptionFilter with FSM diag mirror; next-launch notice once (MarkSeen rename); version constant from project VERSION printed at startup and in About tab |
| E4 | Privacy guarantees | DONE | (this commit) G1-G5 green | | no | check_no_network.sh (network APIs only in GrpcInferenceProvider/WindowsDownloader; file-write APIs forbidden in audio/stt/vad/llm dirs, self-tested offender); doctest asserts zero files created in cwd across fake dictations; README states offline-by-default |
| F1 | CI | DONE | (this commit) G1-G5 green | | no | stale vision_sst workflow replaced: linux-core job (configure/build/ctest/hygiene/privacy/bench smoke) + windows-full job (cache _deps-cache, fetch_deps.ps1, two-phase configure with file:// tarball URLs, MSVC build, ctest; VAD targets skip without onnxruntime); .gitignore no longer ignores .github/ |
| F2 | Repo size | DONE | (this commit) G1-G5 green | | no | 9 tarballs (179 MB) removed from index, _deps-cache/ ignored; scripts/fetch_deps.sh|.ps1 rebuild the cache from pinned URLs; verified: full grpc tarball rebuilt from submodules has an identical file listing to the original artifact (md5 c07d90a8); history-purge question for Devansh in docs/QUESTIONS.md #4 |
| F3 | README | DONE | (this commit) G1-G5 green | | no | full README: features, requirements, build steps with two-phase configure note, model table, first run, hotkeys table, complete settings reference from the schema, troubleshooting (mic blocked, admin apps/UIPI, slow STT, AltGr, paste chords), privacy statement, model license notes; screenshot placeholder |
| F4 | Installer | DONE | (this commit) G1-G5 green | | yes | installer/lwf.iss: per-user (PrivilegesRequired=lowest, {localappdata}\Programs), bundles exe + optional onnxruntime DLLs + optional silero_vad.onnx (MIT; confirm file license) + default json files, no model weights; Start Menu + optional desktop icon + optional HKCU autostart task; uninstaller kills the exe and asks before deleting %APPDATA% data; SmartScreen/code-signing documented in the iss header and checklist |
| F5 | Docs | DONE | (this commit) G1-G5 green | | no | AGENTS.md updated (bench/CI/privacy gates, fetch_deps recipe, streaming/crash/resilience facts, stale vision_sst + smoke-harness notes fixed); docs/ARCHITECTURE.md added (pipeline diagram, thread table, data layout, error paths, build variants); MANUAL_TEST_WINDOWS.md consolidated with a checklist index |

Gates: G1 build -j4 zero new warnings, G2 ctest green, G3 zig type-check of Windows
sources, G4 scripts/check_hygiene.sh, G5 config_v0 backward-compat test.

Environment note: this dev box has no zig and no Windows; G3 is recorded as pending
per task in docs/QUESTIONS.md. Stage A/B were rebuilt from the brief after the dev
environment reset; commits a45eb7a..89ea811 re-land the foundation with the same
architecture contracts (engine extraction, settings store, logging, hotkey core). and Windows-visible behavior is listed in
docs/MANUAL_TEST_WINDOWS.md instead of claimed as verified.

## E1 plan (ledger)

Executed as planned; added generation counter so a stale in-flight segment cannot leak into the next session after Cancel -> Begin.

Files: core/IInferenceProvider.h (additive TranscribeWithPrompt/RequestCancel), core/IAudioCapture.h (additive SampleRate), core/EngineTypes.h (streaming_stt, streaming_min_segment_ms, streaming_pause_ms), core/StreamingDictation.{h,cpp} (single-thread strict FIFO, rolling 200-char context, JoinTexts spacing), core/DictationEngine.{h,cpp} (pause-commit in Tick, tail finish, one-shot fallback on failure, cancel discard), core/SettingsStore.cpp (3 schema rows), stt/LocalWhisperProvider (prompt merge under 600 cap, abort-callback cancel), src/main.cpp (two wiring sites), tests/test_streaming.cpp, bench/lwf_bench.cpp + docs/BENCHMARKS.md.
Tests first: FIFO order under reversed delays, failure fallback to full-buffer one-shot, cancel mid-flight, spacing joins, context prompt carried, short utterance never streams, engine-level long-utterance commit thresholds.

## E4 note

scripts/check_no_network.sh self-tested: planting a winhttp/WSASocket offender in src/core
fails the run; removing it restores OK. Allowlist covers GrpcInferenceProvider (optional
remote, off by default) and WindowsDownloader (explicit user click). File-write allowlist
covers the text stores; audio/stt/vad/llm dirs are write-forbidden.

## CHECKPOINT-E

Stage E (E1-E4) complete. Windows items awaiting Devansh's manual run:
streaming dictation correctness (E1), mic-unplug + model-missing + corrupt-config
behavior (E2), crash file + one-time notice + About version (E3). Linux gates
G1-G5 all green at this point: 150 doctest cases, 9536 assertions, hygiene and
privacy scripts pass, config_v0 backward-compat intact.

## FINAL

Tag v0.1.0-rc1. All plan stages complete:

| Stage | Tasks | Status |
|-------|-------|--------|
| A foundation | A0-A5 | DONE (rebuilt after dev-environment reset; ledger shas above) |
| B input | B1-B3 | DONE (checkpoint-B) |
| C output quality | C1-C9 | DONE (checkpoint-C) |
| D UI | D1-D8 | DONE (checkpoint-D) |
| E speed/reliability | E1-E4 | DONE (checkpoint-E) |
| F shipping | F1-F5 | DONE |

Final gate run (Linux core, gcc 14.2, Release):
- G1 build -j4: zero new warnings (only doctest's own deprecated-cmake notice)
- G2 ctest: 100% pass — 150 doctest cases, 9533 assertions
- G3 zig type-check of Windows-only sources: pending, no zig on this box (docs/QUESTIONS.md #1)
- G4 check_hygiene.sh: OK
- G5 config_v0 backward-compat test: green
- privacy check_no_network.sh: OK
- lwf-bench smoke: runs, prints p50/p95 (docs/BENCHMARKS.md)

Honest limits: Windows-only behavior (hook, tray, overlay, injector, downloader,
installer, crash filter) is adapter code with manual checklists in
docs/MANUAL_TEST_WINDOWS.md — not claimed as verified. models.json urls/sha256
await Devansh (docs/QUESTIONS.md #2). History purge is a manual decision
(docs/QUESTIONS.md #4).


## GUI redesign (R series)

Baseline: docs/UI_AUDIT.md (R0) audits src/ui against the pinned GLFW 3.4 /
Dear ImGui v1.92.9b contracts; docs/design/mockup.html is the color/spacing
reference. Constraint for every R step: visual/UX only - audio, STT, LLM and
injection logic are not touched.

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| R0 | UI audit + design reference | DONE | 2249b56 | n/a (docs only) | no | found D1-D12 defect register incl. shared-context blocker |
| R1 | Theme tokens, one context per window, Mini dot, Full Home/Speech/Cleanup/Privacy/About/Welcome, live-apply settings | DONE (pending Windows verification) | this change | Linux lwf_tests: 162/162 green (3 runs); UI sources syntax-checked against real v1.92.9b headers | yes | fixes audit D1-D4, D6 (fonts pending approval), D9, D10, D13, D14; SettingsWindow + OnboardingWizard absorbed into Full and deleted |

R1 notes:
- Each OS window now owns its Dear ImGui context (audit section 1 fix); the
  loop switches contexts and quits only via the tray.
- `dictation_enabled` was missing from the settings schema, so the tray
  toggle silently wrote nothing (audit D13); added, default on.
- The engine bridge no longer touches the UI from the engine thread; errors
  go through a mutex-guarded attention box drained by the main loop (D14).
- Default built-in font scaled by DPI; real fonts wait for the approval that
  D0 flagged. Display sizes use SetWindowFontScale until then.
- Windows build not run on this box (Linux CI here): MANUAL_TEST_WINDOWS.md
  section R is the gate before R2.

## STT cloud providers (S0-S9)

Opt-in cloud transcription behind the consent gate, cut from the rebase report in
repo-root BASE_DECISION.md. Stage ledger (one commit per stage):

| id | title | status | commit | gates | notes |
|----|-------|--------|--------|-------|-------|
| Phase 0 | base decision + rebase report | DONE | a1d927b..8662441 | n/a | decision B, replayed onto 8e4945a+d87fedc |
| S0 | Gnani contract smoke script | DONE | fd6fe84 | G1-G5 | offline contract tool; live run pending (QUESTIONS #5) |
| S1 | WAV / multipart / clip primitives | DONE | 5cd6f45 | G1-G5 | in-memory audio only |
| S2 | Gnani request/parse/error map | DONE | b6c5e0a | G1-G5 | SttErrorCategory taxonomy |
| S3 | SttRouter + consent gate + fallback | DONE | 91a55ed | G1-G5 | one-way cloud->local |
| S4 | WinHTTP client + app wiring | DONE | e9ee0b6 (rebased) | G1-G2/G4/G5, G3 pending | Windows build check pending |
| S5 | ConsentGate, Redact, Settings > Cloud UI | DONE | 98b0829 | G1-G5, G3 pending | consent + secrets_redaction tests |
| S6 | lwf-cloud-bench | DONE | 8928b0c | G1-G5 | loopback latency + cost model (BENCHMARKS.md) |
| S7 | ElevenLabs Scribe provider + LanguageMap | DONE | 3df580e | G1-G5 | language_map + cancel tests |
| S8 | Meta Muse Voice provider | DONE | 16b6f51 | G1-G5 | two-part JSON+audio multipart |
| S9 | documentation pass + hygiene self-test | DONE | (this commit) | G1-G5 | interface mapping in STT_CLOUD.md |
| E1 | GUI fixes: DPI-safe full window + quarter-size draggable mini | DONE | (this commit) | G1-G2/G4/G5, G3 pending | Windows build check pending |

E1 notes (2026-11 user feedback): the Full window now clamps its creation
size to the monitor work area and follows monitor scale changes live
(content-scale callback, style re-applied with the window's own ImGui
context), so text and controls can no longer render at the wrong scale. The
mini badge is a quarter of its former area (22 px dot), highly transparent
(idle 0.35 / active 0.65), shows a tiny 8.5 px status label, and can be
dragged anywhere with the mouse; the position persists in config
(mini_x/mini_y, -1 = default anchor above the taskbar). Also fixed: the
mini window geometry was DPI-scaled twice, and the attention hover message
drew below the window edge where it was invisible.

Note (re-land): the commits above name the ORIGINAL line's SHAs, which ended at
86624419 and were pruned by the dev-environment reset along with the S0-S4
base. This repo re-lands S5-S9 from the recovered bundle pack and patch set
onto the R1 GUI-redesign line; the S0-S4 files the bundle assumed (SttRouter,
ISttProvider, IHttpTransport, Gnani stack, ClipSplitter, WavEncoder,
MultipartBuilder) are reconstructions constrained by the recovered providers,
tests and docs, and the S4/S5 cloud UI was re-homed from the old
SettingsWindow tab to the R1 Full window's Cloud page. scripts/gnani_smoke.py
(S0) was lost with the base and is not rebuilt here - see QUESTIONS #5.

Final gate run at S9 (Linux core, gcc 14.2, Release): build -j4 clean, doctest
suite green (see git log for the exact case/assert counts of this re-land),
check_hygiene.sh OK (+ --self-test OK), check_no_network.sh OK, config v0
compat green, local-wisper-flow smoke exits 0. Honest limits: all cloud code
is unit-tested with fakes; the live-API smoke (Gnani/ElevenLabs/Muse), the
Windows build/UI checkpoint and the zig type-check await Devansh
(docs/QUESTIONS.md #1 and #5).

## Windows bring-up + GUI (2026-11-06)

Devansh ran the app on Windows and screenshotted a black client area with a tiny
rounded pill at the bottom-left. Root-caused and fixed on `main` (MinGW local
build; Windows CI runner is MSVC):

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| WB1 | model path: exe-dir parent + cwd fallbacks | DONE | ba46cbf, db4e40a | G1 (MinGW exe build) | no | `ResolveDataPath` order: absolute, data_dir, exe_dir, exe_dir.parent, cwd; fixes "failed to open ggml-base.en.bin" when the exe is run from `build/` |
| WB2 | GL context re-assert before NewFrame | DONE | b7f677d | G1 (MinGW exe build) | no | defensive; back-to-back windows re-bind their own GL context in one thread |
| WB3 | Full window collapsed to 140x70 pill (black screen) | DONE | 1a8304f | G1 (MinGW exe build) | partial: both windows render (window capture); clicks untested | root cause: `ImGui::CreateContext()` restores the PREVIOUS context after init; both Init() used `GetCurrentContext()`, so both windows shared ctx_A; the overlay's backend then overwrote `BackendPlatformUserData` (the "already initialized" assert is compiled out in Release); main window rendered with `DisplaySize=140x70`, viewport (0,0,140,70) = bottom-left in GL |
| WB4 | Earlier dark-theme contrast change reverted | DONE | (in 1a8304f) | n/a | n/a | lighter palette was a misdiagnosis; mockup-exact tokens restored (tests/test_overlay_settings_ui.cpp enforces them) |
| WB5 | Mini badge + Full window verified by capture | DONE | 1a8304f | G1 | partial | PrintWindow render shows rail/nav items/Welcome page with card and mini badge (mic + Ready) |
| WB6 | CI repairs (hygiene, whisper patch stderr, lwf_tests) | DONE | d643c4e..f9c3431 | see task log | partial | sub agents launched 2026-11-06; dep provision + Linux green; windows compile reached; NOMINMAX fix f9c3431 pending CI |

Gates: G1 OK (MinGW `-j4` build of the exe). G2 not green this session —
`lwf_tests` does not compile (stale `ScriptedLlm` call sites in test files;
fix in flight). G3 zig pending (no zig on this box). G4 hygiene was FAIL
(`src/ui/Overlay.cpp` windows.h) — fixed via moving `PrimaryWorkArea` into
`src/os/WindowStyles.cpp` (same session); G5 not run.

## Task log

- 2026-11-07: GUI fix/improvement wave run 20261106-gui CLOSED (branch
  `fix/gui-typography-dpi`; see `docs/agent-ops/20261106-gui/SUMMARY.md`).
  - GUI-A2 (T-001): scoped PushFont + design-px typography tokens replace
    obsolete SetWindowFontScale (b21e40f; ApplyImGuiStyle per-context scale
    gate e0a3882).
  - GUI-A1/A3 (T-002/T-003): rail/toggle keyboard nav (NavEnableKeyboard,
    EnableNav, 2 px focus ring, NavActive-gated restore), Copied feedback,
    slider deactivation commit, Welcome save path (3aa4329; review fixes
    07cc957).
  - GUI-A5/A6 (T-005/T-006): mini idle early-out + alloc-free waveform,
    contrast uplift to mockup alphas (0.55 idle / 1.0 active, box 250 px,
    13 px message font) (d9e86dd; review fixes 07cc957).
  - ENG-B1 (T-007): streaming double-commit race serialized via
    commit_mutex_ (8223834; Tick contract narrowed b0cfe38).
  - Independent code+test review per wave; final integration review
    APPROVE-WITH-CHANGES (0 BLOCKER/0 MAJOR). Gates at close: build green,
    lwf-tests 1/1 Passed, check_hygiene OK, check_no_network OK.
  - CI pre-existing dependency-provision failures repaired (miniaudio
    FetchContent hoisted out of FULL; Windows CI fetches deps via
    scripts/fetch_deps.ps1 + file:/// tarball URLs; WindowsHttpClient gated
    to WIN32) in d643c4e/94901cc/081d90d; MSVC-only glfw3native.h→windows.h
    min/max macro collision fixed by project-wide NOMINMAX on WIN32 (f9c3431).
    Windows two-phase link closure: phase-1 full link is impossible by design
    (cold cache → LLAMA_READY=false → placeholder llama target; whisper's
    bundled ggml also lacks ggml_rope_set_offset), so phase-1 now builds
    llama_ext only (9b7c75d; the imported-target ggml link closure was
    considered and rejected — it would double-define ggml in phase 1).
    Test portability: POSIX-only /tmp fixture paths replaced with
    std::filesystem::temp_directory_path() in 4 test files (126da58) —
    production code untouched (Load()==false for unreadable file is correct).
    **CI GREEN: run 42310536369, both jobs success (windows job 128210561461,
    linux job 128210561573), on branch HEAD 126da58.** G3 zig pending (no zig).

- 2026-11-06: T-001 improvement pass (W1-IMPR-A2): F1 MAJOR (ApplyImGuiStyle re-apply
  path) FIXED via per-(context, scale) apply-on-change tracking in src/ui/theme/ThemePalette.{h,cpp};
  F2 MAJOR (changed-behaviour regression coverage) FIXED via new source-scan TEST_CASE guarding
  SetWindowFontScale removal + scoped PushFont design-px mechanism in tests/test_overlay_settings_ui.cpp;
  F3 MINOR (mockup s4=14/s5=13 mapping) FIXED via approved-mapping pin in token test; token split DEFERRED;
  F4 MINOR / F5 NIT DEFERRED. Scoped gates: build green, lwf-tests 218/218 passed, check_hygiene OK.
  Report: docs/agent-ops/20261106-gui/reports/w1-improve/W1-IMPR-A2-T-001.md.

- 2026-11-06: GUI-A2 (T-001) scoped fonts + design-px typography tokens: obsolete
  ImGui::SetWindowFontScale replaced with scoped ImGui::PushFont(nullptr, design_px)
  + ImGui::PopFont using new metrics::font_*_px design tokens (wordmark 18, page title
  28, card title 15, body 13, mini 8.5); FontScaleMain applies DPI once; token
  regression test added. Scoped gates: build green, lwf-tests 1/1 Passed, check_hygiene
  OK. Report: docs/agent-ops/20261106-gui/reports/w1-impl/W1-IMPL-A2-T-001.md.

- 2026-11-06: Windows bring-up + GUI: model path exe-dir-parent/cwd fallbacks
  (ba46cbf/db4e40a), GL context re-assert (b7f677d), root-cause fix for the
  "black screen" — `ImGui::CreateContext()` restores the previous context, both
  Init() paths shared one context and the Full window rendered clipped to the
  140x70 Mini size (1a8304f; verified by window capture; theme palette reverted
  to mockup-exact tokens). CI blockers repaired via sub agents: hygiene
  windows.h lint (moved PrimaryWorkArea into src/os/WindowStyles), whisper patch
  step stderr captured (MSBuild MSB8066), stale lwf_tests call sites. See
  "Windows bring-up + GUI (2026-11-06)" above.

- 2026-11-04: Fixed CI `windows` job failure by pinning the runner to `windows-2022` (the `windows-2025-vs2026` image no longer provides the Visual Studio 2022 generator used by this workflow). Gates for this workflow-only change: G1-G5 not run locally.

- 2026-11-06: Full Windows/MinGW rebuild green again after dependency install + dep fixes.
  - BoringSSL bssl.exe link failure (undefined `fiat_p256_adx_mul`/`fiat_p256_adx_sqr`) root-caused: BoringSSL on WIN32+x86_64 enables the NASM backend whose source list omits the GNU-asm fiat field-element implementations, while the C++ still emits calls to those symbols. Fixed two ways: (1) vendored `third_party/boringssl-with-bazel/src/CMakeLists.txt` now sets `OPENSSL_NO_ASM ON` for MinGW+GNU (portable C fallback, same as gRPC's Bazel CPPFLAGS); (2) repo `CMakeLists.txt` propagates `-DOPENSSL_NO_ASM` via `CMAKE_C_FLAGS`/`CMAKE_CXX_FLAGS` into the gRPC add_subdirectory scope (subproject-internal compiler-ID checks alone did not take effect). Vendored `re2/util/pcre.h` keeps the GCC-15-needed `#include <cstdint>`.
  - Build: clean `cmake --build build --parallel 2` (user cap) — `local-wisper-flow.exe` (32 MB), `bssl.exe`, full gRPC stack, whisper, llama_ext all built; `lwf_tests`/`lwf-bench`/`lwf-cloud-bench` compiled.
  - Gates: G1 (MinGW full build) OK, G2 ctest 23/23 green (lwf-tests 16.5s; re2 suites green; config_v0 compat inside suite), G4 check_hygiene.sh OK, check_no_network.sh OK, G5 config_v0 backward-compat via suite; G3 zig pending (no zig on this box).
  - Phase-2 llama install-closure reconfigure re-run; whisper picked up installed llama (WHISPER_USE_SYSTEM_GGML).

## Studio pages + settings GUI + OpenRouter (20261107-studio)

Branch `feature/studio-pages` from `0e655ec` (1d95fb2 merge + docs cherry-pick). F0 signed 2026-11-07 (D12-D15 in `docs/STUDIO_PLAN.md` §9/§14; plan file corrected to shipped shapes in FINAL-FIX-01). HEAD at gate-w5-1 was `091ec36` (gate GREEN 23/23 in 660.70 s); tip before FINAL-FIX-01 was `4fb18be`. One row per task, gates run recorded per gate.

| id | title | status | commit | gates G1-G5 | manual-check | notes |
|----|-------|--------|--------|-------------|--------------|-------|
| T-001 | F0 docs/STUDIO_PLAN.md + AGENTS D3/D2 amendment | DONE | e9752fc | G1-G2/G4 GREEN, G3 pending | no | plan P01; pixel-palette provisional (R-01) still open |
| T-002..T-004 | AtomicFile + TextDiff + InsightsModel | DONE | a7dbd93, 54abc8a | G1-G2/G4/G5 GREEN (gate-w1-2 23/23), G3 pending | no | W1 improver 3571116 fixed Enforce hang/races |
| T-005,T-010 | StatsStore daily rollup + AppContextResolver category | DONE | 7f4975a | as above | no | 730-day cap, 6 categories |
| T-006 | DictionaryStore v2 4-shape migration | DONE | 2a2cd70, 79e69c1; tombstone f66943b | as above | no | shipped `{schema_version:2, entries}`; `.bak` only-if-absent |
| T-007,T-008 | SnippetStore v2 + TransformStore 10 slots | DONE | 530555f, f9a9f2c | as above | no | built-ins `Polish` / `Prompt Engineer`; ids `snippet-<n>` int64 |
| T-009 | NotesStore index+md | DONE | 894675a, 2a765e9 | as above | no | two-file reconcile, undo toast |
| T-011..T-013 | Engine hooks (stats :475, transform mode, scratchpad target) | DONE | ea22607, ff1c32d (07f6131 W2 improver) | G1-G2/G4 GREEN (gate-w2), G3 pending | no | stats-after-inject; run flow on worker; scratchpad-only |
| T-014 | Nav shell P17 + Settings tabs + last_page | DONE | a4ab50a, 2ff48b5 | G1-G2/G4 GREEN (gate-w3-1), G3 pending | no | 7-item rail + 6 Settings tabs |
| T-015 | StudioPalette + widgets + modal/toast infra P18 | DONE | 860b698, 844dd3e; improver d149726 | as above | no | teal tokens shipped (not navy estimates) |
| T-016..T-020 | Five pages P19-P23 (Dictionary/Snippets/Insights/Transforms/Scratchpad) | DONE | 67c955b, 095ef47, 96fb22c, fc96356, acfaf21; int 76cf674, c9df9df | G1-G2/G4 GREEN (gate-w4-2), G3 pending | not-run (Windows) | one file per page; source-scan tests only on Linux CI |
| T-021 | F9 polish sweep P24 | DONE | 69c8358, 784edfb | G1-G2/G4 GREEN (gate-w5-1 23/23), G3 pending | not-run (Windows) | Esc fix, toggle/link contrast, copy lint |
| T-022 | A1 ApiKeyStore + ConsentGate/store→env | DONE | ff1c32d | as T-011 | no | `api_keys.json` separate, redacted, never `config.json` |
| T-023 | A2 FileDialogs picker + Browse buttons | DONE | acc7c5c, 46cdde4 | G1-G2/G4 GREEN (gate-w4-2), G3 pending | not-run (Windows) | Win32 picker, lexical normalize |
| T-024 | A3 hotkey recorders (5 config + studio shortcuts) | DONE | a2cbf64 | as T-023 | not-run (Windows) | modifier-required + conflict disable |
| T-025 | A4 all-key settings GUI (55 keys: 49+6) | DONE | 5a2a618, 6a87b39 | G1-G2/G4 GREEN (gate-w4-2), G3 pending | not-run (Windows) | carve-outs `mini_x/mini_y/schema_version/first_run` |
| T-026 | C1-C2 OpenRouter provider + contract + gate | DONE | 86605b7, baa650b; improver 8bae0c8, 4fb18be | G1-G2/G4 GREEN (gate-w5-1; openrouter 19/19), G3 pending | not-run (no key; live UNVERIFIED-against-live) | `llm_provider`, `openrouter_model`, try_lock single-flight, Busy, redaction |
| T-FINAL | FINAL-01 MAJOR1-3 + HeatLevel + tombstone comment | DONE | (this commit) | G1-G2 GREEN, G3 pending | not-run (Windows) | llm_provider restart (`hot_apply=false` + banner); Trust card branches on `llm_provider`; HeatLevel = CalendarLevel; comment deleted |

Final gate run at `4fb18be` + FINAL-FIX-01 (this commit):
- configure `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` → exit 0 (`Configuring done`, `Generating done`, `Build files have been written`; ONNX absent expected)
- build `cmake --build build --config Release --parallel 4 --target lwf_tests` → exit 0, `Built target lwf_tests`
- scoped `.\build\lwf_tests.exe --test-case="*studio portable helpers classify heat and clamp and space caps*,*new A4 schema keys default and clamp through the store*,*main window keyboard nav and commit-on-deactivate wiring*,*CalendarLevel*"` → 5/5 cases, 519/519 asserts, SUCCESS
- `ctest --test-dir build -R lwf-tests --output-on-failure` → `1/1 Test #1: lwf-tests Passed 19.97 sec`, `100% tests passed`
- `bash scripts/check_hygiene.sh` → `hygiene: OK`
- `bash scripts/check_no_network.sh` → `no-network: OK`
- G3 zig type-check: pending, no zig on this box (`docs/QUESTIONS.md` #1; per-task record).

Honest limits and really-left residuals (see `docs/STUDIO_PLAN.md` §15 and `docs/agent-ops/20261107-studio/reports/final/FINAL-01.md` F-1..F-7):
- No contacts/accounts/sync feature shipped (out of scope); 5 pages + settings + OpenRouter contracts are the branch. `openrouter_model`/OpenRouter key remain hand-edited (`api_keys.json`/env, Privacy readout only) and now require restart.
- DECISION-1 (no separate `llm_consent_accepted`) still needs Devansh merge confirmation; live OpenRouter smoke stays UNVERIFIED-against-live (docs-verified only); Windows checkpoints I1/D1/S1/T1-T6/S1n-S5n/P1 still owed on hardware.
- Migrations one-way via `.bak` (do not delete `.bak`); branch-internal rollback dual-reads v1+v2, pre-branch `0e655ec` rollback silently empties dictionary/snippets and throws on `transforms.json` (verified from the `0e655ec` loaders) — restore `.bak` for that rollback.
- Follow-ups F-1 (dead `AtomicFile` + divergent `.bak`), F-2 (detached transform worker), F-3 (silent hotkey drop), F-4 (plaintext `api_keys.json`) remain tracked; F-5 (`HeatLevel`/`CalendarLevel`) is CLOSED by T-FINAL.
- `docs/STUDIO_PLAN.md` header/§3 drifts corrected to shipped shapes in T-FINAL; AGENTS.md endpoint sentence + `check_no_network.sh` header OpenRouter prose is a small docs follow-up outside this commit.

## ImGui `Dummy()` fix (2026-11-08)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| IMG-001 | Trailing cursor restore in dict/snippets/insights rows | DONE | `1fce2bc` + `c13f563` on `fix/blank-audio-and-dict-dummy` (replayed from `8773d94`/`d78fc42`) | ctest `lwf-tests` 1/1 (520 cases), hygiene OK, no-network OK; G3 zig pending | yes (hover last row, Windows) | R1 plain-Dummy REJECTED for +8px geometry drift; final compensated form (restore − ItemSpacing.y, then `Dummy`) delta-review APPROVE. Scratchpad parent-window restore pattern left as follow-up (unconfirmed) |

## `[BLANK_AUDIO]` hallucination fix (2026-11-08)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| IMG-002 | Whisper hallucination filter wired into production | DONE | `092ff9f` + `02de5b2` on `fix/blank-audio-and-dict-dummy` | ctest `lwf-tests` 1/1 (528 cases incl. 3 whisperfilter), hygiene OK, no-network OK; G3 zig pending | yes (say hello near mic on Windows; mic-level fault still owed) | `WhisperText::FilterHallucinations` was never called in production; 1-word "hello" never reaches the LLM (`cleanup_min_words=4`). Marker-only → no injection via existing empty guard, incl. command-mode guard. Multi-segment streamed markers + dead `RunEngine` harness still leak (follow-ups) |

## UI hierarchy & density — Full window (20261108-ui-hierarchy)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| UI-H1 | MainWindow + Studio pages/widgets same-value literal→token spelling (T-001/T-002) + literal-bypass test ban (T-004) | DONE | `e262309` + `eb54183` + `8f09615` on `ops/20261108-ui-hierarchy` (merged into current HEAD) | G1 target `lwf_tests` + full exe Built; G2 `ctest -R lwf-tests` 1/1 Passed 19.76s, later 19.97s/19.82s; G4 hygiene OK; no-network OK; G5 config_v0 in-suite green; G3 zig pending (no zig, docs/QUESTIONS.md #1) | no (screenshots P-006 pending) | 37 PushFont-digit → 6 allowlisted; ~20 Dummy/SameLine allowlisted; token values unchanged; dual-palette split documented, not recolored |
| UI-H2 | Token gaps: 9 additive `studio_metrics::*` tokens + stage-2 `space_*→gap_*` + 15-font migration + empty ban table | DONE | `f3ba1e5` (9 tokens) + tr2 + `d088cf4` (2 pin flips) on `ops/20261108-ui-hierarchy` | G1 `lwf_tests` + full exe Built (link ok); G2 1/1 Passed 19.87s (520/520 per W2-IMPL-02: 518/520 → 520/520); G4 hygiene OK; no-network OK; G5 in-suite | no (P-006 screenshots 960×640/760×520 × DPI 1.0/1.5 + Scratchpad editor owed; manual Windows) | Full-9 chosen (D-W2-01); 1 documented `space_xs` reuse (Transforms:576, no 4px studio twin); semantic gaps `gap_micro/inline/compact/section/block`; fonts `font_card_title/heading/subheading/glyph`; structureals (40/rail/widths/`+28/+32`/`pill_h+18`/clamp) stay literal |
| UI-R | Independent review: W1/W2 code+test APPROVE or APPROVE-WITH-CHANGES (0 BLOCKER/0 MAJOR at close) + final integration APPROVE-WITH-CHANGES | DONE | reviewers `W1-REV-01/W1-TREV-01/W2-REV-01/W2-TREV-01/W-DELTA-01` (all committed in `docs/agent-ops/20261108-ui-hierarchy/`) | gates `gate-w1-1/w1-2/w2-1` green (see run folder) | pending P-006 waiver or capture (F-001) | No BLOCKER/MAJOR open; deferred: RowList dead code, glob/file-list expansion, loophole class, CWD partial — Wave-2 follow-ups, not this tranche |

## Full UI redesign — Phase 2 token unify (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| UI-Ph2 | Token unify Ivory primary + tests | DONE | 5736431 | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed 21.92s; G4 `bash scripts/check_hygiene.sh` hygiene: OK; no-network OK; G3 zig pending (no zig, docs/QUESTIONS.md #1) | yes — screenshots owed: 960x640 / 760x520 @ DPI 1.0/1.5 + Scratchpad editor window (Ph7 Windows manual) | `kLightPalette` accent/focus -> brass 0x7A5C25 (single accent); rail 220, `font_body` 14, fade_in 0.15, `radius_card` 10 single; `StudioPalette` deprecated shim Ivory-remapped (teal_700/500 -> accent) + HeatColor brass ramp, pages/widgets still compile; full exe build / screenshots / MainWindow-Mini migration RED deferred to Ph3/Ph4/Ph5/Ph7 (see docs/plans/2026-11-08-full-ui-redesign-phase2-status.md) |

## Full UI redesign — Phase 3 widgets (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| UI-Ph3 | Widgets single-palette migrate + dummy fix | DONE (toast-action contrast needs token follow-up, see notes) | ac00dde, c0a079e | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed 20.65s; G4 `bash scripts/check_hygiene.sh` hygiene: OK; no-network OK; syntax-only compat for pages/MainWindow/Overlay/ScratchpadWindow vs new headers EXIT 0 (full exe rebuild timed out >900s, not run); G3 zig pending (no zig, docs/QUESTIONS.md #1) | yes — screenshots owed: 960x640 / 760x520 @ DPI 1.0/1.5 + Scratchpad editor window (Ph7 Windows manual) | Buttons/Display/Overlays now read Palette with trailing `palette = kLightPalette` (additive; pages still 3-arg compile); zero `kStudioPalette`/hex in widgets; HeatColor kept; BadgeBeta 24 / ProgressPill 28 static (32px = interactive, Widgets.h comment); Display Banner + Toast SetCursor compensated by ItemSpacing + Dummy per d78fc42; toast action accent-on-text measures 2.38 Ivory / 1.80 dark (6.10 is on_accent on accent), fails 4.5 — mid-brass toast token is follow-up, documented in Overlays.cpp DECISION comment |

## Full UI redesign — Phase 4 MainWindow shell (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| UI-Ph4 | MainWindow Ivory primary shell | DONE | 811c704 | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed 23.00s; G4 `bash scripts/check_hygiene.sh` hygiene: OK; no-network OK; `grep -n kDarkPalette src/ui/MainWindow.cpp` 0 matches (code uses 0); G3 zig pending (no zig, docs/QUESTIONS.md #1) | yes — screenshots owed: 960x640 / 760x520 @ DPI 1.0/1.5 + Scratchpad editor window (Ph7 Windows manual) | Shell now `kLightPalette` (ApplyImGuiStyle x2, clear bg, rail + page chrome Pack/Color); rail `kRailWidth = metrics::rail_width` (220) keeps `kRailWidth` pin + `40.0f * scale`; `font_card_title_px`/`metrics::font_` untouched; zero `widgets::` call sites in MainWindow (file-local ToggleSwitch + Draw(scale) delegates only) so widget defaults (`kLightPalette`) already match — no explicit pass needed; Overlay stays dark for Mini badge (Ph6); UiBus/live-apply/Saved dwell/hide-on-close logic untouched |

## Full UI redesign — Phase 5 pages (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| UI-Ph5 | Five pages Ivory migrate (Dictionary/Snippets/Insights/Transforms/Scratchpad) + F9 pin follow-up | DONE | ab0763b, bee7243, 28f16b4, 1032490, 379cc82, 152d15c | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed (521/521 incl. new Phase 5 Dummy-compensation pins); G4 `bash scripts/check_hygiene.sh` hygiene: OK; no-network OK; G3 zig pending (no zig, docs/QUESTIONS.md #1) | yes — screenshots owed: 960x640 / 760x520 @ DPI 1.0/1.5 + Scratchpad editor window (Ph7 Windows manual) | Pages read `kLightPalette`; F9 Transforms pin updated to `hovered ? kLightPalette.text : kLightPalette.accent` (was `kStudioPalette.ink/teal_700`); each page carries cursor Dummy compensation (`SetCursorScreenPos(... - ImGui::GetStyle().ItemSpacing.y); Dummy(ImVec2(0.0f, 0.0f))`) pinned by new TEST_CASE `Phase 5 pages carry cursor Dummy compensation`; `tests/test_dictionary_page.cpp` untouched (no Dummy pins there) |

## Full UI redesign — Phase 6 Mini + Scratchpad (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| UI-Ph6 | Mini Overlay + ScratchpadWindow Ivory primary single-palette | DONE | e84b671 | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed 20.02s; G4 `bash scripts/check_hygiene.sh` hygiene: OK; no-network OK; G3 zig pending (no zig, docs/QUESTIONS.md #1) | yes — screenshots owed: 960x640 / 760x520 @ DPI 1.0/1.5 + Scratchpad editor window, incl. real-hardware light-wallpaper check (Ph7 Windows manual) | Overlay `kDarkPalette` -> `kLightPalette` (using + 2x ApplyImGuiStyle + all Pack/Color 1:1 surface/border/text/muted/accent/ok/err/err_fill; alpha math byte-identical, surface_alpha now 0.90 light, badge 0.55/1.0 unchanged, drag 4px/focus/passthrough/state untouched); ScratchpadWindow `kStudioPalette` -> `kLightPalette` (2x ApplyStudioStyle -> ApplyImGuiStyle, card_bg_white/card_bg -> surface, teal_700 -> accent, ink -> text, muted -> muted, page_bg -> bg; studio_metrics fonts/radii kept, ApplyStudioStyle->ApplyImGuiStyle mapping comment with audit drift 32->16, 8->6, white->transparent); StudioPalette.h kept for HeatColor + studio_metrics in pages; test pins `:400 Pack(kDarkPalette.muted` -> `Pack(kLightPalette.muted` + ScratchpadWindow ApplyImGuiStyle/kLightPalette pin flip; HeatColor/contrast pins untouched GREEN |

## Full UI redesign — Shim deletion (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| UI-ShimDel | Delete deprecated StudioPalette struct/shim (true single-palette unify) | DONE | 1e5651a | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed; G4 `bash scripts/check_hygiene.sh` hygiene: OK; no-network OK; grep zero `kStudioPalette`/`ApplyStudioStyle` in src+tests; `ls src/ui/theme/` = ThemePalette.h/.cpp + ThemeStyle.h only; G3 zig pending (no zig, docs/QUESTIONS.md #1) | yes — screenshots owed: 960x640 / 760x520 @ DPI 1.0/1.5 + Scratchpad editor window (Ph7 Windows manual) | Moved verbatim (values unchanged) `studio_metrics`, `Clamp01`, `HeatLevel`, `HeatColor` (brass ramp #D9CFBA/#E4DDCD/#C3B599/#A28C64/#7A5C25), `UpperSpaced` (same `lwf::ui` namespace, names kept) to ThemePalette.h (ImGui-free); deleted `StudioPalette.h/.cpp` via `git rm` (+ CMake line); retargeted includes in 11 files; test pins: shim-field CHECKs -> `kLightPalette` values (accent==brass==focus), contrast 1:1 mapped (teal_300->HeatColor(3), teal_100->border), HeatColor-hex + studio_metrics-font + HeatColor-ramp pins kept, font-mechanism pin -> ThemePalette.cpp `ApplyImGuiStyle` markers, MainWindow/Overlay pins -> `ApplyImGuiStyle(kLightPalette` present; `test_scratchpad_page.cpp` needed no change (no StudioPalette-file reads; `studio_metrics::` strings still valid) |
\

## Full UI redesign — Phase 7 finalize (docs) (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| UI-Ph7 | Docs close: PROGRESS rows + UI_AUDIT dual-palette retirement note | DONE | (this commit) | G4 `bash scripts/check_hygiene.sh` hygiene: OK; no-network OK; G3 zig pending (no zig, docs/QUESTIONS.md #1); no build needed | yes — screenshots owed: 960x640 / 760x520 @ DPI 1.0/1.5 + Scratchpad editor window (Windows manual, MANUAL_TEST_WINDOWS.md R/E1) | Single-palette record closed: Ph2 (5736431) -> Ph3 (ac00dde, c0a079e) -> Ph4 (811c704) -> Ph5 (ab0763b/bee7243/28f16b4/1032490/379cc82/152d15c) -> Ph6 (e84b671) -> ShimDel (1e5651a); dual-palette (Hunter/Brass + teal Studio) retired, `StudioPalette` shim deleted, guardrails R5-R9 unchanged |

## Blank-audio hallucination filter port — engine-only (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| IMG-003P | Wire token-aware `WhisperText::FilterHallucinations` into `DictationEngine` (raw + post-pipeline + PasteLast) and `CommandModeController`, ported engine-only from `fix/mic-blank-audio` (`8934cdf`) without touching UI redesign files | DONE | (this commit) | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed (25.56s, 536 cases); new TEST_CASEs `blank audio marker yields nothing to inject`, `streamed blank audio marker yields nothing to inject`, `command mode blank audio marker is rejected` (injected 0, LastText empty, IDLE) + whisperfilter token/multiline/punctuation cases 6/6 green (47 assertions); mutation check: unfixed engine fails 2/2 new cases (marker injected, command LLM called); G4 `bash scripts/check_hygiene.sh` hygiene: OK; no-network OK; G3 zig pending (no zig on this box) | yes (say hello near mic on Windows; hardware capture-silence root cause still owed, per IMG-003) | `src/core/WhisperText.cpp` (token-aware filter) compiled via `lwf_core` + `tests/whisperfilter_test.cpp` wired into `lwf_tests`; `WhisperText.h` unchanged; scope: core 4 files + 2 test files + 2 CMake lines + this row only, no `src/ui/**`/`src/os/**` changes |

## Silent-mic visibility — peak gate + live level meter (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| MIC-001 | Surface silent mic as `no sound recorded` + live level meter on Speech page | DONE | (this commit) | G1 `cmake --build build --config Release --parallel 4 --target lwf_tests` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed (26.01s); `lwf_tests.exe` 538/538 passed (27914 assertions) incl 2 new cases `silent pcm yields no sound recorded instead of nothing to inject`, `loud blank transcript yields nothing to inject without silent gate` 2/2 (13 assertions); full exe `local-wisper-flow` Built target local-wisper-flow; G4 `bash scripts/check_hygiene.sh` hygiene: OK (+ --self-test OK); no-network OK; G3 zig pending (no zig on this box) | yes (speak near mic on Windows; watch level bar in Settings > Speech > Microphone level; if bar moves but still no words check log peak; if bar never moves fix OS default device/mute) | Engine gate in `StopAndTranscribe` after `min_tap_samples` check: PCM peak on `full` buffer, if `<0.005` log `audio: peak level <peak> < 0.005 — no sound recorded` (level only, never transcript) + `sink_.OnStatus(no sound recorded — check microphone)` + `FailToIdle` without STT; audible-but-blank still `nothing to inject`; no VAD, STT routing, whisper, filter, injector, config schema change. UI: new `##card_mic_level` after `##card_model` in `DrawSpeechPage`, rect meter surface fill + border + text fill bar, height `metrics::control_min_height * scale` (32), reads `audio_.AudioLevel()` each frame, never reads transcript; visible idle (0) and moves while recording. Scope: `src/core/DictationEngine.cpp`, `src/ui/MainWindow.cpp` (Speech only), `tests/test_engine.cpp` (+2 cases), this row |

## API key rows — cloud + cleanup (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| KEY-001 | Add/remove key UI for cloud + cleanup in Settings | DONE | (this commit) | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed 26.26s (incl 2 new cases `api key entry helpers trim, source and labels` + `api key rows use masked input and named removal`); full exe `local-wisper-flow` Built target local-wisper-flow (33 MB, links); G4 `bash scripts/check_hygiene.sh` hygiene: OK (+ --self-test OK); no-network OK; G3 zig pending (no zig on this box) | yes (Windows: save a key per service in Settings > Cloud > Saved keys and Settings > Cleanup > Cleanup backend; remove with named confirm; check restart banner) | Cloud card `##card_cloud_keys` (Gnani/ElevenLabs/Muse) under Service; Cleanup `##card_cleanup_backend` (OpenRouter + model hint `The model is set in config.json; see the Privacy tab.`); masked `Password` input, accent `Save key`, `Remove saved key` -> named `Remove saved <name> key?` + `Remove <name> key` / `Keep key` (focus on Keep, Esc cancels); dot+word+source (`Saved key`/`Environment key`/`Not set`); stale env-only copy replaced with `Saved keys take precedence over environment variables.`; buffers never seeded, cleared on commit + tab/page switch, never logged; scope `src/core/SettingsUiModel.*` (trim/source/labels) + `src/ui/MainWindow.*` + `tests/test_overlay_settings_ui.cpp` (+2 cases) + this row; `ConsentGate` untouched (OpenRouter pin `no env for openrouter` kept; UI shows env for parity with `main.cpp` fallback) |

## Mic device selection at the capture layer (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| MIC-002 | Enumerate capture devices, open the persisted `mic_device` by name with default fallback, log the active device name (Task A, audio core + wiring) | DONE | (this commit) | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Test #1: lwf-tests Passed 24.94 sec, 100% tests passed out of 1; full exe `cmake --build build --target local-wisper-flow --parallel 4` Building CXX object .../MiniaudioCapture.cpp.obj -> Linking CXX executable local-wisper-flow.exe -> Built target local-wisper-flow (2 pre-existing `-Wattributes` warnings from vendored gRPC headers only, none from this change); G4 `bash scripts/check_hygiene.sh` hygiene: OK; `bash scripts/check_no_network.sh` no-network: OK; G3 zig pending (no zig on this box) | yes (Windows: pick a named mic in Settings > Speech, restart (banner), say hello -> level meter moves and log has `MiniaudioCapture: active capture device: <name>`; put a stale name in config.json `mic_device` -> app still starts, log says `'...' not found; using system default capture device`) | `MiniaudioCapture` ctor is now `(sample_rate, channels, std::string device_name = "")` (additive; sole caller `main.cpp:336` passes `config.Store().GetString("mic_device")`); settings-load-order verified: `SettingsStore` ctor calls `Load()` (`SettingsStore.cpp:81-83`) from `ConfigManager` construction at `main.cpp:307`, before audio at `main.cpp:334` -> no reorder needed; owned `ma_context context_` member (init in ctor, context-init failure warns and degrades to the legacy null-context path; dtor uninits device then context, `isOwnerOfContext` makes double-uninit impossible); selection = first exact name match in `ma_context_get_devices` capture side, `ma_device_id` copied into a local before `ma_device_init` (miniaudio copies it into the device, miniaudio.h:43668-43673), init failure with a selected id retries once with `pDeviceID=NULL` so a stale id never blocks startup; `lwf::Log::Info("MiniaudioCapture: active capture device: <name>")` (`system default` when none selected/matched), name only, never audio; static `ListCaptureDevices()` uses a local RAII context and returns `{}` on any failure; `IAudioCapture`, `Fakes.h`, config schema (`mic_device` hot_apply=false unchanged), DataCallback/Start/Stop/levels byte-identical; no unit tests possible (TU is full-target-only, CMakeLists.txt:347-349) so adapter verified by gates + manual checklist; scope: `src/audio/MiniaudioCapture.{h,cpp}`, `src/main.cpp` (one site), this row |

## Microphone picker in Settings > Speech (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| MIC-003 | Replace the dead free-text `mic_device` row with a real microphone picker on the Speech page (Task B, settings UI) | DONE | (this commit) | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Test #1: lwf-tests Passed 27.30 sec, 100% tests passed out of 1 (incl 2 new cases `microphone picker index maps stored value to the combo entry`, `speech page microphone picker replaces the dead free-text row`); full exe `cmake --build build --target local-wisper-flow --parallel 4` exit=0 -> `Building CXX object .../MainWindow.cpp.obj` -> `Linking CXX executable local-wisper-flow.exe` -> `Built target local-wisper-flow`, zero warnings from MainWindow.cpp (only the pre-existing `-Wattributes` ones from vendored gRPC headers, none from this change); G4 `bash scripts/check_hygiene.sh` hygiene: OK; `bash scripts/check_no_network.sh` no-network: OK; G3 zig pending (no zig on this box) | yes (Windows: Settings > Speech > Microphone card lists `System default` plus every capture device; pick one -> `Saved` + restart banner; restart -> level meter moves while speaking and log has `MiniaudioCapture: active capture device: <name>`; pick `System default` again -> restart -> default device active) | New card `##card_mic_device` in `DrawSpeechPage` between `##card_model` and `##card_mic_level`; combo id `##mic_pick` (entry 0 = `System default` = stored value `""`, then `MiniaudioCapture::ListCaptureDevices()` names); DECISION: device list is enumerated ONCE per app run into `mic_devices_` behind `mic_devices_loaded_` (listing inits a `ma_context`, never per frame); select -> `SetString("mic_device", ...)` + `restart_needed_ = true` + `ApplyAndSave()` (schema `mic_device` hot_apply=false unchanged, so the restart banner stays the contract); pure helper `SettingsUiModel::MatchMicDeviceIndex(names, selected)` owns the index/fallback rule (`""`->0, `names[i]`->`i+1`, stale name->0); REMOVED dead plumbing: `StringFieldRow("mic_device", ...)` + help text in the General Sound card and member `mic_device_field_`; pins ADDED (none weakened, schema-key->control scan still satisfied by the new `SetString("mic_device"` occurrence): `##card_mic_device`, `##mic_pick`, card order model->mic->level, `ListCaptureDevices()` in MainWindow.cpp, `SetString("mic_device"` -> `restart_needed_ = true` -> `ApplyAndSave()` ordering, `mic_devices_loaded_ = true`, negative `StringFieldRow("mic_device"` absent, negative `mic_device_field_` absent in MainWindow.h, `MatchMicDeviceIndex` unit cases; scope: `src/ui/MainWindow.{cpp,h}`, `src/core/SettingsUiModel.{h,cpp}`, `tests/test_overlay_settings_ui.cpp` (+2 cases), `docs/MANUAL_TEST_WINDOWS.md`, this row |

## OpenRouter model-picker core seam (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| OR-CORE-01 | HTTP GET seam + OpenRouter models-list fetcher (Task A, core: no UI, no schema, no provider change) | DONE | (this commit) | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed (incl new case `openrouter models fetch: parse sort dedupe` 41 assertions; `*openrouter*` 24/24 cases 178 assertions); full exe `cmake --build build --target local-wisper-flow --parallel 4` main.cpp compiled + lwf_core (incl WindowsHttpClient::Get) linked, exe relink blocked: local-wisper-flow.exe running (PID 16308, Permission denied, not killed); mutation proof: sort removed -> RED at test_openrouter.cpp:481 `CHECK(result.modelIds == expected) is NOT correct`, restored -> GREEN; G4 `bash scripts/check_hygiene.sh` hygiene: OK; `bash scripts/check_no_network.sh` no-network: OK; G3 zig pending (no zig on this box) | yes (Windows: Refresh later, Task B; live GET /api/v1/models unverified-against-live, docs-verified contract only) | `IHttpTransport::Get` non-pure throwing default (LoopbackTransport bench + GateTransport untouched); `WindowsHttpClient::Get` mirrors Post with L"GET" and no body/Content-Type; new `llm/OpenRouterModels.{h,cpp}` (`OpenRouterModels::FetchOpenRouterModels`: Ok/Network/NonOk/BadJson/Empty, dedupe-first-wins, case-insensitive stable sort, RedactAuthValues+RedactSecret 500-char cap copied from OpenRouterProvider); `OpenRouterContract` additive `kModelsPath`/`kDataField` only; 1 CMake LWF_CORE_SOURCES line; ScriptedTransport Get override + get_requests_ in tests/test_openrouter.cpp; scope: src/stt/cloud/IHttpTransport.h + WindowsHttpClient.{h,cpp}, src/llm/OpenRouterModels.{h,cpp} + OpenRouterContract.h, CMakeLists.txt, tests/test_openrouter.cpp, this row |

## OpenRouter model picker UI (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| OR-UI-01 | Live OpenRouter model picker + manual id fallback in Settings > Cleanup (Task B, UI) | DONE | (this commit) | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed (incl 2 new cases `cleanup model index maps stored value to the fetched list` + `cleanup card OpenRouter model picker wires refresh and persistence`); full exe `cmake --build build --target local-wisper-flow --parallel 4` MainWindow.cpp + main.cpp compiled clean (zero warnings from this change; only pre-existing gRPC `-Wattributes`), exe relink blocked while local-wisper-flow.exe was running (Permission denied, not killed); final rebuild linked clean (`Built target local-wisper-flow`); mutation proofs: manual `restart_needed_ = true` dropped -> RED at test_overlay_settings_ui.cpp:1764, busy guard removed -> RED at :1776, both restored -> GREEN 545/545; G4 `bash scripts/check_hygiene.sh` hygiene: OK; `bash scripts/check_no_network.sh` no-network: OK; G3 zig pending (no zig on this box) | yes (Windows: Settings > Cleanup > Cleanup model: offline Refresh -> error + app still responsive; Refresh with key -> list loads; select -> restart -> cleanup uses model; type id + Use this model -> works; all UNVERIFIED-against-live) | Picker in `##card_cleanup_backend` after the key row, replaces pointer text `The model is set in config.json; see the Privacy tab.` (pin inverted to ASSERT ABSENT + `Cleanup model` present); combo `##openrouter_model` (synthetic current-value entry only when non-empty and not in the fetched list; `No model set` placeholder when empty+no list, selecting it clears via the same SetString path; sorted list after; `##or_<index>` item ids); status TextDisabled (`Model list not loaded.` / `Loading models...` / `<n> models available.` / error with redacted detail from Task A); `Refresh model list` worker-thread fetch via injected `IHttpTransport*` + `MainWindow::ModelListFetcher` (composition-root injection so src/ui stays free of the llm/ tree: the F9 copy lint scans every src/ui literal including #include paths, so `main.cpp` maps `OpenRouterModels::FetchOpenRouterModels` results onto UI primitives; the worker call itself is in MainWindow.cpp, the fetch mapping is in main.cpp — pin moved to main.cpp and documented); `openrouter_models_busy_` (atomic, UI-thread only) + mutex-guarded latch consumed each Cleanup frame; worker joined in `~MainWindow` (bounded by the 8s timeout); key resolution `ResolveOpenRouterKey()` mirrors main.cpp:376-382 (store-first, env fallback); manual `##or_custom` + `Use this model` (`TextField openrouter_model_field_`, seeded once, trimmed on commit, buffer synced on commit, never cleared); every select path is `SetString("openrouter_model", ...)` + `restart_needed_ = true` + `ApplyAndSave()` (schema hot_apply=false unchanged); Privacy empty-state copy is now `not set`; pure helper `SettingsUiModel::MatchModelIndex(fetched, selected)` (`""`/not-found/empty -> -1, case-sensitive); PIN deviations: `FetchOpenRouterModels` call-site pin lives in `src/main.cpp` (F9 copy-lint evidence: `llm/OpenRouterModels.h` include from `src/ui/MainWindow.h` failed F9 lint) |


## Gnani contract fix (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| GNANI-01 | Gnani STT client aligned to live docs contract: `/stt/v3` + `audio_file` + `language_code`, error envelopes parsed, attempts visible in file log | DONE | (this commit) | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed (incl 4 new cases: full live success envelope, auth detail object, 422 validation array, new error envelope type+message; explicit `/stt/v3` literal pin alongside const check); full exe `cmake --build build --target local-wisper-flow --parallel 4` Built target local-wisper-flow; mutation proof: `kPath` reverted to `/v1/asr/recognize` -> RED at the explicit `/stt/v3` CHECK, restored -> GREEN; grep stale shapes over src/ + tests/ = 0 hits; G4 `bash scripts/check_hygiene.sh` hygiene: OK; `bash scripts/check_no_network.sh` no-network: OK; G3 zig pending (no zig on this box) | yes (Windows: Gnani smoke with real key against live API still owed; mic-speaking verification owed) | Contract values from vendor docs checked 2026-11-08 (NOT yet against live API); `ExtractErrorDetail` reads error type+message, detail-object message (+error_code), first 422 array msg only, status classifier untouched; `LogAttempt` keeps cerr line (+detail=) and also writes `[stt] provider=gnani request_id=... status=... error=... detail=...` via `lwf::Log::Warning` (detail already redacted, 200-char display truncation, never transcript); out of scope untouched: startup-snapshot routing, SttRouter, DictationEngine peak gate, SettingsStore/UI, key resolution |

## Gnani keyterms single-field polish (2026-11-08, `ops/ui-full-redesign`)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| GNANI-02 | Gnani `keyterms` as single JSON-array field + 422 fallback detail + UTF-8-safe log truncation | DONE | (this commit) | G1 `cmake --build build --target lwf_tests --parallel 4` Built target lwf_tests; G2 `ctest --test-dir build -R lwf-tests --output-on-failure` 1/1 Passed (incl 2 new cases: single JSON-array keyterms in order, 422 no-msg/no-type fallback; suite now 551 cases); full exe `cmake --build build --target local-wisper-flow --parallel 4` relink blocked: local-wisper-flow.exe running (Permission denied, not killed); mutation proof: repeated keyterms -> RED at `CountOccurrences(body, "name=\"keyterms\"") == 1` (2 == 1) + parse discarded, restored -> GREEN; explicit `/stt/v3` literal pin still green; G4 `bash scripts/check_hygiene.sh` hygiene: OK; `bash scripts/check_no_network.sh` no-network: OK; G3 zig pending (no zig on this box) | yes (Windows: Gnani smoke with real key, keyterms server acceptance unverified-against-live) | `BuildGnaniRequest` collects SafeKeyterm survivors (same filter + `kKeytermsCap`=200, order kept) and emits ONE `keyterms` field via `nlohmann::json` dump, omitted when empty; `ExtractErrorDetail` 422-array fallback `validation error (422)` when msg/type unparseable (statuses untouched); `TruncateDetail` cuts at UTF-8 boundary, stored `attempt.detail` untruncated; ElevenLabs repeated parts untouched |

## UI redesign v2 — Hairline (2026-11-09, ops/ui-redesign-v2)

| id | title | status | commit | gates | manual-check | notes |
|----|-------|--------|--------|-------|--------------|-------|
| BASELINE | Branch cut from HEAD 51807b5; pre-change baseline gates | RECORDED | 51807b5 | `cmake --build build --target lwf_tests` Built; `ctest --test-dir build -R lwf-tests --output-on-failure` Passed 1/1 in 27.39s; `scripts/check_hygiene.sh` OK; `scripts/check_no_network.sh` OK; G3 zig pending (no zig on dev box) | no | Full `cmake --build build -j4` currently fails pre-existing in vendored gRPC re2 `testing` target (`pcre.h` missing `<cstdint>`) — do not touch, recorded as pre-existing |
| T0 | Supersede old redesign plans + stamp external GUI skill doc + PROGRESS record | DONE | (this commit) | `bash scripts/check_hygiene.sh` OK; `bash scripts/check_no_network.sh` OK (rerun for T0); no build/test (docs-only) | no | Banners only, no content deleted, no source/config change; external skill stamp `C:\Users\devan\.config\opencode\skills\skills-cpp-GUI.md` is a non-repo file (cannot be committed with the repo) |
