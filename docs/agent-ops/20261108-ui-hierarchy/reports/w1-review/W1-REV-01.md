<!-- ORCH-DIGEST:START -->
agent: W1-REV-01 | scope: wave 1 (T-001, T-002; T-004 tests as context only) | verdict: APPROVE
findings: BLOCKER 0 | MAJOR 0 | MINOR 1 | NIT 3
claims_audit: 19 checked / 2 unsupported-or-overstated
band_aids_found: none
top_3_risks: P-006 screenshots still pending (pixel-identity unproven visually) | lexical ban cannot catch wrong-value token swap | Transforms header measurement/layout now split token-vs-literal
optimizations: DO-NOW 3 | DO-LATER 4 | NOT-WORTH-IT 3
reversal_check: done
confidence: med
<!-- ORCH-DIGEST:END -->

# Code review — W1-REV-01 (wave 1, code lens)

Scope: `git diff 2c06061..HEAD -- src/ui/` (9 files: MainWindow.cpp + 5 pages + Buttons/Display/Overlays.cpp) [VERIFIED git diff --name-only 2c06061..HEAD -- src/ui/]. READ-ONLY on source; this report is the only file written. Required reading (blind-first, in brief order): STANDING-ORDERS.md, AGENTS.md UI + Build, 00-mission.md, mission plan decisions (hierarchy & density, keep tokens, src/ui only — 00-mission.md:15-33 + ledger P-001/P-002), the diff above, code in context (MainWindow.cpp, pages, widgets, ThemePalette.h, StudioPalette.h, ScratchpadWindow.cpp), THEN implementer reports W1-IMPL-01-T-001.md + W1-IMPL-02-T-002.md, then COMMS.md. W1-IMPL-03-T-004.md read only to understand scanner/cap interplay, not to judge tests (W1-TREV-01 owns the test lens). W1-TREV-01 report file at reports/w1-review/W1-TREV-01.md was NOT opened (sibling isolation); only COMMS headline line 30 was seen after the blind pass. COMMS last line read before finishing: [VERIFIED COMMS.md:30] W1-TREV-01 -> ORCH | DONE | T-004.

Wave base per brief: WAVE_BASE_SHA 2c06061 [VERIFIED briefs/W1-REV-01.md:6]. Ledger lists aecb917 as pre-wave; 2c06061 is the post-approval commit, correct diff base [VERIFIED git log --oneline 2c06061..HEAD].

## 0. Method

Blind pass first (diff + tokens + context, before opening implementer reports), then claims audit (each implementer claim vs HEAD code), then independent typed claims (`[VERIFIED path:line]`, `[RAN <cmd> -> <result>]`, `[ASSUMED]`, `[UNKNOWN]`). Scoped checks only: grep/read, git show/diff, and single TEST_CASE runs of the existing `build/lwf_tests.exe` binary (no rebuild, no full suite, no source writes). No screenshots taken (P-006 is manual Windows, outside this read-only scope). No zig (G3 pending per AGENTS.md). Native-vs-studio token values read from headers every time, never from memory.

What was proven without builds (by construction): every retoken map is same-value by header constants, so `token * scale == literal * scale` for all scales — pixel identity follows algebraically once the header values are verified equal. What still needs visual proof: P-006 before/after screenshots (gate-w1-1.md notes them pending). Build proof at wave level comes from the orchestrator gate, not from this review.

## 1. Blind-pass findings (before implementer reports)

