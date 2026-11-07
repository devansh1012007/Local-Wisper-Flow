<!-- ORCH-DIGEST:START -->
agent: W5-IMPL-P | task: T-021 F9 polish sweep | verdict: COMPLETE with provisional-contrast caveat
findings: 6 root-cause fixes + 1 test-hardening suite; muted blocker resolved with evidence; pixel F0(a) pending (no screenshots in repo); MainWindow copy uncommitted (shared with W5-IMPL-O)
verification: lwf_tests build exit 0; scoped doctest 14/12576 + 113/1581 pass; hygiene OK; no-network OK; exe link blocked by sibling O (compile proven)
<!-- ORCH-DIGEST:END -->

# W5-IMPL-P report — T-021 F9 polish sweep (P24)

## 1. Status: COMPLETE (with F0(a)-provisional contrast verdict; one shared file left uncommitted for orchestrator sequencing)

## 2. Root cause & context summary

F9 is the honest final pass over Waves 3-4's widgets + pages. Context report (pre-change):

1. [VERIFIED src/ui/theme/StudioPalette.h:24-38] Tokens: page_bg FAFAF8, rail/card F4F2EE, white FFFFFF, border E7E4DE, ink 1C1C1C, muted 6E6A62 (darkened by W3-IMPR-01 from 7A776F), teal_700 1E5E5A, teal_500 2F8077, teal_300 7DBFB7, teal_100 CDEBE7, heat_empty CDC6BB, banner_cream F1ECE4.
2. [VERIFIED src/ui/widgets/Overlays.cpp:42-44,91-93 pre-change] Modal/Sheet Esc path: `Begin` sets `visible_=visible`, then `Close()` sets `visible_=false`; `End()` early-returns, skipping `EndPopup()` even though `BeginPopupModal` returned true. Same defect class reachable via page paths that call `Close()` inside Begin before `End()` (TransformsPage reset/diff/how at 576/581/653/674-676). All 8 call sites verified to pair `Begin` then `End` in the same frame (Dictionary 340/381, Insights 362/401, Snippets 232/295, Transforms editor 452/540, reset 554/584, diff 589/655, how 660/676).
3. [VERIFIED ContrastRatio matrix, RAN py script] Every used text pair passes 4.5:1 EXCEPT teal_500-as-text (TransformsPage TextLink hover): 4.49 on page, 4.20 on card. No screenshots exist in repo (`docs/screenshots/` empty), so all verdicts are token-math, never pixel-measured.
4. [VERIFIED src/core/InsightsModel.cpp:190-199 vs StudioPalette.h:75-90] `CalendarLevel` (tested model of record, zero production callers) uses `>=`-style quartile bands (`quarter = day*4/max`); `HeatLevel` (live, drives ActivityGrid) used strict `>` — two classifiers for the same statistic diverging at exact 25/50/75% edges.
5. [VERIFIED src/ui/widgets/Display.cpp + InsightsPage.cpp] Two byte-identical heat-color switches (HeatColor/HeatPack) — the duplication that let (4) drift.
6. [VERIFIED src/ui/widgets/Buttons.cpp:131-148] Studio ToggleSwitch: 28px height vs `studio_metrics::hit_size` token 32; off-track `border` at 1.27:1 vs white (meaningful control boundary below the 3:1 policy; the only existing token passing 3:1 on light fills is `muted`).
7. Copy scan [RAN copyscan over all src/ui literals]: 0 `Oops`, 0 `!` in strings; banned display terms in 5 MainWindow strings only (Trust 958-959, Cloud 1140/1158-1160, Consent 1195-1199, About 1374-1375). All other hits are settings keys, `##ids`, filenames (`whisper_model_path`, `stt_provider`, `config.json`), or intentionally-kept user terms (Cleanup = product feature name, Hotkey, API key).
8. Keyboard: rail focus already NavActive-gated (MainWindow.cpp:481); every `SetKeyboardFocusHere` site is user-initiated (Ctrl+F, button activation, dialog open, note open) — correct ungated. Real gaps: TransformsPage TextLink had EnableNav but no focus ring; Toast action button had no focus ring.

Invariants preserved: kDarkPalette/kLightPalette and mini badge untouched (frozen); `ApplyStudioStyle` remains caller-less opt-in; schema/config untouched; O's OpenRouter disclosure sentences (MainWindow 1295/1332/1378 + Privacy egress 1334) untouched except burning the internal-word prefix of the About sentence (O's egress clause kept verbatim).

## 3. Phased changes executed

