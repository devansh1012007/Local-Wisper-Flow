# Phase 0 — base branch decision (STT cloud providers work)

Status: **awaiting Devansh's decision (A / B / C in section 6)**. Written 2026-10-31 from a fresh clone of `github.com/devansh1012007/Local-Wisper-Flow`. Phase 0 changed no source files; the only artifact is this document.

## 1. What the fresh clone sees

- The GitHub remote exposes exactly one ref: `refs/heads/main` → `e60cd2a3a8c3314508cc0d0923b30682effe7dbb` ("Add unit tests for dictionary, engine, and various components", authored 2026-10-28 +0530). No other branches, no tags (`git ls-remote` verified).
- The 7 local commits are **not on GitHub**. They exist only on Devansh's machine, so this clone cannot inspect them and a commit-level comparison is impossible until they are pushed.
- Topology (derived from the stated "ahead 7 / behind 1" and the observed history): the merge base is `8c8b203` ("Phase 5 completed"); origin/main adds one commit on top of it (`e60cd2a`); local main adds 7 unpushed commits on top of `8c8b203`.
- `e60cd2a` touches exactly 64 files (matches the task brief) and — despite its "unit tests" message — carries the full engine-layer implementations, not only tests.

## 2. Feature-by-feature: does origin/main already have the 7 local commits' work?

| Feature | On origin/main | Files (new in `e60cd2a`) | Key functions / cases | Verdict |
|---|---|---|---|---|
| DictationEngine | yes | `src/core/DictationEngine.{h,cpp}` (129 + 245 lines) | ctor `(IAudioCapture&, IInferenceProvider& stt, DictationCleanup&, ITextInjector&, DictationFSM&, IVadProvider*, EngineSettings, IEngineSink*)`, `Run`, `Sink`, `LastText`; private `HandleEvent`, `PollCapture`, `DrainGate`, `BeginCapture`, `FinishCapture`, `FailToIdle`, `TranscribeAndInject`; `EngineErrorCode`, `IEngineSink` / `EngineStatusSink`, `EngineSettings` | Present |
| WhisperText | yes | `src/core/WhisperText.{h,cpp}` (11 + 88 lines) | `WhisperText::FilterHallucinations`, `WhisperText::IsOnlyHallucination` | Present |
| PreRollBuffer | yes | `src/audio/PreRollBuffer.h` (header-only, 49 lines) | `Write`, `Snapshot`, `Clear`, `Size`, `Capacity` | Present (lives in `src/audio/`, not `src/core/`) |
| Tests | yes | 12 test files + `tests/fakes.h`, wired into the `lwf-tests` doctest target (`add_test` in `CMakeLists.txt`) | 81 cases: engine 14, cleanup 15, vadgate 7, config 8, dictionary 8, apppaths 6, transform 5, router 4, utf8 4, preroll 4, whisperfilter 3, fsm 3; `fakes.h` provides fake capture / STT / injector | Present |

`e60cd2a` also carries work **beyond** the four named areas — things the 7 local commits may or may not also contain: `PersonalDictionary`, `DictationCleanup` with its output guard, `VadGate` (512-sample chunking, head/tail padding, `SpeechInterval`), `EngineEvent` + `EngineEventQueue` (including `Cancel`), `AudioPipe`, `Utf8`, `src/os/AppPaths`, `src/os/WindowsClipboardInjector`, `scripts/check_hygiene.sh`, `scripts/fetch_deps.sh`, the CI workflow update, the README, a `main.cpp` rewrite, and server-side tweaks.

**Caveat:** this table is a feature-level comparison against the task brief's description of the 7 local commits. The unpushed commits themselves cannot be read from here. Only Devansh (or a pushed backup branch) can confirm that `e60cd2a` fully covers them; anything in the 7 commits that is not reflected above becomes a porting candidate under option A.

## 3. Options

### A — base `feature/stt-providers` on origin/main (`e60cd2a`), port only the missing pieces [recommended]

- Needs from Devansh: push local main to a backup branch (e.g. `backup/local-main`) so the 7 commits can be diffed against `e60cd2a`, or simply confirm that `e60cd2a` supersedes them.
- Pros: the baseline already integrates engine + tests + hygiene script + CI; every hook the STT plan needs is present (section 4); zero conflict resolution; nothing that exists only on origin/main is lost.
- Cons: any unpushed refinement inside the 7 commits is invisible until the backup branch is diffed — hence the backup push.

