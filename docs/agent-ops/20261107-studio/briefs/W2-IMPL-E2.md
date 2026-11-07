# Brief — W2-IMPL-E2 (implementer) · wave 2 · run 20261107-studio

## 1. Role and mission
Mission: Transform run flow engine mode (plan P15) — clipboard save → Ctrl+C → 300ms change check → LLM with prompt → preamble/length validation → paste → clipboard restore; 40s diff retention. Root cause: no engine transform mode exists.
Role: implementer.

## 2. Required reading (in order)
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; R4: transcript never logged; tests first for portable logic; background threads never touch widgets)
3. docs/agent-ops/20261107-studio/00-mission.md, 01-work-ledger.md
4. Approved plan: §§D (run flow on engine/worker thread, NOT in UiBus drain), E (P15), H (T-runflow 5 branches + clipboard restore + no-UI-block), I.
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-enginehooks.md — CRITICAL: WindowsClipboardInjector is DEAD code; live clipboard technique = WindowsSelectionCapture:366-437 (read it fully). CommandModeController clipboard-copy technique is the template for Ctrl+C/paste/restore.
6. Wave 1: reports/w1-impl/W1-IMPL-02D-T-00708.md (TransformStore slots/opt-in/shortcut model + {text} handling) — your flow consumes TransformStore entries via loopback/fake ILlmProvider in tests, never a real LLM.
7. Read src/core/{CommandModeController,DictationEngine,UiBus,TransformManager}.{h,cpp} fully. UiBus has NO default case — a new command is silently dropped without a consumer (you add both sides or post NEED for main.cpp wiring).

## 3. Task / acceptance (T-012)
- Engine: new Transform mode + `UiCommandType::StartTransform` trigger (user gesture) + `Win+Alt+O` diff-viewer trigger hook (UI lands Wave 4 — expose the hook + before/after retention; don't build the viewer). Flow exactly per spec §4.4 steps 1-6: save clipboard (all formats possible); Ctrl+C; if clipboard text unchanged after 300ms → "Select some text first", restore, stop; else LLM: `ILlmProvider::Process(text, system_prompt)` with selection wrapped in `<selection>{text}</selection>` delimiters + data-not-instructions sentence (D5); validate (non-empty, < 3x input + 200 chars, no preamble Sure/Here is/Certainly opener) → fail: "Transform failed. Nothing was changed." + restore; success: paste (selection still highlighted), restore clipboard; retain before/after 40s for the diff viewer (in-memory only).
- All of this on the engine/worker thread — NEVER in the UiBus drain loop (T-no-ui-block: assert drain latency is unaffected; test with a blocking fake LLM + timing bound).
- Clipboard restored on EVERY branch (success, no-selection, LLM throw, LLM validation fail) — Windows multi-format restore via WindowsSelectionCapture (Windows-only; guard `WIN32` like other adapters; portable engine logic stays platform-free — factor: `ISelectionProvider` seam if the engine would otherwise gain windows.h).
- Existing engine tests unchanged (diff -- tests/ shows no expectation edits). New tests: 5 branches + clipboard-restored-every-branch (fake clipboard + fake LLM + fake injector) + opt-in OFF registers no shortcuts (assert TransformStore opt-in gate consulted before any shortcut/arm) + transform run never records stats (interface check — if P12 wiring is in-flight, note the check).
- main.cpp / UiBus consumer wiring: UiBus.h is in your write-set (additive StartTransform); src/main.cpp is NOT — post NEED with the exact consumer-loop lines (main.cpp:540-577 range from INV report) for the integration pass.
Acceptance: 5-branch + restore + no-UI-block tests green scoped; existing engine tests untouched; hygiene OK.

## 4. Ownership
WRITE-SET: src/core/DictationEngine.{h,cpp}, src/core/UiBus.{h,cpp} (additive), src/core/TransformManager.{h,cpp} (entry reuse only), src/core/TextDiff.{h,cpp} (diff retention helpers only), tests/test_transform_runflow.cpp (new)
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt, scripts/check_no_network.sh, src/main.cpp, src/ui/**, src/os/** (read WindowsSelectionCapture only), docs/agent-ops (report only).

## 5. Siblings
E1 (stats wiring, same DictationEngine.cpp — CONFLICT RISK): if E1 also edits DictationEngine.{h,cpp} on the StopAndTranscribe path, you share the file. You MUST post CONFLICT in COMMS.md the moment both agents' git status shows DictationEngine.cpp modified — the orchestrator will sequence the merge via explicit-path commits (keep your hunks far from each other: transform mode near the top / new methods; E1 near :475). Do not touch :475 region. E3 owns scratchpad target + A1 keys + main.cpp integration.

## 6. Traps
Never block the UI thread. Never write snippets of selection to logs. Validation must match spec exactly (3x+200 cap; preamble list Sure/Here is/Certainly). 40s retention is in-memory, never persisted. No code comments.

## 7. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; ctest filtered runflow; bash scripts/check_hygiene.sh. Never full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w2-impl/W2-IMPL-E2-T-012.md (ORCH-DIGEST first). Commits explicit paths only; trailers Task: T-012 / Agent: W2-IMPL-E2 / Wave: 2 / Report: <path>. Final <=8 lines.