- Phase 1 — Overlays.cpp: removed `visible_ = false;` from `Modal::Close()` and `Sheet::Close()`. INVARIANT (now code-enforced): `visible_` tracks exactly "Begin() returned true this frame and End() has not yet run", so `End()` always pairs `EndPopup()` after any Close() in between (Esc + Close-then-End paths). Toast: added focus ring on the action button (teal_300, 2px, pill radius, IsItemFocused-gated), mirroring StyleButton.
- Phase 2 — Buttons.cpp ToggleSwitch: height 28px -> `studio_metrics::hit_size` (32); off-track `border` -> `muted` (DECISION: muted is the only existing token >= 3:1 vs light fills; tightening the off-track to ink would erase on/off distinction, and inventing a new mid-gray without F0(a) pixels = design invention). On stays ink; ink-vs-muted = 3.17, distinct.
- Phase 3 — StudioPalette.h: HeatLevel `>` -> `>=` (DECISION: align widget to CalendarLevel, the tested model of record; F4 question resolved by consistency, not screenshots). Unified heat colors: added single `HeatColor(int)` beside HeatLevel; deleted the two duplicate switches (Display.cpp anonymous, InsightsPage HeatPack); both call sites use `lwf::ui::HeatColor`.
- Phase 4 — TransformsPage.cpp: deleted local `kCardRadius` (14) in favor of `studio_metrics::radius_card` (12, used by the other 3 cards); TextLink hover text teal_500 -> ink (4.20 -> 15.24 on card) with the existing teal_500 underline kept as the hover cue (>= 3.0 shape); added focus ring to TextLink (was nav-focusable but invisible).
- Phase 5 — MainWindow.cpp copy only (5 strings): Trust fallback "falls back to local Whisper" -> "still dictated on this computer"; Cloud label "Provider" -> "Service"; Cloud rule "Local Whisper keeps..." -> "Dictation runs on this machine by default. A cloud service sends recordings to its own servers for transcription."; Consent "provider API key"/"local Whisper" -> "service API key"/"still dictated on this computer"; About "Whisper STT, optional local LLM cleanup" -> "On-device speech recognition and optional text cleanup". Egress meaning byte-preserved; O's added clauses untouched.
- Phase 6 — docs/STUDIO_CHECKLIST.md (new): every §8 checkpoint marked PASS (automatable+proven) / MANUAL (Devansh on Windows) / PROVISIONAL (F0(a) pending), with gate status.
- Phase 7 — tests/test_overlay_settings_ui.cpp: HeatLevel edge pins updated 25->2, 50->3, 75->4 (justified: old pins encoded the disproven strict-> semantics; impossible to satisfy both HeatLevel and CalendarLevel at all three edges); +HeatColor 0/1/2/3/4/9 pins; +7 used-pair contrast checks (Toast teal_300/ink, white/ink, focus teal_700/white, underline teal_500/page, toggle muted/page + ink/muted); new `F9 widget sweep pins toggle, link, and popup invariants` (hit_size height, muted off-track exact string, Toast ring, `visible_ = false` count == 4, kCardRadius gone, TextLink ink+ring, HeatColor unified); new `F9 copy lint...` (recursive src/ui literal scan; `##id` stripped; whole-word matching so keys like `whisper_model_path`/`stt_provider` never false-positive; `!` banned).

## 4. Review audit

Self-review per phase + final re-read of full diff (all files mine verified line-by-line):

- CRITICAL — none. End/EndPopup pairing proven by call-site matrix (§2.2); no stale-End path exists today (every caller pairs Begin+End same frame when open; early-returns skip End).
- MAJOR — Toggle +4px height growth in Transforms opt-in row and Scratchpad flow-bar row: both rows are vertically fluid (no fixed clip), rows stay within 52px rhythm. Verified at TransformsPage.cpp:197 and ScratchpadPage.cpp:83 contexts (SameLine layouts, no pixel-fixed heights).
- MAJOR — HeatLevel edge change moves exact-quartile days one level up (25/50/75% of month-max). Frequency: only days at exact quartiles of the month max. Accepted: wrongness of two disagreeing classifiers dominates; F0(a) can re-arbitrate and the pins make any re-flip explicit.
- MINOR — Toast ring uses 5 lines of the same rect math as Buttons.cpp DrawFocusRing instead of a shared helper: the helper lives in an anonymous namespace and Widgets.h is imgui-free by design (adding imgui would infect page headers); inline ring with pill radius is the correct call. Drawn from the same draw list already held in Toast::Draw.
- MINOR — T-copy scans every literal in src/ui (not only display calls) with whole-word matching: display coverage is a superset of display-only scanning; no false positives observed (12576 asserts pass).
- Final: mutation mental check — reverting any single fix fails a specific new pin (HeatLevel edges x3, muted off-track, hit_size height, visible_ count, kCardRadius absence, TextLink string, any banned word / `!`). No debug artifacts, no `//` additions (hygiene green), no new warnings in my TUs (grpc `empty_bases` warnings pre-existing).
- Independent review: not obtained (W5 parallel window). If ORCH classifies HeatLevel semantics as High Risk, status is REVIEW_REQUIRED for the one-function flip line.

