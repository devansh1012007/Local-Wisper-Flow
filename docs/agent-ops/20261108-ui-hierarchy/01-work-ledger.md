# Work ledger — 20261108-ui-hierarchy (orchestrator-owned)

## Plan coverage
| Plan item | Summary | Disposition |
| P-001 | MainWindow native spacing/typography literals → metrics::* | pending Wave 0 → Wave 1 |
| P-002 | Studio pages literals → studio_metrics::* | pending Wave 0 → Wave 1 |
| P-003 | Widgets density consistency | pending Wave 0 → Wave 1 |
| P-004 | kRailWidth 220 vs rail_width 208 drift | default leave+document, pending confirm |
| P-005 | Source-scan tests for bypasses | pending Wave 0 → Wave 1 |
| P-006 | Screenshots before/after | pending Wave 1 manual |
| P-007 | Gates + PROGRESS.md | pending gate runs |

## Tasks (synthesized from Wave 0 digests 2026-11-08)
| Task | Plan items | Domain | Class | Root cause (1 line) | Write-set | Contract-set | Depends on | Conflicts | Wave | Agent | Status |
| T-001 | P-001 | ui/MainWindow | SURFACE | Geometry literals bypass metrics::* | MainWindow.cpp, MainWindow.h | none | — | C1 resolved disjoint | 1 | W1-IMPL-01 | pending |
| T-002 | P-002,P-003 tranche1 | ui/pages+widgets | SURFACE | 37 PushFont + ~30 Dummy digit literals | pages/*.cpp + widgets/* | none | — | C1 resolved disjoint | 1 | W1-IMPL-02 | pending |
| T-004 (P-005) | P-005 | tests | SURFACE | Tests bless literals, no bypass ban | test_overlay_settings_ui + scratchpad + snippets tests | none | — (red-by-design) | C5 resolved disjoint | 1 | W1-IMPL-03 | pending |
| T-003 | Token gaps 24/17/10 + 12px gap | theme | DEEP | No token for 24/17/10 headings, 12px gap | StudioPalette.h (additive) + tranche2 | additive studio_metrics fonts/gaps | T-002 spelling first | C4 ordering | 2 | TBD | deferred |

## Waves
### Wave 0 (Investigation) — DONE 2026-11-08
- Agents: W0-INV-01 a474ef5, W0-INV-02 0b2f8e6, W0-INV-03 ed8c0e4, W0-INV-04 ba26af8
- Digests: 37 PushFont-digit + whitespace literals; dual-palette split proven (ApplyStudioStyle only ScratchpadWindow 91,591); tests pin literals (fail-before required); mockup matches structurally (PARTIAL token coverage); rail 220 vs 208 leave+document per D-rail
- Spot-checked by ORCH: PushFont grep + Dummy grep confirm counts; read W0-INV-02 s5/s9 + W0-INV-03 s5

### Wave 1 (Surface fixes) — APPROVED 2026-11-08 (all waves approved)
- Agents: 3 parallel, disjoint (ownership/W1.tsv, overlap check clean)
- WAVE_BASE_SHA (pre-wave): aecb917e33dbbf43a69df55e6b9df8eef7ed2c1a
- Pre-mortem: hidden coupling via shared widgets (ScratchpadWindow editor shares RowList/Toast/Banner); same-value mapping error silently shifts pixels with no lexical test failure; dual-palette means spelling-only cannot fix identity split; token-gap sites (24/17/10, 12px) must stay allowlisted, never forced to 28/16. Mitigations: same-value-only rule + allowlist in T-004 ban, forbid *Palette.cpp edits, screenshots gate before merge, no palette changes.
- Gate plan: G1 target lwf_tests build, G2 ctest (expect green only after all 3 land; T-004 alone is red-by-design), G4 hygiene, G5 in-suite, G3 pending. Full exe build with long timeout at gate.
- Review plan: 1 code reviewer (MainWindow+pages) + 1 test reviewer (ban + pin flips), each blind-first.
- Status: AWAITING USER APPROVAL (=orchestrator gate)

## Wave 1 triage 2026-11-08 (orchestrator)
- Reviewers: W1-REV-01 APPROVE (0 BLOCKER/0 MAJOR), W1-TREV-01 APPROVE-WITH-CHANGES (0 BLOCKER/1 MAJOR)
- Dispositions:
  - CODE R-W1-001 MINOR (Transforms paired arithmetic) -> FIX (improvement cycle 1, same-value zero delta)
  - CODE R-W1-002/003/004 NIT -> DEFER/REJECT-docs-only (RowList dead code, editor-sharing note, count wording) — no code change
  - TEST R-W1-001 MAJOR (no negative scanner test) -> FIX (improvement cycle 1, direct scanner unit tests)
  - TEST R-W1-002 MINOR (per-site mapping unpinned) -> DEFER to P-006 screenshots (systemic lexical residual)
  - TEST R-W1-003/004 MINOR (file glob, loophole class) -> DEFER Wave 2
  - TEST R-W1-005 MINOR (CWD partial) -> DEFER (pre-existing, not regression)
  - TEST R-W1-006 NIT -> REJECT (unrealistic CI path)
- Improvement: W1-IMPR-01 (Transforms tokens, general) + W1-IMPR-02 (scanner negative test, coder), disjoint files, then re-gate + delta review. 1 of 2 cycles used.
- Improvement results: W1-IMPR-01 code 1f7a71d (Transforms 3 sites same-value), W1-IMPR-02 code bbc8a14 (6 CHECKs negative scanner test, neutered-scanner probe 3/6 fail proven). Gate w1-2 green (ctest 1/1 Passed 19.97s). Delta W1-DELTA-01 APPROVE (fdf5892). Wave 1 CLOSED 2026-11-08. Remain deferred to Wave 2: code R-W1-002 (RowList dead code), test R-W1-002..006 (mapping pins, glob, loopholes, CWD), plus token-gap allowlist 26 sites. Screenshots P-006 still pending.

## Wave 2 tranche close-out 2026-11-08
- W2-IMPL-01 landed: 9 tokens + stage-2 migration, greps 0, ban/pin/negative green, exe links, hygiene OK. Residual: 2 test pins stale (test_snippets 338-341, test_transforms ~380 require native fonts) + 1 space_xs site kept (Transforms:576). Disposition: expand write-set for W2-IMPL-02 (those 2 test files + Transforms comment). space_xs stays as documented native-grid reuse (no 10th token; D-W2-01 escape hatch). Then re-gate.

## Wave 2 decisions (orchestrator, from W2-PLAN-01 critique, 2026-11-08)
- D-W2-01 token set: FULL 9 tokens (4 fonts + 5 gaps). Stage-2 spacing migration ships IN tranche 2 (not deferred), staged commits code-then-tests. done criteria: grep metrics::font_ over 8 files → 0, metrics::space_* → 0 or documented reuse sentence if descoped mid-flight (no silent mix).
- D-W2-02 inventory: implementer MUST generate exhaustive grep list (metrics:: plus all allowlisted values) at implementation time; design tables are illustrative, grep→0 is the verification. Transforms font_body/card lists (~45 hits) migrate exhaustively.
- D-W2-03 qualification: broaden has_studio_font to accept studio_metrics::font_ AND lwf::ui::studio_metrics::font_ (mirror has_metrics two-spelling precedent). Add CHECK(has_metrics_font(MainWindow)) 3-line native proof.
- D-W2-04 rollback order: canonical "revert tests→code, never code alone" (git revert <test> <code> in that order). Recorded as record; tranche ships code-test pair reverting together.
- Wave 2 tranche is SINGLE dedicated agent (design §8.1 §8.2 serialization reason) owning StudioPalette.h + 9 .cpp (incl MainWindow 14 hoist) + tests/test_overlay_settings_ui.cpp.

## Orchestrator recovery note 2026-11-08
- Wave 1 plan commit landed on main (572ec21) instead of ops branch; ORCH cherry-picked it to ops as 81b82db to restore single-branch invariant. Main is now 1 ahead with same docs. No code involved. Verify with git branch -vv before Wave 1.
- Review branch anomaly: W1-REV-01 found HEAD on fix/dict-rows-imgui-error with foreign dirty tree (Dummy fix). Foreign commit 8773d94 exists on fix branch, NOT on ops. ORCH preserved W1-REV-01 report via fix-branch docs commits 570072c/dd83222, cherry-picked to ops as 1c101a7/40c748e. Foreign plan file docs/plans/2026-11-08-dict-rows-imgui-dummy.md left untracked, untouched. ops branch is working branch.

## Conflict resolution notes
- C1 write-write MainWindow vs pages/widgets resolved by W1.tsv disjoint files; widgets assigned solely to W1-IMPL-02 (Wave 1), not split with W1-IMPL-01
- C5 test collision resolved: tests/* sole writer W1-IMPL-03; T-001/T-002 forbid tests
- Investigator disagreement on zero-vs-allowlist for token gaps: W0-INV-02 says allowlist 24/17/10+12px, W0-INV-03 says zero after Wave 1 — resolved as allowlist in Wave 1, deep token decision in Wave 2 (avoids 24→28 visual shift). Reason recorded here (ledger source of truth).

## Deferred / open items
- D-rail retoken vs leave (default leave, reversible)
- npx ui-skills CLI permission (pending user)
- Real TTF fonts (pending approval, out of scope this run)
- Palette unification (separate plan if screenshots show split)

## Out-of-plan discoveries
- (from plan-mode recon) ApplyStudioStyle only in ScratchpadWindow.cpp:91,591; MainWindow never calls it [FACT grep]
- (from plan-mode recon) PushFont literals 15/13/24 in DictionaryPage.cpp:242, Display.cpp:35 [FACT subagent]