1. **All 8-family mappings are same-value, no 13→15 / 15→13 / 16→24 swaps in the diff.** Token values: metrics::space_xs=4, space_s=8, space_m=16 [VERIFIED src/ui/theme/ThemePalette.h:85-87], control_min_height=32 [:97], font_card_title_px=15 [:105], font_body_px=13 [:106]; studio_metrics::font_helper_px=13 [VERIFIED src/ui/theme/StudioPalette.h:58], font_page_title_px=28 [:57]. Mapped pairs in the diff: 8→space_s, 16→space_m, 4→space_xs, 32→control_min_height, 15→font_card_title_px, 13→font_helper_px (studio pages/widgets) and 13→font_body_px (TransformsPage only, same 13). No cross-value pair found in the PushFont/SameLine/Dummy hunks. Theme headers untouched [VERIFIED git diff --name-only: no src/ui/theme/ file in the 9] — token freeze honored.
2. **ALLOWLIST sites are all true token gaps, not forced mappings.** 26 `// ALLOWLIST (Wave 2 T-003)` comments, each immediately above a literal [VERIFIED git diff ALLOWLIST context]. Residual digit literals in the 9 files are exactly: PushFont 24/17/10 (no 24/17/10 font token exists; 24 is space_l spacing, 12 is radius_card — wrong role), gaps 12/10/6 (no 12/10/6 spacing token; neighbours are 4/8/16), and structural zeros `SameLine(0,0)`/`Dummy(0,0)` (0 has no token; 0→4 would shift pixels). No allowlist site has a same-value token it dodged. ScratchpadWindow.cpp SameLine 4/6 literals and mock-note gap expression are outside the 9-file write-set — out of scope for Wave 1, not a miss.
3. **Paired measurement/layout arithmetic now split token-vs-literal (Transforms header).** `DrawKeycapRow` SameLine uses the new token while `KeycapRowWidth` (TransformsPage.cpp:105) still adds `8.0f * scale` raw; `cluster_w` at :189-191 duplicates `10/8/12/8/12 * scale` raw while the rendered SameLines use tokens/allowlisted literals; and the Keycap measure at :107 uses `metrics::font_body_px` while the Keycap widget (Buttons.cpp:154,166) renders `studio_metrics::font_helper_px`. Values are equal today (8=space_s, 13=13), so no pixel change — but the two sides are no longer spelled the same, so any future token edit silently misaligns the header. Details in R-W1-001. Pre-existing button-width arithmetic (`+28` at :186 vs StyleButton FramePadding 16×2, `+32` at :300/304 matching 16×2) is untouched by this wave; not a regression.
4. **No ImGui context / style regression in the diff.** No `CreateContext`/`SetCurrentContext`/`Apply*Style` lines touched; PushFont/PopFont statically balanced in every touched file (TransformsPage 37/37 [VERIFIED grep PushFont vs PopFont], Dictionary 5/5, Display 16/16, Buttons 8/8 with TabRow early-return before Push, Overlays 4/4; Snippets TruncateToWidth Push-once/Pop-on-three-paths verified path-by-path — each return pops [VERIFIED SnippetsPage.cpp:28-74]). All spacing replacements keep `* scale`; all PushFont replacements correctly keep no `* scale` (DPI via FontScaleMain, per ThemePalette.h:161-168). ScratchpadWindow editor provably unaffected (see §8: it calls no changed widget and no style file changed).
5. **RowList retokenization is inert (dead code).** `widgets::RowList` has zero callers in src/ [VERIFIED grep RowList in src/ → only Widgets.h:24 declaration + Display.cpp:25 definition]. The three 15→font_card_title_px edits inside it (Display.cpp) cannot render anywhere. Harmless, but it means P-003 widget coverage via screenshots cannot include RowList.

## 2. Claims audit (implementer claims vs HEAD code)

19 claims checked. 17 supported. 2 unsupported-or-overstated (no pixel impact; documentation accuracy only).

### T-001 (W1-IMPL-01) — 8 claims

