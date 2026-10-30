# PR: Add opt-in cloud STT (Gnani Prisma) with consent gating and local fallback

Suggested **title**: `Add opt-in cloud STT (Gnani Prisma) with consent gating and local fallback`

Suggested **base**: `main` (`e60cd2a`). Branch: `feature/stt-providers` (7 commits,
one per task-chain step; squash freely if you prefer).

Paste everything below the line into the PR body.

---

## Summary

Adds the priority scope (S0–S4) of the cloud STT task chain: an **opt-in** Gnani
Prisma cloud transcription path behind the existing `IInferenceProvider` engine
seam, with a strict consent gate, a one-way cloud→local fallback, in-memory-only
audio handling, and privacy-scrubbed logging. The engine, FSM, and all 81
pre-existing test cases are untouched and still pass.

Cloud STT activates only when **all three** are true: `stt_provider == "gnani"`,
`stt_consent_accepted == true`, and `GNANI_API_KEY` is set. Default behaviour is
byte-for-byte today's behaviour: local Whisper.

## Commits

| Commit | Step | Contents |
| --- | --- | --- |
| `0001` / `0002` | Phase 0 | Base-branch decision record (`docs/BASE_DECISION.md`) — context only, no code |
| `0003` | S0 | `scripts/gnani_smoke.py` contract smoke test + `tests/data/README.md` + gitignore exception |
| `0004` | S1 | Portable primitives: `WavEncoder`, `MultipartBuilder` (header-injection hardened), `ClipSplitter` (silence-aware ≤29.5 s clips) |
| `0005` | S2 | `GnaniContract.h` (single source of truth), `GnaniRequest`, `GnaniResponse` (3 error body shapes, empty transcript = success), `SttErrorCategory` + `ISttProvider` |
| `0006` | S3 | `GnaniProvider` (clip loop, transport-error mapping, between-clip cancellation) and `SttRouter` (consent gate, cloud→local fallback, no fallback on empty/cancelled) |
| `0007` | S4 | `WindowsHttpClient` (`src/os/`, the only WinHTTP user), 5 additive `stt_*` config keys, `main.cpp` wiring, network allowlist in `scripts/check_hygiene.sh`, `docs/STT_CLOUD.md`, README/AGENTS key docs |

## Architecture

```
DictationEngine ──IInferenceProvider──▶ SttRouter (src/core)        [new]
                                          │  local: ProviderRouter ─▶ Whisper / gRPC   [existing]
                                          │  cloud: GnaniProvider  ─▶ IHttpTransport   [new]
                                                        │
                                              WindowsHttpClient (src/os)  [WinHTTP, Windows-only]
```

- The engine seam is unchanged, so `tests/engine_test.cpp` and friends compile
  and pass untouched.
- Long recordings are split at silence boundaries into ≤29.5 s clips (Gnani cap
  is 30 s), one request per clip, budget-capped by `stt_max_clips`.
- Personal dictionary words travel as `keyterms` (capped at 200, unsafe terms
  filtered), mirroring the local initial-prompt hint.

## Privacy guarantees

- Audio never touches disk: WAV bytes are built in memory per request.
- The API key is read only from `GNANI_API_KEY` and never appears in files,
  logs, errors, or tests.
- Log lines carry only: provider name, request id, HTTP status, error category,
  byte counts, latency ms.
- `scripts/check_hygiene.sh` now enforces the network allowlist: WinHTTP only
  in `src/os/WindowsHttpClient.*`, cloud endpoints only in `src/stt/cloud/*`
  and the offline smoke script.

## Config surface (all additive, type-tolerant, clamped)

| Key | Default | Notes |
| --- | --- | --- |
| `stt_provider` | `local` | `gnani` enables cloud; unknown values warn + stay local |
| `stt_consent_accepted` | `false` | Consent gate |
| `stt_language` | `indic-auto` | Gnani language code or comma list |
| `stt_timeout_ms` | `15000` | Clamped 1000–60000 |
| `stt_max_clips` | `40` | Clamped 1–200 |

## Testing

Verified here (Linux sandbox, mirroring CI):

- `cmake --build build -j4 --target lwf-tests` + `ctest`: **140 doctest cases
  pass** (81 pre-existing + 59 new across 9 new test files).
- `sh scripts/check_hygiene.sh`: OK, including the new allowlist rules.
- Windows sources cross-compiled with
  `zig c++ -target x86_64-windows-gnu` (full object codegen, not just syntax):
  `src/os/WindowsHttpClient.cpp` and `src/main.cpp` (with the full `_WIN32`
  wiring) both compile cleanly.
- `src/main.cpp` also passes `g++ -fsyntax-only` for the non-Windows branch.
- `scripts/gnani_smoke.py` self-tested offline: dry-run layout, WAV validation
  (rate/channels/width/duration), missing-key and bad-input exit codes.

**Not verified — needs you:** the sandbox cannot reach `api.vachana.ai`, so
nothing here has been integration-tested against the live Gnani API. Before
merging, run the smoke test with a real key:

```sh
GNANI_API_KEY=... python3 scripts/gnani_smoke.py --file tests/data/sample.wav
```

It prints the live status, all response headers (to confirm the request-id
header name), the body shape, byte counts and latency. If the live contract
differs from the assumed one, adjust `src/stt/cloud/GnaniContract.h` (field
names, endpoint, headers) and the tolerant lookup in `GnaniResponse.cpp` —
both are centralized and covered by fixtures; then re-run `ctest`.

## Review notes

- Base chosen per Phase 0 decision A (`docs/BASE_DECISION.md`): `origin/main`
  at `e60cd2a`; the 7 unpushed local commits were feature-compared in that
  document and their four named areas are already present in the base.
- `ProviderRouter` (gRPC remote) is untouched and still available under the new
  router; the fallback chain when both are enabled is gnani → gRPC → whisper.
- Cancellation: Esc during capture already prevents any cloud call (the engine
  drops the recording first); mid-transcription, the router checks its cancel
  hook between clips (wired to the quit flag) and aborts without falling back.

## Deliberately not in this PR (later task-chain steps)

- S5: in-app consent UI (consent is currently the config boolean)
- S6: live latency/cost benchmarks
- S7/S8: ElevenLabs and Meta Muse providers behind the same `ISttProvider`
- S9: final documentation pass
