# Report — W5-IMPL-INT (integrator, wave 5 of run 20261107-studio)

## ORCH-DIGEST (read this first)

- Status: COMPLETE: CMake registered + MainWindow merged + gates GREEN + committed + nothing of mine uncommitted (COMMS + untracked docs left for orchestrator, named below).
- Integration: CMakeLists.txt +2 lines (O NEED-1 exact lines); MainWindow.cpp = O egress + P 5 hunks + RESEED clause drop; full ctest 23/23, hygiene OK, no-network OK, exe link unblocked.
- DECISION-1 (consent) — REQUIRES DEVANSH CONFIRMATION IN FINAL SUMMARY: O's model verbatim: "`llm_provider=openrouter` + non-empty `openrouter_model` + present key is the opt-in; no separate consent flag; do NOT share `stt_consent_accepted`; do NOT route through `EvaluateCloudConsent`." Code VERIFIED to match (read below, not just O's claim); I changed nothing about it. Do not adopt a different consent model without Devansh confirming — the code today implements exactly the quoted model.
- NEEDs carried: PROGRESS row (outside my WRITE-SET); COMMS/untracked docs (outside WRITE-SET); O NEED-2/3/4 (Cleanup GUI, consent toggle, transport cancel) stay DEFERRED.

## Context & root cause

The wave's blockers were integration-only: (i) O's provider compiled standalone but was CMake-orphaned, so P's exe link failed on undefined OpenRouterProvider (P FINDING); (ii) MainWindow.cpp carried O Privacy/About egress hunks + P 5 copy hunks uncommitted (shared-file sequencing held it); (iii) RESEED (f66943b) fixed the dictionary tombstone but left Privacy copy claiming `restart to reseed empty files` (MainWindow.cpp:1354), now false for the dictionary. Config/progress notes live in the gate record.

## Phased changes executed

### Phase 1 — CMakeLists.txt (O NEED-1, exact lines)

- `LWF_CORE_SOURCES`: `src/llm/OpenRouterProvider.cpp` added after `src/stt/cloud/MuseProvider.cpp` (line 83 per O NEED-1).
- `lwf_tests`: `tests/test_openrouter.cpp` added between `test_onboarding.cpp` and `test_overlay_settings_ui.cpp` (alphabetical, matching file list ordering).
- No font_sources addition (verified grounds by O: zero imgui/`SetWindowFontScale` in new TUs).
`// DECISION:` placement mirrors the STT-cloud provider block — the provider is a core source linked by `lwf_core`, not a test-only TU, since `main.cpp` constructs it in `local-wisper-flow` too.

### Phase 2 — MainWindow.cpp merge (already in tree + one RESEED fix)

Pre-existing uncommitted hunks (audited, kept verbatim, now committed by me):
- P (5 strings): Trust fallback `still dictated on this computer`; Cloud label `Service`; Cloud rule `Dictation runs on this machine by default...`; Consent `service API key` + `still dictated on this computer`; About `On-device speech recognition and optional text cleanup`.
- O: Privacy unconditional `By default no network calls except explicit model downloads. OpenRouter cleanup is the only opt-in exception.`; Privacy model readout `OpenRouter model: <value or "not set (edit config.json)">` (every-schema-key scan anchor for `openrouter_model`); About `All processing stays on this machine unless OpenRouter cleanup is enabled.`; conditional warnings on Cleanup/Privacy/About read `Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.` (consistent 3× wording).

My one substantive edit (RESEED NEED): line 1354 `This cannot be undone. The app keeps running; restart to reseed empty files.` → `This cannot be undone. The app keeps running.` // INVARIANT: the second sentence now claims only that the app keeps running (true: DeleteLocalData clears stores in place) and drops the reseed clause (false for the dictionary since the f66943b tombstone persists across restart; retained for nothing else — TransformStore re-seeds its own built-ins via Reload/Load independently of this sentence). Line 1351 (`Removes statistics, dictionary, ... Settings are kept.`) left untouched: removal is still accurate.
Copy-lint self-check: no `!` in any new/kept string; no internal terms (`Whisper`, `Provider`, `LLM`, `STT`, `FSM`, `VAD`) in display strings changed this wave; `OpenRouter` is a product egress name required by AGENTS.md (Privacy + About statement) and `config.json`/`API key` are filename/user-term carve-outs. Local-first promise preserved on both Privacy and About; conditional disclosures do not duplicate unconditional statements (different subject: `when cleanup is on` vs default postures).

### Phase 3 — Gates (40-min budget, met at ~15 min incl. ctest)

