# SUMMARY — 20261107-studio
Branch: feature/studio-pages · Base: 0e655ec (1d95fb2 + docs cherry-pick) · Head: 73d9ae0 · Waves: W0 + W1-W5 (~30 agents) · Dates: 2026-11-07 (single session; satellites aside)

## 1. Outcome in brief
Five studio pages (Insights, Dictionary, Snippets, Transforms, Scratchpad), the full settings GUI (all 54 schema keys controlled or carved-out, model-path pickers, all-hotkey recorders, api_keys.json store→env), and the opt-in OpenRouter LLM backend shipped end-to-end (backend-first) and integrated behind `llm_provider` defaulting to `local`. Final gate GREEN (23/23 suites). All reviewer BLOCKERs fixed; remaining risk is Windows-hardware/G3/manual-checklist territory plus F0(a) pixel measurement, none claimed as verified.

## 2. Plan coverage
| Plan item | Task | Status |
|---|---|---|
| P01 F0 docs (STUDIO_PLAN + AGENTS D3/D2) | T-001 | DONE (e9752fc/0b2fe28) — F0(a) palette still provisional (R-01: Scratchpad image missing) |
| P02 AtomicFile + gate | T-002 | DONE (a7dbd93; delta PASS) |
| P03 TextDiff | T-003 | DONE (tie-break fix 5aac095) |
| P04 InsightsModel | T-004 | DONE |
| P05 StatsStore | T-005 | DONE (7f4975a) |
| P06 DictionaryStore v2 + P13 count | T-006 | DONE (2a2cd70; self-ref BLOCKER fixed W1-IMPR-01) |
| P07 SnippetStore v2 | T-007 | DONE (530555f) |
| P08+P10 TransformStore + v1→v2 | T-008 | DONE (Transform cap/file-close fix e7b2014) |
| P09 NotesStore | T-009 | DONE (894675a) |
| P11 app_profiles categories | T-010 | DONE |
| P12-P14 stats wiring at DictationEngine.cpp:475 | T-011 | DONE (ea22607; exclusions structural) |
| P15 transform run flow | T-012 | DONE (371a809; timeout/cancel + bounds by W2-IMPR-01) |
| P16 scratchpad target + A1 ApiKeyStore | T-013,T-022 | DONE (ff1c32d; MainWindow sync) |
| P17 nav shell + last_page | T-014 | DONE (a4ab50a) |
| P18 StudioPalette + widgets + Toast/Sheet/Modal | T-015 | DONE (860b698; muted provisional) |
| P19 Dictionary page | T-016 | DONE (67c955b) |
| P20 Snippets page | T-017 | DONE (095ef47 recovered + c9df9df integration) |
| P21 Insights page | T-018 | DONE (96fb22c) |
| P22 Transforms page (+diff viewer) | T-019 | DONE (fc96356) |
| P23 Scratchpad page + editor + Overlay button | T-020 | DONE (acfaf21) |
| P24 / F9 polish | T-021 | DONE (69c8358) |
| A2 FileDialogs + pickers | T-023 | DONE (acc7c5c) |
| A3 hotkey recorders (5 + studio) | T-024 | DONE (a2cbf64) |
| A4 all-key settings | T-025 | DONE (5a2a618) |
| C1-C2 OpenRouter + gate | T-026 | DONE (86605b7 + 405280d integration; error-mapping MAJORs fixed 8bae0c8) |

## 3. Changes by domain
- Core/backend: 8 new stores/helpers (AtomicFile, TextDiff, InsightsModel, StatsStore, DictionaryStore, TransformStore, NotesStore, ApiKeyStore) + SnippetStore v2 + PersonalDictionary count + TextPipeline::ProcessResult + AppContextResolver categories + OpenRouter LLM provider/contract + engine transform mode + stats hook at :475.
- UI: new rail + 6 Settings tabs (General), 5 pages (each own file), ScratchpadWindow (own GL/ImGui context), 14-widget kit, modal/sheet/toast, FileDialogs, all-hotkey recorders, all-54-key cards, Privacy delete-all.
- Docs: docs/STUDIO_PLAN.md (F0), docs/STUDIO_CHECKLIST.md (F9), docs/PROGRESS.md + docs/QUESTIONS.md/FAQ updated (FINAL-FIX-01), AGENTS.md D3/D2 amendments.

## 4. Deep-Dive Spotlight
- **Stats hook at :475.** Dead RunEngine at main.cpp:193 has zero callers (red-team BLOCKER); live success = DictationEngine.cpp:475, exclusions structural (command :458, cancel/empty :463-472, PasteLast :527 separate). DONE.
- **Transform run flow on the worker thread.** NEVER in the UiBus drain (would freeze UI for the LLM call). WindowsSelectionCapture is the LIVE clipboard technique (WindowsClipboardInjector is dead) — 8000ms timeout/cancel honored. DONE.
- **Dictionary migration risk (first-ever Save).** .bak + atomic + 4-shape tests; Enforce self-ref infinite loop caught by review, fixed single-pass. DONE.
- **OpenRouter consent (DECISION-1).** Uses its own consent gate (llm consent, default off) rather than reusing stt_consent_accepted; Privacy+About state both networked opt-ins. Needs Devansh's confirm at merge time (Phase 5 summary).

## 5. Verification evidence
Final gate (gate-final.md): configure 0, build 0, ctest 23/23 (1178.41s), hygiene OK, no_network OK, HEAD 73d9ae0. Every wave's gate is in gates/. Baseline: CI 42310536369 (both jobs success) at 1d95fb2; local MinGW build green at every wave close. G3 zig NOT RUN (no zig on box; § "type-checked", never "works"). TSAN NOT RUN.

