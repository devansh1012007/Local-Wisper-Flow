# Plan: Linux/Ubuntu port — daemon + TUI control plane (no GUI)

- Status: READY FOR APPROVAL
- Tier: L
- Repo state: branch `linux/ubuntu-24.04`, commit `ec597f0`, date 2026-11-21
- Spec: operator message — make Ubuntu work; NO GUI windows; TUI for Windows-only settings/features; background working with hot key.
- Operator / agent: operator / opencode
- Staleness rule: if the repo moves the files listed in section F, re-verify section C and F before acting.
- Plan path: docs/plans/2026-11-21-ubuntu-linux-port-tui.md

## A. Problem, goal, success criteria
- Problem: on Ubuntu the app is a stub (src/main.cpp:926-941 [FACT]) that prints `Echo flow core build: ...` and exits 0. No hotkey, audio capture, STT engine, or text injection exists on Linux. Windows sources/CI untouched [FACT ci.yml windows job].
- Goal: background daemon: global hotkey → record audio → transcribe via local whisper (cloud opt-in behind consent gate) → text lands in focused app; interactive TUI exposing/editing every setting and feature toggle that exists on Windows.
- Success criteria (observable, each with a test ID in H):
  1. `./EchoFlow --daemon` runs in background; hotkey press starts/stops a session (notify-send toast + log line).
  2. Session → local whisper transcript → text appears in focused app: XTest direct type on X11; clipboard copy + "press Ctrl+V" toast on Wayland [Q1 c].
  3. TUI lists/persists every editable config key and feature toggle from config.json; daemon reloads without restart [Q4 c, Q5 c].
  4. CI `ctest --test-dir build` green on the Ubuntu job; build has zero new warnings [G1/G2/G4].
  5. Manual checklist on real Ubuntu: hotkey → transcript → text landed.
- Non-goals (explicitly not doing): Windows GUI/behavior untouched; GUI windows on Linux (spec: NO GUI); tray icon / StatusNotifierItem; .deb/AppImage packaging (L8 deferred, Q1 open); cloud STT remote transport on Linux deferred post-MVP.
- Constraints: `LWF_BUILD_FULL` forced ON on Windows stays [FACT CMakeLists.txt:35-37]; secrets only via `api_keys.json`/env, never `config.json` [FACT AGENTS.md]; config schema additive-only [FACT AGENTS.md].
- Reversibility: two-way — Linux-only new files; main.cpp stub and CMakeLists changes Linux-guarded; rollback = remove new files + revert CMakeLists/src/main.cpp.

## B. Glossary and reading guide
None needed — terms are from existing code (hotkey, STT, VAD, inject, daemon, TUI).

## C. Architecture of the codebase (the part that matters here)
- Module map and responsibilities (5-10 lines):
  - Portable core `lwf_core`: DictationEngine src/core/DictationEngine.cpp:101 [FACT], SttRouter src/stt (cloud first w/ one-way fallback SttRouter.cpp:29-54 [FACT]), SettingsStore::BuildSchema src/core/SettingsStore.cpp:14-75 [FACT], interfaces IInferenceProvider/ITextInjector/IHotkeyListener/IAudioCapture/IClock [FACT pattern finder], CrashReporter src/core/CrashReporter.cpp.
  - Windows-only adapters compiled out on Linux: WindowsHotkeyListener src/os/WindowsHotkeyListener.cpp:96-157 (hook → EngineEvent ring :136 [FACT]), WindowsTextInjector src/os/WindowsTextInjector.cpp:339-364 (inject, clipboard restore :326-336 [FACT]), WindowsTray, WindowsClipboardInjector, WindowsDownloader — all `if(WIN32)` CMakeLists.txt:376-399 [FACT].
  - Linux stub main src/main.cpp:928-941: ConfigManager("config.json") → banner → exit 0 [FACT].
- Entry points and execution flow today, with `[FACT file:symbol]`:
  - Windows: hotkey hook → EngineEvent ring → HandlePress → BeginCapture → MiniaudioCapture::DataCallback src/audio/MiniaudioCapture.cpp:253 → LocalWhisperProvider::RunWhisper src/stt/LocalWhisperProvider.cpp:74-118 → InjectText → history/logs.
  - Linux today: stub prints banner, exits 0.
