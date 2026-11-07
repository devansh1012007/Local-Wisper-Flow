<!-- ORCH-DIGEST:START -->
agent: W0-INV-04 | domain: history/intent + mockup verification | status: DONE
items: 7 verified EXISTS / ALREADY-FIXED / NOT-REPRODUCIBLE / PARTIAL / MISCLASSIFIED
proposed_tasks: 3 (SURFACE 2, DEEP 1)
top_risks: P-001..P-004 must not contradict mockup §3/§5; rail 220 vs 208 drift frozen by tests; dual-palette split unmeasured without screenshots
cross_domain_conflicts: none
out_of_plan_discoveries: 4
confidence: high (git + source + mockup text directly observed)
last_comms_line_read: [2026-11-08T04:05:00Z] ORCH -> ALL | DECISION | WAVE-START
<!-- ORCH-DIGEST:END -->

# Investigation report — W0-INV-04 — history/intent + mockup verification

## 0. Scope and method

Question: does the code match docs/design/mockup.html intent for density/hierarchy, and why do kRailWidth=220, dual palette, and the reverted lightened palette exist — so P-001..P-004 do not contradict mockup.

Scope: read-only. Brief §2 required reading in order: STANDING-ORDERS [VERIFIED templates/STANDING-ORDERS.md:1-45], AGENTS.md, 00-mission, 01-work-ledger, report-investigation, COMMS, mockup.html §3,§5, UI_AUDIT.md, PROGRESS.md (R1/Studio/GUI wave), STUDIO_PLAN.md P17-P24, MANUAL_TEST_WINDOWS.md §R, git log for src/ui/theme + MainWindow. Env: read/git log only, no builds [VERIFIED brief §7].

Missing input: brief names "docs/design/mockup.html sections 3,5" — mockup has §3 Full Home, §4 Full Speech, §5 Type/spacing/radii/controls/motion. I read §3 and §5 fully (plus §4 captions proving same rail/padding claim). No branch ambiguity: inspected ops/20261108-ui-hierarchy [RAN git branch --show-current -> ops/20261108-ui-hierarchy].

Method: mockup CSS + caption values extracted via Get-Content; ThemePalette/StudioPalette/MainWindow read at current HEAD; git log/show for 45031a2, 5ef2c98, 1a8304f, a4ab50a, 860b698; grep rail_width/kRailWidth/ApplyStudioStyle/PushFont; prior agent reports (W3-IMPL-S1, REV-W3-01, IMPR-W3-01, gate-w3-1) read as evidence, not instructions.

## 1. Per plan item (P-xxx)