Verbatim in gates/gate-w5-1.md: configure OK; build exit 0 (exe links — P blocker resolved); ctest 23/23 (660.70s); hygiene OK; no-network OK; `*openrouter*` 19/19, 118 asserts.

### Phase 4 — Checklist + gate record

docs/STUDIO_CHECKLIST.md integration section appended (T-026-integration); docs/agent-ops/20261107-studio/gates/gate-w5-1.md written.

## DECISION-1 — verbatim (O's report §DECISION-1) + verification

> **Chosen: `llm_provider=openrouter` + non-empty `openrouter_model` + present key is the opt-in; no separate consent flag; do NOT share `stt_consent_accepted`; do NOT route through `EvaluateCloudConsent`.**

Verification (read, not trusting): `src/main.cpp:375-410` openrouter branch resolves `api_keys.Get("openrouter")` then `getenv(OPENROUTER_API_KEY)`, requires non-empty key+model, constructs provider — no `stt_consent_accepted` read, no `EvaluateCloudConsent` call. `src/core/ConsentGate.cpp` untouched (no openrouter row: `ApiKeyEnvFor("openrouter")==nullptr`, so `ResolveApiKey` stays store-only; test_apikeystore pins this). STT gate `EvaluateCloudConsent` call site (main.cpp:467-469) is a separate block, untouched. `// INVARIANT: llm_provider==local (default) keeps the exact pre-existing llama/passthrough branch (main.cpp:411-433, verified above).` → FLAG FOR DEVANSH CONFIRMATION IN FINAL SUMMARY. I did not alter consent behavior; O NEED-3 (dedicated `llm_consent_accepted` toggle) remains a DEFERRED additive if he wants two-step consent.

## Review audit

- Self-review Phase 1: placement can't split the list (both insertions are single list items); provider in lwf_core (not lwf_tests-only) because main.cpp needs it at exe link. Verified no other CMake consumer needed (no font_sources, no new link libs — nlohmann already on lwf_core).
- Self-review Phase 2: diff re-read; F9 copy-lint suite covers `src/ui/**` display strings — full ctest green proves no banned term/`!` slipped in. About sentence: P's `On-device...` opening + O's `unless OpenRouter cleanup is enabled.` tail compose grammatically. Privacy sentence: unconditional exception + conditional warning are complementary, not contradictory.
- Final: `git diff` inspected — only CMakeLists.txt (2 lines), MainWindow.cpp (P+O hunks+RESEED), STUDIO_CHECKLIST.md (append), gate record. No `src/core/**`, `src/ui/pages/**`, `src/llm/**`, `src/os/**`, `scripts/**` edits (FORBIDDEN set respected).
- No independent review obtained (W5 parallel window); High-Risk material: none (no auth/billing/migration/dependency changes — all integration).

## Verification evidence (verbatim)

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` → `Configuring done (2.0s)` / `Generating done (10.7s)`.
- `cmake --build build --config Release --parallel 4` → `Built target lwf_tests`, `Built target local-wisper-flow`, BUILD-EXIT:0 (only pre-existing gRPC `empty_bases` warnings).
- `ctest --test-dir build --output-on-failure` → `100% tests passed out of 23`, 660.70s.
- `bash scripts/check_hygiene.sh` → `hygiene: OK`; `bash scripts/check_no_network.sh` → `no-network: OK`.
- `lwf_tests.exe --test-case="*openrouter*"` → 19/19 cases, 118/118 asserts, SUCCESS.

## Unverified assumptions & remaining risks

- G3 zig pending (no zig on box); MSVC lane is CI-only (per AGENTS.md).
- Live OpenRouter happy path unverified (no key); O manual steps 1-7 are Devansh's on Windows. Rate-limit/billing classification is doc-verified only.
- Mid-Post cancel uninterruptible (O risk #1); bounded by 8000ms transport timeout — accepted, not fixed here.
- `docs/PROGRESS.md` row not added (outside WRITE-SET).
- COMMS.md + untracked briefs/reports not committed by me (outside WRITE-SET, explicit-path discipline); orchestrator to commit.

## Git state

Branch `feature/studio-pages` (never switched). Commits: explicit paths only — code commit (CMakeLists.txt + src/ui/MainWindow.cpp) + docs commit (gate + checklist + this report), trailers `Task: T-026-integration / Agent: W5-IMPL-INT / Wave: 5`. Remaining uncommitted: COMMS.md (modified, siblings') + untracked `docs/agent-ops/**/briefs/**` + `docs/agent-ops/**/reports/w4-impl/**` (siblings', outside my WRITE-SET). No stash/reset/push/--no-verify used.
