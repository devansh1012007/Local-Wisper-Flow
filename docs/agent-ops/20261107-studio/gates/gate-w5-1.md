# Gate record — W5 integration (gate-w5-1)

Owner: W5-IMPL-INT · Branch: `feature/studio-pages`
Scope: integrate W5-IMPL-O (T-026 OpenRouter provider, 86605b7+baa650b) +
W5-IMPL-P (T-021 F9 sweep, 69c8358+784edfb+fb1a041) + W4-FIX-RESEED Privacy
copy NEED (f66943b). One file owned: CMakeLists.txt, src/ui/MainWindow.cpp.

## Commands (verbatim outcomes)

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` → `Configuring done (2.0s)`,
  `Generating done (10.7s)`, `Build files have been written`. ONNX Runtime
  absent (VAD targets skipped) — expected per AGENTS.md, unchanged by this wave.
- `cmake --build build --config Release --parallel 4` → `BUILD-EXIT:0`.
  `Built target lwf_tests` + `Built target local-wisper-flow`: the exe link
  blocker W5-IMPL-P filed (undefined OpenRouterProvider) is RESOLVED by the
  CMake registration below. The only warnings are the pre-existing gRPC
  `empty_bases` notes in main.cpp's grpc include chain (named by P's report;
  not mine).
- `ctest --test-dir build --output-on-failure` → `100% tests passed out of 23`,
  `Total Test time (real) = 660.70 sec`, `CTEST-EXIT:0`.
- `bash scripts/check_hygiene.sh` → `hygiene: OK`.
- `bash scripts/check_no_network.sh` → `no-network: OK`.
- Spot check `.\build\lwf_tests.exe --test-case="*openrouter*"` →
  `test cases: 19 | 19 passed`, `assertions: 118 | 118 passed`, `Status: SUCCESS!`
  (18 cases from tests/test_openrouter.cpp + 1 matching case elsewhere).

## What this wave changed (integration only)

- **CMakeLists.txt** (O NEED-1, exact lines): `src/llm/OpenRouterProvider.cpp`
  added to `LWF_CORE_SOURCES` after `src/stt/cloud/MuseProvider.cpp`; `tests/test_openrouter.cpp`
  added to `lwf_tests` between `test_onboarding.cpp` and `test_overlay_settings_ui.cpp`
  (alphabetical). No font_sources addition (provider TUs contain zero imgui).
- **src/ui/MainWindow.cpp**: the working tree already carried O's Privacy/About
  egress statements + P's 5 copy-lint hunks (Home/Cloud/Consent). This wave
  added only the missing RESEED Privacy copy fix (line 1354): `restart to
  reseed empty files` clause dropped — the dictionary tombstone (f66943b) keeps
  delete-all empty across restart, so the reseed clause no longer describes
  it. Result: `This cannot be undone. The app keeps running.` No `!`, no
  internal terms, local-first promise kept (`Local-first voice dictation. ...
  All processing stays on this machine unless OpenRouter cleanup is enabled.`
  in About; `By default no network calls except explicit model downloads.
  OpenRouter cleanup is the only opt-in exception.` in Privacy). No invented
  copy: every other sentence is O's or P's verbatim.
- **DECISION-1 verification** (O's consent model — do NOT silently change): the
  committed code matches the stated model. `src/main.cpp:375` openrouter branch
  resolves the key via `api_keys.Get("openrouter")` → `getenv(OPENROUTER_API_KEY)`
  and never touches `stt_consent_accepted` or `EvaluateCloudConsent`
  (STT gate call site at main.cpp:467-469 is a separate, untouched block);
  `src/core/ConsentGate.cpp` is unchanged (api_keys table has no openrouter
  env fallback). Provider selection + model + key is the opt-in; Privacy/About
  conditional disclosure is in this same tree. If Devansh wants a separate
  `llm_consent_accepted` toggle (O NEED-3), that is a deliberate new opt-in
  key + Cleanup-page UI — NOT folded into this gate.

## Files not owned (named, not edited)

- `docs/PROGRESS.md` — W4-FIX-RESEED NEED (task row) is outside this wave's
  WRITE-SET. Orchestrator to add.
- `docs/agent-ops/20261107-studio/COMMS.md` (modified, not mine) +
  untracked `docs/agent-ops/**/briefs/**` and `reports/w4-impl/**` — not in
  WRITE-SET; committed by orchestrator, not swept into integration commits
  (explicit-path discipline).

## Not run

- G3 zig type-check (no zig on this box; pending on Windows per AGENTS.md).
- Live OpenRouter call + manual checklist steps (need real key; O's report
  lists steps 1-7 for Devansh on Windows).
