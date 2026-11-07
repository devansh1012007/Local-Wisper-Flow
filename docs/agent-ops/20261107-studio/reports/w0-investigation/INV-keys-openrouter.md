# INV-keys-openrouter — W0-INV-04 (run 20261107-studio)

## ORCH-DIGEST
- [VERIFIED src/core/ConsentGate.cpp:13-15] Env table is exactly 3: gnani→GNANI_API_KEY, elevenlabs→ELEVENLABS_API_KEY, muse→META_MODEL_API_KEY. No OPENROUTER_* anywhere in tracked source [RAN git grep -i openrouter|api_keys -> no output; grep tool -> only ledger/mission mentions].
- [VERIFIED src/main.cpp:396-449] Key read is env-only at 2 points: startup `std::getenv(ApiKeyEnvFor(...))` (:398) and live `api_key_source` lambda returning `std::getenv` (:445-449). D3 api_keys.json (A1) must intercept *both* — store→env precedence point is inside those lambdas / a new resolver, not in ConsentGate itself (consent gate only checks `api_key_present` bool).
- [VERIFIED src/stt/cloud/IHttpTransport.h:65-70] POST-only seam: single `virtual HttpResponse Post(const HttpRequest&)`. No Get/verb field. WindowsHttpClient.cpp:96 hardcodes `L"POST"`. OpenRouter `/chat/completions` POST+JSON fits; GET (e.g. /models list) would NOT fit without extending seam.
- [VERIFIED src/core/ILlmProvider.h:5-11] Correct seam for OpenRouter is ILlmProvider (Process(text,system_prompt)+RequestCancel), NOT IInferenceProvider (Transcribe audio). LocalLlamaProvider blocks on caller thread: mutex :95 + synchronous llama_decode loop :153-173; abort via llama_set_abort_callback :108-114.
- [VERIFIED scripts/check_no_network.sh:15,20] C2 must extend network allowlist (:15, 3 pairs) AND endpoint regex (:20, `api\.(vachana\.ai|elevenlabs\.io|meta\.ai)` + `grep -v ^src/stt/cloud/`) for openrouter.ai + src/llm/OpenRouterProvider. Plus file-write ban :25 covers src/llm (OpenRouterProvider must not write files — trivially satisfied).
- [VERIFIED AGENTS.md:80-82,172-173] "keys from environment only" wording to amend for P01/A1+D3 (see §6).

## 1. Question and scope
Verify ConsentGate env table + api_key_source wiring (main.cpp:394-438,445-449), IHttpTransport POST limits, check_no_network.sh lines to extend (C2), ILlmProvider vs IInferenceProvider seam + LocalLlamaProvider:95 blocking, AGENTS.md env-only wording, and absence of OPENROUTER_* refs. Branch feature/studio-pages @0e655ec [RAN git branch --show-current]. Plan sections C/E (A1,C1-C2,D2-D3) read from C:\Users\devan\.opencode\plan\2026-11-07-studio-pages-and-settings-gui.md.

## 2. Summary (what true / risky / unknown)
- True: env-only today (3 providers); POST-only transport suffices for chat completion; ILlmProvider is correct seam; zero OpenRouter code exists.
- Risky: D3 store→env precedence must cover BOTH main.cpp reads or router re-checks env bypassing store; endpoint regex currently bans openrouter.ai outside src/stt/cloud/ so C1 file in src/llm/ FAILS G4 until C2 same-commit edit.
- Unknown: OpenRouter exact host/path/headers/body/response shape — must be taken from vendor docs with check date per AGENTS.md:169-171 + recorded in *Contract.h; NOT verified here (no vendor fetch done, per read-only brief).

