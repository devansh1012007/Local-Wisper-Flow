# Cloud STT providers (Gnani Prisma, ElevenLabs Scribe, Meta Muse Voice)

Local Wisper Flow keeps Whisper fully on-device by default. This feature adds
an **opt-in** cloud path through three providers - Gnani Prisma
(`api.vachana.ai`, STT v3), ElevenLabs Scribe (`api.elevenlabs.io`), and Meta
Muse Voice Transcribe (`api.meta.ai`) - with a strict consent gate and a
one-way fallback to local transcription.

## Opt-in model

Cloud STT is used only when **all three** hold:

1. `stt_provider` names a cloud provider in `config.json`
   (`gnani`, `elevenlabs`, or `muse`; default `local`),
2. `stt_consent_accepted` is `true` (default `false`),
3. the provider's key environment variable is set
   (`GNANI_API_KEY` / `ELEVENLABS_API_KEY` / `META_MODEL_API_KEY`).

Anything else routes to the existing local chain (Whisper, or the gRPC remote
when `use_remote=true`). When cloud is selected but fails — auth, rate limit,
timeout, network, 5xx, malformed response — the router retries nothing and
**falls back to local for that utterance**. The reverse never happens: local
is never upgraded to cloud mid-utterance. Two results never fall back:

- an empty cloud transcript (per the Gnani contract, empty is a success), and
- a cancelled cloud transcription (the user asked to stop).

The decision logic lives once, in `EvaluateCloudConsent`
(`src/core/ConsentGate.*`); the router, the main wiring, and the Settings >
Cloud tab all consume the same verdict.

## Configuration

| Key | Default | Meaning |
| --- | --- | --- |
| `stt_provider` | `local` | `local`, `gnani`, `elevenlabs`, or `muse` (unknown values warn + stay local) |
| `stt_consent_accepted` | `false` | Consent gate; cloud is refused without it |
| `stt_language` | `indic-auto` | Provider-neutral language hint: ISO code or comma list (e.g. `kn,hi`); mapped per provider by `LanguageMap` |
| `stt_timeout_ms` | `15000` | Per-request timeout (clamped to 1000-60000) |
| `stt_max_clips` | `40` | Request budget per dictation (clamped 1-200; ~30 s Gnani/ElevenLabs clips, ~10 min Muse clips) |

All keys are additive and type-tolerant like every other optional key: wrong
types produce a warning and keep the default; out-of-range values are clamped.

The API key is read **only** from `GNANI_API_KEY` and is never written to any
file, log, or error message. Do not put it in `config.json`.

## Data flow

1. The engine hands the trimmed speech interval (16 kHz mono float PCM) to
   `SttRouter` (`src/core/SttRouter.*`) through the unchanged
   `IInferenceProvider` seam.
2. `SttRouter` checks the consent gate, picks cloud or local, and owns the
   cloud-to-local fallback.
3. `GnaniProvider` (`src/stt/cloud/GnaniProvider.*`) splits the audio into at
   most 29.5 s clips at silence boundaries (`ClipSplitter`), encodes each clip
   to an in-memory WAV (`WavEncoder`, 16-bit PCM mono), and builds the exact
   multipart body the smoke script sends (`MultipartBuilder`, `GnaniRequest`).
4. `WindowsHttpClient` (`src/os/WindowsHttpClient.*`, the only file allowed to
   touch WinHTTP) performs the HTTPS POST and returns status/headers/body.
5. `GnaniResponse` parses success and the three documented error body shapes
   and classifies them into `SttErrorCategory`; the provider aborts remaining
   clips on the first failure and records per-request statistics.

Personal dictionary words ride along as `keyterms` (capped at 200, filtered for
commas/line breaks), mirroring the local initial-prompt hint.

## Privacy rules

- **Audio never touches disk.** WAV bytes are built in memory per request and
  freed with the request.
- **Logs carry only**: provider name, request id, HTTP status, error category,
  byte counts, latency in ms. Gnani attempts additionally write one file-log
  line each carrying the attempt's redacted error `detail` (API key masked,
  truncated to 200 chars on display). Never the API key, the audio, or the
  transcript.
- **Network allowlist** (`scripts/check_hygiene.sh`): WinHTTP usage may only
  live in `src/os/WindowsHttpClient.*`; cloud endpoints may only be named in
  `src/stt/cloud/*` and in `scripts/gnani_smoke.py` (the offline contract tool).

## Verify the contract before trusting the cloud path

