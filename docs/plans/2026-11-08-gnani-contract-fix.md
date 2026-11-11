# Gnani STT contract drift fix (2026-11-08)

Branch: `ops/ui-full-redesign` (core-only change; concurrent with model-picker commits).

## User report
"the application is not sending request to the cloud when i am using gnani" + live docs at
`https://docs.gnani.ai/api/introduction/introduction` (speech-to-text page fetched 2026-11-08).

## Root-cause chain (evidence)
1. **Mic silent at engine gate** (log): `audio: peak level 0.00000 < 0.005 — no sound
   recorded` repeatedly (20:51 / 22:0x) → `StopAndTranscribe` short-circuits BEFORE
   `SttRouter::TranscribeWithPrompt`; NO provider (cloud or local) gets audio. Physical:
   mic device selected (`Microphone Array`) delivers ~0 PCM (wrong/silent input end).
2. **Startup snapshot** (`main.cpp:462-509` read once; `main.cpp:510-515` `SttRoutingConfig`
   snapshot; `ApplySettings` never rebuilds the router): saved config says
   `stt_provider: gnani` + `consent_accepted: true`, but the last startup log (22:00) shows
   `stt_provider=elevenlabs ignored: ELEVENLABS_API_KEY is not set` — the RUNNING process
   never adopted gnani. UI shows a restart banner (`MainWindow.cpp:1019`) but a user who
   just saves (no restart) keeps getting local/silent forever. By design, but invisible.
3. **Silent router gate** (`SttRouter.cpp:14-27` `CloudAllowed()` = false → `local_->Transcribe`
   with NO log line). If consent/key gate flips at runtime, fallback is invisible.
4. **Contract drift** (why it would still fail even after restart + mic):
   Live docs (fetched 2026-11-08, `docs.gnani.ai/api/STT/speech-to-text.md`, OpenAPI
   `POST /stt/v3`, servers: `https://api.vachana.ai`):
   - Path: code `/v1/asr/recognize` → docs `POST /stt/v3` (same host `api.vachana.ai`).
   - Header `X-API-Key-ID` ✓, model `prisma-v3.0` ✓, lang `indic-auto` ✓ (valid for v3.0).
   - File field: code `audio` → docs `audio_file` (required; filename ext wav ok).
   - Language field: code `language` → docs `language_code` (required).
   - Keyterms: docs accepts a JSON array string or comma-separated string (v3.0); code
     sends repeated fields — docs accept either form. Keep current shape (already accepted
     per docs examples are single field; repeated-fields form is ambiguous — verify).
   - Response 200: `{"success": true, "request_id", "transcript": "...string...", "model",
     "detected_language_code", ...}`. Our parser reads `transcript` string ✓.
   - Errors: envelope `{"success": false, "error": {"type", "message"}}`; auth
     `{"detail": {"error_code", "message", "status_code"}}`; validation `422 {"detail": [...]}`.
     Our parser reads `error.status/message` and `message/detail`-as-string — auth
     `detail` (object) yields empty detail (classify still ok via status).
5. **Invisible attempt diagnostics**: `GnaniProvider.cpp:34-40` `LogAttempt` writes to
   **`std::cerr` only, and omits `attempt.detail`** (the redacted error). The app's log
   (`lwf.log`, `src/core/Log.cpp`, file-only; no stderr redirection found) never receives
   `[stt] provider=gnani...` lines, so even a failing attempt is invisible to the user /
   file. Details belong in the file log with redaction, consistent with AGENTS.md
   ("redact and mask ... stored log line" — detail already redacted via `RedactAuthValues` +
   `RedactSecret(api_key)` at GnaniProvider.cpp:91-92/102-103/127-128).

## Evidence for tests
- Existing `tests/gnani_provider_test.cpp` with fakes — check current fixtures vs the live
  envelope before changing values (which fixtures parse the new shape?).

## Goal
After restart, with a working mic and a valid key: Gnani provider enabled (startup log),
per-clip POST to `/stt/v3` with `audio_file` + `language_code=indic-auto`, transcript
parsed from the live envelope, fallback only on real error with a redacted, capped detail
IN THE FILE LOG. No provider/stt/schema changes; no hot-apply redesign.

## Approach (chosen)
Small contract realignment + visibility fix, same module only:
- `GnaniContract.h`: kPath -> `/stt/v3`; kAudioField -> `audio_file`; kLanguageField ->
  `language_code`; add source note (`Docs checked 2026-11-08` + URL + "verified from docs,
  NOT yet against live API" per repo rule).
- `GnaniResponse.cpp`: extract auth envelope `detail.message` + `error.type` (in addition to
  existing strings); keep `transcript` string contract; validation `422` detail array -> map
  to InvalidRequest with a synthesized detail from the first `msg` (HttpStatus classify keeps category).
- `GnaniProvider.cpp`: `LogAttempt` -> ALSO emit via `lwf::Log::Warning` (or Info) with
  `detail=` (already redacted, capped 500 by existing helper?) — check how attempt detail is
  built today (`RedactAuthValues` + `RedactSecret` + cap? verify) — keep std::cerr for console.
  Also: empty-clip early return (`GnaniProvider.cpp:50-53`) is correct (engine already gates).
- Tests: update fixtures (new path/fields/success envelope), add: auth-envelope detail
  inclusion, validation-envelope classification, LogAttempt detail presence (can't unit-test
  stderr — test via the struct: assert attempt.detail non-empty after failed parse).
- `docs/STT_CLOUD.md`: update Gnani contract appendix with verified-from-docs note.
- `docs/PROGRESS.md`: one row (user-report + fix + gates + manual check).

## Rejected alternatives
- Minimal rename-only (path+fields): leaves auth-error detail empty and attempts invisible
  in the file log — the next failure would still be undiagnosable.
- Full hot-apply rebuild of the router: bigger, changes invariants; restart semantics are
  by design (bannered). Out of scope; log line at startup already names the state.
- WebSocket/batching: out of scope (REST satisfies ≤30s clips; docs themselves recommend
  REST for short clips).

## Blast radius
Only `src/stt/cloud/Gnani*` + its tests + 2 docs. Router/fallback/provider-vs-local
semantics unchanged. Privacy: no transcripts logged; detail always redacted (existing helper).
No schema/config change; no UI change.

## Acceptance criteria
- AC1: after restart with valid key, startup log has cloud provider gnani enabled; a
  spoken utterance with mic peak > 0.005 produces a POST attempt in the FILE log
  (attempt + redacted detail when failed) and either transcript or visible fallback reason.
- AC2: ctest 1/1, hygiene, no-network, full exe links (same gates as prior tasks).
- AC3: no transcripts/keys in logs (redaction path kept); no new deps.

## Risks
- The Gnani docs may change again; "verified from docs 2026-11-08 (not live API)" must be
  recorded — code comments in contract header must say this.
- Keyterms repeated-fields form: verify against docs examples — if docs tell otherwise
  (JSON-array-in-one-field is the v3.0 documented shape), switch to a single JSON field.
- User-facing: even after this, silent mic blocks everything — independent manual fix
  needed (mic device selection → test mic). Documented as step 1.

## Manual verification (Windows, not claimed)
1. Pick a working input device in Settings > Speech (mic meter moves while speaking).
2. Restart the app. Log tail: `cloud provider gnani enabled` (vs `stt_provider=gnani ignored: ...`).
3. Speak; log/file should show `[stt] provider=gnani ...` attempt lines and `injected: text ready`.
