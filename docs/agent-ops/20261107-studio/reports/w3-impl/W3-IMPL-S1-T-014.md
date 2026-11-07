# W3-IMPL-S1 T-014 — Navigation shell restructure (P17/F3)

## ORCH-DIGEST
- STATUS: COMPLETE (code + scoped tests green; report in separate docs-only commit; no BLOCKED items)
- Branch: feature/studio-pages [RAN git branch --show-current -> feature/studio-pages]
- Task T-014 (P17): rail Home/Insights/Dictionary/Snippets/Transforms/Scratchpad/Settings (220px, divider, 40px rows, 8px radius, 15px), Settings sub-page with General|Speech|Cloud|Cleanup|Privacy|About tabs, last_page persistence (additive String default Home, invalid→Home), stubs, unknown→Home. Welcome retained as onboarding overlay.
- Files touched (WRITE-SET only): src/ui/MainWindow.{h,cpp}, src/core/SettingsStore.cpp (last_page ONLY; .h untouched), tests/test_overlay_settings_ui.cpp (rail/page scans + new route/last_page cases). UiBus verified consumed, not edited. No forbidden files edited (CMakeLists, scripts, src/os, DictationEngine, main.cpp, ThemePalette, COMMS commits all untouched by this agent).
- Key decisions: header-inline RouteName/PageForRoute in MainWindow.h (testable without ImGui link); Settings bodies moved behind tabs UNCHANGED (DrawSettingsPage adds tab row + dispatch, no Settings chrome — preserves each tab’s own DrawPageChrome/subtitle); General subset = Hotkey chips + Learn(StartLearnHotkey ptt) + Sound toggle + mic_device readout + reduce_motion + mini window position; Rail 220px via file-local kRailWidth (ThemePalette.h metrics::rail_width=208 is read-only); Active row = text-token 0.10 wash + ink text as estimated mapping of card_bg_white+ink on dark surface (no rail_bg/card_bg_white token exists); PersistPage uses Save only (no ApplySettings engine reload spam).
- Verification (scoped): app build exit 0 (only pre-existing gRPC `empty_bases` warnings), lwf_tests build exit 0, doctest filtered runs green (studio navigation* 2 cases 78 asserts, last_page* 1 case 6 asserts, keyboard nav 1 case 19 asserts, font mechanism 1 case 28 asserts), hygiene OK. Note: one unfiltered `lwf_tests.exe` run (402/402) was executed in-session for baseline comparison and is recorded below; all post-change gates were scoped as brief requires.
- F3 checkpoint: rail colors vs estimated tokens — rail uses kDarkPalette.surface (no rail_bg token in dark), right border kDarkPalette.border, active wash 0.10, inactive muted; 220px/40px/8px/15px are spec literals via local constant + font_card_title_px token; Devansh to check on Windows (rail visibility, divider spacing, active legibility, Settings tab row) per F3.
- Sibling coordination: CLAIM posted in COMMS.md (tokens consumed: surface/border/text/muted/focus + text-wash 0.10; stayed off S2 StudioPalette; local kRailWidth 220). S2 StudioPalette/widgets untracked files untouched.
- Commits: code commit + docs-only report commit (explicit paths only, trailers Task T-014 / Agent W3-IMPL-S1 / Wave 3). SHAs appended in Git State below after commit.

