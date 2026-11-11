# Brief — W2-IMPL-01 (DEEP IMPLEMENTER coder) · wave 2 tranche 2 · run 20261108-ui-hierarchy
## 1. Mission
Implement approved design: 9 additive studio_metrics tokens + file-local hoists + stage-2 spacing/type migration (zero metrics in studio files) + test flips. Why: Clear 26 allowlist to 0, close palette-split vocabulary leak, same-value spelling only.
## 2. Required reading (in order)
1. STANDING-ORDERS.md 2. AGENTS.md UI frozen + Build 3. 00-mission.md 4. Design: reports/w2-deep/W2-DEEP-01-design.md full 5. Critique closures: reports/w2-deep/W2-PLAN-01-review.md + ledger Decisions D-W2-01..04 (01-work-ledger.md Wave 2 decisions section) 6. First code attempt (legacy W0-CS-05 context? Use design) 7. Implementer reports W1-IMPL-01/02/03 + reviews W1-REV-01/R-W1-001, W1-TREV-01/R-W1-001 8. Template report-implementation.md 9. COMMS tail
Wave 1 HEAD lineage: ea04e1a (Wave 1 close) + docs commits to 3e0ebfb.

## 3. Task and acceptance
Task T-003 tranche 2, single agent, two commits:
Commit 1 (code): StudioPalette.h add 9 tokens (font_card_title_px 15, font_heading_px 24, font_subheading_px 17, font_glyph_px 10, gap_micro 6, gap_inline 8, gap_compact 10, gap_section 12, gap_block 16) with header comments stating not-mockup + radius_card 12-vs-10 warning + dual-use heading note + coupling sentence if needed; migrate allowlisted 26 sites to them (fonts PushFont no *scale, gaps *scale); migrate ALL studio-page PushFont metrics::font_* (45 hits: card_title 15, body 13(headers), helper) to studio twins; migrate studio pages metrics::space_s/m to gap_inline/gap_block (grep list, double-check radius namespaces qualify); hoist MainWindow 14→kStatusDotIndent, widths 240/360→kSearchWidth/kDialogInputWidth; migrate cluster_w/cursor_x 10/12 duplicates; leave structural literals 240/360? No widths hoisted; 18/pill_h, 40, rail 220, button-width +28/+32, 14 insets stay literal per design §6.
Commit 2 (tests): tests/test_overlay_settings_ui.cpp: delete 17 BanAllowance entries → empty table retained, append 9 token CHECKs after :738, flip has_metrics→has_studio (broadened two-spelling per D-W2-03) + add CHECK(has_metrics_font(MainWindow)), rewrite negative dummy_safe per §7.4 (literal 12 violates + tokened empty + universal empty), re-label cap-exceed → digit_violation.
Acceptance (all):
- grep PushFont(nullptr, [0-9] src/ui/ → 0; grep ALLOWLIST (Wave 2 T-003) src/ui/ → 0; grep metrics::font_ src/ui/pages src/ui/widgets → 0; Ban TEST_CASE pass, negative pass, token-pin pass; full ctest 1/1 Passed; full exe Build target local-wisper-flow passes; hygiene OK; no pixel delta (values == literals).
- Report lists every site old→new (cf W1-REV-01 §1 audit) + four-way reversal matrix (§10) or at least ban + pin legs.
Tests that must fail-before: test-only commit fails on HEAD (counts 37→6 PushFont, caps 32) — report counts if applicable.

## 4. Ownership
WRITE-SET: src/ui/theme/StudioPalette.h, src/ui/MainWindow.cpp (14 hoist only), src/ui/pages/DictionaryPage.cpp, InsightsPage.cpp, SnippetsPage.cpp, TransformsPage.cpp, ScratchpadPage.cpp, src/ui/widgets/Buttons.cpp, Display.cpp, Overlays.cpp, tests/test_overlay_settings_ui.cpp
READ-ONLY: ThemePalette.h/.cpp, StudioPalette.cpp, Widgets.h API, Overlay.*, ScratchpadWindow.*, other tests, engine/core (forbidden to edit)
FRONTIER: Need header outside write-set? Follow NEED/DEFERRED-DEPENDENCY protocol, do NOT edit.

## 5. Siblings
None — single dedicated agent (design §8.1). No parallel edits to these files.

## 6. Skip/traps
- ThemePalette.h FROZEN — no value change, no metrics addition; mockup regen not needed (spacing/fonts only, no contrast input) but state reason in report.
- Palette unification (ApplyStudioStyle) explicitly OUT — do not touch *Palette.cpp Apply* or call sites.
- Radius collision 10 vs 12: qualify correctly, do not “fix” values.
- Transforms KeycapRowWidth 105 already tokened (fixed Wave 1) — verify.
- Scratchpad global namespace: verify studio alias reaches new tokens; full exe build required (W1 lesson).
- 40.0f rail pin, kRailWidth 220: do not touch (pin-safe).
- Render scratchpad metrics fully qualified if needed (Scratchpad global namespace) — prefer existing alias.

## 7. Env
Scoped: cmake --build build --target lwf_tests, --target local-wisper-flow, ctest -R lwf-tests, grep counts, hygiene. Never full suite beyond ctest.

## 8. Deliverables
Report: docs/agent-ops/20261108-ui-hierarchy/reports/w2-deep/W2-IMPL-01-report.md (ORCH-DIGEST first, template report-implementation.md). Structure: inventory done, every site old→new table, test plan results, long-run test Source/Load/Hostile/Operability/Reversal, failure-mode table filled, rollout/rollback order (tests→code), residuals.
Commits: code first, then tests, then report docs-only, trailers Task: T-003 Agent: W2-IMPL-01 Wave: 2 Report: <path>
Final ≤8 lines.

## 9. Deep-dive report addendum required (design record already approved; implementation report carries failure table). If scope explodes (>1 sitting), ESCALATE with proposed decomposition, do not creep.
