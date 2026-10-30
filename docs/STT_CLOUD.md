# Cloud STT providers (Gnani Prisma)

Local Wisper Flow keeps Whisper fully on-device by default. This feature adds
an **opt-in** cloud path through Gnani Prisma (`api.vachana.ai`, STT v3) with a
strict consent gate and a one-way fallback to local transcription.

## Opt-in model

Cloud STT is used only when **all three** hold:

1. `stt_provider` is `"gnani"` in `config.json` (default `"local"`),
2. `stt_consent_accepted` is `true` (default `false`),
3. the `GNANI_API_KEY` environment variable is set.

Anything else routes to the existing local chain (Whisper, or the gRPC remote
when `use_remote=true`). When cloud is selected but fails — auth, rate limit,
timeout, network, 5xx, malformed response — the router retries nothing and
**falls back to local for that utterance**. The reverse never happens: local
is never upgraded to cloud mid-utterance. Two results never fall back:

- an empty cloud transcript (per the Gnani contract, empty is a success), and
- a cancelled cloud transcription (the user asked to stop).

## Configuration

| Key | Default | Meaning |
| --- | --- | --- |
| `stt_provider` | `local` | `local` or `gnani` (unknown values warn + stay local) |
| `stt_consent_accepted` | `false` | Consent gate; cloud is refused without it |
| `stt_language` | `indic-auto` | Gnani language code or comma list (e.g. `kn,hi`) |
| `stt_timeout_ms` | `15000` | Per-request timeout (clamped to 1000-60000) |
| `stt_max_clips` | `40` | Requests per dictation (clamped 1-200; ~30 s of audio each) |

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
  byte counts, latency in ms. Never the API key, the audio, or the transcript.
- **Network allowlist** (`scripts/check_hygiene.sh`): WinHTTP usage may only
  live in `src/os/WindowsHttpClient.*`; cloud endpoints may only be named in
  `src/stt/cloud/*` and in `scripts/gnani_smoke.py` (the offline contract tool).

## Verify the contract before trusting the cloud path

The sandbox that produced this code cannot reach `api.vachana.ai`, so the
request/response shapes above follow the written contract plus defensive
parsing; **nothing here has been integration-tested against the live API.**
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

## What is intentionally not here yet

- S5: an in-app consent UI (today consent is the config boolean).
- S6: latency/cost benchmarks (need live API access).
- S7/S8: ElevenLabs and Meta Muse providers behind the same `ISttProvider`.
- S9: full documentation pass.
