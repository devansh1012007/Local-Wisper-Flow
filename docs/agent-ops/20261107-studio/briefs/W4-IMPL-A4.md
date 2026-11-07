# Brief — W4-IMPL-A4 (implementer) · wave 4 · run 20261107-studio

## 1. Mission
All-key settings GUI (plan A4/T-025): every schema key gets a control or a documented carve-out. 13/49 had controls at Wave 0 (36 missing incl. vad_threshold, all history/sound/streaming keys); A4 closes the gap across the 6 Settings tabs (General/Speech/Cloud/Cleanup/Privacy/About) per D11. D6: literally every key. Includes quiet_speech_boost (D-OP4: register it in BuildSchema — Wave 0 proved the toggle is a dead no-op outside the schema; schema-add WITHOUT breaking unknown-key preservation) + llm_provider key (for workstream C — register additive `local` default; C1 consumes it Wave 5) + last_page already Wave 3. Do NOT reimplement Wave 2's keys surface (Cloud consent + ApiKeyStore + Speech pickers stay as they are).

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; additive keys, unknown keys preserved on save; ClampedOnLastLoad; IsHotApply; never break config.json; Privacy page must state OpenRouter cloud egress when llm_provider=openrouter; "Delete all local data" after confirmation per spec S-P1)
3. 00-mission.md, 01-work-ledger.md; plan §§E(A4),H (T-a4 with D-OP3 carve-outs),I.
4. INV-ui-shell (49-key list + 13 exposed — your punch list; MainWindow.cpp quota; SettingsUiModel validators).
5. Wave 3 S1 report (Settings tabs + General layout — place your new cards into it, don't restructure tabs).
6. Read src/core/SettingsStore.{h,cpp} fully (SchemaEntry for carve-out list).

## 3. Acceptance (T-025)
- Every BuildSchema key has a control (Int→slider/number, Bool→ToggleRow, String→InputText/Combo, Double→slider) OR a D-OP3 carve-out with a reason (mini_x/mini_y readout-only "set by drag", schema_version readout, first_run write-only — carve-outs stated in(report).
- Tests: "every key has a control" scan (T-a4) with the carve-outs encoded; clamps honored (out-of-range entry clamps); ApplyAndSave path preserved (hot-apply vs restart-required banner intact).
- Privacy: "Delete all local data" (stats/dictionary/snippets/transforms/notes/api_keys — config kept or reset? spec says removes stats/dictionary/snippets/transforms/notes after confirmation — follow it exactly; api_keys deletion is implied by "all local data" — do it; do NOT delete config.json itself (weights/paths stay), documented).
- Manual checklist P1 listed in report.

## 4. Ownership
WRITE-SET: src/core/SettingsStore.{h,cpp} (schema_additive only), src/ui/MainWindow.{h,cpp} (settings controls only)
READ-ONLY: else. FORBIDDEN: CMakeLists.txt (NEED), scripts/**, src/ui/pages/**, widgets/**, src/os/**.
Siblings: pages (batch 1 landed, Insights batch 2), A3 recorders (A3 also edits MainWindow — CONFLICT RISK: post CONFLICT immediately, declare hunks; recorders vs general cards). MainWindow wiring for new pages = integration NEED.

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; ctest filtered; bash scripts/check_hygiene.sh. Never full ctest. One build at a time (retry once).

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-A4-T-025.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-025 / Agent: W4-IMPL-A4 / Wave: 4. Final <=8 lines.
