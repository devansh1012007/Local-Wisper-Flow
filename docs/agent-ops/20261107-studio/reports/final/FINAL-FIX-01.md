<!-- ORCH-DIGEST:START -->
agent: FINAL-FIX-01 | scope: FINAL-01 MAJOR1-3 + HeatLevel + tombstone comment + 2 smoke checks | verdict: COMPLETE
findings: MAJOR1 FIXED | MAJOR2 FIXED | MAJOR3 FIXED (in-scope; AGENTS endpoint prose DEFERRED write-set) | HeatLevel FIXED | comment FIXED | downgrade VERIFIED | detached worker CARRIED (outside write-set)
claims_audit: FINAL-01 §blind/gates/claims checked against code — MAJOR1-3 reproduced at HEAD before edits; smoke claims verified from 0e655ec loaders + main.cpp lifetime order
band_aids_found: none — schema flag + banner + branched copy + integer-bucket parity only; no sleeps/retries/suppressions
top_3_risks: (1) AGENTS.md/check_no_network.sh OpenRouter prose still stale (FORBIDDEN write-set, docs follow-up); (2) detached transform worker F-2 still open by design; (3) Windows manual pass + live OpenRouter + G3 still NOT-RUN
optimizations: DO-NOW 0 | DO-LATER 0 | NOT-WORTH-IT 0
reversal_check: done (revert = 7 files; HeatLevel pins encode new parity, failing pre-fix at 24 asserts)
confidence: high on portable gates (configure/build/ctest/hygiene/no-network rerun verbatim); Windows runtime/G3/live-key NOT-RUN
<!-- ORCH-DIGEST:END -->

# FINAL-FIX-01 — Final improver (run 20261107-studio, wave 5.1, branch feature/studio-pages)

## 1. Context report (pre-edit)

Required reading done in order: STANDING-ORDERS.md, AGENTS.md, 00-mission.md, 01-work-ledger.md,
FINAL-01.md (full), REV-W5-DELTA.md, docs/PROGRESS.md, docs/STUDIO_PLAN.md. Branch
`feature/studio-pages`, HEAD `4fb18be` at start. COMMS.md read at start / before commit.
Root causes confirmed before touching code [VERIFIED]:

- MAJOR1 [VERIFIED src/core/SettingsStore.cpp:69 + src/ui/MainWindow.cpp:1283-1291]:
  `llm_provider` ships with `hot_apply=true`, and the Cleanup backend combo calls `ApplyAndSave()`
  without `restart_needed_`. The provider is constructed once in `src/main.cpp:368-396`, so the
  GUI pretends the switch is live. `stt_provider` (MainWindow.cpp:1148-1156) already sets
  `restart_needed_=true` — the exact pattern to copy. `openrouter_model` is already
  `hot_apply=false` (SettingsStore.cpp:70).
- MAJOR2 [VERIFIED src/ui/MainWindow.cpp:946-965]: Trust card `cloud_active` keys only off
  `stt_provider` + `stt_consent_accepted`; with `llm_provider=openrouter` it still renders
  "Everything runs on this computer." Same bug class as R-W5-002 (Privacy/About) fixed at
  MainWindow.cpp:1337-1340 / 1378-1381.
- MAJOR3 docs [VERIFIED docs/STUDIO_PLAN.md:§3.1/§3.2/§3.3/§3.6/§1 + docs/PROGRESS.md: zero
  studio rows]: shipped shapes counted from code (DictionaryStore v2, SnippetStore ids, TransformStore
  built-ins, 55 keys, installer untouched, F0 STOP header). One new `//` block
  [VERIFIED `git diff 0e655ec..HEAD -- src/` — the only `+ //` lines are DictionaryStore.cpp:415-421].
- HeatLevel [VERIFIED src/ui/theme/StudioPalette.h:75-90 vs src/core/InsightsModel.cpp:190-199]:
  `HeatLevel` float `>=0.25/0.5/0.75` bands give 25/50/75→2/3/4; `CalendarLevel` integer
  `day*4/max` buckets give 1/2/3. Pre-change `>` also diverged inside bands (e.g. 26→widget 2 vs
  model 1), so flipping to `>=` is not parity — exact parity needs the integer form.
- Smoke (a) downgrade [VERIFIED `git show 0e655ec:...` loaders]: PersonalDictionary expects
  array/`{words,replacements}` (v2 object → empty, returns true); SnippetStore expects array
  (`!is_array → false`); TransformManager expects array (`!is_array → throw`). New branch loaders
  dual-read v1+v2. Smoke (b) detached worker [VERIFIED DictationEngine.cpp:779-792]: `detach()` holds
  raw `llm` pointer; `main.cpp:369-371` providers declare before `engine` (556) so engine destructs
  first but a detached thread can outlive both.

