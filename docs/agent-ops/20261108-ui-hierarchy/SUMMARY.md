# SUMMARY — 20261108-ui-hierarchy
Branch: ops/20261108-ui-hierarchy · Base: 8a6ea2255dd8b54d42371987456c5f53e784575f · Head: 3e70e85 (final reviewer docs) + this summary · Waves: 2 (+ 2.5 follow-up) · Agents: ~20 (4 Wave0 investigators, 3 Wave1 implementers, 2 Wave1 improvers, 2 Wave1 reviewers, 1 delta, 1 deep-design, 1 plan-critique, 1 tranche-2 implementer + 1 follow-up, 2 Wave2 reviewers, 1 final) · Dates: 2026-11-08

## 1. Outcome in brief
Wave 1 normalized ~137 same-value literals across Full-window pages/widgets to `metrics::*`/`studio_metrics::*` (zero pixel delta) and added a lexical literal-bypass ban that caps the 26 token-gap sites. Wave 2 added 9 additive `studio_metrics::*` tokens (4 fonts + 5 gaps), migrated all studio-page PushFont/spacing off `metrics::*` (zero `metrics::font_` in pages/widgets), emptied the BanAllowance table, flipped the test palette-split proof to studio fonts, and preserved every frozen palette value. All gates green locally (G1/G2/G4/G5, G3 zig pending). Independent code+test reviews closed with 0 BLOCKER/0 MAJOR. Remaining gate is P-006 before/after screenshots on real Windows (960×640/760×520 × DPI 1.0/1.5 + Scratchpad editor) — algebraic same-value proof stands in for pixels, manual capture still owed or explicitly waived.

## 2. Plan coverage
| Plan item | Task | Status | Commits | Report |
| P-001 MainWindow literals→metrics | T-001 W1-IMPL-01 | DONE | e262309 | w1-impl/W1-IMPL-01 |
| P-002 Studio pages literals→studio_metrics | T-002 W1-IMPL-02 tranche-1 + W2-IMPL-01 tranche-2 | DONE | eb54183, f3ba1e5 | w1-impl/W1-IMPL-02, w2-deep/W2-IMPL-01 |
| P-003 Widgets density consistency | T-002 (Buttons/Display/Overlays) + W1-IMPR-01 Transforms R-W1-001 | DONE | eb54183, 1f7a71d | w1-impl/W1-IMPL-02, w1-improve/W1-IMPR-01 |
| P-004 kRailWidth 220 vs rail_width 208 | D-rail leave+document (no retoken) | DONE (documented) | MainWindow.cpp comment (T-001) | w1-impl/W1-IMPL-01, W2 design |
| P-005 Source-scan tests for bypasses | W1-IMPL-03 ban+flips, W1-IMPR-02 negative test, W2 tranche empty table+pins+flip, W2-IMPL-02 2 pin flips | DONE | 8f09615, bbc8a14, 0dbe18c, d088cf4 | w1-impl/W1-IMPL-03, w1-improve/W1-IMPR-02, w2-deep/W2-IMPL-01/02 |
| P-006 Screenshots before/after | — | DEFERRED (manual Windows owed) | pending | gate-w1-1/w1-2/w2-1 notes pending |
| P-007 Gates + PROGRESS.md | orchestrator | DONE locally (this commit rows) | this summary commit | gates/gate-w1-1, gate-w1-2, gate-w2-1; PROGRESS.md UI-H1/H2/R rows |

