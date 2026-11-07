# Work ledger — 20261107-studio (orchestrator-owned; source of truth for resume)

## Plan coverage
| Plan item | Summary | Task |
|---|---|---|
| P01 | F0 docs/STUDIO_PLAN.md + AGENTS D3 amendment + palette measure | T-001 |
| P02 | AtomicFile + gate allowlist (same commit) | T-002 |
| P03 | TextDiff word LCS | T-003 |
| P04 | InsightsModel pure formulas | T-004 |
| P05 | StatsStore stats.json rollup | T-005 |
| P06 | DictionaryStore v2 + migration | T-006 |
| P07 | SnippetStore v2 + validation | T-007 |
| P08 | TransformStore 10 slots + {text} auto-append | T-008 |
| P09 | NotesStore index+md | T-009 |
| P10 | transforms.json v1→v2 migration | merged T-008 |
| P11 | app_profiles category defaults | T-010 |
| P12-P16 | engine hooks (stats :475, transform mode, scratchpad target) | T-011..T-013 |
| P17-P18 | nav shell + StudioPalette/widgets/modal-toast infra | T-014..T-015 |
| P19-P23 | five pages | T-016..T-020 |
| P24 | F9 polish sweep | T-021 |
| A1-A4 | ApiKeyStore, FileDialogs, recorders, 49-key settings | T-022..T-025 |
| C1-C2 | OpenRouterProvider + gate | T-026 |

## Tasks (Wave 0 verification pending)
| Task | Plan | Domain | Class | Write-set summary | Status |
|---|---|---|---|---|---|
| T-001 | P01 | docs | SURFACE | docs/STUDIO_PLAN.md, AGENTS.md (D3 text) | pending |
| T-002 | P02 | core | SURFACE | src/core/AtomicFile.*, scripts/check_no_network.sh | pending |
| T-003 | P03 | core | SURFACE | src/core/TextDiff.* | pending |
| T-004 | P04 | core | SURFACE | src/core/InsightsModel.* | pending |
| T-005 | P05 | core | SURFACE | src/core/StatsStore.* | pending |
| T-006 | P06 | core | DEEP | src/core/DictionaryStore.* + PersonalDictionary (count) | pending |
| T-007 | P07 | core | SURFACE | src/core/SnippetStore.* | pending |
| T-008 | P08+P10 | core | SURFACE | src/core/TransformStore.* (+TransformManager wrapper) | pending |
| T-009 | P09 | core | SURFACE | src/core/NotesStore.* | pending |
| T-010 | P11 | core | SURFACE | src/core/AppContextResolver.* (+app_profiles defaults) | pending |
| T-011 | P12-P14 | core | SURFACE | DictationEngine + TextPipeline overload + wiring | pending |
| T-012 | P15 | core/os | DEEP | transform run flow + UiBus + clipboard technique | pending |
| T-013 | P16 | core/ui | SURFACE | scratchpad injection target + UiBus + Overlay button | pending |
| T-014 | P17 | ui | DEEP | MainWindow shell + Settings tabs + source-scan sync | pending |
| T-015 | P18 | ui | SURFACE | StudioPalette + widgets + modal/toast infra | pending |
| T-016..T-020 | P19-P23 | ui/pages | SURFACE x5 | one file per page | pending |
| T-021 | P24 | ui/core | SURFACE | sweep | pending |
| T-022 | A1 | core | SURFACE | ApiKeyStore + ConsentGate + gate allowlist | pending |
| T-023 | A2 | os/ui | SURFACE | FileDialogs + MainWindow Browse buttons | pending |
| T-024 | A3 | ui/os | SURFACE | hotkey recorders (5 + studio) | pending |
| T-025 | A4 | ui/core | SURFACE | all-key settings cards + quiet_speech_boost schema | pending |
| T-026 | C1-C2 | llm | DEEP | OpenRouterProvider + Contract + llm_provider key + gate | pending |

## Waves (planned)
- W0: verification (read-only investigators, 4 domains).
- W1: data layer T-001, T-002..T-010 (parallel, disjoint write-sets; CMakeLists.txt + check_no_network.sh via P02 same-commit + follow-up integration agent if needed).
- W2: engine hooks T-011..T-013 + A1 (T-022) in parallel.
- W3: shell T-014 (single writer for MainWindow) then T-015.
- W4: workstream A T-023..T-025 + pages T-016..T-020 (parallel, disjoint).
- W5: polish T-021 + workstream C T-026. Final review + gate + SUMMARY.

## Conflicts resolved in plan
C1 hook at DictationEngine.cpp:475 (RunEngine dead); C2 gate edits same-commit; C3 dictionary shared instance (D-OP2); C4 worker-thread transform (P15); C5 scan-list per-page timing.