## 6. Review outcomes
- W1: CHANGES-REQUESTED (BLOCKER Enforce loop) → W1-IMPR-01 fixed all → delta PASS.
- W2: CHANGES-REQUESTED (CMake P0 + Start never called) → W2-IMPR-01 fixed → delta PASS.
- W3: APPROVE-WITH-CHANGES (muted provisional, Toast contracts) → W3-IMPR-01 partial → delta (build/ctest green).
- W4: CHANGES-REQUIRED (SnippetsPage not constructed) → W4-IMPR-01 fixed → delta APPROVE-WITH-CHANGES (residual TR-MXD-001 RESEED now fixed f66943b).
- W5: APPROVE-WITH-CHANGES (cancel/timeout, egress, error mapping) → W5-IMPR-01 fixed → delta PASS.
- Final: MERGE-WITH-CHANGES 0 BLOCKER / 3 MAJOR (llm_provider restart flag, Trust card, PROGRESS/STUDIO_PLAN staleness + 1 comment) → FINAL-FIX-01 (a23efb5) all fixed except AGENTS.md endpoint prose (N-1 outside write-set, carried) + F-2 detached worker (carried, outside write-set).

## 7. Deferred / open items (ticket-ready)
1. **F0(a) pixel-measured palette** — Scratchpad screenshot missing; tokens visual-estimated; muted #7A776F fails 4.5 on light fills (provisional until F0(a); Devansh action: provide 5 source images in docs/screenshots/source/).
2. **Manual Windows checklists** (Devansh): I1/I2/I3, D1/D2, S1/S2, T1-T6, S1n-S5n, P1 (docs/STUDIO_CHECKLIST.md) + R2 hardware checks (see docs/MANUAL_TEST_WINDOWS.md).
3. **Live API verification** — OpenRouter (real key + cloud STT Gnani/ElevenLabs/Muse) never run live; contracts verified from docs only.
4. **AGENTS.md endpoint prose** — FINAL-FIX-01's outside-write-set note (stt/cloud endpoint list update for openrouter.ai is in check_no_network.sh but the AGENTS.md prose mapping is unchanged).
5. **F-2 detached transform worker** — engine worker lifetime edge (FINAL-01 §4); carried.
6. **HeatLevel test parity** (StudioPalette >= diverges CalendarLevel by 1) — NEED from W5 (owner filed).
7. **Wave-1 NEED dict_ Reload** essentially resolved via W4-FIX-RESEED tombstone; MAIN residual above.

## 8. Out-of-plan discoveries
- (same as investigations): usage_count has no producer (chips empty until stats plumbing); 3 unscanned headers (font_sources); Home Home: hotkey learn BLANK (A3 NEED cleared? verify at final demo); pre-branch binary can't read v2-migration files (mitigated by .bak); IMPR-01 "Engine-freeze" guess corrected (actually PlayLinkWasASuccess analog — stale grep hits).

## 9. Temporary mitigations
None. No stopgap shipped.

## 10. Risks, rollout and operational notes
- **Migrations:** dictionary.json/snippets.json/transforms.json v1→v2 with .bak reverse path; config.json additive (last_page, llm_provider, keep_usage_stats, openrouter_model, quiet_speech_boost registered); api_keys.json separate + redacted + never config.json.
- **Config/env:** OPENROUTER_API_KEY (store→env via ApiKeyStore); model name `openrouter_model` additive key; `llm_provider=local` default (create copy of OpenRouter backend done but unreachable until opt-in — local-first preserved; Privacy+About state both networked opt-ins).
- **Rollback:** revert any wave commit — atomic, no schema execution to reverse (.bak restore files).
- **Merge consociation:** merge to main only after manual checklists (not automated); push will go to feature/studio-pages only.

## 11. Assumptions made (A-nnn) and decisions taken with the user
- A-001: local MinGW MSYS2 build is representative; MSVC compile errors surface on full CI. (Still holds — CI is the MSVC truth.)
- D1-D15 + D-OP1..4 (see plan §§G,M). D3/D2 amendment text committed live (W1-IMPL-01).
- DECISION-1 (OpenRouter consent): needs Devansh's explicit confirm (Phase 5 summary said it — now open at merge).

## 12. How to review this branch (commit list by task; order to read)
79 commits (0e655ec..73d9ae0). Read in this order: Waves 1-3 (backend + shell + widgets first), then the engine hooks, then pages + settings, then OpenRouter. Gate records in docs/agent-ops/20261107-studio/gates/.

## 13. Agent roster
See docs/agent-ops/20261107-studio/reports/ (W0 INV x4, W1 IMPL x6 + INT, W2 IMPL x3, W3 IMPL x2+SCAN/INT, W4 IMPL D/S/T/N/A2/I/A4/A3 + INT + INT2, W5 IMPL O/P + INT, finals). Ledger: docs/agent-ops/20261107-studio/01-work-ledger.md.

## 14. Residual risk and unverified claims (be candid)
- **Contrast** is provisional (muted fails 4.5 until F0(a)); **G3 zig** never run; **Windows runtime** (dictation, overlay, tray, open-router egress) never executed on hardware by the team (manual checklists outstanding); **live API keys** never used; **TSAN** never run; move-semantics destruction (IMPR-01 MAJOR) was about tests not the Clang-Tidy toolchain; **checklists I1/T1/S3n etc.** remain (they're for Devansh on a Windows machine). Nothing is claimed verified that isn't.
