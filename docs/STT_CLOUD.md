# Cloud STT

## Consent in the app (S5)

The consent gate lives in `src/core/ConsentGate.{h,cpp}`: one function decides
whether cloud is allowed (known provider + `stt_consent_accepted` + a
non-empty key from the provider's environment variable) and returns a
log-safe reason when it is not.

Secret redaction lives in `src/core/Redact.h` (`lwf::RedactSecret`,
`lwf::RedactAuthValues`). Failed cloud attempts keep a redacted error `detail`
in their attempt stats; log lines carry only provider, request id, status,
category, byte counts, and latency — never the API key, never the transcript.

## What is intentionally not here yet

- Cloud providers (Gnani / ElevenLabs / Muse) and the `SttRouter` wiring live
  on the `feature/stt-providers` branch and require the cloud-STT foundation
  (`ISttProvider`, `IHttpTransport`, `WavEncoder`, `MultipartBuilder`,
  `ClipSplitter`, `Gnani*`) that this branch does not yet have.
- Latency/cost benchmarks (`lwf-cloud-bench`, S6).