## 3. Changes by domain
- UI native (src/ui/MainWindow.cpp): 7 lines retokenized (8 literals), 14px hoisted? (Transform cluster duplicates still literal-coupled 10/12); 40.0f + kRailWidth preserved
- UI studio pages (Dictionary/Insights/Snippets/Transforms/ScratchpadPage.cpp): 100 insertions/74 deletions Wave 1; tranche 2 migrates 26 allowlisted + ~45 PushFont + spacing grid to studio twins; palette split vocabulary unified (zero metrics::font_ in pages/widgets)
- UI widgets (Buttons/Display/Overlays.cpp): PushFont/Dummy/SameLine retokenized; Keycap font parity fixed (W1-IMPR-01)
- Theme (src/ui/theme/StudioPalette.h): 9 additive tokens (fonts 15/24/17/10, gaps 6/8/10/12/16); ThemePalette.h untouched, StudioPalette.cpp untouched, Apply*Style untouched
- Tests: 4 files (test_overlay_settings_ui + test_scratchpad_page + test_snippets_page + test_transforms_page) — lexical ban, empty table, 9 pins, negative scanner test (6 CHECKs), palette flip (has_studio_font broadened + MainWindow native proof), stale pin flips

## 4. Deep-Dive Spotlight
T-003 (W2-DEEP-01 design + W2-IMPL-01 implementation): token-gap decisions for 24/17/10 fonts and 12/10/6 gaps. Alternatives: A (additive studio_metrics, same-value) CHOSEN, B (extend frozen ThemePalette.h, mockup-provenance pollution) REJECTED, C (nearest-value shift, silent pixel drift) REJECTED unconditionally. Risks mitigated: wrong-token swap invisible to lexical ban (P-006 screenshot backstop + value-pin), file-local hoists for 14/240/360 (scanner-exempt by design), structural literals stay (40/rail/+28/+32/pill_h+18). Rollout: 2 commits code-then-tests, revert order tests→code (D-W2-04). Follow-ups: space_xs documented reuse (no 10th token), 16-vs-20 card-pad drift (froze values), RowList dead code, glob expansion. Report: reports/w2-deep/W2-DEEP-01-design.md + W2-PLAN-01-review + W2-IMPL-01-report.

## 5. Verification evidence
Baseline (00-mission.md): ctest 1/1 Passed 23.98s, hygiene OK, no-network OK, full build timed out at 120s.
Final (gate-w2-1 + reviews re-ran): build lwf_tests + full exe Built; ctest 1/1 Passed 19.87s (520/520, 27,790/27,790 asserts per final reviewer); hygiene OK; no-network OK; G5 config_v0 inside suite; G3 zig pending (no zig). Fail-before proven: scratchpad/snippets/transforms pins 518/520 → 520/520 after W2-IMPL-02; negative scanner neutered-probe 3/6 fail. Greps: PushFont-digit src/ui/ → 0, ALLOWLIST → 0, metrics::font_ pages+widgets → 0.

## 6. Review outcomes
- Wave 1 code W1-REV-01: APPROVE, 0 BLOCKER/0 MAJOR, 1 MINOR fixed (R-W1-001 Transforms), 3 NIT deferred/rejected docs-only.
- Wave 1 tests W1-TREV-01: APPROVE-WITH-CHANGES, 0 BLOCKER/1 MAJOR fixed (negative scanner), 4 MINOR deferred (mapping pins → screenshots, glob, loopholes, CWD), 1 NIT rejected.
- Wave 1 delta W1-DELTA-01: APPROVE after improvement.
- Wave 2 plan-critique W2-PLAN-01: APPROVE-WITH-CHANGES, 4 MAJOR closed by orchestrator (D-W2-01..04).
- Wave 2 code W2-REV-01: APPROVE-WITH-CHANGES, 0 BLOCKER/0 MAJOR, 1 MINOR (P-006 screenshots), 2 NIT deferred.
- Wave 2 tests W2-TREV-01: APPROVE-WITH-CHANGES, 0 BLOCKER/0 MAJOR, 2 MINOR + 3 NIT deferred (lexical residuals, flaky suspect transient).
- Final integration W-DELTA-01: APPROVE-WITH-CHANGES, 0 BLOCKER/0 MAJOR, 2 MINOR (P-006 screenshots, PROGRESS rows — both closed by this commit).
All dispositions FIXED/DEFERRED/REJECTED with evidence in ledger + reports.

