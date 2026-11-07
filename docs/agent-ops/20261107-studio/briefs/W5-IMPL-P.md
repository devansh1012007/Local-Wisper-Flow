# Brief — W5-IMPL-P (implementer) · wave 5 · run 20261107-studio

## 1. Mission
F9 polish sweep (plan P24/T-021): contrast test on every token pair, copy lint (no "Oops", no "!" in UI text, no internal terms FSM/VAD/LLM/whisper/provider/pipeline), keyboard navigation sweep through every page, re-run all gates. Root cause: Waves 3-4 shipped widgets + pages with estimated tokens and provisional copy — this is the honest final pass.
Role: implementer.

## 2. Required reading (in order)
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; 3:1/4.5:1 contrast policy; tests/test_overlay_settings_ui.cpp token pins; tests changed only when incorrect — you ARE allowed to fix tests that pin disproven token values, saying so; never weaken an assertion to go green)
3. docs/agent-ops/20261107-studio/00-mission.md, 01-work-ledger.md
4. Approved plan: §§E(P24), H, I; spec §9 DoD (portable gates green or "pending on Windows" + reason).
5. Wave 3/4 reports + gates (what sweep items remain: F4 HeatLevel/Toggle (from W3-IMPR-01 NEEDS), Toast cursor/Esc findings, Esc-close EndPopup skip, 14px vs 12 radius disagreement, muted pixel-confirm pending F0(a) — F0(a) needs YOUR action: the 4 screenshots are NOT in the repo; before re-measuring contrast, save/cite the source images from the conversation if available; if you cannot pixel-sample, the sweep's contrast verdict is "provisional, F0(a) pending" — never claim measured).
6. docs/STUDIO_PLAN.md (F0 deliverable from Wave 1: your source for copy rules + token table + checklist).

## 3. Task / acceptance (T-021)
- Contrast: test every StudioPalette text pair (4.5:1) + shapes/borders (3:1); muted #7A776F must be resolved (darken to pass OR restrict usage — the Wave 3 provisional became the blocker; clear it with evidence or mark F0(a)-pending with a concrete Devansh-action line).
- Copy lint: grep every UI string (src/ui/** + pages) for "Oops", "!", FSM/VAD/LLM/whisper/Provider/provider/pipeline(only-but-"cleanup" is allowed?) — decisions per plan copy list; fix each hit; add a source-scan test pin so regressions fail (T-copy).
- Keyboard: sweep every page (rail, tabs, search, sheets, dialogs, editor, pickers) — register NavActive-gated focus; fix what's broken structurally; list manual items for Devansh.
- Gates re-run: build + ctest + hygiene + no_network green (or recorded pre-existing).
- Checklist (§8 I/D/S/T/Sn/P1 items): mark PASS-able automatable vs Devansh-manual in docs/STUDIO_CHECKLIST.md (spec §9) — create/update that file (it's your write-set).
- F4 HeatLevel/Toggle + Toast/Esc items from Wave 3: address or carry forward with the same visibility.

## 4. Ownership
WRITE-SET: src/ui/theme/StudioPalette.{h,cpp}, src/ui/widgets/*, src/ui/pages/*, src/ui/MainWindow.{h,cpp}, src/ui/Overlay.{h,cpp} (copy only), tests/test_overlay_settings_ui.cpp (token/copy scans), docs/STUDIO_CHECKLIST.md (new)
READ-ONLY: else. FORBIDDEN: CMakeLists.txt (NEED), scripts/**, src/core/**, src/os/**, src/main.cpp.
Siblings: W5-IMPL-O (deep dive OpenRouter) — share MainWindow Privacy/About copy: O owns the cloud-egress statement; your copy-lint sweeps the rest; post CONFLICT on overlap.

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; ctest filtered; bash scripts/check_hygiene.sh; bash scripts/check_no_network.sh. Never full ctest. One build at a time.

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w5-impl/W5-IMPL-P-T-021.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-021 / Agent: W5-IMPL-P / Wave: 5 / Report: <path>. Final <=8 lines.