- Data model and persistence touched: config.json read by SettingsStore::BuildSchema [FACT]; history.jsonl via HistoryStore; logs at `~/.local/share/EchoFlow/logs/lwf.log` analog [FACT DataDir.cpp:84-104].
- Properties and invariants: engine/core unchanged verbatim between Windows and Linux paths; SpscRing<EngineEvent> shared by hotkey thread and engine thread (as Windows); schema keys additive-only [FACT AGENTS.md].
- Conventions to follow (with example file): additive-only schema keys (SettingsStore.cpp); secrets redacted/masked (Lwf::RedactSecret) [FACT AGENTS.md]; no GUI windows per spec; docs/UI_AUDIT context-sharing rule N/A (no GLFW/ImGui on Linux).

## D. Proposed design
- Summary (what, in 5 lines):
  - Keep `lwf_core` verbatim; Ubuntu CI builds with `LWF_BUILD_FULL=ON` (whisper + miniaudio + llama).
  - Single binary `EchoFlow`: `--daemon` runs the background session supervisor (hotkey + capture + engine + injector + notifier, event loop, does not exit); default runs the interactive ncurses TUI editing config.json.
  - Hotkey: X11 `XGrabKey` on root window primary; Wayland via evdev opt-in (/dev/input, input-group privilege).
  - Injection: XTest `XSendKey` direct type into focused window on X11; Wayland: clipboard copy (`wl-copy`) + notify-send "press Ctrl+V" toast.
  - Daemon owns config.json via atomic replace [B3 fix]; TUI reloads live via SIGHUP/inotify through ConfigManager [OPINION coupling choice — verify SettingsStore atomic write at spike].
- Before flow / After flow:
  1. Before: stub prints banner exits 0 [FACT src/main.cpp:930-936].
  2. After (daemon): `./EchoFlow --daemon` grabs hotkey via X11 `XGrabKey` root window (primary); Wayland evdev opt-in needs /dev/input + input group [FACT UBUNTU_DECISIONS.md:46-52, R1]; feeds SpscRing<EngineEvent> like WindowsHotkeyListener.cpp:136 [FACT]; press → HandlePress → BeginCapture DictationEngine.cpp:458-531; release → StopAndTranscribe DictationEngine.cpp:533-714 → LocalWhisperProvider::Transcribe (local; cloud opt-in via gRPC only with consent+key SttRouter.cpp:29-54) → LinuxTextInjector: XTest `XSendKey` direct type X11 [FACT libxtst dev installed UBUNTU_DEPS.md:37], clipboard copy + notify-send "press Ctrl+V" toast Wayland.
  3. After (interactive): `./EchoFlow` → TUI lists/edits hotkey_code/hotkey_keysym, whisper_model_path, vad_threshold, stt_provider/consent (keys masked via api_keys.json/env [FACT AGENTS.md]), VAD ONNX toggle, log viewer tail, status line idle/listening/transcribing; edits persist to config.json atomically; daemon reloads without restart.

