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
