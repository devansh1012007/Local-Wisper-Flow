# OpenRouter model picker (2026-11-08) — V2 (review-amended)

Branch: `ops/ui-full-redesign` (sequential after mic work, current HEAD `20dc48a`).

## Problem
The OpenRouter model is chosen by nobody at runtime: it is the verbatim value of the
`openrouter_model` string in config.json (default `""`, `SettingsStore.cpp:70`), read
once at startup (`main.cpp:385-395`), copied into `OpenRouterSettings{key, model}`
and sent as `body["model"]` on every cleanup request (`OpenRouterProvider.cpp:148-172`).
No default model exists (`kDefaultModel = ""`, `OpenRouterContract.h:35`), no GUI editor
exists (Privacy page only displays it: `MainWindow.cpp:1630-1632`), and an empty/invalid
model silently disables cleanup — the user just gets raw, uncleaned text.

## Goal
The user can pick the cleanup model from what OpenRouter actually offers: a picker in
Settings -> Cleanup populated by a live `GET /api/v1/models` fetch, persisted through the
existing `openrouter_model` key with the restart-banner contract, plus a manual id input
as a fallback escape hatch. Fetch happens only on explicit Refresh (never startup/page
open), the key is never logged, and offline-first behavior is preserved.

## Non-goals
- Auto-choosing a "best" model without user confirmation (the picker IS the selection;
  nothing picks for the user).
- Changing how cleanup requests are made (provider request shape, timeouts, retry,
  error taxonomy untouched).
- Changing `openrouter_model` schema (`hot_apply=false` stays; restart banner is the contract).
- Persisting the fetched model list (runtime cache only; refetch on Refresh).

## Facts (verified file:line)
- Schema: `{"openrouter_model", String, 0, "", 0, 0, false}` (SettingsStore.cpp:70);
  `{"llm_provider", String, 0, "local", 0, 0, false}` (:69). Hot-apply pins exist
  (`IsHotApply(llm_provider)==false`, test_overlay_settings_ui.cpp:980).
- Startup: model+key read once (`main.cpp:376-401`); empty -> warning + provider never
  built; provider copies settings (`OpenRouterProvider.cpp:129-130`) and never re-reads.
- Request: `body["model"] = settings_.model` verbatim (:148-172); POST openrouter.ai
  `/api/v1/chat/completions` with `Authorization: Bearer <key>`; contract constants
  in `OpenRouterContract.h` (kHost/kPath/kApiKeyEnv/kApiKeyProvider/kDefaultTimeoutMs).
- Transport is POST-only today: `IHttpTransport::Post()` (IHttpTransport.h:65-70),
  `WindowsHttpClient` hardcodes WinHTTP POST (:67,96). Implementers in tree:
  `WindowsHttpClient` (src) + `FakeTransport`*3 + `ScriptedTransport` + `GateTransport`
  (5 in tests/) + `LoopbackTransport` (bench/lwf_cloud_bench.cpp:55).
- `HttpRequest{host,path,headers,content_type,body,timeout_ms}` (IHttpTransport.h:11-18);
  `Post(const HttpRequest&)` shape.
- MainWindow owns NO IHttpTransport today; the only WindowsHttpClient is a local in
  main.cpp:373. MainWindow's stores are injected via a reset-stores struct
  (main.cpp:596 uses reset_stores.api_keys).
- nlohmann_json already linked to lwf_core (CMakeLists.txt:132) and used by the provider.
- `src/llm/OpenRouterProvider.cpp` is in LWF_CORE_SOURCES (CMakeLists.txt:84);
  `tests/test_openrouter.cpp` already exists (:177) with ScriptedTransport/GateTransport.
- Network gate: WinHTTP only in GrpcInferenceProvider / WindowsDownloader /
  src/stt/cloud/WindowsHttpClient (check_no_network.sh:15); `openrouter.ai` literals
  allowed only under src/stt/cloud/ or src/llm/OpenRouter* (:20).
- UI: today NO control writes `openrouter_model` (grep 0). Cleanup card
  `##card_cleanup_backend` (MainWindow.cpp:1561) holds `##llm_provider` combo (:1566-1579)
  + `DrawApiKeyRow("openrouter",...)` (:1590-1594) + pointer text "The model is set in
  config.json; see the Privacy tab." (:1595), pinned by tests (:1555-1556) — must be
  replaced, pin updated honestly (goal is to retire that pointer).