## E. Change list (atomic, ordered)
| ID | File and symbol | WHAT changes | HOW | WHY this and not the alternative | Depends on | Verified by (test ID) | Risk | Revert |
|---|---|---|---|---|---|---|---|---|
| C01 | CMakeLists.txt | Allow `LWF_BUILD_FULL=ON` on Linux; add Linux sources + libs (X11, Xtst, libnotify, libncurses) | Ubuntu CI passes `-DLWF_BUILD_FULL=ON`; Windows default unchanged | Windows force stays ON [FACT :35-37]; keeps default builds light on Linux | none | lwf-tests green; binary links | low | revert block |
| C02 | src/main.cpp | Linux `main()`: `--daemon` flag → daemon loop with composition root wiring adapters (hotkey listener, capture, engine bridge, STT provider, injector, notifier); default → interactive TUI; `--smoke` flag prints banner + exits 0 (preserves L0 smoke) | `#else` block argc parsing; daemon starts adapters + engine event loop | single binary both modes [Q3 c]; `--smoke` keeps xvfb smoke contract [B5] | C01 | compile Linux; smoke grep passes | med | revert stub |
| C03 | src/os/LinuxHotkeyListener.cpp/h (new) | Global hotkey on X11 | XGrabKey root window; dispatch into EngineEvent ring [like WindowsHotkeyListener.cpp:136] | Q2 b | C02 | manual hotkey press test | med | delete file |
| C04 | src/os/LinuxHotkeyEvdev.cpp/h (new) | Wayland hotkey opt-in | read /dev/input/event* keysyms (input group privs) | Q2 b Wayland | C03 | manual test privileged | high | delete file |
| C05 | src/os/LinuxTextInjector.cpp/h (new) | Text injection | XTest `XSendKey` direct type X11; `wl-copy` clipboard + notify-send toast Wayland | Q1 c | C02 | manual injection test | med | delete file |
| C06 | src/os/LinuxNotifier.cpp/h (new) | Tray replacement | libnotify `notify-send` wrapper (toasts, elevated-app warnings, status) | tray→TUI+toast [Q5 c, B7] | C02 | manual | low | delete file |
| C07 | src/ui/tui/*.cpp/h (new) | TUI settings screen | ncurses [OPINION lib choice]: keys/toggles/log viewer/status; writes config.json; single-reader, daemon single-writer | Q4 c, Q5 c, B3 | C02 | compile; lwf-tests unaffected | med | delete dir |
| C08 | scripts/install_ubuntu_deps.sh + docs/UBUNTU_DEPS.md | Add libncurses-dev; ONNX Runtime prebuilt fetch step; silero_vad.onnx note | offline recipe mirroring gRPC tarball recipe [FACT AGENTS.md deps] | Q6 b | CI | script idempotent | low | revert |
| C09 | SettingsStore schema + config.json defaults | Additive keys: `hotkey_keysym` (int default 0 = unmapped on Linux), daemon flags; Windows `hotkey_code` VK semantics not honored on Linux | BuildSchema additive-only [FACT AGENTS.md] | avoids `hotkey_code` Windows↔Linux collision [B2] | C07 | config unit tests | low | revert schema |
| C10 | .github/workflows/ci.yml linux-ubuntu-24 job | `-DLWF_BUILD_FULL=ON`; ONNX Runtime + silero_vad.onnx present | job steps | Q6 b/Q7 b | C01,C08 | ctest green + smoke banner | low | revert ci.yml |

Ordering rationale: C01/C02 make a runnable binary; C03-C05 give end-to-end; C07 control plane; C08-C10 CI/deps. System works/testable at each step.

## F. Files affected
| File | Change type | Change IDs | Why it must change | Who else depends on it |
|---|---|---|---|---|
| CMakeLists.txt | modify | C01 | Linux full build | everything |
| src/main.cpp | modify | C02 | entry modes + composition root + --smoke | C03-C07 |
| src/os/LinuxHotkeyListener.cpp/h | new | C03 | hotkey X11 | C02 |
| src/os/LinuxHotkeyEvdev.cpp/h | new | C04 | hotkey Wayland opt-in | C03 |
| src/os/LinuxTextInjector.cpp/h | new | C05 | injection | C02 |
| src/os/LinuxNotifier.cpp/h | new | C06 | tray→toast | C02 |
| src/ui/tui/* | new | C07 | settings/features control plane | C02 |
| scripts/install_ubuntu_deps.sh | modify | C08 | deps | C01,C06 |
| docs/UBUNTU_DEPS.md | modify | C08 | deps list | — |
| docs/UBUNTU_TEST_MATRIX.md | modify | C10 | smoke update | C08,C10 |
| .github/workflows/ci.yml | modify | C10 | full Ubuntu job | C01,C08 |

## G. Alternatives and decisions
| ID | Decision | Options considered | Chosen | Why chosen | Why others rejected | What would change our mind |
|---|---|---|---|---|---|---|
| D1 | NO GUI windows; TUI control plane | (a) GLFW/ImGui port (L4), (b) TUI, (c) config-file only | TUI [Q4 c, Q5 c] | spec: NO GUI; runtime settings needed; notify-send/TUI = desktop integration not GUI windows [B7] | GUI port out of scope; config-only no runtime control | operator override |
| D2 | Single binary: daemon + interactive | (a) daemon only, (b) interactive only, (c) both | both [Q3 c] | matches "background working" + accessible settings | daemon-only inaccessible settings | — |
| D3 | Hotkey X11 primary + evdev Wayland opt-in; headless = no hotkey documented limit | X11 only / evdev only / both | both primary X11 [Q2 b, B4] | Wayland lacks GlobalShortcuts portal [FACT UBUNTU_DECISIONS.md:46-52] | evdev-only needs privileges | — |
| D4 | Injection XTest X11; clipboard+Ctrl+V toast Wayland | direct-type only / clipboard only / both | both [Q1 c] | X11 supports XTest; Wayland has no inject path | clipboard-only slower on X11 | — |
| D5 | VAD: fetch ONNX Runtime + silero_vad.onnx; degrade gracefully if offline fails | optional-disabled / fetch | fetch [Q6 b, B6] | VAD gating requested | disabled | offline fetch fails |
| D6 | LLM cleanup: llama on Linux | out of scope / llama on Linux | llama on Linux [Q7 b] | Windows cleanup parity | out of scope | llama ExternalProject fails on Linux |
| D7 | `--smoke` flag preserves L0 smoke contract (banner + exit 0 under xvfb) | keep stub / new flag / drop smoke | new flag [B5] | xvfb smoke must keep passing after stub replaced | drop smoke breaks L0 CI | operator drops L0 smoke |

D# agreed with operator via Q1-Q7 answers (Round 1: Q1 c, Q2 b, Q3 c, Q4 c, Q5 c, Q6 b, Q7 b). Rejected ideas recorded so nobody reopens them blindly.

## H. Tests and validation
- What existing tests guarantee / do not guarantee: lwf_tests 540 cases portable core [FACT test auditor]; Linux CI job ctest [FACT ci.yml:83]; Windows-only adapters carry no unit tests today [FACT].
- Test traceability: requirement/risk -> test:
  - T01 manual/e2e: Ubuntu machine: hotkey → transcript → text landed; expected signal = notify-send toast + history.jsonl + focused-app content. Proves C03-C05.
  - T02 manual: TUI changes a key → daemon applies without restart; expected signal = log line "config reloaded". Proves C07+C09+B3.
  - T03 manual: Wayland → clipboard copy + "press Ctrl+V" toast appears; expected signal = toast + clipboard content. Proves C05.
- Baseline: lwf-tests passes (G2 green); build zero new warnings (G1 G4 hygiene); Windows CI untouched.
- Non-test validation and pass criteria: `./EchoFlow --daemon` runs background, silent except logs; `--smoke` prints banner + exits 0 under xvfb [B5]; config.json unchanged for untouched keys.

## I. Edge cases and failure modes (only the important ones, derived from this code)
| Case | Where it bites | Expected behavior | Handled by (change/test) |
|---|---|---|---|
| Hotkey already grabbed | LinuxHotkeyListener::Start | fail fast, notify-send error + log | C03+C06 |
| No mic device | MiniaudioCapture fallback [FACT] | engine degrades no-gating [AGENTS.md:175] | existing engine |
| whisper model missing | ResolveModelPath fallbacks | ModelMissing mode, app continues [AGENTS.md:81] | existing |
| Elevated target app (sudo) | inject blocked | elevated notification via notify-send | C05+C06 mirroring WindowsForegroundIsElevated |
| Wayland no inject path | LinuxTextInjector | clipboard copy + "press Ctrl+V" toast | C05 |
| config.json syntax error | SettingsStore::Load | keep previous valid config, error toast | existing |
| Torn config read (TUI vs daemon) | SettingsStore | atomic replace + SIGHUP/inotify reload single-writer | C02+C07+C09 [B3] |

## J. Impact, second-order effects, compatibility
- Blast radius (callers, interfaces, schema, config, CI, docs): Linux build only; Windows CI/jobs untouched [FACT ci.yml windows job]; lwf_tests sources unchanged.
- Backward compatibility and migration/backfill: schema additive-only [FACT AGENTS.md]; `hotkey_code` keeps Windows VK semantics; new `hotkey_keysym` Linux-specific [B2].
- Concurrency and error-handling impact: daemon engine thread + hotkey thread share SpscRing (as Windows); TUI reads config.json only (no socket coupling) [OPINION].
- Performance and cost impact: daemon idle ~0% CPU; ONNX Runtime + llama add build time/deps size.
- Security and privacy impact: global hotkey grab = keylogger-adjacent (document opt-in); clipboard transient restored after inject_delay_ms [mirrors WindowsTextInjector.cpp:326-336]; notify-send carries no transcripts; api_keys.json separate [FACT AGENTS.md].
- Second-order effects: daemon+TUI split keeps GUI-free assumption; future GUI port reuses engine+adapters.

## K. Rollout, rollback, observability (mandatory for L)
- Rollout order and flags: land on linux/ubuntu-24.04; CI job linux-ubuntu-24 runs full build + ctest; manual test on real Ubuntu before merging to main.
- Rollback procedure and point of no return: remove new files C03-C07 (`src/os/Linux*.{cpp,h}`, `src/ui/tui/*`), revert C01/C02 (`CMakeLists.txt`, `src/main.cpp`) and C08-C10 (deps scripts/docs, ci.yml); `main` and Windows unaffected. Point of no return: none before merge.
- How we will know it works or is failing: logs at `~/.local/share/EchoFlow/logs/lwf.log` analog [FACT DataDir.cpp:84-104]; notify-send toasts; TUI status/log viewer.

## L. Risks, assumptions, open questions
| ID | Type | Description | Likelihood x impact | Mitigation or how to verify | Owner | Status |
|---|---|---|---|---|---|---|
| R1 | risk | Wayland evdev hotkey needs input-group privilege (`usermod -aG input`) | Medium x High | document opt-in; X11 primary works unprivileged | operator | open |
| R2 | risk | ONNX Runtime + silero_vad.onnx offline fetch may fail | Medium x Medium | offline tarball recipe like gRPC; degrade gracefully (VAD off, engine works) | operator | open |
| R3 | risk | hotkey_code VK vs Linux keysym mismatch | Medium x Medium | new `hotkey_keysym` key + mapping table | operator | open |
| A1 | assumption | libncurses5-dev installable on noble | — | install script adds it | agent | open |
| Q1 | question | .deb/AppImage packaging (L8) deferred — acceptable? | — | confirm | operator | open |
| Q2 | question | Spike SettingsStore atomic write + SIGHUP/inotify reload before C07/C09? | — | confirm | operator | open |

Pre-mortem top causes of failure and mitigations:
1. hotkey grab conflict → notify-send error + fallback message [B1 mitigated by composition root wiring in C02].
2. model path resolution fails → ModelMissing handled, T01 fails visibly [AGENTS.md:81].
3. Wayland inject silently fails → clipboard fallback documented.
4. daemon/TUI config reload race → atomic write + SIGHUP/inotify [B3 mitigated by C02+C07+C09].
5. ONNX missing → VAD off, engine still works [AGENTS.md:175].

## M. Decision log and discussion changelog
- D1-D8: agreed with operator via Q1-Q7 answers (Section G), dated 2026-11-21.
- Rejected ideas (so nobody reopens them): GUI port; config-only TUI-less; clipboard-only injection; daemon-only; VAD disabled; dropping smoke.
- Round changelog: initial draft (red-team fixes B1-B8 applied), cold-read test passed [task output], operator approval pending.

## N. Approval record
- Readiness checklist result: all items true; blocking questions answered; success criteria observable via T01-T03; every change entry has file/symbol/what/how/why/dependency/test/revert; assumptions have early/cheap verification (ncurses probe, ONNX fetch step); cold-read subagent answered all six questions CONFIDENT; operator read pending.
- Accepted risks (explicit): R1-R3.
- Operator approval: PENDING — reply `APPROVED` (optionally with changes) or tell me what to revisit.