## 3. ConsentGate env table + api_key_source wiring
- [VERIFIED src/core/ConsentGate.cpp:8-16] `kCloudProviders[] = {{"gnani","GNANI_API_KEY"},{"elevenlabs","ELEVENLABS_API_KEY"},{"muse","META_MODEL_API_KEY"}}`, `kCloudProviderCount=3`. `ApiKeyEnvFor()` :42-45 returns env name or nullptr; `EvaluateCloudConsent()` :47-71 only consumes `inputs.api_key_present` bool — never reads env/store itself.
- [VERIFIED src/main.cpp:396-404] Startup: `ApiKeyEnvFor(stt_provider_name)` → `std::getenv(key_env)` (:398) → `api_key_present=!empty` → verdict. Provider construction (gnani :408-417, elevenlabs :418-428, muse :429-438) uses that snapshot `api_key` string.
- [VERIFIED src/main.cpp:445-449] Live re-check: `stt_routing.api_key_source = [stt_provider_name]{ std::getenv(ApiKeyEnvFor(...)) }`. [VERIFIED src/core/SttRouter.h:13-18] comment: "read at call time so gate reflects live environment"; [VERIFIED src/core/SttRouter.cpp:25] `CloudAllowed()` calls `api_key_source()` per utterance. Bench fake at bench/lwf_cloud_bench.cpp:143 same shape.
- **D3 precedence point [INFERENCE, high confidence]:** A1 ApiKeyStore must be consulted where env is read — i.e. replace/augment both main.cpp lambdas (and any OpenRouter equivalent) with `store->Get(provider) else getenv(env)`. ConsentGate needs no change (bool input). OpenRouter as ILlmProvider has NO consent gate today — C1 must decide: reuse ConsentGate pattern or new LlmConsent check; plan C1 says "key from api_keys.json (store→env)" [plan :90].
- Env names are the single source: AGENTS.md:81-82 says "(single source in src/core/ConsentGate.cpp)".

## 4. IHttpTransport POST limits — OpenRouter fit
- [VERIFIED src/stt/cloud/IHttpTransport.h:11-18] `HttpRequest{host,path,headers,content_type,body(vector<uint8_t>),timeout_ms}`; :20-24 `HttpResponse{status,body,headers}` + case-insensitive `Header()` :26-43. Arbitrary headers (Authorization: Bearer …) + JSON body fully expressible — proven by Muse: [VERIFIED src/stt/cloud/MuseRequest.cpp:57-62] host/path/Bearer header/content_type/body; [VERIFIED src/stt/cloud/MuseContract.h:7-10] kHost/kPath/kApiKeyHeader/kApiKeyEnv consts; [VERIFIED tests/muse_provider_test.cpp:21-51] FakeTransport implements Post, captures requests.
- [VERIFIED src/stt/cloud/WindowsHttpClient.cpp:96-98] `WinHttpOpenRequest(..., L"POST", ...)` hardcoded; no verb field anywhere. [VERIFIED src/stt/cloud/IHttpTransport.h:62-64] comment: "The only network seam of the cloud STT stack."
- **GET needed? [INFERENCE, high]:** Core chat completion = POST JSON → POST response body JSON parse. No GET needed for cleanup/transform use. GET would only be needed for model-list/dec discovery UX (not in plan C1) — if wanted later, seam must grow a Get() + WindowsHttpClient verb param (new gate surface). Plan R-02 assumption [plan :183] "IHttpTransport fits OpenRouter (POST+headers+JSON)" is consistent with this evidence; first C1 task per 00-mission.md:20 is fake-transport test mirroring muse_provider_test.cpp:66-89.

## 5. ILlmProvider vs IInferenceProvider seam
- [VERIFIED src/core/ILlmProvider.h:5-11] `Process(text,system_prompt)->string; RequestCancel(){}`. Consumers: [VERIFIED src/core/DictationCleanup.h:12,29] `DictationCleanup(ILlmProvider*)`, [VERIFIED src/core/CommandModeController.h:25,34], [VERIFIED src/main.cpp:150-182] PassthroughLlm + LocalProviderStack (delegates ProcessText→llm_.Process) + RouterLlm.
- [VERIFIED src/core/IInferenceProvider.h:6-20] STT seam: `Transcribe(audio_pcm)`, `ProcessText(text,instruction)`, `TranscribeWithPrompt`. SttRouter (:28) implements this for STT routing only; cleanup stays local per SttRouter.h:27. OpenRouter doing text→text must NOT implement IInferenceProvider (would imply audio Transcribe).
- [VERIFIED src/llm/LocalLlamaProvider.cpp:89-95] `Process` locks `mutex_` at :95 then runs synchronous tokenize/decode/loop to completion (:119-191); abort only via `llama_set_abort_callback` checking `abort_requested` (:108-114, set by RequestCancel :85-87). So blocking behavior = caller-thread block until done/cancelled — OpenRouter Post() has same character (sync HTTP, timeout via HttpRequest.timeout_ms, WindowsHttpClient default 15000ms :68). Plan P15 worker-thread rule [plan :49,76] applies equally to C1.
- No `llm_provider` key exists today [VERIFIED grep SettingsStore.cpp:32-36 shows only llm_model_path/cleanup_*]; C1 must add it (plan :90,138 → 52 keys total).