## 5. Long-run test (standing order 8)

Source: HeatColor/HeatLevel now single-sourced in StudioPalette.h (three implementations -> two); muted is the single secondary-text token; Close() no longer owns frame state. Load: O(cells) grid unchanged; Toast ring adds one rect when focused. Hostile: nullptr ids/labels already guarded (W3); HeatColor(9) default -> heat_empty covered. Operability: Esc now closes modal/sheet via the imgui path with EndPopup always called; every interactive widget draws its own focus affordance; copy is greppable via T-copy. Reversal: token + widget internals + test appends; muted hex revert is one token; `visible_` guard removal affects open/close only.

## 6. Verification evidence (verbatim)

- [RAN cmake --build build --config Release --parallel 4 --target lwf_tests -> Built target lwf_tests, exit 0]
- [RAN lwf_tests.exe --test-case="*studio*,*F9*,*palette*,*contrast*,*font mechanism*" -> 14 passed / 12576 passed asserts, SUCCESS]
- [RAN lwf_tests.exe --test-case="*ransform*,*nsight*,*nippet*,*ictionar*,*cratchpad*" -> 113 passed / 1581 passed asserts, SUCCESS]
- [RAN bash scripts/check_hygiene.sh -> hygiene: OK; bash scripts/check_no_network.sh -> no-network: OK]
- [RAN cmake --build build ... --target local-wisper-flow -> my 4 TUs (StudioPalette/Buttons/Display/Overlays) + pages all COMPILE; LINK FAILS on undefined OpenRouterProvider symbols referenced from main.cpp (sibling W5-IMPL-O in-flight wiring, NOT my files) — posted FINDING; NOT-RUN for link/exe]
- NOT-RUN: full 23/23 ctest (scoped mandate); G3 zig (no zig on box, pending on Windows); Windows pixel/DPI/manual checkpoints (no window runtime).

Solution validation before submit (runs used as evidence): build exit 0 (twice), doctest SUCCESS (twice), hygiene OK, no-network OK, link failure attributed to sibling with symbol evidence.

## 7. Unverified assumptions & remaining risks

- [ASSUMED] Pixel-measured F0(a) palette will land within tolerance of the token table; contrast verdict is therefore PROVISIONAL for pixel fidelity though the WCAG math on the committed tokens is proven (never claimed measured). Devansh action: save the 5 screenshots to docs/screenshots/source/ and re-run T-contrast.
- [ASSUMED] Heat ramp below 3:1 (teal_300 2.01, teal_100 1.21, heat_empty 1.62 vs page) is a Wave-1 estimate to be arbitrated at F0(a); no ramp change made here without screenshots.
- [UNKNOWN] Toggle off=mute-gray affordance vs on=ink on real hardware; manual item.
- Carried risks (NOT mine, NOT fixed, reported here): R-W4-001 Snippets unconstructed in main.cpp; R-W4-002 delete-all under-deletes/resurrects (checklist P1); R-W4-003 dictionary delete stale-enforce; R-W4-004 usage_count dead; R-W4-005/006 hotkey field staleness/validation; R-W3-009 metrics::rail_width 208 vs 220 (ThemePalette, forbidden); CalendarLevel zero callers (src/core, forbidden) — recommend ORCH decide whether the model or the widget copy is canonical long-term.
- MainWindow.cpp copy hunks are UNCOMMITTED: the file also carries W5-IMPL-O egress hunks (Privacy ~1334, About ~1374 egress clause) that I must not sweep into my commit; exact hunks in §3 Phase 5. Orchestrator to sequence (e.g. partial-stage by hunk).

## 8. Git state

Branch feature/studio-pages (verified before/after; never switched). Code commit: explicit paths `src/ui/theme/StudioPalette.h src/ui/widgets/Buttons.cpp src/ui/widgets/Display.cpp src/ui/widgets/Overlays.cpp src/ui/pages/InsightsPage.cpp src/ui/pages/TransformsPage.cpp tests/test_overlay_settings_ui.cpp` (MainWindow.cpp deliberately excluded — see §7). Docs commit: this report + docs/STUDIO_CHECKLIST.md. Left uncommitted (never staged by me): src/ui/MainWindow.cpp (mine 5 copy hunks + O egress hunks), COMMS.md (protocol), O's files (SettingsStore.cpp, main.cpp, check_no_network.sh, OpenRouterProvider.*).