The Gnani contract below was checked against the live vendor docs on
2026-11-08 (`https://docs.gnani.ai/api/STT/speech-to-text.md` + OpenAPI spec);
**it has NOT yet been verified against the live API.** That changes the stale
values the re-landing originally shipped: request path `/v1/asr/recognize` →
`/stt/v3`, multipart field `language` → `language_code` (required,
`indic-auto` valid for v3.0), multipart field `audio` → `audio_file`
(`audio.wav`, wav ext ok). The rest matches the docs: host `api.vachana.ai`,
header `X-API-Key-ID` (required), `model` `prisma-v3.0`, `keyterms` string
(JSON-array or comma-separated, v3.0 only). Success 200 is
`{"success": true, "request_id": ..., "transcript": "...", "model":
"prisma-v3.0", "detected_language_code": "hi-IN", "processing_time": ...}`;
audio < 0.1 s returns success with an empty transcript. Errors come in three
envelopes: `{"success": false, "error": {"type": "<TYPE>", "message":
"..."}}`, auth/rate `{"detail": {"error_code", "message", "status_code"}}`,
and validation 422 `{"detail": [{type, msg, loc, input}]}`. `GnaniResponse`
reads all three (error type+message, detail message, first-array msg).

Before relying on cloud mode, run the smoke test with a real key:

```sh
GNANI_API_KEY=... python3 scripts/gnani_smoke.py --file tests/data/sample.wav
```

It prints the status, every response header (to locate the real request-id
header name), the parsed body shape, byte counts and latency. If the live shape
differs, adjust `src/stt/cloud/GnaniContract.h` (single source of truth: field
names, endpoint, header names) and the tolerant field lookup in
`src/stt/cloud/GnaniResponse.cpp`, then re-run `ctest`. Use `--dry-run` to
inspect the exact multipart layout without spending a request.

## Cancellation

Cancelling a dictation (Esc while capturing) never reaches the cloud: the
engine drops the recording before transcription starts. Mid-transcription, the
router checks its cancel hook between clips — wired to the app quit flag — and
aborts remaining requests without falling back. A stuck in-flight request is
bounded by `stt_timeout_ms`.

## Consent in the app (S5)