- [C1] "1 code file (MainWindow.cpp), .h unchanged" — SUPPORTED [VERIFIED git diff --name-only includes only src/ui/MainWindow.cpp from T-001; MainWindow.h absent].
- [C2] "Same-value substitutions verified against ThemePalette.h values" — SUPPORTED. All 7 sites re-verified value-equal: h 32→control_min_height(32), pad 4→space_xs(4), bottom_h 8+32→space_s+control_min_height, switch_h 32→control_min_height, SameLine 8→space_s (×2), tab_pad 16→space_m [VERIFIED MainWindow.cpp hunks + ThemePalette.h:85-97]. kRailWidth 220 + D-rail comment correct: rail_width token is 208, so leaving 220 local is the locked decision (00-mission.md:31).
- [C3] "PushFont digits 0 before → 0 after" — SUPPORTED [VERIFIED grep PushFont(nullptr, [0-9] in MainWindow.cpp → 0].
- [C4] "SameLine 2→0; Dummy 2→2 (14.0f allowlist)" — SUPPORTED [VERIFIED grep SameLine digits in MainWindow.cpp → 0; Dummy(14.0f) at :558 and :928 unchanged]. The 14.0f sites have no native token (studio activity_cell/gauge_stroke 14 is wrong palette) — leaving them was correct.
- [C5] "40.0f pin + kRailWidth preserved" — SUPPORTED [VERIFIED tests/test_overlay_settings_ui.cpp:663-664 still required; code still contains both].
- [C6] "lwf_tests does NOT compile MainWindow.cpp, verified by token-member read-through" — SUPPORTED and honestly disclosed [VERIFIED CMakeLists.txt:138-195 lwf_tests sources contain no src/ui/*.cpp; src/ui/MainWindow.cpp enters only via the full-exe target at :366]. At report time compile was unverified; closed later by gate (gate-w1-1.md:8 full exe Built).
- [C7] §11 "7 literals retokenized" / COMMS "7 literals" — IMPRECISE (NIT, R-W1-004). The diff replaces 8 literals on 7 lines (bottom_h carries two literals on one pre-wrap line). No value error; count counts lines, not literals.
- [C8] "No ApplyStudioStyle/ApplyImGuiStyle line changed; hygiene-safe comment" — SUPPORTED [VERIFIED diff has no Apply* hunk; added comment contains no TODO/FIXME/windows.h; gate hygiene OK].

### T-002 (W1-IMPL-02) — 11 claims

- [C9] "8 files in write-set" — SUPPORTED [VERIFIED diff --name-only lists exactly those 8].
- [C10] "PushFont digits 37→6, all allowlisted (Insights 10/24, Scratchpad 17, Buttons 24)" — SUPPORTED [VERIFIED grep PushFont(nullptr, [0-9] over src/ui → 6 matches: InsightsPage.cpp:131,268,291,308; ScratchpadPage.cpp:190; Buttons.cpp:124].
- [C11] "Dummy/SameLine digits 64→20, all allowlisted" — SUPPORTED [VERIFIED grep SameLine digits in the 5 pages + widgets: Transforms 7 + Insights 2 = 9; Dummy digits: Dictionary 3 + Insights 1 + Scratchpad 3 + Snippets 2 + Transforms 2 = 11; total 20. Caps in tests/test_overlay_settings_ui.cpp:1136-1159 match these HEAD counts entry-by-entry].
- [C12] "No 13/15/8/16/4 digit literal remains in the three APIs" — SUPPORTED for the three APIs [VERIFIED residual literals are only 24/17/10/12/6/0]. Note the claim is scoped to PushFont/Dummy/SameLine; raw arithmetic literals (KeycapRowWidth:105, cluster_w) remain by design — see R-W1-001, disclosed in T-002 §13 for KeycapRowWidth but not for cluster_w.
- [C13] "Every mapped site same-value, so * scale products identical" — SUPPORTED (§1 table). No wrong-swap found.
- [C14] "ScratchpadPage metrics:: → lwf::ui::metrics:: namespace fix; full local-wisper-flow build clean" — SUPPORTED [VERIFIED ScratchpadPage.cpp:18 alias for studio_metrics, global scope, and qualified lwf::ui::metrics:: at 4 edited sites; gate-w1-1.md:8 Built local-wisper-flow]. Without this the build would fail on `metrics::` resolution — caught and fixed pre-merge, correctly disclosed.
- [C15] "ctest 517/518, sole failure is scratchpad 15.0f pin owned by T-004 flip" — SUPPORTED as a transient attribution [VERIFIED HEAD test_scratchpad_page.cpp:362 now requires 15.0f absent post-flip; pre-flip pin would necessarily fail after the literal removal. Wave gate green after T-004 (gate-w1-1.md:9, and independent single-case runs §3)]. Implementer correctly flagged it as caused-by-tranche, not pre-existing.
- [C16] "Hierarchy/tokens/scoped verification: hygiene OK" — SUPPORTED [VERIFIED gate-w1-1.md:10 hygiene OK; no windows.h in diff].
- [C17] "Shared widgets (RowList/Toast/Banner/Keycap/Chip/IconButton/ProgressPill) are used from ScratchpadWindow editor" — UNSUPPORTED/OVERSTATED (R-W1-003). Grep of ScratchpadWindow.cpp shows zero widgets:: calls; its font sites already use studio_metrics::font_helper_px unchanged (:445,466,605) [VERIFIED grep widgets::|PushFont in ScratchpadWindow.cpp]. RowList has no callers anywhere; Toast/Keycap/Chip callers are Dictionary/Snippets/Transforms pages only. AppliedStudioStyle appears only in ScratchpadWindow (00-mission/ledger note) but no style file changed in this wave. The editor is unaffected by a stronger argument (no shared call sequence + no style change) than the one claimed.
- [C18] "T-002 §13 out-of-scope: TransformsPage.cpp:105 arithmetic left as-is" — SUPPORTED as disclosure, but flagged as incomplete: it discloses KeycapRowWidth:105 yet omits the sibling cluster_w arithmetic at TransformsPage.cpp:189-191 and the cursor_x duplicate at :308 (same class; :308 duplicates an allowlisted 12, so no token alternative exists today). See R-W1-001.
- [C19] "ScratchpadPage ScratchpadWindow still needs screenshot check" — SUPPORTED as residual risk (not BLOCKER per brief). Values identical by construction; screenshot P-006 still pending manual Windows [VERIFIED gate-w1-1.md:13].

## 3. Long-run test (Source / Load / Hostile / Operability / Reversal)

- **Source:** The regression net is the lexical ban + flipped pins. Reintroducing a digit literal in PushFont/Dummy/SameLine/SetNextItemWidth produces a per-file violation or cap-exceed in `ScanLayoutLiterals` [VERIFIED tests/test_overlay_settings_ui.cpp:1174-1256; BanAllowance table :1136-1159]. Independent scoped runs at HEAD: [RAN build\lwf_tests.exe --test-case="full window layout APIs use tokens not digit literals outside the token-gap allowlist" -> 1 passed, 29 assertions passed]; [RAN --test-case="snippets page sources stay off the font scale ban and carry the brief copy" -> 1 passed, 41 assertions passed]; [RAN --test-case="scratchpad page source wires shell widgets and notes store" -> 1 passed, 36 assertions passed]. Font token values are value-pinned (font_card_title_px==15, font_body_px==13 [VERIFIED test_overlay_settings_ui.cpp:230-241]); spacing values are not value-pinned, and token-carrying lines are exempt from the literal scan — so a **wrong-value token swap** (e.g. code 13→font_card_title_px 15, or 8→space_m 16) passes every automated check and is caught only by this manual value audit + P-006 screenshots. That limitation is inherent to the lexical approach and is stated as residual risk, not hidden.
- **Load (100x, concurrency, partial failure):** N/A — pure compile-time spelling with `inline constexpr float` constants, no runtime object, allocation, ordering, or threading change. [ASSUMED no hidden runtime effect; the diff contains no new state, heap, or async surface.] Partial failure mode is a missing source file → `REQUIRE_FALSE(content.empty())` aborts loudly [VERIFIED test_overlay_settings_ui.cpp:1281-1283], never a silent pass.
- **Hostile input:** N/A — no user-input, string, format, or path handling changed. Geometry derives only from the file-level `scale` parameter, multiplied identically as before.
- **Operability:** Rollback is `git revert` of the two code commits (e262309, eb54183) — no migration, config, or data. Observability is visual: P-006 before/after at 960×640 + 760×520, DPI 1.0/1.5 must be pixel-identical (values frozen), including the ScratchpadWindow editor window. Those screenshots are **not yet captured** (gate-w1-1.md:13) — the only unproven operability leg.
- **Reversal:** Reverting any mapped token to its digit literal makes the ban TEST_CASE fail (violation for non-allowlisted values, or cap-exceed for allowlisted values). Reverting the 17.0f/10/24 or 12/6 allowlisted sites to any token is impossible without a visual shift — there is no token to revert to (Wave 2 T-003). Reverting the whole wave restores literals with zero value delta and keeps history/config untouched.

## 4. Findings

### R-W1-001 [MINOR] Paired measurement arithmetic left as raw literals (Transforms header)

Evidence: `DrawKeycapRow` SameLine uses `metrics::space_s * scale` [VERIFIED TransformsPage.cpp:95] while `KeycapRowWidth` adds `width += 8.0f * scale` [VERIFIED TransformsPage.cpp:105]; `cluster_w` duplicates the header gaps raw as `label_w + 10.0f*scale + info_w + 8.0f*scale + toggle_w + 12.0f*scale + caps_w + 8.0f*scale + caption_w + ...` [VERIFIED TransformsPage.cpp:189-191] while the rendered SameLines at :199 (10 allowlisted), :203 (space_s), :208 (12 allowlisted), :210 (space_s) use tokens/allowlisted literals; Keycap measure font `metrics::font_body_px` [VERIFIED TransformsPage.cpp:107] vs Keycap render `studio_metrics::font_helper_px` [VERIFIED Buttons.cpp:154,166]. Today values coincide (8=space_s, 13=13), so zero pixel delta — but the layout and its width model are now spelled differently where they were previously both literals, so a future token edit breaks alignment silently. T-002 §13 disclosed :105 but not :189-191. The `cursor_x` duplicate `(12.0f * scale + create_w)` [VERIFIED TransformsPage.cpp:308] pairs with an allowlisted 12 (no token exists), so it is unavoidable until Wave 2; the 8s are the actionable part.

Fix (DO NOW fast-follow, same value, zero pixel delta, one file): `TransformsPage.cpp:105` `8.0f * scale` → `metrics::space_s * scale`; `:189-190` both `8.0f * scale` → `metrics::space_s * scale`; `:107` `metrics::font_body_px` → `studio_metrics::font_helper_px` (matches the Keycap widget it measures). Leave 10/12 raw (allowlisted) until Wave 2 T-003 adds tokens. Why MINOR not MAJOR/BLOCKER: no current misrender, no data/control-flow risk; it is a latent-maintenance split, fixable in minutes whenever slots are available. It does not block merge.

### R-W1-002 [NIT] RowList retokenization touches dead code

Evidence: `widgets::RowList` declared Widgets.h:24, defined Display.cpp:25, zero callers in src/ [VERIFIED grep RowList in src/]. The three 15→font_card_title_px edits in Display.cpp therefore cannot affect any window. No action required for Wave 1; screenshots cannot cover it. Recommendation DO LATER: delete RowList or add a render path/test so the retokenized code is live (or keep as intentionally-dead API with a comment).

### R-W1-003 [NIT] T-002 blast-radius note overstates editor sharing

Evidence: ScratchpadWindow.cpp contains no widgets:: call [VERIFIED grep widgets::|RowList|Keycap|Chip in ScratchpadWindow.cpp → no match for calls; PushFont sites already use studio_metrics::font_helper_px at :445,466,605]. T-002 §6 claim that RowList/Toast/Banner/Keycap/Chip/IconButton/ProgressPill are "used from ScratchpadWindow editor" is thus unsupported as written. The correct statement is stronger for safety: the editor shares neither the changed call graph nor any changed style file (ThemePalette.cpp/StudioPalette.cpp untouched), so same-value widgets cannot shift it. No code change needed; correct the record and keep P-006 editor screenshot as belt-and-braces.

### R-W1-004 [NIT] T-001 literal-count wording (8 literals on 7 lines)

Evidence: diff replaces literals 32,4,8+32,32,8,16,8 = 8 literals across 7 pre-wrap lines [VERIFIED MainWindow.cpp diff hunks]. Report says "7 literals" (counts lines). No value error. Documentation-only.

No BLOCKER and no MAJOR found. In particular: no pixel-value change (all maps same-value per §1), no theme/palette value change, no scope creep beyond the 9 src/ui files (plus T-004 tests), no `windows.h`/OS-header/TODO hygiene breach, no file-write/network additions, no PushFont/PopFont imbalance, no scale-drop.

## 5. Attempts to break (all attempted, none succeeded as a wave breaker)

| Trap (per brief §3/§6) | Attempt | Result |
|---|---|---|
| Wrong same-value mapping (13→15, 15→13, 16→24) | Re-derived every hunk's old-literal→new-token against header constants | NOT FOUND. 13 maps only to 13-tokens; 15 only to 15; 16 only to 16; 8 only to 8; 4 only to 4; 32 only to 32. |
| Allowlist explosion | Enumerated all 26 ALLOWLIST comments + all residual digit literals + all 17 BanAllowance caps | Each allowlisted value has no same-value token; caps match HEAD counts exactly (verified §2 C11). 26 comments are 1:1 with 26 sites, not a blanket skip. |
| Dual-palette leak | Checked metrics::* vs studio_metrics::* per file, including radius_card name collision (metrics 10 vs studio 12) | No value leak: studio 15s correctly fall back to metrics::font_card_title_px (only 15 token); studio 13s use font_helper_px; no radius changes at all. Coupling note only (studio pages now depend on metrics::* until Wave 2 splits them). |
| Context/style regressions | Inspected Push/Pop pairing branch-by-branch + `* scale` retention + CreateContext/SetCurrentContext absence | No imbalance, no dropped scale, no context calls in diff. TruncateToWidth Pop-on-every-return verified. |
| Allowlist disguise (literal hidden behind token line) | Reviewed scanner exemption (`metrics::` substring skips the line) | No such line in the wave diff. The exemption is a future-risk channel (a line could carry both and escape), noted as scanner limitation, not a wave defect. |
| Editor-window visual regression via shared widgets | Traced ScratchpadWindow call graph + style files | No shared changed calls, no style change. Unaffected by construction; screenshot still required per brief. |

## 6. Checklist

- [x] Scope: src/ui/ only for code (ThemePalette.h/StudioPalette.h, Overlay.cpp, ScratchpadWindow.cpp, MainWindow.h untouched) + tests only via T-004 ownership. No engine/schema/UiBus/main.cpp.
- [x] Tokens frozen: no palette/metric value edits; rail 220 documented (D-rail), not retokened to 208.
- [x] Same-value rule: verified per-site against header constants.
- [x] PushFont/PopFont balance + `* scale` retention verified.
- [x] ImGui context invariant: no CreateContext/SetCurrentContext change; overlay untouched.
- [x] Hygiene/network/file-write: no windows.h, TODO, network, or write-API additions in diff; gate hygiene/no-network OK (gate-w1-1.md:10-11).
- [x] Security: no input/secrets surface touched.
- [x] Screenshots: P-006 NOT verified (manual Windows, pending) — recorded under §9, not claimed.
- [x] Dead code / cross-palette semantic misfits (knob 24→space_l, 12→16) consciously avoided by both implementers — correct.

## 7. Optimizations (code-structure only; no CPU claim — compile-time constants)

DO NOW (3) — fast-follow single commit, zero pixel delta:
- R-W1-001 fix: KeycapRowWidth gap + cluster_w gaps → `metrics::space_s`; Keycap measure font → `studio_metrics::font_helper_px`.

DO LATER (4) — Wave 2 T-003 / follow-up:
- Add tokens for the 26 allowlisted sites (12/10/6 gaps; 24/17/10 fonts; MainWindow 14 pad; SetNextItemWidth 240/360 stay structural) and replace them; include Transforms cursor_x 12 and Scratchpad expression `pill_h + 18.0f*scale`.
- Tokenize or centralize Transforms button-width models (`+28/+32`) from StyleButton FramePadding (pre-existing approximation, out of wave).
- Delete or exercise dead `widgets::RowList`.
- Split studio pages off `metrics::*` (introduce `studio_metrics::font_card_title_px`/spacing equivalents) and address the `radius_card` 10-vs-12 namespace collision before it bites.

NOT WORTH IT (3):
- Sweeping AddText paddings, icon geometry, knob/track sizes (52/24/20), tab_h 36, focus-ring 2px, hairline 1px into tokens this wave — no same-role tokens; high churn, unguarded by the 5-API scan, no visual gain.
- Forcing 12→16, 10→8, 24→28 etc. to shrink the allowlist — shifts pixels, violates same-value/frozen-token rule.
- Any visual "improvement" conflated with this normalization — mission success is pixel-identity plus hierarchy structure; P-006 must show null diff for mapped sites.

## 8. Cross-agent interactions

- T-001 (MainWindow.cpp) vs T-002 (8 files) write-sets are disjoint [VERIFIED ownership/diff name-only] — no merge hazard between the two code tranches; both preserve `40.0f` pin + kRailWidth.
- T-002's transient ctest red (scratchpad 15.0f pin) is the designed T-004 flip, not a regression: HEAD pins now require absence [VERIFIED test_scratchpad_page.cpp:362, test_snippets_page.cpp:342-343]. Gate green after all three landed [VERIFIED gate-w1-1.md:9 + §3 independent single-case runs].
- T-004 scanner caps are ceilings derived from HEAD (not comment-matching): the `// ALLOWLIST` comment style is therefore irrelevant to test outcome — T-002 §14's worry that the ban might not recognize the comment style is moot; the ban keys on file+api+value.
- Per brief §5, W1-TREV-01 runs in parallel (test lens); its report was deliberately not opened for this code lens. COMMS line 30 headline (APPROVE-WITH-CHANGES, 1 MAJOR, test-coverage gap) was seen after the blind pass and does not change the code verdict here; the substantive overlap is the value-correctness residual risk in §3 (lexical ban cannot catch a wrong-value token), which for the code lens is covered manually by this review + P-006, and for the test lens is a coverage item for the orchestrator to sequence. A code APPROVE with a test-lens follow-up item is not a contradiction.
- out_of_scope spelled correctly by both implementers: T-001 MainWindow leftover classes + T-002 expression/named-constant forms are accurately described; the only omission is T-002's cluster_w (R-W1-001), now covered.

## 9. Unverified / remaining risks

- [UNKNOWN visual] P-006 screenshots (960×640 + 760×520, DPI 1.0 + 1.5, before/after + Scratchpad editor) not captured in this review (read-only, manual Windows). Expected null diff for mapped sites by value proof; the gate must still produce them before declaring hierarchy proof.
- [UNKNOWN hardware] Real-font metrics: all PushFont measurements use design-px; with real TTFs the token equality still holds (constant==constant), but truncation/wrapping edge cases (long words, truncation budgets) were reasoned, not rendered.
- [UNKNOWN] G3 zig type-check pending (no zig on dev box, per AGENTS.md; record in docs/QUESTIONS.md). Full-exe compile is covered by gate; zig MSVC-compat type-check is not.
- [ASSUMED] Runtime `scale` values are positive DPI factors; `scale=0` would collapse both literal and token forms equally (0==0), so no divergence even then.
- [UNKNOWN] Future Wave 2 token edits will change values that studio pages now import via `metrics::*` (coupling noted in §7). The schedule for unifying/splitting belongs to the Wave 2 plan, not this wave.

## 10. Commands run by this reviewer (scoped only)

- `git branch --show-current` → ops/20261108-ui-hierarchy [VERIFIED]
- `git --no-pager diff 2c06061..HEAD -- src/ui/` (full blind read) + `git diff --stat`/`--name-only`, `git log --oneline 2c06061..HEAD`
- grep/read sweeps: PushFont/SameLine/Dummy digit literals; ALLOWLIST adjacency; PushFont/PopFont counts; RowList callers; widgets:: call graph in ScratchpadWindow; ThemePalette/StudioPalette token values; CMakeLists lwf_tests sources.
- `.\build\lwf_tests.exe --test-case="full window layout APIs use tokens not digit literals outside the token-gap allowlist"` → 1 passed, 29 assertions passed
- `.\build\lwf_tests.exe --test-case="snippets page sources stay off the font scale ban and carry the brief copy"` → 1 passed, 41 assertions passed
- `.\build\lwf_tests.exe --test-case="scratchpad page source wires shell widgets and notes store"` → 1 passed, 36 assertions passed
- NOT-RUN (out of scoped/read-only remit): full ctest, full rebuild, hygiene/no-network (gate-w1-1.md records them), zig (G3 pending), screenshots (P-006, manual Windows).