## 6. check_no_network.sh exact lines (C2) + AGENTS.md wording (P01/A1)
- [VERIFIED scripts/check_no_network.sh:15] network allowlist: `grep -vE '^(src/stt/GrpcInferenceProvider\.(cpp|h):|src/os/WindowsDownloader\.(cpp|h):|src/stt/cloud/WindowsHttpClient\.(cpp|h):)'` — OpenRouterProvider itself makes no WinHTTP calls (uses IHttpTransport), so no network-allowlist change needed UNLESS it includes winhttp directly (it must not). File-write ban :25 `src/audio src/vad src/llm` also applies — OpenRouterProvider must stay write-free.
- [VERIFIED scripts/check_no_network.sh:20] endpoint regex: `grep -rnE 'api\.(vachana\.ai|elevenlabs\.io|meta\.ai)' src | grep -v '^src/stt/cloud/'` — C2 must add `openrouter\.ai` (likely `api\.openrouter\.ai`) AND let src/llm/OpenRouterProvider.* (+OpenRouterContract.h) pass the `grep -v` filter, i.e. extend to `grep -vE '^(src/stt/cloud/|src/llm/OpenRouterProvider|src/llm/OpenRouterContract)'` or move contract under src/stt/cloud/. Same-commit rule per plan :15,86,91.
- AGENTS.md exact wording to amend [VERIFIED AGENTS.md:80-82]: "`stt_consent_accepted=true` plus the provider's key from the environment only (`GNANI_API_KEY` / `ELEVENLABS_API_KEY` / `META_MODEL_API_KEY`, single source in `src/core/ConsentGate.cpp`)" and [VERIFIED AGENTS.md:172-173]: "Secrets (API keys) only ever cross the process boundary via the environment; they are redacted from every stored log line (lwf::RedactSecret / RedactAuthValues)." D3 amendment (plan :120,86) must change "environment only" → store→env precedence + api_keys.json rules (redacted, never config.json, per plan :163,169).

## 7. Redaction
- [VERIFIED src/core/Redact.h:28-37] IsSensitiveHeaderName covers authorization/proxy-authorization/xi-api-key/x-api-key-id/cookie — OpenRouter `Authorization: Bearer` already covered; [VERIFIED src/core/Redact.h:39-74] RedactAuthValues masks values. ApiKeyStore (A1) must use RedactSecret/RedactAuthValues, never log keys (plan :163).

## 8. Absence of OpenRouter refs
- [RAN git grep -n -i "openrouter|api_keys" -- . -> no output] + [VERIFIED grep tool: OPENROUTER|openrouter|api_key_source|ApiKeyStore|api_keys.json -> 9 matches, all api_key_source/STT or ledger/mission docs]. No OPENROUTER_* env, no openrouter.ai, no llm_provider key, no ApiKeyStore, no api_keys.json in tracked source. Greenfield behind `llm_provider=local` default.

## 9. Baseline / tests
- Not run (read-only investigator, no build per brief; full suite forbidden by STANDING-ORDERS §16). Relevant existing tests to mirror: tests/muse_provider_test.cpp (FakeTransport request-build), tests/secrets_redaction_test.cpp, tests/stt_router_test.cpp:60 api_key_source fake, tests/cleanup_test.cpp (ILlmProvider scripted fakes). No scratch files created.

## 10. Risks (ranked)
1. C2 gate regex missed → G4 red on first push (plan I:161). Evidence: :20 bans endpoint outside src/stt/cloud/.
2. Store consulted at only one of two main.cpp reads → router re-check bypasses store (or provider snapshot stale). Evidence: :398 vs :445-449.
3. OpenRouter without consent gate → cloud text egress without opt-in (D2/J:169 requires Privacy/About disclosure). No LlmConsent exists today.
4. GET creep (model list) forces transport seam growth — out of C1 scope, keep POST-only.

## 11. Unknowns
- OpenRouter host/path/JSON schema/model id — resolve from vendor docs + record in OpenRouterContract.h with check date (AGENTS.md:169-171). Owner: T-026.
- Whether OpenRouter needs HTTP-Referer/X-Title headers — same vendor-doc check.
- Timeout/cancel semantics for sync Post vs cleanup_timeout_ms — design decision for T-026 (reuse HttpRequest.timeout_ms + RequestCancel atomic like LocalLlamaProvider:56,85).

## 12. Out-of-scope observations
- src/stt/cloud/WindowsHttpClient.cpp is WIN32-only [VERIFIED CMakeLists per plan :34]; Linux build uses fakes only.
- AGENTS.md:23-26 network allowlist description ("exactly three places") will also need P01 amendment alongside :80-82 (noted, not investigated).

## 13. Scratch files created
- None (this report is the sole write).
