# Brief — W5-IMPL-O (deep-dive implementer) · wave 5 · run 20261107-studio

## 1. Mission
OpenRouter as a non-local LLM model provider (plan C1-C2/T-026, workstream C — the ONLY networked feature; D2 explicit gate exception). Root cause: cleanup currently supports only local llama / passthrough; `llm_provider` key exists (A4 registered it, default local) but no backend implements `openrouter`.
Role: deep-dive implementer (design-with-the-code; report's D-section is your design record).

## 2. Required reading (in order; read in full — this is the DEEP task)
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md (+ templates/report-deepdive-addendum.md for your report's D1-D7)
2. AGENTS.md (no invented vendor API shapes — contracts from vendor docs, recorded in src/stt/cloud/*Contract.h style pattern; secrets store→env; redaction lwf::RedactSecret; windows.h only in src/os/; never break anything local (default local))
3. docs/agent-ops/20261107-studio/00-mission.md, 01-work-ledger.md
4. Approved plan: §§C (ILlmProvider seam, IHttpTransport POST-only — RESOLVED UNKNOWN: chat-completion fits, GET would need seam growth — you are the verifier), D, E (C1-C2), H (T-request-build w/ fake transport), J (Privacy+About statement of cloud egress), I (gate failure modes).
5. Investigations: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-keys-openrouter.md (ConsentGate/api_key_source/API key names: OPENROUTER_API_KEY must flow via ApiKeyStore store→env like the STT trio; C2 = extend check_no_network.sh endpoint regex + src/llm allowlist); INV-enginehooks (ILlmProvider in DictationCleanup worker; LocalLlamaProvider blocks on caller thread with mutex+decode loop — your provider must respect the same cancellation/timeout contract that Wave 2 set: 8000ms timeout+cancel+restore — re-read W2-IMPR-01 report).
6. Read src/core/ILlmProvider.h, src/core/DictationCleanup.{h,cpp} (Process → WorkerQueue, guards), src/stt/cloud/IHttpTransport.h + GnaniContract.h (contract-header precedent), src/stt/cloud/WindowsHttpClient.{h,cpp} (WinHTTP — the only production transport; your provider must take IHttpTransport& injected, never sockets itself), src/core/ApiKeyStore.* (OPENROUTER_API_KEY entry), src/core/ConsentGate.* (llm consent? consent gate today covers STT only — decide + document whether OpenRouter LLM gets its own consent or shares stt_consent_accepted; your choice needs a DECISION row in the report for the orchestrator to confirm).
7. Wave 1-4 reports for stores/wiring patterns (main.cpp LLM construction at :346-367 per INV).

## 3. Task / acceptance (T-026)
- src/llm/OpenRouterContract.h (NEW): vendor contract recorded from OpenRouter docs (chat completions endpoint host/path, Authorization header, model field, request/response JSON shapes) with the docs-checked date in the header.
- src/llm/OpenRouterProvider.{h,cpp} (NEW): `OpenRouterProvider : ILlmProvider` — Process(text, system_prompt) POSTs completion via injected IHttpTransport (POST-only seam OK); respects RequestCancel + 8000ms timeout (bounded below cleanup_timeout_ms; restoration = caller-side); maps vendor failures to typed outcomes (auth/rate/5xx/timeout) without throwing across seams; never logs text or key (redact attempt detail like GnaniProvider does); model from `openrouter_model` config (additive key, default empty → provider decorative until set; llm_provider=openrouter switches in main.cpp).
- main.cpp: construct the provider (ApiKeyStore store→env for OPENROUTER_API_KEY); when llm_provider=local (default) behavior is EXACTLY today. Consent: use the decision from §2.6 above; the Privacy page + About statements (J) MUST be updated (A4 owns those pages — either edit them in your write-set or NEED them; better: edit them if touching them stays small — declare in your write-set).
- Gate (C2): scripts/check_no_network.sh — extend the network-API allowlist for src/llm/OpenRouterProvider.{cpp,h} + endpoint regex for openrouter.ai (same commit as the .cpp). The script is YOUR gate ownership this wave.
- Tests (tests/test_openrouter.cpp, new): request-build golden tests (method/host/path/headers/body shape w/ fake transport); response parsing (choices/finish/error shapes); failure mapping (auth → typed, rate → typed, 5xx → typed, timeout → typed, malformed JSON → typed); redaction (key never in detail); cancel honored mid-flight (fake transport that blocks); POST-only respected. Hermetic fakes only — NO live network in tests.
- Manual checklist (for Devansh, honestly NOT run): set key via api_keys.json presence check (or env fallback), set llm_provider=openrouter + model, dictate, confirm cleanup result + redacted logs; local mode still untouched. List in report.

## 4. Ownership
WRITE-SET: src/llm/OpenRouterProvider.{h,cpp}, src/llm/OpenRouterContract.h, src/core/SettingsStore.cpp (openrouter_model additive ONLY), src/main.cpp (LLM construction), src/ui/MainWindow.{h,cpp} (Privacy + About statements ONLY), scripts/check_no_network.sh (endpoint + allowlist ONLY), tests/test_openrouter.cpp (new)
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt (NEED for integrator), all other src/**, docs/agent-ops (report only).

## 5. Siblings
Wave 5 runs: W5-IMPL-P (F9 polish sweep: contrast/copy-lint/keyboard) — you share MainWindow.* (statements) with its sweep of About/Privacy copy. Post CONFLICT on first overlap; W5-IMPL-P defers to you on Privacy/About text (your contract statement). Nobody else edits scripts/**, src/llm/**.

## 6. Traps
AGENTS.md: "Never invent model URLs, SHA-256 values, vendor prices or API shapes: contracts come from vendor documentation pages and are recorded in .../*Contract.h ... with the date they were checked." — do NOT rely on memory for the OpenRouter API shape; record the URL you verified (you may fetch the public vendor docs if network allows, else mark the contract UNVERIFIED and say so — honesty over invention; mark R-02's resolution accordingly). POST-only seam: if the API needs a GET for health, the provider skips it (degrade, don't grow the seam silently — NEED). Never store the key in config.json. No code comments. No live network calls in tests.

## 7. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; ctest filtered; bash scripts/check_hygiene.sh; bash scripts/check_no_network.sh. Never full ctest. One build at a time.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w5-impl/W5-IMPL-O-T-026.md (ORCH-DIGEST first + D1-D7 deep-dive addendum FIRST — design decision record before code; options, invariants, failure-mode table, load/perf/security/observability analysis, rollout/rollback, open risks). Commits explicit paths; trailers Task: T-026 / Agent: W5-IMPL-O / Wave: 5 / Report: <path>. Final <=8 lines.