Invariants preserved: additive-only config keys (raw_unknown_ round-trip untouched); no new deps;
no `//` additions (AGENTS.md); copy-lint (whole-word ban, no `!`) on every new display literal;
single-flight/copy/redaction semantics untouched; F-2/DECISION-1/migrations described, not faked.

## 2. Phased changes executed

### Phase 1 — MAJOR1 restart flag (FIXED)
- `src/core/SettingsStore.cpp:69`: `llm_provider` `hot_apply true → false` (mirrors
  `openrouter_model`/`llm_model_path`). `SettingsUiModel::ClassifyDiff` now classifies
  local↔openrouter as `restart_keys`.
- `src/ui/MainWindow.cpp`: backend combo sets `restart_needed_=true` before `ApplyAndSave()`
  (mirrors `stt_provider`); `DrawCleanupPage` now calls `DrawRestartBanner(scale)` after
  `DrawPageChrome` (Speech/Cloud already did).
- `tests/test_overlay_settings_ui.cpp`: `CHECK(...IsHotApply("llm_provider")) → CHECK_FALSE`;
  added lexical guards that `restart_needed_=true` precedes `ApplyAndSave()` after `##llm_provider`
  and that `DrawRestartBanner(scale)` appears after `DrawCleanupPage`. These fail on the pre-fix tree
  (restart precedes save; banner absent) and pass post-fix.
- Self-review: no `any`/suppression; banner is the existing `restart_needed_` mechanism, not a new flag.

### Phase 2 — MAJOR2 Trust card (FIXED)
- `src/ui/MainWindow.cpp` Trust card: added `openrouter_cleanup = llm_provider == "openrouter"`;
  cloud sentence unchanged; new `Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is
  on.` warning (same house sentence as Privacy/About/Cleanup, `Color(kDarkPalette.warn)`);
  `Everything runs on this computer.` now gated on `!cloud_active && !openrouter_cleanup` with
  helper `Cloud speech and OpenRouter cleanup are off.` Copy-lint green (no banned whole-words,
  no `!`); exhaustive when both opt-ins are on.

### Phase 3 — HeatLevel parity (FIXED)
- `src/ui/theme/StudioPalette.h`: rewrote `HeatLevel` as integer buckets identical to
  `CalendarLevel` (`value >= month_max → 4`, else `value*4/month_max` clamped `1..3`, int64 math).
  DECISION: CalendarLevel is canonical (tested model of record; zero production callers today, so the
  flip is behavior-only at edges and cheap; F0/a can re-arbitrate both together).
- `tests/test_overlay_settings_ui.cpp`: pins flipped to CalendarLevel values (3/10→1, 6/10→2,
  8/10→3, 25/50/75→1/2/3, 26/51/76→1/2/3) + exhaustive parity sweep
  `HeatLevel == CalendarLevel` for month_max in {0,1,10,100} × value in -5..105 (included
  `core/InsightsModel.h`). Mutation check: reverting to `>=` float fails the sweep at every
  exact-quartile and inside-band point.

### Phase 4 — Comment + docs (FIXED in-scope; AGENTS prose DEFERRED)
- `src/core/DictionaryStore.cpp`: deleted the 7-line tombstone `//` block (:415-421). Code unchanged;
  tombstone behavior still pinned by `test_dictionarystore.cpp` cases.
- `docs/STUDIO_PLAN.md`: header → IMPLEMENTED (F0 signed 2026-11-07, code is truth); §3.1 → shipped
  `{schema_version:2, entries:[...]}` + downgrade note; §3.3 → `Polish`/`Prompt Engineer` + slots 3..n
  migration; §3.2 → `snippet-<n>`/`snippet-<ts>-<n>` int64; §3.6 → 49+6=55 with the six rows
  (`llm_provider` `hot_apply=false`); §2/§10 installer row → read-only/unchanged; §5 → shipped teal
  is truth (navy table marked discarded estimate); §9(a)/(b) → shipped teal provisional + fonts
  unbundled; §13 R-01/R-02/R-09/R-10 statuses; §14 checklist ticked; new §15 post-implementation
  truth (shipped set, F0 answers, residuals really left F-1..F-7, G3, one-way `.bak`).
- `docs/PROGRESS.md`: new “Studio pages + settings GUI + OpenRouter (20261107-studio)” section with
  one row per task T-001..T-026 + T-FINAL (commit, G1-G5, manual-check, notes), the verbatim
  FINAL-FIX-01 gate record, HEAD SHAs, G3-pending, honest limits (no contacts/accounts feature;
  DECISION-1 unconfirmed; live UNVERIFIED; Windows owed; `.bak` policy; F-1..F-4 tracked, F-5 closed).
