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

### Wave 1 (Surface fixes) — READY FOR APPROVAL
- Agents: 3 parallel, disjoint (ownership/W1.tsv, overlap check clean)
- WAVE_BASE_SHA (pre-wave): 8f2ff6c
- Pre-mortem: hidden coupling via shared widgets (ScratchpadWindow editor shares RowList/Toast/Banner); same-value mapping error silently shifts pixels with no lexical test failure; dual-palette means spelling-only cannot fix identity split; token-gap sites (24/17/10, 12px) must stay allowlisted, never forced to 28/16. Mitigations: same-value-only rule + allowlist in T-004 ban, forbid *Palette.cpp edits, screenshots gate before merge, no palette changes.
- Gate plan: G1 target lwf_tests build, G2 ctest (expect green only after all 3 land; T-004 alone is red-by-design), G4 hygiene, G5 in-suite, G3 pending. Full exe build with long timeout at gate.
- Review plan: 1 code reviewer (MainWindow+pages) + 1 test reviewer (ban + pin flips), each blind-first.
- Status: AWAITING USER APPROVAL (=orchestrator gate)

## Orchestrator recovery note 2026-11-08
- Wave 1 plan commit landed on main (572ec21) instead of ops branch; ORCH cherry-picked it to ops as 81b82db to restore single-branch invariant. Main is now 1 ahead with same docs. No code involved. Verify with git branch -vv before Wave 1.

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