## 7. Deferred / open items (ticket-ready)
- P-006 screenshots: capture 960×640 + 760×520 × DPI 1.0/1.5 + Scratchpad editor before/after on Windows, file pixel-identical diff or record waiver (final F-001 MINOR). Blocks hierarchy-proof declaration.
- RowList dead code: Display.cpp widgets::RowList zero callers (W1-REV-01 R-W1-002). Delete-or-exercise follow-up.
- Glob/file-list expansion: 9-file hardcoded list can miss new sources (W1-TREV-01 R-W1-003). Glob + exclusion-filter follow-up.
- Lexical loophole class: headers unscanned, Dummy 2-component height, multi-line anchors, token-comment smuggle (W1-TREV-01 R-W1-004). P2 follow-ups.
- Per-site mapping pins: wrong-value swap survives lexical scan (W1-TREV-01 R-W1-002). P-006 is the catch; cheap per-file token-presence pins optional.
- Card padding 16-vs-20 drift, dual-palette render split (dark context + teal cards), 40/rail structural literals: frozen/pre-existing, out of scope.
- G3 zig: pending (no zig on dev box, docs/QUESTIONS.md #1).

## 8. Out-of-plan discoveries
- Foreign fix/dict-rows-imgui-error commit 8773d94 (Dummy after cursor restore) reused ops HEAD; untouched (foreign plan file left untracked). Recorded in ledger.
- T-002 Scratchpad metrics namespace fix (qualified lwf::ui::metrics::) — caught pre-merge, correctly disclosed.
- 1 space_xs site left as documented native-grid reuse (Transforms:576) rather than 10th token (orchestrator D decision).

## 9. Temporary mitigations
None. No stopgaps shipped; both waves are root-cause spelling fixes.

## 10. Risks, rollout and operational notes
- Migrations: none (compile-time spelling only). Config/schema/UiBus/main.cpp untouched. Rollback: git revert tests→code order (D-W2-04); reverting any token to its literal fails the ban (reversal proof). P-006 screenshots are the visual backstop for wrong-token swaps.
- Residual risk: wrong-value token swap (e.g. gap_section 12 used where gap_compact 10 meant) passes all tests; catch is review-time + screenshots.

## 11. Assumptions A-nnn and decisions
- A-001..003 baseline/env assumptions (00-mission.md). D1-D5 scope locks confirmed.
- D-rail: leave 220 local, document dead 208 token (never retokened).
- D-W2-01 full 9 tokens incl. gap_inline/gap_block. D-W2-02 exhaustive grep verification. D-W2-03 broadened studio-font check + MainWindow native proof. D-W2-04 revert tests→code.

## 12. How to review this branch
git diff 8a6ea225..HEAD --stat -- ':!docs/agent-ops' for code; read w2-deep/W2-DEEP-01-design.md then reports/w1-impl + w2-deep/W2-IMPL-01-report; run `ctest --test-dir build -R lwf-tests` + `bash scripts/check_hygiene.sh` + `bash scripts/check_no_network.sh`; G3 zig pending per docs/QUESTIONS.md #1; screenshots on Windows before merge (or waiver).

## 13. Agent roster
See briefs/ + reports/ dirs (01-work-ledger.md Waves 0-2). Key: W0-INV-01..04, W1-IMPL-01..03, W1-REV-01/W1-TREV-01/W1-IMPR-01/02/W1-DELTA-01, W2-DEEP-01/W2-PLAN-01/W2-IMPL-01/02/W2-REV-01/W2-TREV-01/W-DELTA-01 — all committed.

## 14. Residual risk and unverified claims
P-006 screenshots not captured in this environment (Windows manual owed); real TTF metrics not rendered; G3 zig not run; 1 transient unreproduced ctest failure noted by W2-TREV-01 (flaky suspect, passed 3× thereafter). Pixel claims are algebraic (token==literal) + reviewer value audits, not pixel renders — but screenshots remain the authoritative visual gate.