- DEFERRED-with-reason: AGENTS.md endpoint sentence (“only in src/stt/cloud/*”) and
  `check_no_network.sh` header OpenRouter prose — both outside the brief WRITE-SET (AGENTS.md not
  listed; `scripts/**` FORBIDDEN). Recorded as a docs follow-up in §15/§6; no functional change.

### Phase 5 — Smoke checks (VERIFIED / CARRIED)
- Downgrade claim VERIFIED from the `0e655ec` loaders (empty-true / false / throw per file, see §1)
  and documented precisely in STUDIO_PLAN §3.1 + §15 and PROGRESS (replaces “cannot read” shorthand
  with the per-file behavior). No code change needed.
- Detached worker carried (F-2): file `src/core/DictationEngine.cpp` is outside WRITE-SET and the
  proper fix (`join()`/shared ownership) crosses the write-set boundary. Lifetime evidence recorded
  above; left as a tracked follow-up, not a band-aid. DEFERRED-DEPENDENCY/CARRIED, not faked.

## 3. Review audit

- Phase 1 self-review: MINOR — initial Trust/banner test edit contained a tautological check
  (`find < find + find`); corrected to `!= npos` before build. No functional impact.
- Phase 2 self-review: copy-lint literals re-scanned manually (whole-word `openrouter` ≠ banned
  `provider`/`stt`/`llm`; no `!`); `TextColored` style matches Cleanup/Privacy/About. No CRITICAL/MAJOR.
- Phase 3 mutation mental check: float `>=` or `>` each fail the sweep (edges + inside bands);
  integer form passes all 444 sweep points + 16 pins. Overflow avoided via int64 `*4`.
- Final review: `git diff` contains only the 7 write-set files (+ report); no debug artifacts,
  no `TODO`, no new `//` (re-grepped `+ //` → zero post-fix). No CRITICAL/MAJOR open. Independent
  review: not required (no High-Risk surface — no public-API/schema break, no auth/crypto/billing/DB,
  no new dep; schema change is a flag flip, not a format break).
- Write-set note (declared, not hidden): `tests/test_overlay_settings_ui.cpp` is listed as
  “HeatLevel alignment only”, but MAJOR1’s `IsHotApply("llm_provider")` assertion and the FINAL-01
  restart-banner check live in that same file. Flipping that one assertion (plus the two banner/Trust
  lexical guards in the adjacent MainWindow scan) is part of MAJOR1’s definition — leaving it would
  leave `ctest` RED. No other file was touched; `DictationEngine.cpp`, `AGENTS.md`, `scripts/**`,
  `CMakeLists.txt`, `src/main.cpp` are untouched.

## 4. Verification evidence (verbatim)

Commands run on branch `feature/studio-pages` after all code/test/docs edits:

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` → `Configuring done`, `Generating done`,
  `Build files have been written to: .../build`, exit `0` (ONNX absent expected; deprecation
  warnings only).
- `cmake --build build --config Release --parallel 4 --target lwf_tests` → `Built target lwf_tests`,
  exit `0`.
- `.\build\lwf_tests.exe --test-case="*studio portable helpers classify heat and clamp and space caps*,*new A4 schema keys default and clamp through the store*,*main window keyboard nav and commit-on-deactivate wiring*,*CalendarLevel*"` →
  `test cases: 5 | 5 passed | 0 failed | 513 skipped / assertions: 519 | 519 passed | Status: SUCCESS!`, exit `0`.
- `ctest --test-dir build -R lwf-tests --output-on-failure` → `1/1 Test #1: lwf-tests Passed 19.97 sec`,
  `100% tests passed out of 1`, exit `0`.
- `bash scripts/check_hygiene.sh` → `hygiene: OK`, exit `0`.
- `bash scripts/check_no_network.sh` → `no-network: OK`, exit `0`.
- NOT-RUN: full unfiltered `ctest` (brief scopes to `lwf-tests`), Windows manual checkpoints
  I1/D1/S1/T1-T6/S1n-S5n/P1, G3 zig (no zig), live authenticated OpenRouter call (no key).

## 5. Unverified assumptions & remaining risks

- Assumes MinGW MSYS2 build represents MSVC CI (A-001 carried); G3 stays pending.
- Trust/Cleanup copy verified lexically + copy-lint; pixel layout on Windows still owed (F-6).
- `openrouter_model`/key have no GUI editor (readout only) — documented, restart now explicit (F-7).
- AGENTS endpoint prose + F-1/F-2/F-3/F-4 + DECISION-1 confirmation + font bundle (R-01 pixel) remain
  tracked follow-ups in STUDIO_PLAN §15; none is hidden as DONE.

## 6. Git state

- Branch `feature/studio-pages`. Code+docs commit `a23efb5` (7 files); report commit follows with
  trailers (code commit trailer omission disclosed: created with title-only `-m` before this
  section existed; `--amend` is forbidden, so trailers land on the report commit).
- COMMS.md intentionally left uncommitted (ORCH-owned per Standing Orders 5); other
  untracked `docs/agent-ops` briefs/reports under `reports/final/`, `reports/w4-review/`,
  `reports/w5-review/` left for the orchestrator (explicit-path discipline).