- Key resolution for openrouter at startup bypasses ConsentGate (manual store-first
  env-fallback in main.cpp:376-382; ConsentGate has no openrouter row).
- Unselected-state buffers in MainWindow use the `TextField` struct with `seeded`
  semantics (MainWindow.h:185-194); key-row buffers clear on commit/tab switch.
- `MatchMicDeviceIndex` (SettingsUiModel.h:33) returns `i+1` for a `names` list with a
  "System default" sentinel at 0. The model combo has NO sentinel; contract must be
  defined independently (see Task B).
- Copy-lint bans whole words {oops,fsm,vad,llm,whisper,provider,providers,pipeline,
  pipelines,stt} + any `!` in literals across all of src/ui (test :1112-1184).
  Copy-lint scans literals only; runtime strings (errorDetail) are NOT covered by it —
  runtime text must still be redacted + capped for hygiene, not lint.
- `PushFont`+`TextUnformatted` card-title pattern; `BeginCombo` + `SetNextItemWidth(-1.0f)`
  (`MainWindow.cpp:1530-1537`); card-order precedent in mic card tests.

## Planner-review objections -> resolutions (recorded 2026-11-08)
- CRITICAL missing 6th implementer (LoopbackTransport bench): resolved by a
  non-pure throwing `Get` default — no bench churn, no STT/fake breakage; only
  ScriptedTransport (tests/test_openrouter.cpp) overrides it.
- CRITICAL missing CMakeLists entry: Task A adds `src/llm/OpenRouterModels.cpp` to
  LWF_CORE_SOURCES explicitly.
- CRITICAL UI-thread block (8s GET in ImGui frame): Refresh runs OFF-THREAD on a worker
  with a result latch; button shows busy until latch consumed; AC2 reframed to "no
  synchronous block, Refresh is the only network trigger". Timeout stays 8000ms
  (kDefaultTimeoutMs) because the app never freezes.
- MAJOR `Get(path,headers)` loses host/timeout: seam is `Get(const HttpRequest&)`
  mirroring `Post`, body empty.
- MAJOR buffer under-specified: Task B uses `TextField openrouter_model_field_` with a
  defined seed/commit contract (declared, non-secret value: seeded once from current
  value, committed trimmed, buffer synced on commit; NOT cleared like key rows).
- MAJOR MatchModelIndex contract: Task B defines the synthetic-entry rule first; helper
  returns -1 for "not in fetched list" (no sentinel semantics).
- MINOR errorDetail sanitization: RedactAuthValues + 500-char cap (copied from provider
  :176-182); copy-lint applies to literals only.
- MINOR hundred-item combo: only the open combo iterates Selectables — rendering cost
  accepted (same as mic picker precedent).

## Approach (chosen) — V2 with review amendments

Sequential two tasks, disjoint files, contract fixed here:

Task A (core): HTTP GET seam + models fetch/parse (lwf_core, no UI).
- `IHttpTransport`: add NON-PURE virtual `Get(const HttpRequest&)` default-implemented to
  throw the same transport exception family `Post` uses (read the file first; mirror its
  failure mode). No implementer is forced to change except those that need it:
  `WindowsHttpClient` (implements via WinHTTP GET; WinHTTP-only file, gate-safe);
  `ScriptedTransport` in tests/test_openrouter.cpp (override, scriptable response queue
  mirroring how it scripts Post; GateTransport inherits the throwing default — revisit
  if provider tests call Get, they don't).
- `WindowsHttpClient::Get`: WinHTTP GET with the request's host/path/headers/timeout
  (read how Post builds `WinHttpOpenRequest`; mirror with GET verb, no body).
  WindowsHttpClient.cpp is WIN32-guarded in lwf_core (CMakeLists.txt:127-129).
- New fetcher in `src/llm/OpenRouterModels.{h,cpp}` (OpenRouter* prefix keeps the
  `openrouter.ai` literal allowlisted and the gate green):
  - CMakeLists.txt: add `src/llm/OpenRouterModels.cpp` to LWF_CORE_SOURCES next to
    OpenRouterProvider.cpp (:84). (CRITICAL fix — was missing.)
  - `OpenRouterModelList FetchOpenRouterModels(IHttpTransport&, const std::string& apiKey)`
    -> `{ FetchStatus status; std::vector<std::string> modelIds; std::string errorDetail; }`
    with `FetchStatus ∈ {Ok, Network, NonOk, BadJson, Empty}` (+ `NoKey` ONLY if key
    required; see below).
  - `GET /api/v1/models`; send `Authorization: Bearer <key>` only when key non-empty
    (endpoint semantics are not assumed: with no key, an Ok or failure are both handled).
    Timeout = `kDefaultTimeoutMs` (8000).
  - Parse with nlohmann_json; extract `data[].id` strings only; skip non-objects and
    non-string ids; dedupe (first occurrence); sort alphabetically case-insensitive for
    stable display. Ignore all other fields.
  - `modelIds` empty with parse-Ok -> `Empty` (UI shows "no models returned").
  - errorDetail: `RedactAuthValues(body/message)`, capped 500 chars (copy provider :176-182).
    Copy-lint irrelevant (literals only); cap+redact is the hygiene control.
  - Single call, no retry. Called ONLY from Task B's Refresh (worker thread).
- Tests in `tests/test_openrouter.cpp` (ScriptedTransport serves Get):
  happy path (ids+sort+dedupe in one feed), malformed JSON -> BadJson, missing/wrong-type
  `data` -> BadJson, non-string id skipped, empty `data` -> Empty, duplicate ids deduped,
  header `Authorization` present iff key non-empty, non-2xx -> NonOk with redacted detail
  (key bytes not echoed), transport throw -> Network. Mutation-proof: every status branch
  has a test that fails if it collapses (e.g. Ok with garbage).
  No `WindowsHttpClient::Get` verb test exists (WinHTTP untestable in lwf_tests) — pinned
  via full-exe build + MANUAL row (Task B); accepted as repo-sanctioned adapter limit.

Task B (UI): picker in Cleanup card (after key row), off-thread fetch + latch.
- Transport owner: `MainWindow` gets an optional `IHttpTransport*` via a setter
  (mirror how stores are injected; main.cpp wires the existing WindowsHttpClient.
  Null = Refresh shows "available after restart"-style unavailable state, never crashes).
- State (MainWindow members):
  - `std::vector<std::string> openrouter_models_` (runtime cache, never persisted),
  - `bool openrouter_models_busy_` + worker std::thread detached/joined model (see below),
  - `std::string openrouter_models_error_` (status line; TextDisabled),
  - `TextField openrouter_model_field_` (manual-id fallback buffer).
- Fetch threading: "Refresh model list" click spawns the worker calling
  `FetchOpenRouterModels(*transport, resolvedKey)` writing into a result latch
  (mutex-protected result slot); each Cleanup-card frame consumes a ready latch
  (busy=false, error/list updated). Button shows "Loading…" while busy; clicks during
  busy are no-ops (no re-entry). App loop never blocks (8s max off-thread).
  Thread lifetime: joined via a small RAII guard; worker joined before MainWindow
  destruction (or app exit path already destroys stores first — document; never let
  the thread outlive MainWindow). Non-blocking guarantee is the AC (verifiable by
  the MANUAL freeze check).
- Picker combo `##openrouter_model` (BeginCombo + SetNextItemWidth(-1), ##llm_provider pattern):
  - Entries: synthetic entry = current configured value when non-empty AND not in the
    fetched list (so the active model is always visible); fetched list after it.
    If list empty AND no current value -> single placeholder entry "No model set".
  - `MatchModelIndex(fetchedIds, selected)` (SettingsUiModel): returns index i into the
    FETCHED list; `""` or not-found -> -1. UI derives combo index: synthetic-present ?
    (match>=0 ? match+1 : 0) : (match>=0 ? match : 0). Unit tests: found, not-found
    (-1), empty selected (-1), empty list (-1), case-sensitive ("GPT-x" vs "gpt-x" miss).
  - Select -> `settings_.SetString("openrouter_model", id); restart_needed_ = true;
    ApplyAndSave();` (order pinned like ##llm_provider). Re-select allowed (consistent).
  - Each item id suffixed `##or_<index>` (unique even for duplicates, mic precedent).
  - Status word: "Not loaded" / "<n> models available" / error text (TextDisabled,
    no banned words, no `!`).
- Manual fallback line (escape hatch): `InputText("##or_custom")` "Type a model id" +
  "Use this model" -> `SetString` path above. `TextField openrouter_model_field_` seeded
  once from the current value; commit syncs buffer; NOT cleared on commit/tab switch
  (declared value is not secret — mirror model-path field lifecycle, not key rows).
- Privacy page `OpenRouter model:` line stays a read-only mirror; empty-state copy becomes
  "not set" (retire "(edit config.json)").
- Own transport must not be created per Refresh (WinHTTP init cost) — single injected instance reused.
- Pins to add (presence+ordering, hardend mic style): `##openrouter_model`,
  "Refresh model list", per-occurrence `SetString("openrouter_model"` restart-order,
  `FetchOpenRouterModels` call presence in MainWindow.cpp, busy-guard present
  (mutation: removing the busy check -> pin red), latch-consumption presence.
  Update retired-pointer pin (:1555-1556): assert the OLD literal
  "The model is set in config.json" is ABSENT and the new card copy is present (rename,
  not delete — the old pointer is the stated retirement target).
  No count-pins exist for Cleanup (verify first).
- Docs: PROGRESS row; MANUAL_TEST_WINDOWS R-c: (1) offline Refresh -> error, app still
  responsive (freeze check, watch overlay+full window stay live while Loading), (2)
  Refresh with key -> list loads, (3) select -> restart -> cleanup uses model
  (verify via Privacy mirror + a real cleanup utterance), (4) type id manually -> same.

## Rejected alternatives
- Auto-select "best" model: the user said "dynamically select model based on what is
  available" — populating the selector from the live list satisfies it; silently picking
  for them would be a different (worse) feature.
- Persisting the fetched list in config: stale, bloats additive-only schema.
- Fetching on startup/page-open automatically: violates offline-first; Refresh only.
- Putting the GET call in src/llm (WinHTTP there): fails check_no_network.sh:15.
- Manual-id-only InputText (no live list): reviewer noted it solves 80% — rejected
  because the user's explicit ask is the dynamic list; the manual input survives as fallback.
- Making Get pure-virtual: forces LoopbackTransport (bench) + all future fakes to grow
  the method; throwing default is strictly less churn.
- Making Get `(path, headers)`: loses host+timeout; `HttpRequest` mirror is the honest seam.
- Fixing `main.cpp`'s store-first key hand-roll (ConsentGate unification): out of scope,
  pinned behavior; fetcher reuses the same resolution rule (store-first -> env).
- Synchronous Refresh on the UI thread: 8s freeze of Full+Overlay (rejected by review).

## Acceptance criteria
- AC1: with a present key and network, Refresh loads OpenRouter's actual model ids;
  selecting one persists `openrouter_model` + restart banner; Privacy mirrors it.
- AC2: the app makes NO network call on startup or while browsing pages; Refresh is the
  ONLY trigger. Refresh never freezes the UI (off-thread + busy latch; re-click while
  busy is a no-op). Without network/key, Refresh shows a clear error, changes nothing,
  never crashes.
- AC3: provider behavior unchanged (`body["model"]` verbatim; no schema change; no new
  dependency; cleanup error taxonomy untouched). `Get` default-throw leaves STT + bench untouched.
- AC4: gates green (ctest 1/1, hygiene, no-network, full exe links); no existing pin
  weakened (retired-pointer pin renamed: old literal asserted ABSENT); copy-lint clean;
  PROGRESS + MANUAL rows added (incl. freeze check).

## Risks
- OpenRouter listing auth semantics unknown from code (unverified live API): both with/without
  key handled; if 401 without a key surfaces, the error routes the user to save the key.
- Batch/list response size: models endpoint returns several hundred ids; vector parse-all
  bounded by 8s timeout; only open combos iterate Selectables (accepted).
- Rate limits / vendor breakage: error state is the mitigation; no retry.
- Thread ownership: the worker must not outlive MainWindow; RAII join in destructor.
- `isDefault`-style backend ambiguity: n/a here; sort is stable (lowercase comparison).
