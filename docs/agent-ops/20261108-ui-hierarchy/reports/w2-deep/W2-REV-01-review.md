<!-- ORCH-DIGEST:START -->
agent: W2-REV-01 | scope: wave2 tranche-2 (9 tokens, stage-2 migration, file-local hoists) ea04e1a..HEAD | verdict: APPROVE-WITH-CHANGES
findings: BLOCKER 0 | MAJOR 0 | MINOR 1 | NIT 2
claims_audit: 14 checked / 1 unsupported (P-006 pixel proof, honestly flagged by implementer)
band_aids_found: none (no error swallowing, no test weakening, no nearest-value mapping; file-local hoists are the scanner's sanctioned escape, not a weakening)
top_3_risks: (1) wrong-token swap (12<->10) passes lexical ban+pins, caught only by reviewer audit + pending P-006 screenshots; (2) dual-use font_heading_px couples heading and IconButton glyph sizes; (3) single space_xs native borrow is documented but still cross-palette coupling
optimizations: DO-NOW 0 | DO-LATER 5 | NOT-WORTH-IT 2
reversal_check: done (reversal logic traced + implementer probes cited + scanner/pin mechanism verified in code; no source mutated by reviewer)
confidence: high
last_comms_line_read: [2026-11-08T05:45:00Z] W2-IMPL-01 NEED T-003 (tail read at start; no later lines in file at review time)
<!-- ORCH-DIGEST:END -->

# Code review — W2-REV-01 — Wave 2 tranche-2 diff (ea04e1a..HEAD)

## 0. Method
Brief `briefs/W2-REV-01.md` then required reading in order: STANDING-ORDERS (`templates/STANDING-ORDERS.md:1-45`), `AGENTS.md` UI section, `00-mission.md`, design `W2-DEEP-01-design.md`, critique `W2-PLAN-01-review.md` (D-W2-01..04 closures in git log: `3e0ebfb` full-9 + rollback), diff `git diff ea04e1a..HEAD -- src/ui/ tests/` + headers/`.cpp` edits, THEN implementer reports `W2-IMPL-01-report.md` + `W2-IMPL-02-report.md` + gate `gate-w2-1.md`. Blind pass (diff + headers + grep, before reading implementer reports) is reported in §1; claims audit in §2. Read-only on source/tests; only write is this report. No source mutated by reviewer; reversal probed by reasoning + implementer's recorded probes, not by re-mutating the shared tree.

Commands run this session [RAN]:
- `git branch --show-current` → `ops/20261108-ui-hierarchy`; `git log --oneline ea04e1a..HEAD` → `f3ba1e5` (code), `0dbe18c` (tests), `d088cf4` (W2-IMPL-02 flips), gate `867fe2e`.
- `git diff ea04e1a..HEAD -- src/ui/theme/ThemePalette.h src/ui/theme/ThemePalette.cpp src/ui/theme/StudioPalette.cpp` → empty (frozen headers untouched).
- `git diff ea04e1a..HEAD -- src/ui/` filtered for `Apply*Style` → no matches (no applicator touch).
- Greps: `ALLOWLIST (Wave 2 T-003)` in `src/ui/` → 0; `PushFont(nullptr, [0-9]` in `src/ui/` → 0; `metrics::space_xs` in `src/ui/pages/` → 1 code site (`TransformsPage.cpp:577`) + 1 comment (`:576`); hoists `kStatusDotIndent|kSearchWidth|kDialogInputWidth` → only `MainWindow.cpp`, `DictionaryPage.cpp`, `ScratchpadPage.cpp`.
- Same-value audit script (130 added `studio_metrics::*` lines vs preceding removed literals/tokens): 119 directly confirmed; 11 flagged with empty preceding-removed (script artifact on interleaved `-Dummy/+PushFont` hunks) — all 11 manually re-verified in full diff (Snippets `font_card_title_px` sites each pair with removed `metrics::font_card_title_px`; Transforms `gap_inline/gap_section/font_helper_px` sites pair with removed `metrics::space_s`/12.0f/`metrics::font_body_px`). **Result: 0 confirmed wrong-value mappings.**
- `cmake --build build --target lwf_tests --parallel 4` → Built target; `.\build\lwf_tests.exe -tc="*layout*,*tokens follow the shape scale*,*snippets*,*transforms*"` → 23 cases, 263/263 assertions SUCCESS; `ctest --test-dir build -R lwf-tests` → 1/1 Passed 19.81s; `bash scripts/check_hygiene.sh` → OK; `bash scripts/check_no_network.sh` → OK. G3 zig pending (no zig on dev box, per AGENTS.md).

## 1. Blind-pass findings (before implementer reports)
1. **Palette freeze holds.** `ThemePalette.h/.cpp` and `StudioPalette.cpp` have empty diff; the only header change is additive `StudioPalette.h` (+27 lines, see §2). No color/contrast input changed — spacing/font-size tokens do not enter luminance (`ThemePalette.h:141-159` structure), so no mockup regen duty arises. `ApplyStudioStyle`/`ApplyImGuiStyle` call sites: no diff lines mention them — dual-palette recolor does **not** occur in this tranche.
2. **Tokens are additive only.** `src/ui/theme/StudioPalette.h:66-85` adds exactly 9 constants (`gap_micro 6`, `gap_inline 8`, `gap_compact 10`, `gap_section 12`, `gap_block 16`, `font_card_title_px 15`, `font_heading_px 24`, `font_subheading_px 17`, `font_glyph_px 10`); zero existing values modified (diff shows only `+` lines in that region plus the `radius_card` warning comment `:43-45`, comment-only).
3. **Stage-2 migration is complete in the 8 studio files.** `Display.cpp` 5 sites (`font_card_title` x3, `font_page_title` x2), `Buttons.cpp` 2 (`font_heading` glyph + `font_card_title`), `TransformsPage.cpp` ~82 site edits; `Overlays.cpp` correctly untouched (no diff — already studio vocabulary). `MainWindow.cpp` native `metrics::*` usage correctly **retained** (native file, not migrated).
4. **File-local hoists stay file-local.** `kStatusDotIndent=14.0f` in `MainWindow.cpp:62` (anonymous/file scope, 2 use sites `:562,:932`); `kSearchWidth=240`/`kDialogInputWidth=360` in `DictionaryPage.cpp:22-23` and `kSearchWidth=240` in `ScratchpadPage.cpp:21` — all inside anonymous namespaces (`ScratchpadPage.cpp:13-23`, `DictionaryPage.cpp:14-23`), so no global token pollution. Trap "14 hoist stays file-local" satisfied.
5. **Structural literals preserved.** `kRailWidth 220` (`MainWindow.cpp:58` area, no diff on that line), `40.0f` rail rows, `pill_h + 18.0f`, button-width `+28/+32` models: no diff lines touch them (filtered diff confirms). `radius_card` collision documented in `StudioPalette.h:43-45` only; no radius value changed.
6. **`space_xs` escape hatch not inflated.** Exactly one code site keeps the native token: `TransformsPage.cpp:577` `Dummy(..., metrics::space_xs * scale)` with an intentional-reuse comment at `:576`; the header comment at `StudioPalette.h:63-65` documents the coupling ("do not edit either side expecting the other not to move"). No other `metrics::space_*` remains in `src/ui/pages` + `src/ui/widgets` (grep shows only this site + MainWindow/Overlay native files).
7. **Tests are not weakened — they are strengthened.** Empty `kBanAllowances` (was 17 entries, 32 caps) makes the ban strictly stronger: any new digit literal now violates with no ceiling to hide behind. Negative legs rewritten from cap-dependent (`dummy_safe` empty-by-cap) to 3 explicit legs (literal 12 violates / tokened empty / universal empty) — the tokened-empty leg alone would be vacuous (scanner exempts `studio_metrics::` lines), but the literal-violates leg restores the digit path, exactly the W2-PLAN-01 Minor-1 fix. Flip checks broadened to both `studio_metrics::font_` and `lwf::ui::studio_metrics::font_` spellings (W2-PLAN-01 Major-3 fix) and native proof added for MainWindow (Minor-2 fix).
8. **No band-aids.** No error swallowing, no sleeps/retries, no test deletions (all flipped CHECKs assert presence of the new vocabulary; digit bans retained), no hardcoded test inputs.

## 2. Claims audit (implementer claims vs code)
| # | Claim | Verdict | Evidence |
|---|---|---|---|
| 1 | 9 additive tokens, same-value 6/8/10/12/16 + 15/24/17/10 | TRUE | [VERIFIED `src/ui/theme/StudioPalette.h:66-85`] values match literals inventoried in design §1 |
| 2 | `grep PushFont digit → 0`, `ALLOWLIST → 0`, `metrics::font_` pages+widgets → 0 | TRUE | [RAN greps this session: 0 / 0; anchored `metrics::font_` in pages+widgets 0 (MainWindow native retained intentionally)] |
| 3 | Ban table 17 entries → empty, 26 comments → 0 | TRUE | [VERIFIED `tests/test_overlay_settings_ui.cpp:1147` `kBanAllowances[] = {}`] [RAN grep ALLOWLIST → 0] |
| 4 | 9 token pins + flip + negative rewrites land | TRUE | [VERIFIED `test_overlay_settings_ui.cpp:739-747` 9 CHECKs] [VERIFIED `:1290-1330` flip incl. MainWindow `has_metrics_font`] [VERIFIED `:1332-1380` 3 dummy legs] |
| 5 | `cluster_w`/`cursor_x` parity digits migrated (10→compact, 12→section) | TRUE | [VERIFIED `TransformsPage.cpp` cluster_w/cursor_x hunks: `10.0f→gap_compact`, `12.0f→gap_section`, `space_s→gap_inline`] |
| 6 | Scratchpad global-namespace resolves via alias | TRUE | [VERIFIED `ScratchpadPage.cpp:18` `namespace studio_metrics = lwf::ui::studio_metrics;`] + full `lwf_tests` link green |
| 7 | Structural stays literal (40/rail/pill_h+18/+28+32) | TRUE | [VERIFIED filtered diff: no touches] |
| 8 | `space_xs` single documented reuse | TRUE | [VERIFIED `TransformsPage.cpp:576-577` + `StudioPalette.h:63-65`] |
| 9 | Full `local-wisper-flow` target links | TRUE (per gate) | [VERIFIED `gate-w2-1.md:7` Built target] — reviewer did not re-link full exe (scoped to `lwf_tests` per brief §7); `lwf_tests` link green is the scoped proof |
| 10 | ctest 520/520 after W2-IMPL-02 flips | TRUE | [RAN `ctest -R lwf-tests` → 1/1 Passed 19.81s] [RAN scoped run 263/263 SUCCESS] |
| 11 | Fail-before: 518/520 → 520/520 on W2-IMPL-02 flips | TRUE | [VERIFIED `W2-IMPL-02-report.md:44-49` commands + diff `d088cf4` flips exactly the 2 needles] |
| 12 | Reversal probes (ban literal + pin value) each fail exactly one CHECK | TRUE (reasoned + cited) | Mechanism verified in scanner/pin code (`test_overlay_settings_ui.cpp` token exemption + `CHECK(metrics::gap_section == 12.0f)`); implementer's recorded outputs at `W2-IMPL-01:172-176`. Reviewer did not re-mutate shared tree (read-only). |
| 13 | Design §8.2 claim "pin flips already landed Wave 1" is false | TRUE | [VERIFIED `ea04e1a` state via `d088cf4` diff: `test_snippets_page.cpp`/`test_transforms_page.cpp` still required `metrics::font_` pre-flip] |
| 14 | Pixel identity (A10) | TRUE algebraically; UNSUPPORTED visually | Value proof + this review's 130-line audit cover mapping; **no screenshots run** (implementer honestly states P-006 not run; gate-w2-1 §14 "still owe"). See R-W2-REV-001. |

Claims checked: 14. Unsupported: 1 (visual pixel proof — disclosed, not hidden).

## 3. Long-run test (STANDING-ORDERS §8)
- **Source:** Ban TEST_CASE (empty allowlist) + 9 value pins (`studio palette layout tokens follow the shape scale`) + flipped `has_studio_font`/`has_metrics_font` checks + negative scanner TEST_CASE (digit/font, universals, `-1.0f` sentinel, 3 dummy legs). Deterministic, no ImGui link, ms-scale. `REQUIRE_FALSE(content.empty())` aborts loudly on missing source. [RAN: 23 cases 263/263 SUCCESS].
- **Load (100x / concurrency / partial failure):** N/A — pure compile-time spelling; `inline constexpr float` constants, zero runtime delta, `*scale` products identical. Partial failure mode is a missing source file → loud `REQUIRE_FALSE`, never silent pass.
- **Hostile input:** N/A — no user-input/format/path handling touched. The only "hostile" case is a future digit literal smuggled into an unlisted file; mitigation is the explicit `layout_sources` list + "new sources must be added here" comment (`test_overlay_settings_ui.cpp:1257-1258`) + per-file G3 type-check. Scanner deliberately does not cover `ImDrawList` geometry, `PushStyleVar` padding, or button-width arithmetic (documented, design §6).
- **Operability (observe / roll back / migrate):** Observe via P-006 screenshots (960×640/760×520 × DPI 1.0/1.5 + editor window) — **not run** (see §4 R-W2-REV-001). Roll back: revert **test commit first, then code commit** (`git revert <test> <code>`; D-W2-04 closure) — never code alone, or pins reference missing symbols. No data/config migration (compile-time spelling only); revert reproduces Wave-1 caps byte-identical because values never moved.
- **Reversal (which test fails if the fix is reverted?):** Revert any token to its literal → ban TEST_CASE fails (empty table, no cap). Change any token value → pin TEST_CASE fails on that exact CHECK. Reintroduce `metrics::font_*` on a studio page → flipped `has_studio_font` fails (total revert) — the 2 pre-W2-IMPL-02 ctest failures (518/520) prove this leg is load-bearing. Revert a hoist to a literal → ban fails (identifier-anchor loss). Wrong-token swap (12↔10) → **no automated test fails** (scanner exempts tokened lines; pins prove values, not mapping) — this is the known residual, mitigated by reviewer audit + P-006. All other reversals are covered; implementer's scripted probes confirm ban + pin legs.

## 4. Findings
### R-W2-REV-001 [MINOR] P-006 screenshots still pending — pixel gate unproven
**Problem:** Algebraic same-value proof + this review's 130-line audit establish mapping correctness, but the lexical scanner cannot detect a wrong-token swap (12↔10, 24↔28) — design §9.1 and W1-REV-01 §3 both state this residual explicitly. No before/after screenshots exist at either window size or DPI, so the visual backstop has never run for this tranche.
**Consequence:** A 2px header-cluster or legend shift would ship green. Probability is low (audit found 0 swaps), but the gate is the only guard that does not depend on reviewer diligence.
**Evidence:** [VERIFIED `gate-w2-1.md:14` "still owe manual Windows run"; `W2-IMPL-01:39,211` "not run"]. [RAN audit: 0/130 confirmed swaps].
**Fix:** Run P-006 (960×640 + 760×520, DPI 1.0 + 1.5, Home/Settings/Welcome + 5 studio pages + Scratchpad editor) and attach to the gate ledger before merge. No code change. If the Windows box is unavailable, ORCH must explicitly record "pixel gate waived on algebraic proof + reviewer audit" rather than leaving it silently owed.

### R-W2-REV-002 [NIT] Code/test commit trailers omitted
**Problem:** `f3ba1e5` and `0dbe18c` commit messages omit the required `Task:/Agent:/Wave:/Report:` trailers (STANDING-ORDERS §15). History is not rewritten (`--amend` forbidden); the omission is self-reported in `W2-IMPL-01:242-244`.
**Consequence:** None functional; process traceability only.
**Fix:** None in history. Noted for future tranche commits.

### R-W2-REV-003 [NIT] Dual-use coupling: `font_heading_px` serves headings + IconButton glyph
**Problem:** `Buttons.cpp:123` IconButton glyph (24px optical size in a 32px hit box) shares `font_heading_px` with Usage/Streak headings. A future glyph resize would move headings and vice versa.
**Consequence:** Low today; divergence cost is one constant later.
**Evidence:** [VERIFIED `src/ui/theme/StudioPalette.h:79-81` documents the dual use and the split rule; design §3 offered the split and was approved as single].
**Fix:** None now — documented and approved. Split into `font_icon_px` only if glyph sizing diverges (header records the rule).

No BLOCKER or MAJOR findings. No wrong-value mapping, no palette-freeze violation, no `Apply*Style` touch, no structural-literal mishandling, no test coupling/weakening was found.

## 5. Attempts to break (all failed — code holds)
1. **12↔10 swap:** Searched every `gap_section`/`gap_compact` migration against its removed literal. All section sites pair with 12.0f (or `space_m` 16 for `gap_block`); all compact sites pair with 10.0f. `cluster_w`/`cursor_x` parity digits migrate in lockstep with their rendered `SameLine`s (TransformsPage header/My-Transforms hunks). No swap found in 130 added token lines.
2. **24↔28 / 15↔13 / 17↔10 font swaps:** Headings pair 24.0f→`font_heading_px`; Recents 17.0f→`font_subheading_px`; glyphs 10.0f→`font_glyph_px`; 15s pair `metrics::font_card_title_px`→studio twin; 13s pair `metrics::font_body_px`→`font_helper_px`; 28s pair `metrics::font_page_title_px`→studio twin. 28px sites (Transforms 5, Display 2) are the one design-enumeration gap (W2-PLAN-01 Major-2) — implementer correctly migrated them under the stage-2 "zero `metrics::font_*`" rule rather than leaving a mixed vocabulary.
3. **Metrics reappear:** Anchored greps for both `metrics::font_` and `lwf::ui::metrics::` PushFont forms in pages+widgets → 0. A future reintroduction fails the flipped `has_studio_font` checks (total) — mechanism proven by the 518/520 pre-flip state.
4. **Palette freeze violation:** `ThemePalette.h/.cpp` diff empty; `StudioPalette.h` diff is pure addition. Contrast pins untouched and unaffected by construction.
5. **Apply*Style touch / dual-palette recolor:** No diff lines reference `ApplyStudioStyle`/`ApplyImGuiStyle`; `ScratchpadWindow.cpp`/`Overlay.cpp` untouched (out-of-scope Windows files correctly excluded).
6. **Structural literal mishandling:** `kRailWidth`, `40.0f`, `pill_h+18`, `+28/+32` button models, row insets — none appear in the diff as changed lines. `kStatusDotIndent` hoist is file-local (anonymous namespace) and value-identical (14.0f).

## 6. Checklist
- [x] Tokens additive only; no existing value changed (`StudioPalette.h` pure `+`)
- [x] `ThemePalette.h` frozen; `Apply*Style` untouched; no palette/unification recolor
- [x] Same-value mapping audited (130 lines, 0 swaps)
- [x] File-local hoists stay file-local (14/240/360, anonymous namespaces)
- [x] `space_xs` documented reuse at exactly 1 site, header + site comments
- [x] Structural literals (40/rail/widths/pill_h+18/+28+32) preserved
- [x] Tests strengthened (empty ban, 9 pins, flip both spellings, 3 dummy legs, MainWindow native proof)
- [x] Scoped tests green (263/263) + `ctest -R lwf-tests` 1/1 + hygiene + no-network OK
- [ ] P-006 screenshots (R-W2-REV-001 — manual Windows, outside code review's power to supply)
- [x] No band-aids; no test weakening; no swallowed errors

## 7. Optimizations
| # | Item | Class | Notes |
|---|---|---|---|
| O1 | `layout_sources` explicit list → glob + exclusion filter | DO-LATER | Deferred correctly; needs 4 exclusions (Overlay, ScratchpadWindow, ThemePalette/StudioPalette internals) + `<filesystem>` sorting. Standalone test-improvement item, not a token dependency (design §7.5). |
| O2 | Split `font_heading_px` into heading + glyph tokens | DO-LATER | Only if glyph sizing diverges; header already records the rule (R-W2-REV-003). Zero pixel change either way. |
| O3 | 10th token for the 4px `space_xs` site | DO-LATER | Would close the last cross-palette borrow — a DEEP decision, not a tranche-2 add (approved set is 9). |
| O4 | Centralize button-width chrome (+28/+32 vs `FramePadding (16,8)`) | DO-LATER | W1-REV-01 DO LATER item; needs a `button_padding` token + widget API decision. Unscanned assignments, correctly left literal here. |
| O5 | Hoist 14px row insets / `pill_h+18` / TabRow `12.0f` hit formula to named constants | DO-LATER | Hygiene only; unscanned geometry, no cap at stake. |
| O6 | Nearest-value mapping to existing tokens | NOT-WORTH-IT | Rejected (Option C): shifts pixels under a green test — the exact failure this tranche exists to prevent. |
| O7 | Extend `metrics::*` with studio sizes | NOT-WORTH-IT | Rejected (Option B): edits the frozen mockup header and falsely claims mockup provenance for screenshot-derived sizes. |

## 8. Cross-agent interactions
- W2-IMPL-01 → W2-IMPL-02 handoff is clean: NEED line (2 read-only test flips) was the sole acceptance gap; W2-IMPL-02 closed exactly those 2 needles + 1 comment (`d088cf4`), with fail-before 518/520 → pass-after 520/520. This review independently re-ran the pass-after state (green) and verified the flip diffs mirror the approved `has_studio_font` lambda exactly, including both spellings.
- W2-PLAN-01's 4 MAJOR items: Major-1 (7-vs-9) closed by D-W2-01 full-9; Major-2 (exhaustive inventory) closed by implementer's full grep (45 font + 46 spacing sites, incl. unenumerated 28px sites); Major-3 (qualification) closed by both-spellings `has_studio_font`; Major-4 (rollback order) closed by D-W2-04 test-then-code. Minors 1/2/6 closed in the test diff; Minor-4 (glob) correctly deferred; Minor-7 (screenshots) remains as R-W2-REV-001.
- Sibling W2-TREV-01 (test reviewer) runs in parallel; this code review's test-coupling conclusions (§1.7, §5.3) are stated from direct diff/test reading and do not depend on TREV's verdict.

## 9. Unverified
- P-006 visual pixel identity (R-W2-REV-001) — algebraic + audit proof only.
- Full-exe `local-wisper-flow` link re-run — relied on `gate-w2-1.md:7` + implementer report; reviewer scoped to `lwf_tests` per brief §7 (link of `lwf_tests`, which compiles the same translation units' headers, is green).
- G3 zig Windows type-check — pending, no zig on dev box (AGENTS.md); ScratchpadPage global-namespace alias resolution is proven by the successful `lwf_tests` link, not by zig.
- Reversal probes — mechanism verified in code + implementer's recorded probe outputs; reviewer did not re-mutate the shared tree (read-only per brief).
- Last COMMS line: `[2026-11-08T05:45:00Z] W2-IMPL-01 -> ORCH | NEED | T-003` (read at start of this session; file tail at that line).
