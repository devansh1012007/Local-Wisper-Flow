# Brief — W4-IMPL-A2 (implementer) · wave 4 · run 20261107-studio

## 1. Mission
Model-path setup in the GUI (plan A2/T-023): Win32 file-picker for whisper_model_path + llm_model_path (currently plain InputText + ValidateModelPath in MainWindow DrawSpeechPage). No file picker exists anywhere in src/ today. Root cause: typing full model paths by hand.
Role: implementer.

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; windows.h ONLY in src/os/ — hygiene gate fails any other file; no binary assets)
3. 00-mission.md, 01-work-ledger.md; plan §§E (A2), H, I; D9 decision (picker + text field).
4. INV-ui-shell (MainWindow.cpp:706-730 whisper_model_path InputText + ValidateModelPath; :954-981 onboarding catalog; llm_model_path has NO UI today); Wave 3 S1 report for General/Speech tab layout (your pickers land on Speech tab + onboarding step 3).
5. Read src/core/SettingsUiModel.cpp:49 ValidateModelPath semantics — keep them.

## 3. Acceptance (T-023)
- src/os/FileDialogs.{h,cpp} (new): GetOpenFileName wrapper — filter (GGUF, BIN, all files), initial dir from the model's parent dir (or data dir), returns selected path or empty-on-cancel; windows.h lives ONLY in this dir; no other file may include windows.h.
- Speech tab: Browse… button next to whisper_model_path InputText (keeps text entry + ValidateModelPath + restart banner); same for llm_model_path (new field — empty disables cleanup, document in helper text).
- Onboarding step 3: Browse… button next to the model path field (fallback manual entry preserved).
- Cancel = no change, no error. Invalid path = existing ValidateModelPath message (don't invent new copy).
- Tests: non-Windows-safe helpers only (path normalize, filter build); Win32 picker itself is MANUAL (list in report: pick, cancel, bad extension, UNC path attempts — PASS/FAIL placeholders for Devansh on Windows hardware). G3 zig pending → report honestly as "type-checked", never "works".
- Gate: FileDialogs.cpp needs check_no_network.sh? NO (no file writes) — verify and state it.

## 4. Ownership
WRITE-SET: src/os/FileDialogs.{h,cpp} (new), src/ui/MainWindow.{h,cpp} (Speech + onboarding controls only)
READ-ONLY: else. FORBIDDEN: CMakeLists.txt (NEED), scripts/**, src/core/**, src/ui/pages/**, widgets/**, docs/agent-ops (report only).
Siblings: 4 studio pages (own files); Insights + all-key settings in batch 2. MainWindow.* is exclusively yours this wave for settings edits — S1 Wave 3 closed; note pages' wiring NEEDs for integration.

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target local-wisper-flow; bash scripts/check_hygiene.sh; bash scripts/check_no_network.sh (confirm still OK). Never full ctest. One build at a time.

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-A2-T-023.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-023 / Agent: W4-IMPL-A2 / Wave: 4 / Report: <path>. Final <=8 lines.