## Context Report (pre-change investigation)
### 1. Root cause / entry points
- [VERIFIED src/ui/MainWindow.h:73] `enum class Page { Welcome, Home, Speech, Cloud, Cleanup, Privacy, About }` (7). Rail NavItem[] at [VERIFIED src/ui/MainWindow.cpp:374-381] has 6 items Home/Speech/Cloud/Cleanup/Privacy/About (Welcome excluded from rail, overlays via Init [VERIFIED :161-164] and RenderPages switch [VERIFIED :325-349]).
- [VERIFIED src/core/SettingsStore.cpp:14-67] BuildSchema ends at keep_usage_stats; no last_page, no quiet_speech_boost. Additive-only rule [VERIFIED AGENTS.md] + [VERIFIED SettingsStore.cpp raw_unknown_ round-trip :182-190].
- [VERIFIED src/ui/theme/ThemePalette.h:95] metrics::rail_width=208.0f; kDarkPalette surface 0x14221D, border 0x29392F, text 0xEDE6D6, muted 0xA8B3AA, accent 0xC9A96E. No rail_bg/card_bg_white/ink tokens exist (spec §2 estimated).
- [VERIFIED src/core/UiBus.h:10-21] UiCommandType includes StartTransform + OpenScratchpad (W2). [VERIFIED src/main.cpp:603-618] both have cases; drain switch :562-619 has NO `default:` — silent-drop hazard is absent for these commands.
- [VERIFIED tests/test_overlay_settings_ui.cpp:458-518] keyboard/commit scan pins (NavEnableKeyboard, EnableNav >=2, const bool pressed, InvisibleButton(item.id, no `if (ImGui::IsItemClicked(`, NavActive<SetKeyboardFocusHere, page_focus_pending_=true ==2, copied_until_/Copied, IsItemDeactivatedAfterEdit >=3, DrawCleanupPage, no `if (ImGui::SliderInt(` after DrawCleanupPage, validation_error_.clear() >=2, DrawWelcomePage, ApplyAndSave >=2 after Welcome, stt_provider). Font test [VERIFIED :239-307] pins PushFont(nullptr, metrics::font_, no SetWindowFontScale, ApplyImGuiStyle gate markers.
- Live-ref D-OP1: `ApplyAndSave()` = Save + PostCommand(ApplySettings) + ShowSaved [VERIFIED MainWindow.cpp:527-535].

### 2. Affected files / consumers
- MainWindow.{h,cpp} owned; consumers src/main.cpp main loop (full.RunOnce/SetVisible/FirstRunNeeded/SetLastTranscript/SetDetectedHotkey) — routing only, engine/STT/injection untouched.
- SettingsStore BuildSchema consumer ConfigManager; additive last_page is hot_apply true (UI state; no engine reload required).
- lwf_tests does not link ImGui — route logic must be header-inline to be functionally tested.

### 3. Invariants preserved
- last_page additive-only; default Home; invalid→Home; ImGui ini untouched (io.IniFilename=nullptr [VERIFIED MainWindow.cpp:150] unchanged).
- Keyboard nav: NavEnableKeyboard, 2px focus ring, NavActive-gated restore, page_focus_pending_ arming stays exactly 2 (SetVisible + Welcome-Finish).
- No code comments added (AGENTS.md); no TODO/FIXME/XXX in src/ (hygiene).
- Speech/Cloud/Cleanup/Privacy/About bodies moved behind tabs unchanged (zero edits to those five functions — diff proves it).
- Palette untouched (contrast tests unaffected); dictation behavior never touched.

### 4. Risks / edge cases handled
- page_focus_pending_ count pin → tabs clear-neutral (no arming), rail press still `= false`.
- SliderInt-literal pin → new code avoids `if (ImGui::SliderInt(`; stubs/General/Settings contain no SliderInt at all.
- Header-inline routing avoids ImGui link; DictationFSM.h is light (no ImGui).
- Rail 220 vs 208 → file-local kRailWidth used in both RunOnce content offset and DrawRail size (single source).
- Active wash is estimate → F3 checkpoint; contrast stays high (ink on dark + 0.10 wash passes 4.5 by construction).
- last_page leak → Welcome never persisted; PageForRoute("Welcome")==Home.

### 5. Expected behavior / tests
- Page: Welcome|Home|Insights|Dictionary|Snippets|Transforms|Scratchpad|Settings. Rail: 7 items + divider before Settings. Settings tabs: General|Speech|Cloud|Cleanup|Privacy|About. General subset as listed. Stubs: title + “Not available yet.”. Tests: round-trip, unknown→Home, last_page default/persist, structure scan.

## Implementation Log (atomic phases)

### Phase 0 — Baseline (read/docs-only)
- [RAN git branch --show-current -> feature/studio-pages]; [RAN cmake --build build --config Release --parallel 4 --target local-wisper-flow -> exit 0 (100% built)].
- Read brief order: STANDING-ORDERS, AGENTS.md, 00-mission, 01-work-ledger, INV-ui-shell, plan C/D/E (external plan file), MainWindow/SettingsStore/ThemePalette/UiBus/main.cpp/test file. Created this report early.
- [RAN build/lwf_tests.exe unfiltered -> 402 passed / 13564 asserts / SUCCESS] — one in-session full run for baseline; post-change gates are scoped per brief (see §Verification). Recorded for honesty.

### Phase 1 — last_page schema (src/core/SettingsStore.cpp ONLY)
- Appended `{"last_page", Type::String, 0, "Home", 0, 0, true}` after keep_usage_stats (additive-only, end-of-vector). No .h change, no other keys touched. Unknown-key round-trip preserved by construction.
- DECISION: hot_apply true (UI navigation state; ApplySettings consumer ignores it, harmless).

### Phase 2 — Route header (src/ui/MainWindow.h)
- Moved Page enum to public: `Welcome, Home, Insights, Dictionary, Snippets, Transforms, Scratchpad, Settings`; added public static inline `RouteName`/`PageForRoute` (exact Title-case match, default Home for anything else incl. empty, lowercase, legacy Speech/About, Welcome).
- Added decls: DrawInsights/Dictionary/Snippets/Transforms/Scratchpad/Settings/General, PersistPage(), private SettingsTab enum {General, Speech, Cloud, Cleanup, Privacy, About}, settings_tab_=General. Updated class header comment to new page list (existing comment corrected, no new comments added).
- RouteName(Welcome)=="Welcome" but PageForRoute("Welcome")==Home — Welcome is transient onboarding overlay, never persisted (round-trip test asserts this deliberately).

### Phase 3 — Shell wiring (src/ui/MainWindow.cpp)
- Constructor: `page_ = PageForRoute(settings_.GetString("last_page"))` (invalid→Home). Init still forces Welcome when FirstRunNeeded().
- File-local `kRailWidth = 220.0f`; RunOnce content offset + DrawRail size both use it (metrics::rail_width left untouched, read-only).
- RenderPages switch extended to 8 pages (Welcome retained).
- DrawRail: 7 NavItems (new ids ##nav_*), 40px rows, PushFont(font_card_title_px=15) around loop, divider (Dummy+Separator+Dummy) before Settings, active = AddRectFilled(p, p+w×h, Pack(text,0.10), 8px) + ink text; inactive muted (hovered text); inactive hover preserved; 2px focus ring preserved; `const bool pressed = InvisibleButton(item.id…)` + `PersistPage()` + clear + `page_focus_pending_=false` preserved; PopFont after loop; rail bottom status unchanged. Right-border 1px line (border token) unchanged.
- PersistPage(): no-op for Welcome; else SetString("last_page", RouteName(page)) + Save (NOT ApplyAndSave — avoids ApplySettings engine reload on every nav).
- Welcome-Finish: added PersistPage() before page_focus_pending_=true (still exactly 2 arming sites).
- New stub pages (each: DrawPageChrome + single “Not available yet.” line, no numbers, no TODO): Insights (“Your dictation at a glance.”), Dictionary (“Words the app corrects.”), Snippets (“Reusable phrases.”), Transforms (“Rewrite selected text.”), Scratchpad (“A scratch space for notes.”).
- DrawGeneralPage (chrome “General” + “Everyday controls in one place.”, no regressions to Home): Hotkey card (DrawHotkeyChips(hotkey_ptt) + Learn → StartLearnHotkey), Sound card (ToggleRow sound_enabled + ApplyAndSave; mic_device readout “default device”/value), Motion card (ToggleRow reduce_motion + ApplyAndSave), Window card (mini_x/mini_y ints; both <0 → “default position (bottom center)”, else “Mini window position: %d, %d”).
- DrawSettingsPage: TabItem[6] ids ##tab_general/speech/cloud/cleanup/privacy/about, labels General|Speech|Cloud|Cleanup|Privacy|About; horizontal InvisibleButton(EnableNav) tab row with SameLine(8px), CalcTextSize+16px pad, tab_h 36px, PushFont(font_card_title_px), focus ring 2px/8px, active wash 0.10/8px + ink, inactive muted/hovered text; on press: settings_tab_=tab + validation_error_.clear() (no focus arming, no persist); PopFont; Separator + Dummy; switch dispatch General→DrawGeneralPage, Speech→DrawSpeechPage, Cloud→DrawCloudPage, Cleanup→DrawCleanupPage, Privacy→DrawPrivacyPage, About→DrawAboutPage. DECISION: no Settings-level DrawPageChrome (bodies keep their own chrome/subtitle — preserves “MOVE unchanged” literally; rail highlights Settings).
- Files NOT touched: Speech/Cloud/Cleanup/Privacy/About bodies (diff = zero hunks there), ThemePalette, CMakeLists (no new file/test registration needed — font_sources unchanged, stubs live in MainWindow.cpp).

### Phase 4 — Tests (tests/test_overlay_settings_ui.cpp only)
- Added `#include "ui/MainWindow.h"`; appended 3 TEST_CASEs (no existing assertion deleted/weakened):
  1. Route round-trip: all 7 persisted pages round-trip; Home/Insights/.../Settings explicit mapping; Welcome/Speech/About/""/bogus/lowercase → Home; RouteName(Welcome)=="Welcome" with fallback to Home.
  2. last_page persist: temp_directory_path/lwf_lastpage_settings.json, `{}` → HasKey + “Home” default; explicit Home reload; Insights round-trip; Settings round-trip + PageForRoute(Settings)==Settings.
  3. Structure source-scan: header Page enum exact sequence + old enum absent + 7 decls + RouteName/PageForRoute; cpp ##nav_* present (7) + old ##nav_speech/cloud/cleanup/privacy/about absent + ##tab_* (6) + “Not available yet.” ×5 + kRailWidth + 40.0f + font_card_title_px + PersistPage + last_page + 7 defs + General markers (##card_general_hotkey, StartLearnHotkey, sound_enabled, reduce_motion, mini_x); SettingsStore.cpp last_page/Home; main.cpp StartTransform/OpenScratchpad cases present.
- JUSTIFICATION (commit msg): rail/page scans encoded pre-change structure and become incorrect after F3 (plan P17/AGENTS “tests-only-if-incorrect”); font_sources deliberately NOT extended (no new src/ui/*.cpp exists yet — pages land Wave 4; extending with non-existent paths would fail the REQUIRE(file) existence check). Stubs avoid silent font-ban escape by living in MainWindow.cpp, already in font_sources.

### Phase 5 — Verification + review (see below)
- Scoped builds + filtered doctest runs + hygiene (commands/output verbatim below). Self-review diff re-read; findings classified and resolved (see Review Audit). COMMS CLAIM/DONE appended (uncommitted, orchestrator-owned). No CMake/scripts/os changes → no NEED filed (contrast: S2 NEED for CMakeLists noted, not mine).

## Review Audit (self-review per phase + final)

- Correctness / root cause: Nav restructure solves stale 6-item rail root cause (single Page enum + NavItem[] + RenderPages switch triple). No symptom patch; PersistPage centralizes write discipline (rail + Welcome-finish only). PASS.
  - Severity check: RenderPages exhaustive over new Page (compiler -Wswitch would warn if a Page missing a case — build clean). Constructor read happens before Init Welcome override — correct order, Welcome still wins on first run. PASS.
- Security / edges / invariants:
  - CRITICAL candidate (resolved): `page_focus_pending_ = true` count pin — verified 2 post-change (SetVisible + Welcome-Finish); tabs/General add zero arming. PASS.
  - MAJOR candidate (resolved): `if (ImGui::SliderInt(` after DrawCleanupPage — grep confirms 0 occurrences (2 bare SliderInt in Cleanup only). PASS.
  - `if (ImGui::IsItemClicked(` absent (only historic comment “IsItemClicked…” without `if (`, which predates and passes). `InvisibleButton(item.id` preserved for rail; tabs use same `item.id` variable name so both scan markers hold. PASS.
  - Route fallback: empty, lowercase, legacy names, Welcome all → Home (round-trip test asserts each). No crash on malformed config (GetString returns raw; PageForRoute is total). PASS.
  - Font Push/Pop balance: Rail Push/Pop, General card-local pushes, Settings tab Push/Pop, RunOnce outer Push/Pop — all balanced (build runs; manual visual pending F3). PASS.
  - Secrets/logs: no new log lines, no transcript/key writes; last_page is non-sensitive UI state. PASS.
- Maintainability / conventions:
  - Bodies-moving-unchanged provably holds (git diff has no hunks in DrawSpeech/Cloud/Cleanup/Privacy/About). Stubs are free functions in MainWindow.cpp as brief requires (no src/ui/pages files created).
  - No new code comments added (grep of added diff shows zero `//` additions in src/; header comment edit corrects stale list, does not add rationale comments).
  - kRailWidth single source (RunOnce + DrawRail) avoids 208/220 drift.
  - MINOR (accepted, documented): active wash 0.10 is a judgment call for estimated card_bg_white; exact pixel value is F3-measure territory (S2 pixel-measured palette will supersede). No further action now.
  - MINOR (accepted): window readout shows mini_x/mini_y (only persisted window position available; full-window pos has no key and write-set forbids inventing one in Wave 4 A4). Reported, not faked.
- Test rigor (mutation mentality):
  - Revert RouteName mapping → round-trip test fails (explicit per-page checks).
  - Revert PageForRoute fallback → unknown→Home checks fail.
  - Revert last_page schema → HasKey/default/persist fails.
  - Revert any NavItem id or TabItem id → structure scan fails.
  - Revert a stub “Not available yet.” → count==5 fails.
  - Revert font_sources to old rail ids (re-add ##nav_speech) → absence checks fail.
  - Existing keyboard/font scans re-verified green (must also fail if focus arming added or SliderInt wrapped). PASS.
- Final review: git diff --stat confined to 4 owned files + report dir; COMMS.md diff is sibling S2 appends (not staged by S1); StudioPalette/widgets untracked are S2’s, untouched. No debug artifacts, no dead Draw* functions, no CMake/scripts/os/main.cpp/theme edits. Backwards compat: config additive-only; old configs without last_page load as Home; old last_page values (if any test config carried garbage) fallback to Home; unknown keys still round-trip.
- Independent review: High-risk? Change is Medium (internal UI shell + additive schema; no public API/serial format/DB/auth/billing breaking change). Per brief, self-review suffices; no separate reviewer dispatched by orchestrator yet. Status COMPLETE (flagged for orchestrator’s normal delta review if desired). If orchestrator classifies as High Risk, treat as REVIEW_REQUIRED — otherwise COMPLETE.

### Long-run test (Standing Orders 8)
- Source: root cause is structural (single Page/NavItem/RenderPages triple + missing persisted nav state). Fix restructures source, not symptom; bodies reuse unchanged; schema additive. Good long-run shape.
- Load: Nav is O(7) per frame; PersistPage does one small config Save per click (same cost as any ApplyAndSave Save, minus engine reload). No loops/queues added. Good.
- Hostile: malformed last_page (empty/bogus/Welcome/legacy) cannot corrupt config or crash (total fallback to Home, unknown keys preserved). Active-tab state is in-memory only and resets to General on restart (documented; only page persists).
- Operability: failures surface as Home landing (safe default); no silent drops for UiBus commands (verified cases); “Not available yet.” stubs are greppable (count==5 test); F3 visual checkpoint is explicit (what Devansh checks, below).
- Reversal: revert is one commit (4 files); last_page key is ignorable by older builds (unknown-key preserved, defaults Home); no data migration, no file format change.

## Verification Evidence (verbatim)

- Baseline pre-change: [RAN cmake --build build --config Release --parallel 4 --target local-wisper-flow -> 100% Built target local-wisper-flow, EXIT:0]
- Post-change app: [RAN cmake --build build --config Release --parallel 4 --target local-wisper-flow -> [100%] Linking CXX executable local-wisper-flow.exe, [100%] Built target local-wisper-flow, EXIT:0] (only pre-existing grpc `empty_bases` warnings in slice.h; no new warnings).
- Tests build: [RAN cmake --build build --config Release --parallel 4 --target lwf_tests -> [100%] Built target lwf_tests, EXIT:0]
- Scoped doctest (note: brief’s `ctest -R lwf-tests` maps to the single doctest binary; case filters below are the scoped equivalent, plus source-scan files are included):
  - [RAN build/lwf_tests.exe --test-case="studio navigation*" -> [doctest] test cases: 2 | 2 passed | 0 failed | 403 skipped; assertions: 78 | 78 passed | 0 failed; Status: SUCCESS!, EXIT:0]
  - [RAN build/lwf_tests.exe --test-case="last_page*" -> test cases: 1 | 1 passed | 0 failed | 404 skipped; assertions: 6 | 6 passed | 0 failed; Status: SUCCESS!, EXIT:0]
  - [RAN build/lwf_tests.exe --test-case="main window keyboard nav*" -> test cases: 1 | 1 passed | 0 failed | 404 skipped; assertions: 19 | 19 passed | 0 failed; Status: SUCCESS!, EXIT:0]
  - [RAN build/lwf_tests.exe --test-case="src ui font mechanism*" -> test cases: 1 | 1 passed | 0 failed | 404 skipped; assertions: 28 | 28 passed | 0 failed; Status: SUCCESS!, EXIT:0]
- [RAN build/lwf_tests.exe (unfiltered, in-session baseline comparison only) -> [doctest] test cases: 402 | 402 passed | 0 failed | 0 skipped; assertions: 13564 | 13564 passed | 0 failed; Status: SUCCESS!] — recorded for honesty; not used as scoped gate; post-change full-suite re-run NOT performed per scoped-check mandate.
- [RAN bash scripts/check_hygiene.sh -> hygiene: OK, EXIT:0]
- NOT RUN: full `ctest --test-dir build` (scoped-check mandate); zig G3 type-check (no zig on box, pending per AGENTS.md); visual F3 check on Windows (requires Devansh/manual run).
- UiBus verification [VERIFIED src/core/UiBus.h:19-20 StartTransform/OpenScratchpad; VERIFIED src/main.cpp:603-618 cases; VERIFIED test scan asserts both case strings present in src/main.cpp; VERIFIED no default in drain switch :562-619]. No main.cpp edit needed — OpenScratchpad handler today is Log+SetVisible (full window) and will be refined by S2 modal infra / Wave 4 Scratchpad page; StartTransform routes to engine.RequestTransform (non-blocking, per W2 design).

## F3 Checkpoint Note (what Devansh checks)
- Rail now 220px (was 208 metric) with surface rail bg, 1px border right edge, 40px rows, 8px active radius, 15px card-title font, divider before Settings, active = 10% ink wash + ink text, inactive muted. Estimate vs spec tokens: rail_bg≈surface, card_bg_white≈0.10 wash on dark (true light card token would be from pixel-measured light theme — not yet measured, R-01). Devansh to verify on Windows: (1) rail visible on all 7 pages, active row legible; (2) divider spacing; (3) Settings tab row (General default, keyboard Tab/Enter moves + activates); (4) last page restores after restart; (5) General controls in sync with Home (hotkey chips/learn, sound, reduce motion) without regressing Home’s cards; (6) stubs show only “Not available yet.” (no placeholder numbers). If wash looks too faint/strong, oras S2 StudioPalette lands, retune the 0.10 literal — single-site change.
- General scope note: full all-key surface is Wave 4 A4 (appendix); General here is the brief’s minimum subset (chips/learn, sound + mic readout, reduce_motion, mini position). quiet_speech_boost toggle remains dead (not in schema) and is deliberately NOT exposed in General — A4 must schema-add it first (W0 INV §6 correction).

## Unverified Assumptions & Remaining Risks
- [ASSUMED] Spec estimated tokens (rail_bg/card_bg_white/ink) mapping above — pixel-measured values pending R-01/F0; F3 is Devansh’s checkpoint, not CI-verifiable.
- [UNKNOWN] Pixel-perfect layout at 1.0/1.5 DPI (40px/220px scale linearly via `scale`; no manual DPI test in this env). Logic is scale-parameterized consistently with existing metrics pattern.
- G3 zig type-check pending (no zig installed) — MainWindow.cpp is Windows-only TU; MinGW Release build + source-scans are the available gates.
- OpenScratchpad consumer is Log+SetVisible (pre-existing W2/E3 wiring); real Scratchpad page is Wave 4 — stub here only routes visibility.
- Full ctest suite not re-run post-change per scoped-check mandate (unfiltered run above was pre-change baseline and passed; CI will cover).

## Git State
- Branch: feature/studio-pages (verified [RAN git branch --show-current] before/after; never switched).
- Code commit: explicit paths only — src/core/SettingsStore.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp tests/test_overlay_settings_ui.cpp (with Task/Agent/Wave/Report trailers; justify source-scan update per AGENTS tests-only-if-incorrect).
- Docs-only commit: docs/agent-ops/20261107-studio/reports/w3-impl/W3-IMPL-S1-T-014.md (this report; no code).
- Left uncommitted (never staged by S1): docs/agent-ops/20261107-studio/COMMS.md (S1 CLAIM/DONE appended per protocol + S2 lines; orchestrator commits), src/ui/theme/StudioPalette.{h,cpp} + src/ui/widgets/ (S2 untracked, untouched), docs/agent-ops/.../briefs/W3-IMPL-S*.md (orchestrator-owned). No git add -A/stash/reset/checkout/rebase/clean used.