### B — rebase the 7 local commits onto origin/main

- Needs from Devansh: push local main first.
- Pros: preserves the incremental 7-commit history.
- Cons: all 7 commits overlap the 64 files of `e60cd2a` (same subject matter, including a `main.cpp` rewrite), so expect conflicts at essentially every step; intermediate commits cannot be verified on Windows from this sandbox; slowest path given the Nov 10, 2026 11:59 PM IST deadline (~10 days out).

### C — keep both branches separate

- Cut `feature/stt-providers` from local main instead.
- Cons: that branch would miss `e60cd2a`-only work the STT plan directly depends on — `scripts/check_hygiene.sh` (enforces the ground rules), the doctest suite + `tests/fakes.h` (the template for the new Linux tests), the ConfigManager additive-key pattern (G9 backward compatibility), `AppPaths`, `WindowsClipboardInjector`. Most of `e60cd2a` would still have to be ported, making this option A with extra steps. Not recommended.

## 4. Facts the STT plan relies on (verified on origin/main, applies to whichever base)

- Engine seam: `DictationEngine` takes `IInferenceProvider&` (`src/core/IInferenceProvider.h`: `Transcribe(const std::vector<float>&)`, `ProcessText`). S4 swaps the STT seam to `ISttProvider` / `SttRouter` behind an adapter so the existing engine tests keep compiling.
- Cancel path: `EngineEventType::Cancel` already exists (`src/core/EngineEvent.h`); Esc → `CancelToken` wiring lands in S3/S4.
- Hallucination filter: `WhisperText::FilterHallucinations` is currently applied inside `LocalWhisperProvider::Transcribe` (`src/stt/LocalWhisperProvider.cpp:96`) — the planned move into the engine (so it covers every provider) is contained.
- ClipSplitter input: `VadGate` exposes `SpeechInterval()` / `SilenceMs()` today, not per-sample silence candidates. S1 needs a small `VadGate` extension to emit silence-candidate sample indexes — noted now so it is not discovered late.
- Keyterms source: `PersonalDictionary::Words()` (deduplication is already tested). `kPromptCharCap = 600` caps the LLM prompt; the 200-item keyterms cap for Gnani is new.
- Config baseline (G9): current keys end with `dictionary_path`, `avoid_altgr`, `whisper_*`; `config_test.cpp` already asserts "config v0 keys still load" and "config new keys load when present" — the pattern the new `stt_*` keys must follow.
- HTTP + JSON (G7): no `IHttpClient` exists yet; WinHTTP will be a new `src/os/` pair. `nlohmann/json` is already FetchContent-pinned and linked into both `local-wisper-flow` and `lwf-tests`, so no new dependency is required.
- Network allowlist: `src/stt/` today contains only `LocalWhisperProvider` and `GrpcInferenceProvider`; the new `src/stt/cloud/*` files plus `src/os/WindowsHttpClient.*` will define the allowlist surface for the new gate.

## 5. Minor repo observations (not Phase 0 scope; logged for later)

- `server/__pycache__/server.cpython-312.pyc` is tracked by `e60cd2a` even though `.gitignore` was updated in the same commit — candidate for removal in a later hygiene commit.
- AGENTS.md calls `.github/workflows/ci.yml` a stale leftover of an unrelated Python project, but `e60cd2a` modified it (+72 lines) — reconcile during the S9 docs pass.
- `e60cd2a`'s message ("Add unit tests…") understates its scope (it carries full implementations) — worth knowing when reading history.

## 6. Decision (to be filled in by Devansh)

- [ ] A — base on origin/main, port only the missing pieces (recommended)
- [ ] B — rebase the 7 local commits onto origin/main
- [ ] C — keep both branches separate

Chosen: **PENDING**.

Regardless of the choice: push local main to `backup/local-main` (or similar) first, so the 7 commits are safe and diffable. After the decision, `feature/stt-providers` will be cut from the chosen base, a diff report of the 7 commits versus the base will be produced if the backup branch is available, and S0 (`scripts/gnani_smoke.py`, `tests/data/README.md`) starts, stopping at CHECKPOINT S0.
