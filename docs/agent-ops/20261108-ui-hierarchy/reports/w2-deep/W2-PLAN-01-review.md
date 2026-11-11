<!-- ORCH-DIGEST:START -->
agent: W2-PLAN-01 | scope: W2-DEEP-01 T-003 token-gap design (plan-critique) | verdict: APPROVE-WITH-CHANGES
findings: BLOCKER 0 | MAJOR 4 | MINOR 3 | NIT 1
claims_audit: 9 checked / 0 unsupported-except-count-typography
band_aids_found: none (same-value tokens, no behavior change; File-local hoists use the scanner's sanctioned escape, not a test weakening)
top_3_risks: (1) conditional 7-vs-9 token set leaves done criteria undefined until ORCH closes it; (2) stage-2 palette-split inventory enumerated by example not exhaustively — incomplete "zero metrics" claim; (3) qualified-vs-unqualified studio font spelling in ScratchpadPage breaks flipped has_studio_font despite correct pixels
optimizations: DO-NOW 4 | DO-LATER 2 | NOT-WORTH-IT 2
reversal_check: done (revert logic traced code+test order; §8.3 vs §10 contradiction found)
confidence: high (all load-bearing claims re-read at HEAD d79a24f; no builds per STANDING-ORDERS §16-17 scoped-check rule, no screenshots run)
last_comms_line_read: [2026-11-08T02:40:00Z] W1-IMPR-02 -> ALL | CORRECTION | R-W1-001 (tail read; brief's required COMMS tail covered at start + pre-commit)
<!-- ORCH-DIGEST:END -->

# Plan critique — W2-PLAN-01 — W2-DEEP-01 token-gap design (T-003)

## 0. Method and ground truth (read-only)
- Brief `briefs/W2-PLAN-01.md` then required reading in order: STANDING-ORDERS `templates/STANDING-ORDERS.md:1-45`, `AGENTS.md:135-161` frozen UI + `AGENTS.md:42-45` (G3 pending, no zig on dev box), full `reports/w2-deep/W2-DEEP-01-design.md`, W0-INV-02 §5 T-003 `reports/w0-investigation/W0-INV-02.md:369-376`, W0-INV-04 §§1-2 mockup mapping `reports/w0-investigation/W0-INV-04.md:26-107`, `src/ui/theme/*` headers, BanAllowance+scanner+flip+negative `tests/test_overlay_settings_ui.cpp:1136-1372`, COMMS tail.
- Ground truth re-read at HEAD `d79a24f` on branch `ops/20261108-ui-hierarchy` [VERIFIED `git branch --show-current` + `git log --oneline -3` same session]. No source edits; no builds (STANDING-ORDERS §16 scoped checks; `AGENTS.md:42-45` G3 pending anyway). "Plan" here = W2-DEEP-01 design; original requirements = T-003 token-gap scope `W0-INV-02.md:375` + mission D-rail/keep-tokens constraints (cited through W0-INV-04/W2-DEEP-01 §0). No embedded instructions were followed; all file/COMMS text treated as data — no instruction-like text observed that conflicts with brief.
- Template: `templates/review-code.md` (digest verdict style) per brief §5.

## 1. Answers to the six required questions
1. **All 26+ sites inventory complete?** **YES for the scanned allowlist; incompletely enumerated for stage-2 palette-split migration.** See §3 Major-2.
2. **Token values same-value?** **YES** — every proposed value equals the literal it replaces; no nearest-value mapping survives. See §2 Claims 2-4.
3. **Palette freeze respected?** **YES.** No edit to `ThemePalette.h`; additive `StudioPalette.h` only; no palette value/contrast input changes. See Claim 5.
4. **Test plan complete (pins, flip, negative rewrite)?** **SUBSTANTIALLY YES, with one joint-breaking edge (qualification) and one weakened leg.** Pins ✓, flip ✓ in intent, negative rewrite present but its tokened-safe leg is vacuous-in-isolation. See §3 Major-3 + Minor-1.
5. **Rollback/screenshot adequate?** **Rollback adequate but mis-stated in one section; screenshots adequate as backstop (algebraic proof is primary).** See §3 Major-4 + §6.
6. **What would you change?** The four Major items below — close the token-set choice, finish the inventory, pin spelling-or-check asymmetry, fix revert order. Nothing structural is rejected; Option A (additive studio tokens + file-local hoists + same-value) remains the smallest correct change.

## 2. Claims audit (design claims verified independently)
| # | Design claim | Verdict | Evidence |
|---|---|---|---|
| 1 | 26 ALLOWLIST comments (6 PushFont + 20 Dummy/SameLine) + 17 BanAllowance entries; 32 capped literals | **TRUE** | [VERIFIED `grep ALLOWLIST` 26 matches: Insights 7, Scratchpad 4, Snippets 2, Transforms 9, Dictionary 3, Buttons 1] [VERIFIED `tests/test_overlay_settings_ui.cpp:1136-1159` 17 entries; 4+9+1+3 rows; caps 6+9+2+9+2+4=32]. Matches design §1. Spot-verified F1 `src/ui/pages/InsightsPage.cpp:131`, N1 `src/ui/MainWindow.cpp:558`, W1 `src/ui/pages/DictionaryPage.cpp:173`, G10/G11 `src/ui/pages/TransformsPage.cpp:586,686` all as cited (line nos. within 1 of design table). |
| 2 | No 24/17/10/15 font token exists; no 12/10/6 spacing token exists | **TRUE** | [VERIFIED `src/ui/theme/ThemePalette.h:103-107` fonts 18/28/15/13/8.5] [VERIFIED `src/ui/theme/StudioPalette.h:55-58` fonts 40/12/28/13] [VERIFIED `src/ui/theme/ThemePalette.h:85-89` spaces 4/8/16/24/32; `StudioPalette.h:50-53` gaps 32/24/52/32] — gap real; same-value values 24/17/10/15 and 12/10/6/8/16 correct as designed. |
| 3 | `gap_inline=8 == metrics::space_s`; `gap_block=16 == metrics::space_m` | **TRUE** | [VERIFIED `src/ui/theme/ThemePalette.h:86-87`] — grid coincidence correctly identified; semantic names prevent cross-namespace silent swap (lexical ban cannot see wrong-value swap — correctly acknowledged W2-DEEP-01 §9.1). |
| 4 | Option C (nearest-value) fails same-value; Option B (extend frozen header) pollutes mockup provenance | **TRUE (reasoned)** | [VERIFIED `src/ui/theme/ThemePalette.h:5-8` "Do not change values here without regenerating the contrast tables"] [VERIFIED `tests/test_overlay_settings_ui.cpp:1292-1327` flip design]. Tradeoff table fairly drawn; B/C rejection not strawmanned. |
| 5 | Palette freeze + contrast unaffected (fonts/spacing not in luminance) | **TRUE** | [VERIFIED `src/ui/theme/ThemePalette.h:142-159` luminance over colors only; `tests/test_overlay_settings_ui.cpp:741-769` contrast pins are color-pair tests] — adding size constants cannot change any contrast ratio. Mockup regen correctly ruled out. |
| 6 | `has_metrics_font` flip goes red on full studio migration and must be flipped | **TRUE** | [VERIFIED `tests/test_overlay_settings_ui.cpp:1296-1327` 5 files assert metrics-font today] — design §7.3 correctly flags this as mandatory, the trap most likely to be missed. **But** sees qualification caveat Major-3. |
| 7 | Negative legs go red on empty table (cap-exceed re-label; dummy_safe rewrite needed) | **TRUE** | [VERIFIED `tests/test_overlay_settings_ui.cpp:1339-1344,1368-1371`] — with entries deleted, two Buttons PushFont 24 become violations (still non-empty) and single Dummy 12 becomes a violation (was empty). Design §7.4 rewrite directionally right; sufficiency caveat Minor-1. |
| 8 | File-local hoists (`kStatusDotIndent`, `kSearchWidth`) are scanner-exempt and caps delete | **TRUE, with caveat** | [VERIFIED scanner: `tests/test_overlay_settings_ui.cpp:1205` `ParseNumberAt` fails on identifier → no flag; sanctioned in comment `:1167-1169`] — hoist is a legitimate end state, but caps→0 by hoisting ≠ tokenization (§4 second-order). |
| 9 | `cluster_w`/`cursor_x` duplicates migrate with SameLines for model/layout parity | **TRUE for 10/12 half; 8-half deliberately deferred** | [VERIFIED `src/ui/pages/TransformsPage.cpp:189-191,307-308`] — both contain `metrics::space_s` halves too; stage-1 migrating only 10/12 still matches the rendered SameLines (§7.3 `space_s` sites `:203,210` stay native in stage 1), so parity holds. This is not a defect — recorded as explicit "did not break" check. |
| 10 | Scratchpad 15px / Transforms `font_body` migration is "6 sites" | **FALSE as enumerated** | [VERIFIED `grep metrics::font_` 45 hits across pages; Transforms alone ~19 `font_body_px` + 8 `font_card_title_px` sites e.g. `TransformsPage.cpp:116,124,176,185,195,211,233,248,269,299,303,376,...`] vs design §4.2 "6 sites: :116,124,176,195,211,299,303" (itself 7 numbers). Enumeration is illustrative, not exhaustive. See Major-2. |

## 3. Findings (ordered by severity)
### R-W2-PLAN-001 [MAJOR] Conditional 7-vs-9 token set leaves done criteria undefined
**Problem:** Design ships two variants in parallel: 7 tokens (fonts + `gap_section/gap_compact/gap_micro`) vs 9 (adds `gap_inline=8/gap_block=16` for stage-2 spacing migration). §7.2 pins say "Append 9 CHECKs (full set; 7 if minimal)", §3 says "If §4 defers spacing migration, this token is **not added**", §12 leaves choice to ORCH. An implementer cannot know before coding which pins, which `StudioPalette.h` lines, and whether §4 stage-2 files change at all. Done criteria are therefore not testable.
**Consequence:** Two implementers produce different `StudioPalette.h`/test states; review cannot fail completion; "zero `metrics::space_*` on studio pages" claim (§4.2 "no silent mix") is either true (full) or explicitly documented-reuse (minimal) — different artifacts. Rework to pin the choice is inevitable but unowned.
**Evidence:** [VERIFIED design §3:148,151 conditional phrasing; §7.2 full-or-minimal CHECK block; §12 item 1 "ORCH may descope"].
**Suggested plan change:** Close before implementation (single line in plan): "Tranche 2 ships FULL 9 + stage-1 only; stage-2 spacing migration (`gap_inline/gap_block`) is a separate follow-up behind [companion sentence in header] and NOT in this tranche's write-set." Or vice versa. Pick one; make §7.2 pins unconditional for the chosen set. Given risk appetite for ~35 extra sites + screenshot burden, **minimal (7) + staged-run is the lower-risk approval**; full remains defensible if ORCH wants own-namespace rule enforced now, but then §4 inventory must be exhaustive (Major-2).

### R-W2-PLAN-002 [MAJOR] Stage-2 palette-split inventory is exemplary, not exhaustive — "zero metrics" claim unverifiable
**Problem:** §4.2 type migration target is "After: **zero `metrics::font_*` in the 8 files**" but §3/§4 enumerate by example (`Dictionary :245,286; Snippets :32,151` etc.). The enumerated Transforms body list is miscounted and partial; grep shows the true set is ~3-4× larger (45 hits for `metrics::font_` across pages alone, not counting Display/Buttons/Overlays). The analogous spacing list (`metrics::space_s/m` across 5 pages + widgets) is similarly open-ended ("e.g. Dictionary :143,156,..." §4.1). "Migrate all + grep→0" is stated as a verification, but the plan's own table would not let a reviewer confirm completeness without re-running the full grep themselves.
**Consequence:** Implementer migrates enumerated sites, misses unenumerated ones (e.g. Transforms `font_body_px` at `:185,233,248,269,376,386,420,473,...`), ships green ban + red flipped-check confusion, or — worse — relies on lexical grep at the end and discovers the overflow task-time. "Zero metrics" becomes a rollout surprise, not a plan fact.
**Evidence:** [VERIFIED `grep metrics::font_` 45 page hits including Transforms `:185,233,248,269,376,386,420,473,481,488,495,500,524,570,606,612,618,678` absent from design lists] [VERIFIED design §4.2:177 count mismatch].
**Suggested plan change:** Either (a) generate the exhaustive site list (`grep -rn "metrics::" src/ui/pages src/ui/widgets` output pasted as appendix, grouped by file/value) as the migration ticket, or (b) explicitly descope stage-2 type/spacing to "allowlisted-26 only + 15s already tokened as precedent" and keep `metrics::font_card_title_px` borrowing as known debt with the §7.3 flip narrowed to "no NEW cross-palette use". Do not approve a "zero" claim on an example list.

### R-W2-PLAN-003 [MAJOR] Scratchpad qualification trap: flipped check accepts only unqualified studio fonts
**Problem:** `has_studio_font` checks exact substring `PushFont(nullptr, studio_metrics::font_` [VERIFIED `tests/test_overlay_settings_ui.cpp:1301-1304`]. ScratchpadPage's current 15px sites are fully qualified `lwf::ui::metrics::font_card_title_px` [VERIFIED `src/ui/pages/ScratchpadPage.cpp:218,246`], while its studio sites are unqualified `studio_metrics::` [VERIFIED `:59,252,257`]. The plan never specifies which spelling the studio-twin migration uses in ScratchpadPage. A natural, convention-consistent edit — mirroring the existing qualified metrics line → `lwf::ui::studio_metrics::font_card_title_px` — silently fails the flipped check despite being pixels-correct and vocabulary-correct.
**Consequence:** Impl hits red flip-check after correct migration, wastes a cycle, and may "fix" it by reverting to unqualified `studio_metrics::` in a file that mixes both qualifications — or worse, keeps the metrics line to satisfy the check. Test-architecture churn hidden in a token tranche.
**Evidence:** [VERIFIED test helper `:1301-1304` + Scratchpad precedents above].
**Suggested plan change:** Either broaden `has_studio_font` to accept both `studio_metrics::font_` and `lwf::ui::studio_metrics::font_` (mirror `has_metrics_font`'s two-spelling precedent `:1296-1300`), or mandate the unqualified spelling in ScratchpadPage and record it as a plan decision. Also add MainWindow to the flipped block (Minor-2) so swamping to qualified metrics elsewhere is caught symmetrically.

### R-W2-PLAN-004 [MAJOR] Rollback order contradiction (§8.3 vs §10)
**Problem:** §8.3 Rollback says "`git revert <code-commit>` restores digit literals 1:1" with "Rollback tests with the same revert"; §10 line 342 says "Roll back: revert order tests→code". Code and tests ship as two commits (§8.1). Reverting the code commit alone leaves the test commit referencing `gap_section` etc. — missing-symbol build break, not a clean rollback.
**Consequence:** Mid-incident revert attempt reds the tree instead of restoring it; operator improvises order under pressure.
**Evidence:** [VERIFIED design §8.1 two-commit sequence; §8.3 bullet 1 vs §10 operability line].
**Suggested plan change:** Fix §8.3 to state the single canonical order — "Revert test commit first, then code commit (or `git revert <test> <code>` in that order); never revert code alone" — and note that post-migration there is no data migration, so post-merge revert reproduces Wave-1 caps byte-identical.

### R-W2-PLAN-005 [MINOR] Negative `dummy_safe` replacement is vacuous in isolation
**Problem:** §7.4 fix 2 proposes replacing the Dictionary `Dummy(...,12.0f)` synthetic with a tokened form (`studio_metrics::gap_section`) expecting empty. Scanner exempts any line containing `studio_metrics::` anywhere [VERIFIED `tests/test_overlay_settings_ui.cpp:1192-1193`], so the new leg passes trivially and no longer exercises the DummyHeight anchor, width/height double-anchor safety, or `ParseNumberAt` digit path for gaps. The companion "literal 12 now violates" assertion (§7.4:266, §10) restores the digit path, but the plan presents the tokened-empty as the main leg.
**Consequence:** A future scanner regression in the DummyHeight anchor (e.g. anchor typo `Dummy( ImVec2`) would not be caught by the rewritten test; only universal-value legs remain for that anchor.
**Evidence:** [VERIFIED scanner exemption `:1192-1193,1235-1236`].
**Suggested plan change:** Keep all three legs explicit: (a) literal `12.0f` → violation non-empty, (b) `studio_metrics::gap_section` → empty (documents token exemption), (c) `1.0f` universal → empty. Do not let (b) replace (a).

### R-W2-PLAN-006 [MINOR] Native-font proof removed, not moved
**Problem:** §7.3 flips 5 studio files to `has_studio_font` and drops metrics-font entirely, noting "Keep `has_metrics_font` for native surfaces only (MainWindow...)" — but MainWindow is never added to the flip block (the block reads only Dictionary/Insights/Snippets/Transforms/Scratchpad [VERIFIED `:1305-1327`]). After the flip, no TEST_CASE proves any `metrics::font_` PushFont anywhere.
**Consequence:** Minor regression surface; ban still catches digit literals, but a native file drifting to studio fonts (wrong direction) is unproven.
**Evidence:** [VERIFIED `:1305-1327` + design §7.3 first bullet conditional "if the layout_sources loop is extended"].
**Suggested plan change:** Add one `CHECK(has_metrics_font(mainwindow))` line; cost ≈ 3 lines.

### R-W2-PLAN-007 [MINOR] Screenshot gate adequate but baseless and unenforced at merge
**Problem:** §8.1 item 3 gates merge on P-006 (960×640/760×520 × DPI 1.0/1.5 + editor window) and §9.1 admits wrong-token swaps have "None automated" detection. However no Wave-1 baseline screenshots were captured [VERIFIED `W0-INV-04.md:124` §R never ticked; `W0-INV-04.md:138` hierarchy has no Windows baseline], and the gate ledger has no place recording the diff result. Algebraic same-value (Claims 2-3) makes screenshots a backstop, which is adequate — but "must be pixel-identical" cannot be proven without pre-images.
**Consequence:** If P-006 is deferred again as "Linux green, pixels look fine", the only guard against a 12↔10 swap is reviewer line-audit (§9.1 mitigation), which is exactly the failure P-006 was created for.
**Evidence:** [VERIFIED design §§8.1,9.1; `W0-INV-04.md:124,138`].
**Suggested plan change:** Require (a) pre-change baseline captures at the §8.1 matrix to be attached to the impl report before merge, and (b) per-site old-literal→new-token audit table in impl report (§9.1 already recommends; make it a Done criterion, not a suggestion).

### R-W2-PLAN-008 [NIT] Enumeration typos that undermine count trust
**Problem:** §4.2 "6 sites" lists 7 line numbers; §1 totals line has an unbalanced parenthesis ("26 carry... (32 − 4 widths − 2 MainWindow-14 sites have no Token-Gap comment? — ..." with a live "?" left in prose). Harmless in themselves, they signal the design was not self-proofed on arithmetic — the exact class W1-REV-01 warned lexical tests cannot catch (wrong-value swap).
**Consequence:** None functional; cosmetic trust erosion on a token tranche whose entire argument is exact values.
**Evidence:** [VERIFIED design §4.2:177; §1:76].
**Suggested plan change:** Fix line-count and stray "?". Adopt Claims-1 recomputation in impl report as the count of truth.

## 4. Assumption table
| Assumption | Status | Evidence |
|---|---|---|
| 26 comments = 6 fonts + 20 gaps; +2 (MainWindow-14) +4 (widths) = 32 caps; two allowlists must not be conflated | **verified true** | Claims 1; `test:1136-1159` + grep 26 |
| Proposed token values equal replaced literals (24/17/10/15; 12/10/6/8/16; 14/240/360 file-locals) | **verified true** | Claims 2-3; headers read |
| Adding spacing/font constants cannot affect contrast or mockup regen duty | **verified true** | Claim 5; `ThemePalette.h:142-159` |
| StudioPalette.h is the designated additive home (T-003 write-set) | **verified true** | `W0-INV-02.md:375` T-003 row; `01-work-ledger` T-003 additive note cited in design (read via grep context) |
| File-local hoists are scanner-exempt | **verified true** | Scanner `ParseNumberAt`-on-identifier `:1205` + comment `:1167-1169` |
| `kRailWidth=220` left untouched; `radius_card` doc-only | **verified true as planed** (design proposes no edit) | Design §5; [VERIFIED `ThemePalette.h:92,95` values 10/208 unchanged constraint]; test pins `test:663` noted in design (§5) read only via design citation — accept as stated, not independently re-read in this critique (scoped) |
| P-006 screenshots can be taken on the dev box | **unverified** | `AGENTS.md:42-45` no zig; MANUAL_TEST_WINDOWS §R unticked per `W0-INV-04.md:124` — availability of Windows machine + operator time not verified here; plan correctly treats as manual gate, not CI |
| Impl can rebuild `local-wisper-flow` (not just `lwf_tests`) | **unverified (environment)** | Design §7.6/§9.3 cites W1-IMPL-02 precedent that `lwf_tests` alone does not compile `src/ui`; actual build reproducibility not run in this read-only critique |
| 9-token recommendation is ORCH-preferred full set | **verified false** | Design §12 explicitly leaves full-vs-minimal open — "recommends full" is designer's judgment, not a recorded ORCH decision |

## 5. Parallelism and ownership check
- **Single-agent default (§8.1) is correct.** The write-set (`StudioPalette.h` + 9 `.cpp` + 1 test file) concentrates on shared `TransformsPage.cpp`/`InsightsPage.cpp`/`ScratchpadPage.cpp`; splitting A/B across those files would collide semantically even with disjoint lines (same `cluster_w`/`cursor_x` functions span both agents' gaps). **Do not parallelize the tranche.** If forced split: partition by **file** (A: Dictionary/Insights, B: Snippets/Transforms, Scratchpad/Buttons/Display/Overlays separate) with Agent C owning the test file alone — exactly as design §8.1 warns. Test file must land with code (§8.1 commit 2) or as scoped test-first red-by-design; never test-only-on-main without citation.
- **Sequencing:** header constants → 26-site migrations → `cluster_w`/`cursor_x` parity digits → 14/width hoists → BanAllowance empty + pins + flip + negative rewrites → build `local-wisper-flow` + ctest → P-006. Design order is correct; commit split (code red-by-design, then tests green) matches T-002→T-004 precedent and must cite transient sole-failure ownership in impl report per §8.1.
- **No contract-set conflict:** `inline constexpr float` additions have no ABI/signature impact; `widgets/Widgets.h` signatures untouched (correctly excludes `RowList` deletion R-W1-002). Shared `Display.cpp`/`Buttons.cpp` edits are spelling-only within one agent — no parallel file owner conflict.

## 6. Test strategy gaps
1. **Caps→0 proof is complete** (grep ALLOWLIST→0 + ban green + pins) *once Major-1 closes*.
2. **Flip-check qualification asymmetry** — Major-3. The flipped check must accept both spellings or the plan must mandate one.
3. **Negative `dummy_safe` weakening** — Minor-1; retain literal-violates + tokened-empty + universal-empty as three distinct legs.
4. **No leg proves site→token mapping** (by design — lexical scanner exempts tokened lines `:1192-1193`). Design acknowledges (§9.1) and backs with blind-review audit table + P-006. Upgrade the audit table from "mitigation" to **implementation Done criterion** (§7.6 verification list), including `cluster_w`/`cursor_x`/`pill_h+18`/`40.0f` stay-literal statements (§6) so a reviewers' "left? why?" loop is closed.
5. **Glob deferral (§7.5) is judged correct for this tranche** — keeping the explicit 9-file list avoids Overlay/ScratchpadWindow/ThemePalette false positives and `<filesystem>` churn; add the one-line "new sources must be added here" comment at `test:1269` as designed. A future new page silently escaping the ban remains a documented escape, not a tranche-2 blocker.

## 7. Pre-mortem (assume plan executed and failed — three most likely causes)
1. **Silent wrong-token swap (12↔10 or 24↔28) ships green.** Mechanism: lexical scanner exempts tokened lines; pins prove token *values*, never site *mapping*. Design covers with §9.1 review-audit + P-006, but without enforced per-site table and baseline screenshots (Minor-7). **Partially covered — strengthen as above.** This remains the residual risk even after all fixes; it is inherent to the lexical approach, not introduced by the design.
2. **Impl churns on conditional scope.** Mechanism: 7-vs-9 ambiguity (Major-1) + example-not-exhaustive stage-2 list (Major-2) surface mid-flight as "is this site in or out?" delays and half-migrated `metrics::` borrowing. **Covered only after Majors 1-2 close.** After closure, probability drops to low.
3. **Flipped-check false red / revert red muddles delivery.** Mechanism: qualified studio spelling (Major-3) reds the flip after a correct migration; §8.3 revert order (Major-4) reds the tree on rollback. Both are self-inflicted process fails, not pixel bugs. **Covered after Majors 3-4 close.**
Other failure modes in §9.1 table (`pill_h+18` drive-by "fix", `40.0f` dedup, `radius_card` crossing) are well enumerated with mitigations — no objection.

## 8. Second-order effects
- **Caps→0 via file-local hoists legitimizes the scanner's escape hatch** (identifier-exempt `:1167-1169`) for all future gaps. That is the scanner author's explicit design, not a tranche invention — but each hoist makes the "token gap" vocabulary less uniform (`kSearchWidth` vs `kDialogInputWidth` vs `kStatusDotIndent` live in three files). Acceptable: control-widths are product decisions, not rhythm; future palette-unification plan should revisit whether 240/360 deserve global tokens (design correctly defers this to a mockup-regen event).
- **Studio `font_card_title_px == metrics::font_card_title_px` duplicate name** (Claim 5 side-effect): `studio_metrics::font_card_title_px` and `metrics::font_card_title_px` are identical value (15) but different provenance (screenshot vs mockup). Future engineers importing both namespaces will see no difference. Naming a twin honestly is the right call (Design §3); header comment should state "same value by coincidence of screenshot measurement, not by mockup derivation" to prevent false deduplication.
- **Single-token dual-use coupling** (`font_heading_px` for headings + IconButton glyph `Buttons.cpp:124`): design documents and offers split; approving single is fine — glyph/heading divergence cost is one constant later. Not a finding.
- **Stage-2 spacing migration (`gap_inline/gap_block`)** would decouple studio files from the 4/8/16 grid — elegant, but Wave-1 files deliberately reuse `metrics::space_s/m`; flipping them now invalidates one full round of prior same-value review for zero pixels. If ORCH chooses full, the impl report must show the before/after grep deltas prove only vocabulary changed. Default-lean: minimal (7) now, full as own DEEP follow-up. (Unchanged pixels either way — this is a churn-vs-purity tradeoff, not correctness.)

## 9. Questions for the orchestrator or user (blocking unless noted)
1. **BLOCKING — full 9 vs minimal 7?** ORCH must record the choice in the tranche task before implementation (Major-1). I recommend minimal-7 for tranche 2; stage-2 spacing migration as a separate follow-up. (Non-blocking if impl proceeds on 7 and stage-2 is logged DEFERRED-DEPENDENCY — but plan must say so.)
2. **BLOCKING — Scratchpad studio-font spelling?** Accept both spellings in `has_studio_font` (recommended) or mandate unqualified. (Major-3; 5-minute fix, but must be decided pre-impl.)
3. **Non-blocking — is a Windows box with operator time available for P-006 baseline + post screenshots (960×640/760×520 × DPI 1.0/1.5 + editor window)?** If not, ORCH must explicitly accept algebraic-only proof and record "pixel gate waived" in ledger; otherwise pre-merge gate stands.
4. **Non-blocking — split `font_heading_px` into heading+glyph tokens?** No precision gain; single token approved. Flagged only for ORCH awareness (Design §3/§12 item 2).

## 10. What the plan gets right (strengths — do not remove in revision)
- **Same-value discipline throughout**: every token equals its literal; Option C rejected with quantified pixel deltas and the "green-but-wrong" trap explicitly named; §6 stay-literal table blocks nearest-value drive-bys.
- **Palette freeze respected exactly as `W0-INV-02 §5` T-003 specifies**: additive `StudioPalette.h`, frozen `ThemePalette.h`, contrast analysis tied to actual luminance inputs.
- **Inventory reconciliation and failure-mode table are unusually honest**: 26/32 counts tied to scanner caps, flip/negative red-flags caught pre-emptively, four-way reversal matrix stated. The design catches its own traps (§7.3, §7.4) rather than hiding them.
- **`kRailWidth`/`40.0f`/`radius_card` left frozen with rationale** — the highest-risk drive-by edits are explicitly forbidden for this tranche.
- **Sequencing humility**: single-agent preference over parallel file splitting, empty-table retention to keep scanner live, and glob deferral with reason — all the right granularity calls.

## 11. Scratch files created
none (read/grep only; this report is the sole write in WRITE-SET).

## 12. Verdict restatement
**APPROVE WITH CHANGES** — the design is fundamentally sound (inventory reconciles, values are same-value, freeze is honored, test approach is correct in shape). The four Major items are bounded, mechanical closures (decide token-set scope, exhaust the stage-2 list, fix the spelling/check asymmetry, fix revert order), not redesigns. On those changes alone, implement as designed with Option A. **No BLOCKER/CRITICAL issue; no pixel-affecting defect; no frozen-header violation.** Rejecting the design would cost a full rework cycle for a text-level patch.
