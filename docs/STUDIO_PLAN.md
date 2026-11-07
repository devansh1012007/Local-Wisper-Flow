# Studio pages plan (F0) — repo copy of approved plan

- Status: **F0 STOP — awaiting Devansh sign-off**. No UI code until Devansh signs this F0. Backends may land in order P02-P11 only as gated by F0 approval; shell/pages (P17-P23) are blocked until F0 is signed.
- Branch: `feature/studio-pages` from origin/main `1d95fb2` (PR #2 merged). Local docs commit `d7595d1` (PROGRESS/SUMMARY CI-green record) is NOT in origin/main — cherry-pick onto main before work starts or the docs record is lost. Baseline `0e655ec` (1d95fb2 merge + docs cherry-pick). Baseline gates at `1d95fb2`: CI run 42310536369 both jobs success; local configure 0, app build 0, ctest lwf-tests 1/1, hygiene OK. G3 zig pending (no zig on box).
- Spec: operator message 2026-11-07 (five pages + F0-F9) plus five settings-GUI bullets.
- Operator / agent: Devansh / planning session (MiMo); implementation = future agent sessions.
- This file is the repo copy of `C:\Users\devan\.opencode\plan\2026-11-07-studio-pages-and-settings-gui.md` (approved plan, P01-P24, A1-A4, C1-C2, decisions D1-D15 + D-OP1..D-OP4). If files in §4 move, re-verify §4 and §2 before acting.
- Spec-F ↔ change-ID mapping: F0=P01, F1=P02-P11, F2=P12-P16, F3=P17, F4=P19, F5=P20, F6=P21, F7=P22, F8=P23, F9=P24. P18 (spec §2 visual system) has no F-checkpoint of its own — it lands with F3 and is checkpointed by F4-F8. Workstream A=A1-A4, C=C1-C2. `CMakeLists.txt` is modified in the same commit as every phase that adds sources/tests (not a separate change).

## 1. Problem, goal, success criteria

- Problem: config is hand-edited (13/49 keys have GUI controls); dictionary/snippets are load-only with no UI; transforms/insights/scratchpad unwired or absent; no personalization UI exists.
- Goal: (A) full config surface in GUI incl. API keys, model paths, hotkey recorders; (B) five screenshot-matched local-only pages, backend-first, spec F0→F9; (C) OpenRouter non-local LLM backend.
- Success criteria: gates G1-G4 green per commit (allowlist edits land in the SAME commit as the first file needing them — P02/A1/C2); existing engine/STT/injection tests pass unchanged; network gate passes with only those P02/A1/C2 gate edits; manual checkpoints I1/D1/S1/T1/S1n-S5n/P1 pass on Windows; F0 answers (a)-(f) (see §9) accepted before any UI code.
- Non-goals: accounts/sync/team/mobile; percentile ranking (gauge labelled "Average"); PNG image export (share dialog ships with disabled "Coming soon" buttons); binary assets without R7 approval; CJK word segmentation (v1); snapshot-UI retrofit (see D-OP1).
- Constraints: AGENTS.md (no code comments; `windows.h` only in `src/os/`; OpenRouter contract recorded from vendor docs with check date); never break `config.json`/`dictionary.json`/`snippets.json`/`transforms.json`; tests changed only when incorrect (rail source-scans become incorrect at F3/P17 — same-commit update with justification).
- Reversibility: UI two-way; JSON migrations one-way mitigated by `.bak` + additive keys; `api_keys.json` deletion harmless.

## 2. File list (F0 deliverable §2)

| File | Type | IDs | Why | Dependents |
|---|---|---|---|---|
| `docs/STUDIO_PLAN.md` | new | P01 | F0 deliverable (this file) | operator |
| `src/core/{AtomicFile,TextDiff,InsightsModel,StatsStore,DictionaryStore,TransformStore,NotesStore,ApiKeyStore}.{h,cpp}` | new | P02-P09,A1 | backends | pages, tests |
| `src/core/{SnippetStore,PersonalDictionary,AppContextResolver,TransformManager,TextPipeline,SettingsUiModel}.*` | modify | P06,P07,P08,P10,P11,P13,P14,A4 | v2 + counts + result struct + `{text}` contract + wired editor models | pipeline |
| `src/core/{DictationEngine,UiBus,ConsentGate}.*` | modify | P12,P15,P16,A1 | hooks + commands + key source | main loop |
| `src/main.cpp` | modify | P12,P15,P16,A1,C1 | wiring (`RunEngine` NOT touched — dead, zero callers) | app |
| `src/ui/MainWindow.*` | modify | P17,A2,A3,A4 | shell + settings | page files |
| `src/ui/pages/*.cpp`, `src/ui/widgets/*`, `src/ui/theme/StudioPalette.*` | new | P18-P23 | pages | tests |
| `src/ui/Overlay.*` | modify | P16 | note button + `dirty_` wake (S3n checkpoint) | main loop |
| `src/os/FileDialogs.*` (new), `WindowsHotkeyListener.*` | new/modify | A2,A3 | picker + recorders | UI |
| `src/llm/OpenRouterProvider.*` (new) | new | C1 | workstream C | cleanup |
| `scripts/check_no_network.sh` | modify | P02,A1,C2 | same-commit allowlists | G4 |
| `CMakeLists.txt` | modify | same commit as every phase adding sources/tests | new sources + tests + `font_sources` scan list | G1 |
| `tests/test_*.cpp` (new + targeted updates) | new/modify | see §11 | gates | CI |
| `AGENTS.md` | modify | A1 | D3 key-persistence rule amendment (text pre-approved in P01) | agents |
| `installer/lwf.iss` | modify | P10 | seeds v1 `transforms.json` array [FACT lwf.iss:43] | TransformStore migration |
| `src/ui/theme/ThemePalette.*` | read-only | P18 | dark tokens frozen; StudioPalette separate | — |

Ordering rationale: P01 stop → data P02-P11 (green before UI) → hooks P12-P16 → shell P17 + base widgets P18 → workstream A → pages P19-P23 → polish P24 → C last. Every phase independently green; no phase depends on a later one; every gate edit travels with the file it unblocks; `CMakeLists.txt` and the source-scan file lists are updated inside each page/phase's own commit (each P19-P23 row extends `font_sources` with its new file).

## 3. Data files and migration plan (F0 deliverable §3)

All data files live under `%APPDATA%\LocalWisperFlow` (`LWF_HOME` overrides): `config.json`, `dictionary.json`, `snippets.json`, `app_profiles.json`, `history.jsonl`, `models.json`, `logs/`, `models/`, plus new `stats.json`, `notes/`, `api_keys.json` (see §3.6-3.8). Write safety for every new store: tmp + flush + rename + `.bak` copy of the previous file (factor of `SettingsStore.cpp:217-228`, which DOES carry `.bak` — `HistoryStore.cpp:81-108` does NOT, so P02 factors the SettingsStore shape), corrupt JSON → `.bad` rename + empty state, never silent overwrite. Schema keys are additive-only; unknown keys preserved on save (`SettingsStore.cpp:180-196`).

### 3.1 `dictionary.json` — 4 shapes → v2 (P06, R5)

Current store: `PersonalDictionary::Load` at `src/core/PersonalDictionary.cpp:89-139`, load-only, no `Save` anywhere in repo (grep 0 hits). It accepts two root shapes today:

- Root array: each string element → word. Non-string elements ignored.
- Root object: optional `words` (array of strings) + optional `replacements` (array of `{from, to}` objects with both fields present as strings in the object branch).

The 4 migration shapes for `DictionaryStore` (T-mig x4) are the partitions of those two roots actually seen in the wild and pinned by tests:

| Shape | On-disk form | Migration |
|---|---|---|
| S1 | Array of strings `["btw", "xyz"]` | Words keep order; `replacements = []`; write v2 object `{version:2, words:[...], replacements:[]}` on first Save. Empty string / >60-char entries dropped (`PersonalDictionary.cpp:130-134`). |
| S2 | Object with `words` only `{"words":["a"]}` | Same as S1; missing `replacements` defaults to `[]`. |
| S3 | Object with both `{"words":["a"], "replacements":[{"from":"a","to":"b"}]}` | Full fidelity; entry order preserved; dedupe exact duplicates (T-dup). |
| S4 | Object with `replacements` only `{"replacements":[{"from":"a","to":"b"}]}` (words missing) | Words default to `[]`; replacements preserved. |

v2 entry fields (additive beyond today's `{word, from, to}`): `version:2` envelope; per-word `starred` (bool, default false, used by Dictionary page sort/star, checkpoint D1) and `added_at` (ISO date string, default `""` for migrated entries); per-replacement unchanged `{from, to}`. Any other root (number, string, `null`) or unparseable JSON follows the corrupt path: rename to `.bad`, return empty + seed (§3.1 seed). First Save always writes `.bak` of the pre-migration file. `PersonalDictionary::Enforce` order is unchanged (replacements first `PersonalDictionary.cpp:233-246`, then exact-case fix + fuzzy EditDistance==1 for len>=6 at `:256-276`); P13 adds a fix-count return via additive out-param. D-OP2 (single shared dictionary instance: pipeline + page + cloud keyterms, `main.cpp:369,414` dual load) means page edits reload the SAME `PersonalDictionary` object or pipeline/keyterms go stale (T-reload).

Seed: if `dictionary.json` is absent on first run, `DictionaryStore` seeds one entry `btw` (plan P06) so the Dictionary page is not empty. Missing-file is NOT corrupt — seed, never throw (contrast `TransformManager` missing-file throw, see §3.3).

### 3.2 `snippets.json` — v1 → v2 (P07, R5)

Current store: `SnippetStore::Load` at `src/core/SnippetStore.cpp:76-128`, expects a JSON array; each element must be an object containing `trigger` (string) and `expansion` (string). Entries with empty trigger are skipped; `expansion.size() > 4000` is rejected with warning `snippet '<trigger>' rejected: expansion over 4000 chars`; duplicate triggers are last-wins with warning `duplicate snippet trigger '<trigger>', last wins`. No `Save` exists. v1 on-disk shape is therefore `[{trigger, expansion}]`.

v2 envelope: `{"version":2, "snippets":[{id, trigger, expansion, created_at, updated_at}]}`. `id` is a stable lowercase hex string derived on migration as `s-<n>` in file order (or existing value if already present); `created_at`/`updated_at` default to `""` for migrated entries and to the save timestamp for new entries. Migration reads the v1 array (and tolerates the v2 envelope on already-migrated files), applies the same 4 validation messages as Load today (trigger-empty reject, over-4000 reject, duplicate last-wins, non-object/missing-field skip) — T-val x4 pins each message — then writes v2 atomically with `.bak`. `Expand` semantics preserved: whole-phrase `Normalize` equality by default; `contains` mode does whole-word find/replace (`SnippetStore.cpp:152-186`); mixed casing/punctuation handled by `Normalize` (`:61-74`); longest-match wins when multiple triggers match (T-match); empty expansion deletes (spec R5; T-val). Trigger/expansion limits (`expansion <= 4000`) enforced on every Save with the same 4 messages surfaced to the Snippets page sheet (checkpoint S1). Missing file → seed with empty list + empty state, never throw.

### 3.3 `transforms.json` — v1 → v2, slot layout (P08/P10)

Current: `TransformManager::LoadFile` at `src/core/TransformManager.cpp:34-76` REQUIRES `{text}` in every prompt (throws `:63-65`) and THROWS on missing file (`:38`) and on parse failure (`:45-46`). Loader expects exactly a JSON array `[{name, prompt}]` (`:49-76`). The shipped file `transforms.json:1-14` is v1 array with 3 entries (Fix Grammar / Make Formal / Summarize), all containing `{text}`. Installer seeds v1 to the install dir (`installer/lwf.iss:43`, `.gitignore:11`), so a fresh install has v1 PRESENT, not missing.

v2 slot layout (P08): 10 fixed slots; slots 1-2 are built-ins (Fix Grammar, Make Formal — prompts migrated verbatim from v1); slots 3-10 empty/custom. Each slot carries `{slot, name, prompt, shortcut, enabled, updated_at}`; `prompt <= 2000` chars; `{text}` auto-appended when absent (D7 — editor copy unchanged, spec §4.4); per-slot shortcut recorder (A3) with conflict-check against `HotkeyBinding` registry (T-conflict → conflict disables + card warning). Migration (P10, T-transform-v1): on first load, if the file is a v1 array, convert entry order to slots 1..n in order, write v2 atomically with `.bak`, one-way (never write v1 again). Missing file → seed built-ins 1-2 (never construct `TransformManager`'s throwing ctor on the raw path — TransformStore wraps or seeds first; `TransformManager` itself is left throwing per existing contract). Opt-in gate (P22): when transforms opt-in is OFF, no shortcuts register and cards dim (T-optin). Run flow (P15) reuses `CommandModeController` technique (see §4), not the dead `WindowsClipboardInjector`.

### 3.4 `stats.json` — new daily-rollup store (P05, Insights)

Greenfield file. `StatsStore` (P05) persists a daily rollup keyed by `YYYY-MM-DD`: `{date, dictations, words_total, words_corrected, dictionary_fixes, wpm_60s, by_app:{<app>:count}, by_category:{<category>:count}}` plus a trailing `recent` ring for the 60 s WPM floor. Rules: 730-day cap with oldest-day eviction (T-stats incl. cap eviction); thread-safe mutex (engine worker calls `Record()`); generation counter bumped atomically on every `Record()` so the Insights page re-reads the snapshot during its normal per-frame render when the counter changes (render-driven refresh, no polling, no timer). `Record()` contract is pinned before P12 lands: takes the `ProcessResult` counts from P14 (`words_corrected`, `dictionary_fixes`) plus raw/cleaned app + duration — app/duration are NOT present at the hook today (INV-enginehooks finding: engine has no app context; `now_ms` is discarded at `DictationEngine.cpp:379`), so P12 must plumb new `IActiveWindow` capture + clock-based duration to `Record(app, duration_ms)` or drop those fields; engine-hook implementers must not assume they exist. Missing/corrupt `stats.json` → `.bad` + empty rollup. No legacy shape to migrate.

### 3.5 `notes/` — new two-file store (P09, Scratchpad)

Greenfield directory. Layout: `notes/index.json` (array of `{id, title, pinned, updated_at, snippet}`) + `notes/<id>.md` (Markdown body, one file per note). Two-file atomicity by ordering (red-team #6): write `notes/<id>.md` first (tmp+rename), then `notes/index.json` (tmp+rename). Load reconciles: index entries with missing `.md` are dropped; orphan `.md` files with no index entry are re-adopted (or pruned per pinned rule) — recovery by reconciliation, not a 2-file transaction (T-orphan-reconcile). CRUD/pin/search + 5 s undo toast support; ScratchpadPage reads the store; dictation targets the editor buffer only (P16, never other apps — hotspot `DictationEngine.cpp:475` exclusions apply). Missing `notes/` → create on first Save; corrupt index → `.bad` + empty index + orphan re-adopt sweep.

### 3.6 `config.json` — additive keys, no migration (A4)

`SettingsStore::BuildSchema()` at `src/core/SettingsStore.cpp:16-64` = 49 keys today (verified `Select-String` count). Keys are additive-only across releases; unknown keys round-trip `raw_unknown_` (`SettingsStore.cpp:180-196`); Save re-emits unknown keys verbatim (`:190-229`). No format migration: older files load by filling defaults for missing keys (`:167-178`) and clamping out-of-range numbers (`:146-149`, recorded in `clamped_`). Three additive keys land (D6 scope note at A4 time = 49 + 3 = **52 keys**, all with a control or a documented carve-out D-OP3):

| Key | Type (proposed) | Default | Source of truth / consumer |
|---|---|---|---|
| `last_page` | String | `""` (→ Home) | P17 route round-trip (T-route): `MainWindow::Page` ↔ string; unknown value → Home. `hot_apply = true`. |
| `llm_provider` | String | `"local"` | C1 OpenRouter opt-in; `"local"` default = no behaviour change. |
| `quiet_speech_boost` | Bool | false | D-OP4: has a LIVE UI toggle today (`src/ui/MainWindow.cpp:760`) but sits outside the 49 — `SetBool→SetNumber` early-returns for unknown keys (`SettingsStore.cpp:257-261`) and `GetBool` returns false (`:245-249`), so the toggle never persists and `main.cpp:476,561` always reads false into `EngineSettings.quiet_speech_boost` (`EngineTypes.h:25`). Adding to BuildSchema is a behaviour FIX, not cosmetics. `hot_apply = true`. |

Carve-outs (D-OP3, T-a4 scan): `mini_x`/`mini_y` (set by drag, no control), `schema_version` (readout), `first_run` (write-only; owned by Welcome flow). Every other key (incl. internals per D6) has a real control; readout counts as a control. A4 keys consumed live must be added to the `ApplySettings` consumer at `main.cpp:557-569` (today: `min_hold_ms`, `quiet_speech_boost`, `sound_enabled`, `streaming_*`, `spoken_commands_aggressive`, `snippet_mode`, `reduce_motion` + `LoadBindings`) or they apply only after restart.

### 3.7 `api_keys.json` — new, opt-in cloud keys (A1, D3)

Greenfield file next to `config.json` (`%APPDATA%\LocalWisperFlow\api_keys.json`, same `LWF_HOME` resolution). Shape: `{"gnani":"...", "elevenlabs":"...", "muse":"...", "openrouter":"..."}` (absent key = no stored key). Rules (D3, amended AGENTS.md text): separate file, NEVER `config.json`; values redacted from every stored log line via `lwf::RedactSecret`/`RedactAuthValues` (`src/core/Redact.h:39-74`); masked (`****`) in GUI, never echoed; `IsSensitiveHeaderName` already covers `Authorization: Bearer` (`src/core/Redact.h:28-37`). Precedence is store→env: consult `ApiKeyStore::Get(provider)` first, fall back to `std::getenv(ApiKeyEnvFor(provider))` where `ApiKeyEnvFor` is the single source in `src/core/ConsentGate.cpp:13-15` (3 providers: `GNANI_API_KEY` / `ELEVENLABS_API_KEY` / `META_MODEL_API_KEY`). Both read points must be intercepted — startup snapshot `main.cpp:398` and live `api_key_source` lambda `main.cpp:445-449` (`SttRouter.cpp:25` calls it per utterance) — or one path will bypass the store (INV-keys-openrouter §3). `ConsentGate` itself needs no change (it only consumes `api_key_present` bool). Deletion of the file is harmless (falls back to env). Write path goes through P02 atomic helper with `.bak`; file-write allowlist entry lands in the same commit (G4 same-commit rule).

### 3.8 `app_profiles.json` — additive `category` (P11)

File shape today (`AppContextResolver.cpp:69-115`): `{"profiles":{<key>:{prompt_addendum, casual, trailing_period, use_llm, paste_chord}}, "rules":[{"match":{process, title_contains, title_regex}, "profile":<key>}]}`. `AppProfile` (`AppContextResolver.h:9-16`) has no `category` field. Rules ignore unknown fields, so additive `category` is safe to add. P11 adds `category` (6 categories: Email / Chat / Code / Notes / Search / Other — spec §4.1) with process-name first, title second resolution (T-cat-mapping); seeds defaults via `UseBuiltInProfiles()` (`main.cpp:126-170`: email/chat/code/notes/default). No writer exists in repo today (grep 0 `Save` hits; shipped file is hand-authored/untracked) — P11 must add `Save` + seed defaults. No format break: existing profiles load with `category` defaulting to Other.

## 4. Architecture (why the hooks land where they do)

- Stats hook is `DictationEngine::StopAndTranscribe` → `injector_.InjectText(final_text)` at `src/core/DictationEngine.cpp:475` on the engine worker thread (`:66`). `RunEngine` at `src/main.cpp:193` (containing `:254`) is DEAD (zero call sites) — do not hook there. Exclusions are structural so hooking only `:475` records exactly successful dictations: command mode early-returns at `:458` (before pipeline `:461`); cancel/empty return at `:463-472` (before `:475`); `PasteLast` is the separate `HandlePasteLast` at `:527` (never reached from `:475`); `CommandModeController` injects separately at `:134`. Verified by INV-enginehooks (5 pin-claims). Stats `Record()` is called on the worker thread → store needs a mutex (P05). `TextPipeline::Process` returns string-only today (`TextPipeline.cpp:74-104`, order spoken→cleanup→dictionary→snippets) → P14 adds additive `ProcessResult {text, words_corrected, dictionary_fixes}` overload; old signature delegates. `words_corrected` = word-LCS diff raw vs cleaned (P03 `TextDiff`); `dictionary_fixes` = `Enforce` fix count (P13 additive out-param). Guard-rejections (cleanup falls back to raw) count as 0 corrected (T-zero-when-no-cleanup).
- Transform run flow (P15) executes on the engine/worker thread, same pattern as command mode (`CommandModeController::Run` at `src/core/CommandModeController.cpp:73-148`: WorkerQueue + `future.wait_for` + `RequestCancel()` + 3 s drain). New `UiCommandType::StartTransform` only TRIGGERS it from the UI (the UiBus drain at `main.cpp:541-577` is synchronous on the main thread — LLM work never runs there, or the UI freezes for `cleanup_timeout_ms`; `LocalLlamaProvider::Process` blocks on the caller thread `src/llm/LocalLlamaProvider.cpp:95`). Clipboard technique = `WindowsSelectionCapture` at `src/os/WindowsTextInjector.cpp:366-437` (save → Ctrl+C → poll ≤300 ms at 20 ms → restore at `:439-470`). `WindowsClipboardInjector` (`src/os/WindowsClipboardInjector.cpp:191-249`) is dead code (0 instantiations) — do not copy as template. Flow: clipboard save → Ctrl+C → 300 ms check → LLM → validate (preamble/3x+200) → paste → restore; 40 s diff retention (T-runflow 5 branches + clipboard restored each + no-UI-block).
- Scratchpad injection (P16): `ITextInjector` is single-method `InjectText(const std::string&)` (`src/core/ITextInjector.h:5-10`) — no iface change. Scratchpad target is a new impl/decorator + routing flag (`OpenScratchpad` command); transcript goes to the editor buffer ONLY, never other apps (T-never-elsewhere). Mini-overlay note button (S3n) needs `dirty_=true` on hover/poll change or it freezes (Overlay idle early-out `Overlay.h:74-78`; 10 `dirty_=true` sites in `Overlay.cpp`).
- UI shell (P17/P18): `MainWindow::Page` enum + rail `NavItem[]` (`MainWindow.h:73-85`, `MainWindow.cpp:351-434`, 7 pages today; 996-line monolith; no modal/popup/toast infra — zero `BeginPopupModal` in `src/`; only "Saved"/"Copied" dwells at `:308-315,690-694`). New rail: Home / Insights / Dictionary / Snippets / Transforms / Scratchpad | Settings. Settings tabs: General (D11) / Speech / Cloud / Cleanup / Privacy / About. `MainWindow` holds live refs, writes via `ApplyAndSave()` (`:527-535`, 16 calls + def). `UiBus::ReadStatus()` has zero callers. `UiCommandType` today has 8 values (`UiBus.h:10-19`); P15/P16 add `StartTransform` + `OpenScratchpad`; the main.cpp drain switch has NO default case (`:542-576`) so new values without cases are silently ignored. Live refs stay (D-OP1 — no snapshot retrofit; enforce "widgets on main thread only"). One ImGui + one GL context per window; `CreateContext()` return is kept, never re-fetched via `GetCurrentContext()` (dark-screen bug, AGENTS.md). Dark tokens in `src/ui/theme/ThemePalette.h` are frozen (contrast-tested `tests/test_overlay_settings_ui.cpp:176-224`); StudioPalette is separate and must not change dark values or `saved_dwell`. New `BeginLearnMode` target-key parameter is needed for A3 (learn mode is a single global hardcoded to `hotkey_ptt` at `main.cpp:579-585`); per-key targeting, pending-target state, and conflict-check API are new work (no conflict API exists — `HotkeyBinding` only Parse/Serialize/IsSatisfied at `HotkeyBinding.h:22-24`).

## 5. Palette table — light theme (PROVISIONAL)

> **Status flag (mandatory): visual estimate pending pixel-picker; source images to be saved to `docs/screenshots/source/`.** The values below are PROVISIONAL VISUAL ESTIMATES for planning P18 only. They are NOT pixel-measured and must be replaced by exact hexes extracted from the five source images (Insights, Dictionary, Snippets, Transforms received 2026-11-07; Scratchpad outstanding per R-01) before StudioPalette tokens are frozen. Contrast policy applies to the MEASURED values: every text pair ≥ 4.5:1, every meaningful colour pair ≥ 3:1 (T-contrast). Do not lighten/alter the existing dark tokens to "fix" perceived contrast (viewport/context bug history in AGENTS.md).

| Token | Role | Estimate (PROVISIONAL) | Notes |
|---|---|---|---|
| `bg` | window / page background | `#FAF8F2` (warm ivory) | brightest surface in screenshots |
| `surface` | cards, rail, editor | `#FFFFFF` (near-pure) |  |
| `border` | 1 px hairlines | `#E5DFD2` | decorative; may sit below 3:1 by policy |
| `text` | primary text | `#1C2B26` (near-black green) |  |
| `muted` | secondary text | `#6B7672` |  |
| `accent` | primary button, active nav | `#1F3A5F` (navy) | pair with `on_accent` for 4.5:1 |
| `on_accent` | text on accent fill | `#FFFFFF` |  |
| `ok` | success / Ready | `#3F6B52` |  |
| `warn` | restart / opt-in notes | `#8A6D2B` |  |
| `err` | error copy | `#9B2C2C` |  |
| `err_fill` | attention dot | `#B5565F` |  |
| `focus` | 2 px focus ring | `#1F3A5F` |  |
| `surface_alpha` | mini badge opacity | `0.90` | mini-overlay palette untouched |

Existing dark palette (`kDarkPalette` in `src/ui/theme/ThemePalette.h:46-61`) and light palette (`kLightPalette`, `:64-79`) remain the enforced source of truth for the current UI (`tests/test_overlay_settings_ui.cpp:176-191`). StudioPalette (P18) is a NEW `src/ui/theme/StudioPalette.{h,cpp}` carrying the measured values once the pixel-picker pass is done; the table above is the starting point for that measurement, not the final tokens.

## 6. Fonts — EB Garamond + Inter, OFL (D12)

Approved for bundling: **EB Garamond** (display wordmark + page titles) and **Inter** (UI body/labels) — both SIL Open Font License 1.1, approved per R7 / D12 operator sign-off 2026-11-07.

| Family | Intended file names (to be placed under `src/ui/fonts/` or equivalent) | License | Estimated size |
|---|---|---|---|
| EB Garamond | `EBGaramond-Regular.ttf`, `EBGaramond-Medium.ttf`, `EBGaramond-Italic.ttf`, `EBGaramond-Bold.ttf` (or variable `EBGaramond[wght].ttf`) | SIL OFL 1.1 (include `OFL.txt` alongside) | ~200-800 KB for 4 static cuts; ~300 KB for variable |
| Inter | `Inter-Regular.ttf`, `Inter-Medium.ttf`, `Inter-SemiBold.ttf`, `Inter-Bold.ttf` (or variable `Inter[opsz,wght].ttf`) | SIL OFL 1.1 (include `OFL.txt` alongside) | ~350 KB per static cut (~1.2 MB for 4); ~350 KB for variable |

Rules: only OFL fonts are bundled; no other binary assets without R7 approval. License text ships with the fonts. Final sizes/hashes recorded at P18 download time (this table records intent — sizes above are estimates, `[ASSUMED]` pending the actual TTF downloads from the Google Fonts / GitHub releases). No `SetWindowFontScale` — typography via scoped `PushFont(nullptr, design_px)` + `PopFont` through `metrics::font_*_px` design tokens, DPI exactly once via `style.FontScaleMain` (see `ThemePalette.h:161-169`). New page/widget files must be added to `font_sources` in `tests/test_overlay_settings_ui.cpp:252-260` in the same commit or they escape the `SetWindowFontScale` ban.

## 7. Banner copy policy (D13) + label style (D14) + share dialog (P21)

- Banner copy (F0(c)+(d), D13): original banner copy; dark blurred gradient placeholder; NO logos/photos; italic accent word OFF. The Insights share dialog (P21) is a placeholder with disabled "Coming soon" buttons for the PNG/SaveImage actions (PNG export is a non-goal). No internal terms, no "Oops", no exclamation marks (F9 copy-lint, P24 — `T-copy`).
- Labels (F0(e), D14): all-caps 12 px labels kept as in screenshots (sentence case rejected). Wordmark 18 px, page title 28 px, card title 15 px, body 13 px, mini 8.5 px per existing `metrics::font_*_px` tokens (`ThemePalette.h:103-107`).

## 8. Pages summary (spec F0-F9 mapping)

| ID | Page | Contents | Store / hook |
|---|---|---|---|
| P19 (F4) | Dictionary | add/duplicate/search/star/undo; dialog + sort; checkpoint D1 | `DictionaryStore` (shared instance) |
| P20 (F5) | Snippets | exact validation copy (4 messages); sheet; checkpoint S1 | `SnippetStore` v2 |
| P21 (F6) | Insights | tabs "Your usage" / "Your voice"; gauge labelled "Average" (never percentile); share dialog disabled buttons; generation-counter refresh; checkpoint I1 | `StatsStore` + `InsightsModel` |
| P22 (F7) | Transforms | editor, shortcut recorder, diff viewer (Win+Alt+O); opt-in gate dims cards/shortcuts; checklists T1-T6 | `TransformStore` + run flow P15 |
| P23 (F8) | Scratchpad | editor window 760x620 (min 480x360), tabs, Markdown; Overlay note button; checklists S1n-S5n | `NotesStore` + P16 target |

## 9. F0 answers for Devansh

### (a) Palette / source images
Pixel-measured palette is BLOCKED on the five source images. Net: PROVISIONAL visual-estimate table in §5 (flagged "visual estimate pending pixel-picker"); the five images must be saved to `docs/screenshots/source/` (Insights/Dictionary/Snippets/Transforms received 2026-11-07; Scratchpad outstanding — R-01), then a pixel-picker script extracts exact hexes and T-contrast re-runs on measured values. No StudioPalette token is frozen until then.

### (b) Fonts
EB Garamond + Inter (both OFL) approved for bundling (R7); exact file names/sizes recorded in §6 before download — **D12** (operator sign-off 2026-11-07).

### (c) Banner copy
Original banner copy — **D13** (operator sign-off 2026-11-07).

### (d) Banner imagery
Dark blurred gradient placeholder; no logos/photos; italic accent word OFF — **D13** (operator sign-off 2026-11-07).

### (e) Label casing
All-caps 12 px labels kept as in screenshots — **D14** (operator sign-off 2026-11-07).

### (f) "Your voice" tab
Tab stays with empty state "Not available yet." — **D15** (operator sign-off 2026-11-07). Insights tabs are "Your usage" / "Your voice" (spec §5.1, included in P21).

Decisions D1-D11 + D-OP1..D-OP4 are recorded in the approved plan §G (D1 branch from `1d95fb2`; D2 OpenRouter workstream C; D3 `api_keys.json`; D4 allowlist; D5 embed `{text}` in `default prompts` as `<selection>{text}</selection>` + data-not-instructions sentence; D6 all 49 keys; D7 auto-append `{text}`; D8 order F0-F3 → A → F4-F9 → C; D9 picker+text model paths; D10 recorders for all hotkeys; D11 6th "General" Settings tab; D-OP1 live refs; D-OP2 shared dictionary instance; D-OP3 T-a4 carve-outs; D-OP4 register `quiet_speech_boost`).

## 10. Change list (ordered, atomic) — summary

P01 F0 file list/migration/answers/palette (this commit). P02 `AtomicFile` + gate allowlist same commit. P03 `TextDiff` LCS. P04 `InsightsModel` pure formulas. P05 `StatsStore` daily rollup 730-day cap. P06 `DictionaryStore` v2 4-shape migration `.bak` seed. P07 `SnippetStore` v2 ids/timestamps/Save/4 messages/longest-match. P08 `TransformStore` 10 slots built-ins 1-2. P09 `NotesStore` index+md CRUD/pin/search/undo. P10 transforms v1→v2 migration + installer. P11 `AppContextResolver` `category` 6 categories. P12 `StatsStore::Record` after successful inject at `DictationEngine.cpp:475` only. P13 `Enforce` fix-count return. P14 `TextPipeline::ProcessResult` overload. P15 engine transform mode + `UiCommandType::StartTransform` (worker thread). P16 scratchpad injection target + `OpenScratchpad` + Overlay note button. P17 `Page` enum + rail + Settings tabs (General D11) + `last_page` + source-scan updates same commit. P18 `StudioPalette` + widgets + toast/sheet/modal infra. P19-P23 pages. P24 contrast/copy/keyboard sweep. A1 `ApiKeyStore` + `api_key_source` store→env + gate allowlist same commit + AGENTS.md rule amendment (pre-approved here). A2 `FileDialogs` picker. A3 hotkey recorders (5 config + studio shortcuts). A4 Settings controls for all 49 keys (52 at A4 time). C1 `OpenRouterProvider` + `OpenRouterContract` + `llm_provider` key. C2 gate allowlist + doc. Full detail per row (symbol, what, how, why, depends, verified-by, risk, revert) is in the approved plan §E — preserved by reference; this file carries the ordering and the F0-expanded sections.

## 11. Tests and validation

- Existing guarantees: engine/STT/injection expectations untouched; source-scan UI tests pin rail markers / `PushFont` / `ApplyAndSave` counts / `font_sources` list (`tests/test_overlay_settings_ui.cpp:252-307,458-518`) → rail+base updated at P17/P18 same-commit, and each page landing (P19-P23) adds its new file to the scan list in the same commit (assertions encode pre-change structure = incorrect after change); store tests (`test_snippets`/`test_dictionary`/`test_transform`) are baseline.
- Traceability: formulas→T-formulas (wpm 0/59/60 s, midnight streak, gap longest-streak, calendar quartiles, shares sum 100/0, distinct apps); 4 migration shapes→T-mig; 4 snippet messages + longest-match/casing/punct/empty→T-val; atomic crash (stale tmp ignored)→T-atomic; stats 730-day cap→T-stats; Enforce count→T-count; pipeline result→T-diff; categories→T-cat-mapping; keys redaction→T-keys-redacted; OpenRouter request build w/ fake transport→T-request-build; run-flow 5 branches + clipboard restored each + no-UI-block→T-runflow; opt-in OFF registers nothing→T-optin; record exclusions (cancel/empty/cmd/pastelast/transform/scratchpad)→T-no-record; scratchpad never injects elsewhere→T-never-elsewhere; route round-trip/unknown→Home→T-route; notes CRUD/search/pin/undo + orphan reconcile→T-notes; transform v1→v2→T-transform-v1; dictionary reload→T-reload; contrast every token pair→T-contrast; every-key-has-control (D-OP3 carve-outs)→T-a4.
- Baseline: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4 && ctest --test-dir build` + `scripts/check_hygiene.sh` + `scripts/check_no_network.sh` — green at `1d95fb2` (CI run 42310536369). G3 zig: pending on this box, recorded honestly per task.
- Non-test validation: manual checkpoints I1/D1/S1/T1-T6/S1n-S5n/P1 (spec §8) by Devansh on Windows; screenshots into `docs/screenshots/` (spec §9).

## 12. Edge cases and failure modes

| Case | Where it bites | Expected | Handled by |
|---|---|---|---|
| Fresh run, `transforms.json` absent | `TransformManager` ctor throws [`TransformManager.cpp:38`] | seed built-ins, never construct on raw path | P08/P10 |
| Installer-seeded v1 `transforms.json` | [`lwf.iss:43`] | convert to v2 on load, `.bak` | P10/T-transform-v1 |
| Corrupt dictionary/snippets file | Load-false paths | `.bad` rename + empty state, never silent overwrite | P06/P07 + tests |
| Crash between md and index rename | P09 two files | load reconciles (drop dangling / adopt orphan) | T-orphan-reconcile |
| Stale `.tmp` after crash | `tests/test_settings.cpp:119` precedent | ignored on load | P02/T-atomic |
| `Record()` on engine worker thread | `DictationEngine.cpp:66,:475` | mutex-guarded store | P05 |
| Pipeline dictionary stale after page edit | `main.cpp:369,414` dual load | shared instance reloads | D-OP2/T-reload |
| `pasteLast` double-count | `DictationEngine.cpp:527` | not recorded (separate method, hook at `:475` only) | T-no-record |
| LLM call freezes UI | `LocalLlamaProvider.cpp:95` blocking | run flow on worker thread | P15/T-no-ui-block |
| Shortcut conflicts | `HotkeyBinding` registry | conflict disables + card warning | P08/T-conflict |
| Gate edit forgotten | `check_no_network.sh` | G4 would fail first push | same-commit rule §E |
| Rail restructure breaks source-scans incl. `font_sources` | `test_overlay_settings_ui.cpp:252-307,458-518` | update same commit, justified | P17/P18 |
| `api_keys.json` leaked to logs | `RedactSecret` paths | never logged, masked in GUI | A1/T-keys-redacted |

## 13. Risks, assumptions, open questions

| ID | Type | Description | L x I | Mitigation | Owner | Status |
|---|---|---|---|---|---|---|
| R-01 | risk | Scratchpad screenshot missing; palette = visual estimates until pixel pass | H x M | 4/5 received 2026-11-07; F0 waits for 5th; pixel-measure in P01 | Devansh | 4/5 received, pending |
| R-02 | assumption | `IHttpTransport` fits OpenRouter (POST + headers + JSON body) | L x M | first C1 task = contract from vendor docs + fake-transport test | impl | open (POST-only confirmed INV-keys-openrouter §4; GET /models out of C1 scope) |
| R-03 | risk | first Save overwrites real dictionary/snippets | M x H | `.bak` + T-mig + T-atomic | impl | designed |
| R-04 | risk | G3 zig unavailable | H x L | record "pending on Windows" honestly | Devansh | accepted |
| R-05 | risk | modal infra + `NavEnableKeyboard` regressions | M x M | F3 checkpoint + keyboard sweep F9 | impl | open |
| R-06 | risk | D6 exposes `schema_version` etc. | L x L | clamps bound values; readout-style control (D-OP3) | Devansh | accepted (D6) |
| R-07 | — | hook site `main.cpp:254` | — | RESOLVED: dead code; hook = `DictationEngine.cpp:475` | — | closed |
| R-08 | risk | D5 embedded `{text}` vs spec "text as data" | M x M | default prompts embed `<selection>{text}</selection>` + delimiters sentence | impl | designed |
| R-09 | risk | local docs commit `d7595d1` not in `origin/main` | L x L | cherry-pick onto main before starting | Devansh | open |
| R-10 | risk | local checkout on `fix/gui-typography-dpi`, not main | M x M | switch/branch from `1d95fb2` before P02 | impl | open (current branch `feature/studio-pages` @ `0e655ec`) |

Pre-mortem top causes (post-red-team): (1) hook at dead `RunEngine` → Insights always empty [mitigated: P12 at `:475` + T-no-record]; (2) gate edit lags file → CI red [same-commit rule]; (3) migration eats user file [`.bak`+T-mig+T-atomic]; (4) dictionary dual-source staleness [D-OP2+T-reload]; (5) UI freeze on transform LLM call [P15 worker-thread + T-no-ui-block]; (6) source-scan cascade at F3 [same-commit updates]; (7) scope creep across 3 workstreams [phase gates, C last].

## 14. Approval gate (F0 sign-off checklist for Devansh)

Devansh must explicitly accept each line before UI work (P17+) begins. Reply with exact wording per line or approve the file as-is.

- [ ] (a) Palette approach: PROVISIONAL visual estimates in §5 accepted pending pixel-picker; 5 source images to be saved to `docs/screenshots/source/`, then exact hexes extracted (R-01).
- [ ] (b) Fonts: EB Garamond + Inter (OFL) file names in §6 approved (D12).
- [ ] (c)(d) Banner: original copy + dark blurred gradient placeholder, no logos/photos, italic accent OFF (D13).
- [ ] (e) Labels: all-caps 12 px kept (D14).
- [ ] (f) "Your voice" tab: stays with "Not available yet." (D15).
- [ ] Data migrations (§3.1-3.8) accepted, incl. 4-shape dictionary table, snippets v2 envelope, transforms slot layout, `stats.json`/`notes/`/`api_keys.json` shapes, 52-key `config.json` (49 + `last_page` + `llm_provider` + `quiet_speech_boost`).
- [ ] AGENTS.md amendment text (D3 `api_keys.json` + D2 OpenRouter exception, text only) accepted — edits are in the same review batch as this file.
- [ ] Workstream order (data → hooks → shell → A → pages → polish → C) accepted (D8).
- [ ] Outstanding non-UI hygiene (R-09/R-10 branch hygiene) assigned.

Signed: _Devansh, date: ____ (F0 approved → UI code unblocked from P17; backends P02-P16 proceed as already gated.)