### P-001 — MainWindow literals vs mockup §3/§5 intent [VERIFIED]
**Verdict: PARTIAL — code structurally matches mockup, density tokens exist but token coverage is incomplete.**
**Evidence:**
- [VERIFIED docs/design/mockup.html CSS: .rail width 208px (.rail), .content padding 32px (.content), .cards gap 16px, .card padding 16px radius 10, .row min-height 44px, .page-title 28px serif, .card-title 15px, .big-line/.label/.value 14px, .helper/.note 13px, .btn height 32px radius 6, .toggle 52x32, .chip height 36px]
- [VERIFIED docs/design/mockup.html §3 captions: "Window 960 by 640 by default, minimum 760 by 520, native title bar. Rail 208 px. Content padding 32 px, widest 640 px, left aligned." repeated verbatim in §3 dark, §3 light, §4 dark/light]
- [VERIFIED src/ui/theme/ThemePalette.h:81-137] metrics mirror §3/§5 exactly: space_xs 4 / s 8 / m 16 / l 24 / xl 32 [VERIFIED :85-89]; radius_control 6 / radius_card 10 / radius_pill 999 [:91-93]; rail_width 208 [:95]; content_max 640 [:96]; control_min_height 32 [:97]; fonts wordmark 18 / page-title 28 / card-title 15 / body 13 / mini 8.5 [:103-107]; full 960x640, min 760x520 [:110-113]; saved_dwell 1.5 [:135].
- [VERIFIED src/ui/MainWindow.cpp:56] `constexpr float kRailWidth = 220.0f;` — lives next to `namespace metrics = lwf::ui::metrics;` [:51], not using metrics::rail_width.
- [VERIFIED MainWindow.cpp:374,437] both rail uses (RunOnce content offset, DrawRail size) use kRailWidth*scale.
- [VERIFIED MainWindow.cpp:380-381] content padding correctly uses metrics::space_xl (32) — one of few correct token uses.
- [VERIFIED MainWindow.cpp:452,487,489,571,587,796,1598] Dummy spacing uses metrics::space_* in rail/card paths — pattern EXISTS.
- [VERIFIED MainWindow.cpp:469] `const float item_h = 40.0f * scale;` — literal rail row height; mockup .nav-item height is 36px. Rail rows are 4px denser than mockup, text at font_card_title 15px [:470] vs mockup nav 14px (.nav-item font-size 14px). Density drift confirmed, not palette drift.
- [VERIFIED MainWindow.cpp:449,470,567,895+ (20+ sites)] PushFont uses metrics::font_* correctly for wordmark/card-title/page-title. No SetWindowFontScale in src/ui/MainWindow.cpp [VERIFIED grep SetWindowFontScale -> 0 hits in MainWindow; ban test at tests/test_overlay_settings_ui.cpp exists].
- [VERIFIED MainWindow.cpp:555,925] `Dummy(ImVec2(14.0f*scale,0))` — two literal 14px spacings (mockup has no 14px spacing token; grid is 4/8/16/24/32). These are exactly the P-001 class (literal bypass).
- [VERIFIED ThemePalette.h:106] font_body_px = 13.0f while mockup §5 type scale defines body 14px (s4: 14px sans body/controls) and helper 13px (s5). Code collapses body+helper to 13. This is pre-existing token decision from GUI-A2 (b21e40f), not a Phase-0 discovery: token test pins it [VERIFIED STUDIO_PLAN.md:152 "body 13 px" mapping]. P-001 must preserve it; changing to 14 would contradict shipped token + test, and is out of this run's "keep tokens" scope [VERIFIED 00-mission.md:9].
**Root cause:** P17 rail rewrite (a4ab50a) introduced file-local kRailWidth=220 and 40px rows without updating the mockup token; earlier GUI wave left 14px literals and no 14px space token. MainWindow still mostly token-driven; literals are scattered, not systemic.
**Blast radius:** 40px vs 36px rail rows affect all 7 nav items + divider spacing [VERIFIED :459-467,469]; 14px literals at 2 sites.
**Fix options:** (a) normalize literals to nearest metrics (14->16 or 8+4+2 split needs decision); do NOT change 40->36 or font_body 13->14 in this run (visual change beyond spacing-only scope). (b) Document drift.
**Long-run test:** revert one literal -> source-scan fails (P-005). Load: 7 items * 4px delta = 28px vertical — screenshots at 960x640 vs 760x520 will show it. Hostile: N/A (spacing). Operability: screenshot gate catches pixel regression source-scan misses.
**Classification SURFACE.** WRITE-SET: src/ui/MainWindow.cpp, src/ui/widgets/* (shared). No contract-set. Test: source-scan + screenshots.
**Acceptance:** no new spacing/typography literals without metrics token; screenshots show no 4px rail-row delta introduced by the fix (fix must not silently retoken).

### P-002 — Studio pages literals vs studio_metrics intent [VERIFIED]
**Verdict: PARTIAL — studio_metrics exists and matches a different (screenshots-derived) intent, not mockup §5.**
**Evidence:**
- [VERIFIED src/ui/theme/StudioPalette.h:40-69] studio_metrics: radius_banner 16 / radius_card 12 / radius_row 10 / radius_input 10 / radius_button 8 / radius_chip 6 / page_padding 32 / gap 24 / row_height 52 / hit_size 32 / fonts banner 40 / caps 12 / page-title 28 / helper 13 [:42-58]. These do NOT match mockup §5 (radius_card 10, radius_control 6, row 44, body 14). They match the 2026-11-07 screenshots per STUDIO_PLAN §5.
- [VERIFIED docs/STUDIO_PLAN.md:116-136] §5 light table is explicitly "PROVISIONAL VISUAL ESTIMATES … NOT pixel-measured" and the navy estimates were discarded; shipped teal tokens are source of truth [VERIFIED StudioPalette.h:24-38 teal_700 #1E5E5A etc.]. Pixel pass R-01 still open [VERIFIED STUDIO_PLAN.md:167,219].
- [VERIFIED src/ui/pages/SnippetsPage.cpp:32,132,150,190,233,237,248,251,268] PushFont literals 15.0f/13.0f (10 sites) instead of studio_metrics or metrics tokens.
- [VERIFIED src/ui/pages/DictionaryPage.cpp:242,283,290,299] same literals 15.0f/13.0f.
- [VERIFIED src/ui/pages/InsightsPage.cpp:127,263,285,301] literals 10.0f/24.0f (4 sites) — ex-ledger "PushFont literals 15/13/24" [VERIFIED 01-work-ledger.md:36].
- [VERIFIED src/ui/widgets/Overlays.cpp:127,132,142,148] literals 13.0f (4 sites).
- [VERIFIED src/ui/widgets/Display.cpp, Buttons.cpp] consume kStudioPalette tokens heavily (50+ refs each) — widgets are token-consistent on color, only font literals drift.
**Root cause:** P19-P23 pages landed per-file (67c955b, 095ef47, 96fb22c, fc96356, acfaf21) with literals copied from screenshots/spec before studio_metrics font tokens were fully wired; no source-scan enforced token use at the time (gate-w8-1 font_sources gap [VERIFIED gate-w3-1.md:54-67]).
**Blast radius:** 5 page files + 2 widget files. Fonts only, no palette values involved.
**Fix options:** map literals to studio_metrics::font_* or metrics::font_* (needs 1:1 mapping decision: 15->font_card_title_px, 13->font_helper_px, 24/10->no token yet — leave + document).
**Long-run test:** revert one literal -> P-005 scan fails. Load: 10px/24px literals in Insights charts — screenshots at both window sizes.
**Classification SURFACE.** WRITE-SET: src/ui/pages/*, src/ui/widgets/*.
**Acceptance:** source-scan for PushFont( numeric literals in pages/widgets goes to zero or documented allowlist; no palette value changes.

### P-003 — Widgets density consistency [VERIFIED]
**Verdict: EXISTS as inconsistency (PARTIAL).**
**Evidence:**
- [VERIFIED src/ui/widgets/Buttons.cpp:58,69,73] StyleButton heights 40px (primary), row_height 52 via studio_metrics in pages vs metrics control_min 32 and mockup btn 32/chip 36. Buttons default 40px height ≠ mockup 32px control minimum ≠ studio row 52.
- [VERIFIED src/ui/theme/StudioPalette.h:52] studio row_height 52.0f vs mockup .row min-height 44px.
- [VERIFIED src/ui/widgets/Display.cpp:51] 1.0f*scale border literal vs metrics hairline concept (no border token).
**Root cause:** two independent density systems (mockup metrics vs studio_metrics) grown for different sources (D0 mockup vs 2026-11-07 screenshots) and never reconciled. This is the "dual-palette" density analog.
**Classification SURFACE** if only heights/spacing normalized via structure; DEEP if palette unification attempted (see P-004 risk). Recommendation: do NOT unify in this run; normalize within each system separately.
**Acceptance:** each system internally consistent; cross-system delta documented, not silently merged.

### P-004 — kRailWidth 220 vs rail_width 208 drift + D-rail recommendation [VERIFIED]
**Verdict: EXISTS, history fully determined. Recommendation: LEAVE 220 local, document dead token (matches mission default).**
**Evidence:**
- [VERIFIED ThemePalette.h:95] rail_width = 208.0f (mockup value).
- [VERIFIED MainWindow.cpp:56,374,437] kRailWidth = 220.0f, used at both sites.
- [RAN git log --oneline -- src/ui/theme src/ui/MainWindow.cpp -> a4ab50a is the commit that introduced it]
- [RAN git show a4ab50a -- src/ui/MainWindow.cpp -> `+constexpr float kRailWidth = 220.0f;` and both `metrics::rail_width * scale` sites replaced with `kRailWidth * scale`]
- [VERIFIED a4ab50a commit message: "Rail Home/Insights/Dictionary/Snippets/Transforms/Scratchpad/Settings (220px, divider, 40px rows, 8px radius, 15px)"]
- [VERIFIED docs/agent-ops/20261107-studio/reports/w3-impl/W3-IMPL-S1-T-014.md:8] "Rail 220px via file-local kRailWidth (ThemePalette.h metrics::rail_width=208 is read-only)"
- [VERIFIED docs/agent-ops/20261107-studio/COMMS.md:54] W3-IMPL-S1 CLAIM: "local kRailWidth 220" / ev src/ui/MainWindow.cpp DrawRail
- [VERIFIED docs/agent-ops/20261107-studio/reports/w3-review/REV-W3-01.md:168-172] R-W3-009 [NIT] dead rail_width 208 vs live 220; "update token to 220 or delete it" — flagged NIT, DO LATER.
- [VERIFIED docs/agent-ops/20261107-studio/reports/w3-improve/IMPR-W3-01.md:30] DEFERRED-DEPENDENCY: fix needs ThemePalette.h (FORBIDDEN write-set); NEED posted to ORCH.
- [VERIFIED docs/agent-ops/20261108-ui-hierarchy/00-mission.md:31] D-rail default (reversible): leave kRailWidth=220 local, document token drift, do NOT retoken.
- [VERIFIED tests/test_overlay_settings_ui.cpp:663] `CHECK(content.find("kRailWidth") != npos)` — test PINS the existence of kRailWidth. Deleting it breaks CI.
**Root cause:** P17/T-014 (W3-IMPL-S1, 2026-11-07) needed 220 for the 7-item rail (new Insights/Dictionary/Snippets/Transforms/Scratchpad items vs mockup's 9-item list at 36px rows). Agent correctly stayed out of forbidden ThemePalette.h and used a file-local constant, deferring the token update via NEED. Nobody closed the NEED; the token stayed dead. 220 is not a typo — it was a deliberate, reviewed, shipped choice with justification in the S1 report.
**D-rail recommendation with evidence:** LEAVE 220. Reasons: (1) changing 220->208 shrinks rail 12px * scale, changing content width at 960x640 and 760x520 — screenshots gate would need re-capture and nav labels (Scratchpad, Transforms) risk clipping at min size; (2) test at :663 pins kRailWidth, so "retoken" is not a one-line change; (3) mission D3 "Keep tokens" + D-rail default both say leave+document; (4) mockup 208 was designed for 9 short nav labels (Home/Dictation/Speech/Cleanup/Dictionary/Snippets/History/Privacy/About) while shipped rail has 7 long Studio labels — 220 is arguably more correct for shipped content. Document: add a comment at MainWindow.cpp:56 noting metrics::rail_width=208 is dead/mockup-historical, kRailWidth=220 is live, and any future retoken needs mockup regen + test update. That comment-only change is SURFACE and safe.
**Classification: SURFACE for leave+document; DEEP for retoken (palette/token change, re-screenshot, test rewrite). Do DEEP only under separate plan.**
**WRITE-SET for leave+document:** src/ui/MainWindow.cpp (comment) + this report. No ThemePalette.h edit. Test plan: source-scan still finds kRailWidth (test green); screenshot gate proves no visual change.
**Acceptance:** ORCH confirms leave+document; future retoken tracked as DEEP follow-up, not Wave 1.

### Dual palette persistence (Hunter/Brass dark vs Studio teal light) [VERIFIED]
**Verdict: EXISTS by design, Wave 4 deferred, not a bug for this run.**
**Evidence:**
- [VERIFIED ThemePalette.h:46-79] kDarkPalette Hunter/Brass + kLightPalette Ivory/Navy — frozen mockup values, header comment "Do not change values here without regenerating contrast tables" [:5-8].
- [VERIFIED StudioPalette.h:24-38] kStudioPalette teal light theme — provisional, separate struct, separate metrics namespace.
- [VERIFIED docs/STUDIO_PLAN.md:38] "src/ui/theme/ThemePalette.* read-only | P18 | dark tokens frozen; StudioPalette separate"
- [VERIFIED W3-IMPL-S003-T-015.md:23] "ApplyStudioStyle defined but never called — ApplyImGuiStyle(kDarkPalette) remains the only applied palette" (Wave 3 state).
- [VERIFIED docs/agent-ops/20261107-studio/gates/gate-w3-1.md:106-107] "ApplyStudioStyle vs ApplyImGuiStyle single-palette rule remain Wave 4 checkpoints … not gate-verifiable here."
- [VERIFIED W3-IMPL-S003-T-015.md:43] "Wave 4 must apply exactly one palette per ImGui context (remaining risk, documented not coded around)" — the two Apply* functions have independent (context,scale) gates, so alternating them per-frame would double-scale [VERIFIED IMPR-W3-01.md:13].
- [VERIFIED current code] ApplyStudioStyle has ZERO callers in MainWindow + Overlay [VERIFIED grep: only ScratchpadWindow.cpp:91,591 + tests/test_overlay_settings_ui.cpp:876,881 pinning absence] — Wave 4 never landed the single-palette rule. Studio pages draw with kStudioPalette tokens directly without switching the context style, so Full window currently shows dark chrome + teal cards by pixel mix. That is the dual-look the ledger pre-mortem warns about [VERIFIED 01-work-ledger.md:25].
- [VERIFIED 01-work-ledger.md:32] "Palette unification (separate plan if screenshots show split)" — deferred item for this run.
**Why it persists:** P18 shipped tokens/widgets as additive (safe), left the context-style switch to Wave 4, and Wave 4 (F9 sweep 69c8358) fixed contrast/copy but never wired the single-palette switch. Each wave respected its write-set (ThemePalette.h frozen). Unification needs a per-context palette decision + screenshot gate + pixel-measured teal (R-01) — a DEEP change.
**Classification: DEEP. Do NOT attempt in Wave 1.** Constraint for P-001..P-003: normalize spacing/fonts within each palette system only; never mix metrics:: (dark) with studio_metrics:: (teal) in one page. Screenshots must be checked for split-look; if split is more than acceptable, file DEEP follow-up.
**Trap compliance:** no color lightening proposed; ThemePalette values untouched.

### WB4 failed lightened palette (reverted) [VERIFIED]
**Verdict: ALREADY-FIXED / reverted, correctly.**
**Evidence:**
- [RAN git show 5ef2c98 --stat -> "fix: dark theme contrast - cards and borders invisible", bg 0x0E1714->0x1A2A24, surface 0x14221D->0x455A50, border 0x29392F->0x5A7068]
- [RAN git show 1a8304f --stat -> "Revert the earlier dark-theme contrast change (unnecessary now; exact mockup token values enforced by tests/test_overlay_settings_ui.cpp)" + context fix]
- [VERIFIED docs/PROGRESS.md:185] "WB4 | Earlier dark-theme contrast change reverted | DONE | (in 1a8304f) | lighter palette was a misdiagnosis; mockup-exact tokens restored"
- [VERIFIED docs/UI_AUDIT.md:3-11] addendum: DisplaySize=140x70 Mini badge / (0,0,140,70) viewport = black screen; fixed in 1a8304f.
- [VERIFIED AGENTS.md UI section] "Do NOT lighten colors to fix perceived contrast — the dark-screen bug was a viewport/context bug, not a color bug; the earlier lightened-palette change was reverted."
**Root cause of failure:** low surface-vs-bg contrast (1.11:1) was misread as the cause of the black screen; actual cause was GetCurrentContext() returning the other window's context after CreateContext(). Lightening changed the palette without fixing rendering, and broke the test-pinned mockup values.
**Constraint restated:** P-001..P-004 must not propose any palette value change without mockup regen + contrast tables [VERIFIED ThemePalette.h:5-8]. This run proposes none.

### UI_AUDIT D-register + R1 status (for hierarchy context) [VERIFIED]
**Evidence:** [VERIFIED docs/UI_AUDIT.md:294-314] D1 BLOCKER shared context, D2 crash Checkbox(nullptr), D3 UB Begin-outside-frame, D4 overflow, D5 OpenHistory, D6 fonts, D7 fixed metrics, D8 passthrough, D9 Save/Cancel, D10, D11 FPS, D12 includes, D13 dictation_enabled, D14 data race.
- [VERIFIED docs/UI_AUDIT.md:320-340] R1 fixed D1-D4, D9, D10, D11, D12, D13, D14; D5 unchanged (History page is R2); D6/D7/D8 partial (fonts pending approval, work-area refresh R3, drag R3).
- [VERIFIED docs/PROGRESS.md:111] R1 DONE (pending Windows verification), 162/162 Linux tests.
- [VERIFIED docs/MANUAL_TEST_WINDOWS.md:293-383] §R checklist still UNVERIFIED (R-a..R-e, R2-a..R2-d) — "Every item below is still UNVERIFIED until ticked on this machine" [:300]. P-006 screenshots are therefore the first real verification of hierarchy/density.
**Implication:** P-001..P-005 changes land on top of R1 + Studio (T-014..T-025) which were Linux-green but not Windows-verified. Screenshot gate (P-006) is load-bearing, not ceremonial.

## 2. Duplicates / shared root causes

- **Shared root X: two token systems, no cross-system guard.** metrics (mockup) + studio_metrics (screenshots) both define radius/font/spacing with different values (card 10 vs 12, body 14 vs 13, row 44 vs 52). pages mix them freely (InsightsPage uses both at :350,365). P-001..P-003 share this root; fix by documenting boundary per file, not by merging systems.
- **Shared root Y: local constants shadow dead tokens.** kRailWidth shadows metrics::rail_width; PushFont literals shadow both font tokens. Same anti-pattern, different files. P-005 source-scan should cover both (numeric PushFont + bare *0.0f*scale spacing + ImVec2 literals in pages).
- **Duplicate:** 01-work-ledger "ApplyStudioStyle only in ScratchpadWindow" [VERIFIED re-confirmed via grep] — confirms MainWindow never calls it, dual-look is inherent, not accidental.
- **Duplicate:** STUDIO_PLAN §5 navy table vs shipped teal — STUDIO_PLAN.md:136 explicitly marks navy as discarded history; do not treat §5 table as token source.

## 3. Out-of-plan discoveries

1. **Mockup body 14px vs shipped font_body_px 13px** [VERIFIED mockup §5 s4 14px vs ThemePalette.h:106 13px] — shipped token intentionally collapses body+helper to 13 (GUI-A2 b21e40f, ~2 sites). P-001 must preserve; any "restore mockup body size" would be a token change + test rewrite. Flagged here so Wave 1 does not "fix" it inadvertently. Severity: LOW for density (1px delta).
2. **Mockup nav height 36px vs shipped 40px rail rows** [VERIFIED mockup .nav-item 36px vs MainWindow.cpp:469 40px] — 4px/row denser, 7 rows = 28px vertical. Likely intentional for Studio label legibility (P17 commit message lists 40px explicitly), but undocumented vs mockup. Wave 1 should not shrink it without screenshots.
3. **Fixture: MANUAL_TEST_WINDOWS §R never ticked** [VERIFIED :300] — hierarchy work has no Windows baseline pixels; P-006 before-screenshots are new evidence, not regression check.
4. **Studio teal never pixel-measured (R-01)** [VERIFIED STUDIO_PLAN.md:167] — any screenshot gate that "approves" teal accepts provisional values permanently. Record as accepted, not verified.

## 4. Assumptions and unknowns

- [ASSUMED] Mockup CSS values equal mockup §5 table (spot-checked .rail/.content/.card vs captions — consistent). Full 181-line HTML not rendered; CSS text is the source.
- [ASSUMED] a4ab50a 220px choice was visually approved at the time (commit message states it as spec; no screenshot attached in git).
- [UNKNOWN] Whether 40px rail rows / 220px rail ever had a screenshot approval — S1 report cites no image. Resolve: ORCH asks Devansh; until then treat as shipped intent.
- [UNKNOWN] Whether dark+teal mixed look in Full window (current state) is acceptable to Devansh — needs P-006 screenshots. Resolve: screenshot review.

## 5. Recommended task breakdown and ordering

| # | Task | Class | Scope | Acceptance |
|---|---|---|---|---|
| 1 | P-001+P-003 (MainWindow+widgets spacing/fonts) | SURFACE | MainWindow.cpp, widgets/* | literals -> metrics::*; no palette change; screenshots unchanged except intended density |
| 2 | P-002 (Studio pages literals) | SURFACE | pages/* (5 files) | literals -> studio_metrics::* or documented allowlist for 10/24px; no palette change |
| 3 | P-004 document | SURFACE | comment at MainWindow.cpp:56 + report | leave 220, note dead 208 token; test :663 stays green; no ThemePalette.h edit |
| 4 | P-005 source-scan guard | SURFACE | test_overlay_settings_ui.cpp | fails on reintroduced literal; covers PushFont + *scale spacing |
| 5 | P-006 screenshot gate | — (manual) | Windows 960x640 + 760x520, DPI 1.0+1.5 | before/after attached; dual-palette accept/reject decided |
| 6 | (Follow-up, NOT this run) rail retoken 220↔208 | DEEP | ThemePalette.h + mockup regen + tests | separate plan with pixel pass R-01 |
| 7 | (Follow-up, NOT this run) palette unification / single-style rule | DEEP | StudioPalette wiring + per-context style | separate plan; needs ApplyStudioStyle vs ApplyImGuiStyle decision per context |

**Ordering:** 1 and 2 can run in parallel (disjoint write-sets except shared widgets — ledger C1: if T-001 and T-002 both touch widgets/*, serialize). 4 after 1+2 (scan must pin the fixed state). 3 anytime (comment-only). 5 gates merge of 1-4. 6-7 out of scope.
**Conflicts:** none with siblings INV-01..03 (this report is history/intent only; INV-01 owns architecture, INV-03 owns test blast radius).
**Top risks:** (1) Wave 1 "fixes" 220->208 or 40->36 as drive-by — would contradict D-rail and invalidate the :663 pin; mitigation: this report + mission lock. (2) Screenshot gate skipped because "Linux CI green" — §R was never verified on Windows; insist on P-006. (3) Dual-palette split judged in screenshots but filed as "will fix later" without owning task — ledger already lists it as separate plan; keep it there.

## Appendix: evidence index

- Mockup §3 caption + CSS: docs/design/mockup.html (.rail 208, .content 32, .window 960) [VERIFIED read + shell extraction]
- Mockup §5 type/spacing/radii/controls: .s1-s5, .space-bar, .radius-chip, .token-h [VERIFIED]
- Token defs: ThemePalette.h:45-137; StudioPalette.h:24-69 [VERIFIED read]
- kRailWidth: MainWindow.cpp:56,374,437 [VERIFIED read + git show a4ab50a]
- Lightened palette: git show 5ef2c98, 1a8304f; PROGRESS.md:182-186 [VERIFIED]
- Dual palette defer: gate-w3-1.md:106; W3-IMPL-S003-T-015.md:23,43; IMPR-W3-01.md:13,30 [VERIFIED read]
- R-W3-009: REV-W3-01.md:168; IMPR-W3-01.md:30 [VERIFIED read]
- Test pins: tests/test_overlay_settings_ui.cpp:663 (kRailWidth), :876,881 (ApplyStudioStyle absence) [VERIFIED grep]
- Manual-test status: MANUAL_TEST_WINDOWS.md:293-383 [VERIFIED read]

## Scratch files created

none (no builds, no scripts; read/git-log only per brief §7)