The consent gate lives in `src/core/ConsentGate.{h,cpp}`: one function decides
whether cloud is allowed (known provider + `stt_consent_accepted` + a
non-empty key from the provider's environment variable) and returns a
log-safe reason when it is not. The router, `main`, and the settings UI all
read the same verdict, so the config boolean, the wiring, and the UI cannot
drift apart.

Settings > Cloud tab (S5) turns consent into an explicit in-app action: a
provider combo (local/gnani today) and an "I understand; enable cloud
transcription" checkbox that explains what leaving the device means. Changing
either needs a restart. The API key is never entered in the UI - it stays in
the environment. Onboarding keeps pushing local-first; cloud consent is a
deliberate Settings action.

Failed cloud attempts keep a redacted error `detail` in their attempt stats
(server text with the API key masked, auth header values masked by
`lwf::RedactAuthValues` from `src/core/Redact.h`). Detail values are stored
in memory for the UI and tests; Gnani attempts also write one `[stt] ...`
line per attempt to the file log (`lwf::Log::Warning`) carrying the redacted
detail truncated to 200 chars, so failures are visible in `lwf.log`.
Transcripts are never logged.

## Latency and cost model (S6)

`lwf-cloud-bench` (Linux, portable) measures what can be measured without the live API:
WAV encode ~3 ms and request build ~5 ms per 30 s clip, clip splitting free, the
router/consent gate sub-0.1 ms, and a round-trip of roughly `clips x (rtt + server_ms)`
through a loopback transport. Numbers and the reading are in `docs/BENCHMARKS.md`; the
server-side half of the equation lands here after the S0 smoke run against the real
endpoint. Cost is a formula, not a promise: `audio_hours x price_per_hour`, with the
Muse Voice Transcribe $0.18/h as the one vendor-verified constant so far.

## ElevenLabs Scribe (S7)

`stt_provider="elevenlabs"` routes dictation through ElevenLabs Speech to Text
(Scribe). Contract, verified against the vendor's API reference on 2026-11-04:

- `POST https://api.elevenlabs.io/v1/speech-to-text`, auth header `xi-api-key`,
  key read only from `ELEVENLABS_API_KEY`.
- Multipart fields: `model_id` (default `scribe_v2`), optional `language_code`
  (ISO-639-1/-3; omitted for auto-detect), optional repeated `keyterms` parts
  (each ≤ 50 chars, ≤ 5 words, characters `< > { } [ ] \` rejected by the vendor;
  we cap at 200 like Gnani), and the `file` part (in-memory WAV, same as Gnani).
- Success body: `text` plus `language_code`/`language_probability`/`words`; we
  use `text` only. Errors are classified by the shared HTTP status map (422 →
  invalid_request, 401/403 → auth, 429 → rate_limited, 413 → payload, 5xx →
  server); the documented error shape is `{"detail": {"status", "message"}}`
  and the parser also accepts the string form.
- Fallback, cancellation, empty-transcript, and clip-budget semantics are
  identical to the Gnani provider (same clip loop, same router).

Language handling: `stt_language` is provider-neutral. `sttlang::ElevenLabsLanguageCode`
(`src/stt/cloud/LanguageMap.cpp`) takes the first valid ISO token of the comma
list ("kn,hi" → "kn") and falls back to auto-detect for `auto`, `indic-auto`,
or unknown tokens. `sttlang::MuseLanguageBias` (used in S8) maps codes to the
vendor's language names and expands `indic-auto` to the Indic subset of Muse's
documented 25-language list.

## Meta Muse Voice Transcribe (S8)

`stt_provider="muse"` routes dictation through Meta's Muse Voice Transcribe on
the Meta Model API. Contract, verified against dev.meta.ai/docs/speech-to-text
on 2026-11-04:

- `POST https://api.meta.ai/v1/asr/transcribe`, header `Authorization: Bearer
  <key>`, key read only from `META_MODEL_API_KEY` (a Model API dashboard key).
- Multipart body with exactly two parts: `request` (JSON settings part,
  `Content-Type: application/json`) and `audio` (RIFF/WAV, mono 16-bit PCM at
  16 kHz). The JSON carries `mode: "PUSH_TO_TALK"` (single-turn dictation,
  the vendor default), `model: "muse-voice-transcribe-1.0"`,
  `audioEncoding: "WAV"`, optional `languageBias` (vendor language NAMES) and
  optional `keywords` (vocabulary biasing, capped at 200 by us).
- Vendor caps: audio ≤ 10 minutes and body ≤ 32 MB per request (413/400
  otherwise), so `MuseContract::kMaxClipMs` is 595000 - one clip covers a
  normal dictation and only extreme sessions split.
- Success body: `transcript` (complete text in every mode) plus `sessionId`,
  `audioDurationMs`, `turns`; we use `transcript` and record `sessionId` as
  the request id. Failures: 413 → payload_too_large, 400 → invalid_request,
  429 → rate_limited, 5xx → server.
- Language bias: `sttlang::MuseLanguageBias` maps the shared `stt_language`
  value - "kn,hi" → ["Kannada","Hindi"], "indic-auto" → the Indic subset of
  the vendor's documented 25-language list, anything unknown → omitted
  (auto-detect).
- Fallback, cancellation, empty-transcript, key redaction, and clip-budget
  semantics are identical to the other providers.

Vendor pricing note (used by `lwf-cloud-bench`): Muse Voice Transcribe is
documented at $0.18 per audio hour, rounded down to whole seconds; failed and
429-billed requests are not billed.

## Interface mapping vs the original task brief

The brief named files that the rebased codebase solved with the existing
architecture; this is the mapping, so nobody hunts for ghosts:

| Brief | Actual | Where |
|---|---|---|
| `SttTypes.h` | `ISttProvider.h` (SttTranscription, SttAttemptStats) + contract headers | `src/core/ISttProvider.h`, `src/stt/cloud/*Contract.h` |
| `CancelToken.h` | `SttCancelCheck` hook wired to the app quit flag | `src/core/ISttProvider.h`, `SttRouter::SetCancelCheck` |
| `IHttpClient.h` | `IHttpTransport` (Post returns status/headers/body) | `src/stt/cloud/IHttpTransport.h` |
| `SttErrorKind` | `SttErrorCategory` (auth/rate_limited/timeout/network/invalid_request/payload_too_large/server/cancelled) | `src/core/ISttProvider.h` |
| `LanguageMap` | `sttlang::ElevenLabsLanguageCode` / `sttlang::MuseLanguageBias` | `src/stt/cloud/LanguageMap.*` (S7) |
| consent test | `tests/consent_test.cpp` | S5 |
| secrets_redaction test | `tests/secrets_redaction_test.cpp` + provider detail-redaction cases | S5 |
| language_map test | `tests/language_map_test.cpp` | S7 |
| cancel test | cancel cases in every provider test + `stt_router_test` | S3/S7/S8 |
| network_allowlist test | `scripts/check_hygiene.sh --self-test` (plants offenders, asserts they are caught) | S9 |

## What is intentionally not here yet

- The live-API half of the latency numbers and the first real transcription
  per provider (needs the Windows checkpoint, docs/QUESTIONS.md #5).
- Streaming/cloud realtime (Gnani STT v3 and Muse realtime WebSocket) - the
  current scope is one-shot dictation utterances.

## Stage ledger

| Stage | Deliverable | Commit |
|---|---|---|
| Phase 0 | base decision + rebase report | a1d927b..8662441 |
| S0 | `scripts/gnani_smoke.py` + tests/data guide | fd6fe84 |
| S1 | WavEncoder / MultipartBuilder / ClipSplitter | 5cd6f45 |
| S2 | Gnani request/response/contract + error categories | b6c5e0a |
| S3 | SttRouter with consent gate and cloud-to-local fallback | 91a55ed |
| S4 | WindowsHttpClient + main wiring | e9ee0b6 |
| S5 | ConsentGate, Redact, in-app consent UI | 98b0829 |
| S6 | lwf-cloud-bench latency/cost model | 8928b0c |
| S7 | ElevenLabs provider + LanguageMap | 3df580e |
| S8 | Meta Muse provider | 16b6f51 |
| S9 | documentation pass + hygiene self-test | (this commit) |
