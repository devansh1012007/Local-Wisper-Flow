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

## Tasks (drafted from Wave 0 recon, to be confirmed)
| Task | Plan items | Domain | Class | Root cause (1 line) | Write-set (proposed) | Contract-set | Depends on | Conflicts | Wave | Agent | Status |
| T-001 | P-001,P-003 | ui/MainWindow+widgets | SURFACE | Literals bypass tokens | MainWindow.cpp, widgets/* | none (spacing only) | — | C1 vs T-002 | 1 | TBD | pending |
| T-002 | P-002 | ui/pages | SURFACE | Literals bypass studio tokens | pages/* (5 files) | none | — | C1 vs T-001 if shared widgets | 1 | TBD | pending |
| T-003 | P-005 | tests | SURFACE | No guard against new literals | test_overlay_settings_ui.cpp | none | — | C5 vs T-001/T-002 | 1 | TBD | pending |

## Waves
### Wave 0 (Investigation, read-only)
- Agents: W0-INV-01 (MainWindow architecture), W0-INV-02 (patterns/tokens), W0-INV-03 (tests/blast), W0-INV-04 (history/intent + mockup)
- Status: pre-writing

### Wave 1 (Surface fixes)
- Pre-mortem (draft): hidden coupling via shared widgets; source-scan tests may miss pixel regressions; dual-palette look needs screenshot to confirm. Mitigation: disjoint write-sets (MainWindow vs pages vs tests), screenshots gate before merge, no palette changes.
- Status: pending user approval after Wave 0 synthesis

## Deferred / open items
- D-rail retoken vs leave (default leave, reversible)
- npx ui-skills CLI permission (pending user)
- Real TTF fonts (pending approval, out of scope this run)
- Palette unification (separate plan if screenshots show split)

## Out-of-plan discoveries
- (from plan-mode recon) ApplyStudioStyle only in ScratchpadWindow.cpp:91,591; MainWindow never calls it [FACT grep]
- (from plan-mode recon) PushFont literals 15/13/24 in DictionaryPage.cpp:242, Display.cpp:35 [FACT subagent]
