# W4-IMPL-A2 T-023 — Model-path file pickers (A2)

## ORCH-DIGEST
- Status: DEFERRED-DEPENDENCY (code complete; app link + committed tests pending integrator CMake wiring — NEED posted; portable helpers + TU compiles verified)
- Branch: feature/studio-pages
- Task: T-023 (plan A2) — Win32 file picker for whisper/llm model paths; Browse… on Speech tab + onboarding step 3.
- Write-set: src/os/FileDialogs.{h,cpp} (new), src/ui/MainWindow.{h,cpp} (Speech + onboarding only). CMakeLists.txt is FORBIDDEN → NEED posted for integrator wiring (both app + lwf_tests registration, exact lines in §NEED). Tests file also outside write-set → NEED for integrator test registration; helpers verified via standalone scratch test (not committed).
- Key facts verified: llm_model_path has no UI today; empty llm path disables cleanup at startup (main.cpp:357-391); ValidateModelPath only rejects empty/over-500 (SettingsUiModel.cpp:49-57), preserved unchanged; windows.h must stay in src/os (hygiene gate); MainWindow.cpp cannot include windows.h — follows repo forward-declare pattern (ShellExecuteA decl MainWindow.cpp:22) with opaque void* owner HWND (WindowStyles convention).
- Evidence: [RAN scoped cmake build -> MainWindow.cpp compiles clean, link fails ONLY on 2 FileDialogs symbols pending NEED]; [RAN standalone helper test -> ALL OK, exit 0]; [RAN hygiene OK + no-network OK]. Manual Windows-hardware checklist open below (Devansh). G3 zig pending → "type-checked" (MinGW g++), never "works".

## Context Report (pre-change investigation)

### 1. Root cause / entry points
- [VERIFIED src/ui/MainWindow.cpp:722-753] DrawSpeechPage "Model" card: single whisper_model_path InputText (##model_path) + ValidateModelPath on IsItemDeactivatedAfterEdit + restart banner. No picker exists anywhere ([VERIFIED grep GetOpenFileName|FileDialog|Browse in src -> zero hits]). Root cause: hand-typed full paths.
- [VERIFIED src/ui/MainWindow.cpp:1105-1183] DrawWelcomePage step 3 lists onboarding catalog only; there is no model-path field on step 3 today. Brief's "fallback manual entry preserved" is therefore implemented as: add editable InputText + Browse… together (manual entry remains possible; picker is additive). Restart/commit semantics identical to Speech card.
- [VERIFIED src/core/SettingsStore.cpp:29,32] whisper_model_path default "ggml-base.en.bin" (restart, hot_apply=false), llm_model_path default "" (restart). [VERIFIED src/main.cpp:357-391] empty llm path -> cleanup_llm_available=false -> DictationCleanup disabled regardless of cleanup_enabled; so helper text "Leave empty to turn off cleanup." is factually correct.

### 2. Affected files / consumers
- New: src/os/FileDialogs.h/.cpp (namespace FileDialogs, WindowStyles-style opaque void* owner). Consumer: MainWindow.{h,cpp} only. No other callers, no config-schema change, no new settings keys.
- Build: CMakeLists.txt WIN32 local-wisper-flow sources + lwf_tests sources (NEED, integrator-owned). lwf_tests does not link MainWindow.cpp, so picker wiring only proves out in the full app binary.

### 3. Invariants preserved
- ValidateModelPath semantics unchanged (src/core untouched).
- Cancel = no change, no error, no ApplyAndSave, validation_error_ untouched.
- Invalid path = existing ValidateModelPath message only; no new copy, no new validation (bad extension is accepted by ValidateModelPath by design; loader fails later via existing startup ModelMissing path).
- No code comments added (AGENTS.md); no windows.h string in src/ui; no TODO/FIXME/XXX; no file-write APIs in new code (FileDialogs is read-only dialog; check_no_network unaffected by construction, to be verified).
- Existing source-scan pins preserved: IsItemDeactivatedAfterEdit count only grows (>=3); page_focus_pending_=true stays 2 (no new arming); no `if (ImGui::SliderInt(` added; validation_error_.clear() untouched.

### 4. Risks / edge cases
- New TU not linked until CMakeLists wired → full-app link will show undefined FileDialogs symbols until integrator applies NEED; verification uses standalone TU compiles + scratch helper test, honestly reported as DEFERRED-DEPENDENCY for linking.
- Long/UNC paths: dialog buffer 32768 wide chars; picked paths >500 chars rejected by existing ValidateModelPath (not widened — out of scope, src/core forbidden).
- Unicode paths: wide-char dialog + UTF-8 conversion (avoids ANSI truncation in non-ASCII user dirs).
- Owner HWND: opaque void* from glfwGetWin32Window forward-declared in MainWindow.cpp (mirrors ShellExecuteA pattern); FileDialogs casts to HWND internally under _WIN32; null owner falls back to unowned dialog.

### 5. Expected behavior / tests
- Speech ##card_model: whisper InputText + Browse…; below, new "Cleanup model" llm InputText + Browse… + helper "Leave empty to turn off cleanup."; both commit on deactivation (ValidateModelPath → SetString → restart_needed_=true → ApplyAndSave()); restart banner kept.
- Onboarding step 3: whisper InputText + Browse… + helper; manual entry preserved.
- Helpers (non-Windows-safe, portable): NormalizeModelPath (trim, strip one surrounding quote pair, /→\), InitialDirForModel (parent of current path or data dir), BuildModelFileFilter (GGUF+BIN+All, double-null terminated wide).
- Win32 picker: MANUAL on Windows hardware (pick, cancel, bad extension via All files, UNC) — PASS/FAIL placeholders in report.
- G3 zig: pending (no zig on box) → report as "type-checked" (MinGW g++ -fsyntax-only), never "works".

## Implementation Log (atomic phases)

### Phase 0 — Investigation (read-only)
- [VERIFIED] Brief order read: STANDING-ORDERS, AGENTS.md, 00-mission, 01-work-ledger, plan §§A2/E/H/I + D9, INV-ui-shell, W3-IMPL-S1 report, SettingsUiModel.cpp:49, MainWindow.* (Speech 722-753, Welcome 1105-1183), CMakeLists build graph, check_hygiene.sh / check_no_network.sh, DataDir/PathResolver, main.cpp:357-391 (empty-LLM-disables-cleanup).
- [VERIFIED grep] No GetOpenFileName/FileDialog/Browse anywhere in src/ — picker is greenfield.
- [VERIFIED] Onboarding step 3 has NO path field today (catalog + Skip only) → "fallback manual entry preserved" implemented as InputText+Browse added together.
- [VERIFIED] lwf_tests does not link MainWindow.cpp (CMakeLists.txt:137-141 lists only src/os portables) → picker wiring proves out only in the full app binary; helper tests need separate registration.
- Created this report early; posted COMMS START + CLAIM + NEED (CMakeLists lines 132/348-356) before editing.

### Phase 1 — src/os/FileDialogs.{h,cpp} (new)
- `namespace FileDialogs`, WindowStyles-style: header portable (no windows.h), impl #ifdef _WIN32 with <windows.h>+<commdlg.h>, non-Windows stubs returning {}.
- API: PickModelFile(current_path, void* owner_hwnd) + one-arg overload (nullptr); NormalizeModelPath; InitialDirForModel; BuildModelFileFilter (wstring on WIN32 / string elsewhere, GGUF+BIN+All double-null terminated).
- Picker: OPENFILENAMEW, 32768-char buffer (long/UNC paths), OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER, title "Select model file", initial dir = parent of current path or GetDataDir(), UTF-8 <-> wide conversion (unicode user dirs), cancel/error -> {} (no change, no error).
- No code comments added (AGENTS.md); no file-write or network APIs (dialog is read-only) → check_no_network unaffected by construction.

### Phase 2 — src/ui/MainWindow.{h,cpp} (Speech + onboarding only)
- Header: `char llm_model_path_buffer_[512]`, `std::string llm_model_error_`, `PickAndApplyModelPath(buffer, size, key, error&)` decl, <cstddef> include.
- CPP: `#include "os/FileDialogs.h"`; `extern "C" void* glfwGetWin32Window(GLFWwindow*)` forward-decl next to ShellExecuteA (no windows.h in TU; cdecl confirmed — GLFWAPI is NOT stdcall, so no __stdcall on the decl); file-local DialogOwnerHwnd() (opaque void* like WindowStyles).
- `PickAndApplyModelPath`: pick → empty = cancel (return false, zero side effects) → NormalizeModelPath → copy into buffer → ValidateModelPath into caller's error (existing copy only) → empty: SetString + restart_needed_=true + ApplyAndSave().
- Speech ##card_model: whisper InputText width narrowed for Browse… + SameLine button ##browse_whisper; Separator + "Cleanup model" llm InputText + Browse… ##browse_llm + per-field error render + helper "Leave empty to turn off cleanup." (matches main.cpp:357-391 semantics). Whisper text-entry, validation, restart banner unchanged.
- Onboarding step 3: whisper InputText ##welcome_model_path + Browse… ##browse_welcome + error render, inserted between checksum line and Skip; manual typing + Skip/Fallback preserved.

### Phase 3 — Self-review fixes
- MAJOR (resolved): shared validation_error_ for whisper+llm would misattribute an llm error to the whisper field → per-field `llm_model_error_` + error-ref param + render under each field + clear on Settings tab switch (next to existing clear).
- MINOR (resolved): missing <cstddef> for std::size_t in header (compiled only via <string> transitively) → added.
- Verified source-scan pins still hold (see §Review Audit).

## NEED (integrator, CMakeLists.txt — FORBIDDEN to me)
Add `src/os/FileDialogs.cpp` to:
1. WIN32 `local-wisper-flow` sources — CMakeLists.txt:348-356 block (after DataDir.cpp alphabetically: ActiveWindow, DataDir, FileDialogs, SingleInstance, ...).
2. `lwf_tests` sources — CMakeLists.txt:137-141 block (next to DataDir.cpp), so helpers stay testable on both platforms (stub compiles clean on Linux).
3. New `tests/test_filedialogs.cpp` (doctest) registration in the same lwf_tests block; suggested cases: NormalizeModelPath (trim/quotes/slashes/empty), InitialDirForModel (empty→data, bare→data, path→parent, UNC parent), BuildModelFileFilter segments (*.gguf;*.bin + *.* present, double-null terminator). My standalone scratch equivalents passed (see §Verification); scratch file deleted, not committed.

## Review Audit (self-review per phase + final)
- Correctness / root cause: hand-typed paths are the root cause; picker is additive (text entry kept everywhere); no symptom patch. Cancel touches nothing; pick commits through the same ValidateModelPath→SetString→restart→ApplyAndSave path as typing. PASS.
- Security / edges / invariants:
  - Over-500-char picks: snprintf truncates into 512-buffer, ValidateModelPath(>500) rejects, nothing saved (no silent wrong save). PASS.
  - Windows order: initial dir falls back to GetDataDir() (creates dir as today); picker never writes files. PASS.
  - Unicode: wide dialog + UTF-8 conversion; lpstrInitialDir wide. PASS (code-verified, hardware-pending).
  - glfwGetWin32Window decl is cdecl (no __stdcall): GLFWAPI on this toolchain is plain dllimport-equivalent; a stdcall decl would corrupt the stack — explicitly avoided and the TU compiled (build log). PASS.
  - Source-scan pins (tests/test_overlay_settings_ui.cpp): page_focus_pending_=true ==2 (2 found, untouched); validation_error_.clear() ==3 (>=2); IsItemDeactivatedAfterEdit ==5 (>=3); no `if (ImGui::SliderInt(` added; ApplyAndSave count in Welcome only grows (>=2); NavEnableKeyboard / InvisibleButton(item.id / PushFont markers untouched. PASS.
  - Hygiene: no windows.h string under src/ui (grep zero); FileDialogs.{h,cpp} live in src/os (gate-exempt dir); no TODO/FIXME/XXX; no windows.h winhttp; no binary assets. PASS.
- Maintainability / conventions: three Browse sites share one PickAndApplyModelPath (no copy-paste); button ids unique (##browse_whisper/##browse_llm/##browse_welcome) so ImGui ids never collide; file-local helper mirrors WindowStyles opaque-HWND convention; no src/core / src/ui/pages / widgets touched. PASS.
- Test rigor (mutation mentality): revert NormalizeModelPath slash mapping → scratch assertion fails; revert InitialDirForModel parent extraction → scratch fails; revert cancel early-return → settings would be overwritten with empty (visible in code path, MANUAL cancel check below); remove any Browse… button → manual check fails. Committed-test registration is integrator NEED (this agent's write-set excludes tests/ + CMakeLists). PASS with deferral noted.
- Final review: git diff confined to MainWindow.{h,cpp} + new FileDialogs.{h,cpp} (+ COMMS UNCOMMITTED lines, orchestrator-owned, not staged). No debug artifacts, no CMake/scripts/os-other changes. Backwards compat: no schema change, no new keys, no file-format change; unknown-key round-trip untouched.
- Independent review: change is Medium risk (internal UI + additive picker; no public API/serial/auth/billing change). Per brief, self-review suffices; status DEFERRED-DEPENDENCY for linking + committed tests, not REVIEW_REQUIRED. Orchestrator may dispatch delta review if desired.

### Long-run test (Standing Orders 8)
- Source: root cause is missing picker (hand-typed paths); fix adds the actual pick mechanism (FileDialogs) + wiring at all three briefed sites, keeping text entry. Good long-run shape (single PickAndApplyModelPath for future model-path UIs).
- Load: dialog is modal few-shot; no loops/queues; O(path length) normalize. Good.
- Hostile: malformed config paths (empty/overlong/UNC/unicode/bare) cannot crash or corrupt config (picker never writes; validator gates save; unknown keys preserved). Cancel/error = no-op. Good.
- Operability: failures surface via existing ValidateModelPath copy only; dialog errors are silent by design (brief: cancel=no error); NEED lines above make the missing link greppable. Good.
- Reversal: revert 4 files; no data migration; picker removal leaves text entry working. Good.

## Verification Evidence (verbatim)
- [RAN g++ -std=c++17 -fsyntax-only -DNOMINMAX -Isrc src/os/FileDialogs.cpp -> EXIT 0] (Win32 TU type-checks; G3-zig equivalent honestly pending — not claimed as "works").
- [RAN standalone helper test (temp scratch, g++ + FileDialogs.cpp + DataDir.cpp + PathResolver.cpp, -lcomdlg32 -lole32 -lshell32 -luuid -> linked and ran: "filter wide size=61" / "filedialogs check: ALL OK" / EXIT 0] — NormalizeModelPath ×5, InitialDirForModel ×4, BuildModelFileFilter segments all asserted; scratch deleted after, not committed.
- [RAN cmake --build build --config Release --parallel 4 --target local-wisper-flow -> "Building CXX object ... MainWindow.cpp.obj" succeeded (zero MainWindow errors), then link failed ONLY on: undefined reference to FileDialogs::PickModelFile(..., void*) + FileDialogs::NormalizeModelPath(...)] — exactly the NEED above; pre-existing gRPC `empty_bases` warnings only, no new warnings. After NEED is applied the same build must link (symbols already proven linkable by the standalone test exe).
- [RAN bash scripts/check_hygiene.sh -> "hygiene: OK"] (after final edits re-ran -> OK).
- [RAN bash scripts/check_no_network.sh -> "no-network: OK"] — FileDialogs is dialog-only (GetOpenFileNameW), zero file-write/network calls; no gate edit needed (brief §3 confirmed).
- [VERIFIED grep windows.h in src/ui -> zero matches; TODO|FIXME|XXX in FileDialogs.* -> zero].
- NOT RUN: full ctest (scoped mandate; lwf_tests unaffected except pushed NEED for test registration); zig (not installed — G3 pending); full-binary link (blocked on NEED).
- Tests intentionally not added under tests/ (read-only write-set; NEED posted with suggested cases).

## Manual checklist (Devansh, Windows hardware) — PASS/FAIL placeholders
1. Speech tab: click Browse… next to whisper path → dialog opens in current model's parent dir (or data dir when empty) → pick a .bin → field updated, saved, restart banner shows. [ ] PASS / [ ] FAIL — notes: ____
2. Speech tab: Browse… → Cancel → no field change, no error, no "Saved" flash. [ ] PASS / [ ] FAIL — notes: ____
3. Speech tab: Browse… → set filter "All files" → pick a .txt (bad extension) → accepted by dialog + ValidateModelPath (both per design: filter is a hint; loader reports missing at startup). [ ] PASS / [ ] FAIL — notes: ____
4. Speech tab: Browse… → navigate to UNC share (\\server\share) → pick → path stored and resolves at restart. [ ] PASS / [ ] FAIL — notes: ____
5. Speech tab cleanup field: Browse… pick .gguf → saved; clear field + Enter → "Leave empty to turn off cleanup." holds (cleanup disabled at startup, main.cpp:358-391). [ ] PASS / [ ] FAIL — notes: ____
6. Onboarding step 3: Browse… picks + manual typing both set whisper_model_path; Skip advances. [ ] PASS / [ ] FAIL — notes: ____
7. Over-500-char path (type 501 chars manually) → existing "model path is too long" shown, not saved. [ ] PASS / [ ] FAIL — notes: ____

## Unverified Assumptions & Remaining Risks
- [ASSUMED] Open/short-name file with >500 chars is rejected by pre-existing ValidateModelPath (out of scope to widen — src/core forbidden); user with extreme UNC paths hits that message (documented behavior, not a regression).
- [UNKNOWN] Dialog appearance/owner-masking on real hardware (opaque HWND from glfwGetWin32Window) — code compiles and links (standalone + app-object evidence); visual behavior is MANUAL checklist items above.
- Full app binary cannot link until integrator applies the CMakeLists NEED; anything requiring a running binary (dialog open, restart load, manual checklist above) is unverified until then.
- Sibling agents' untouched files (src/ui/pages/, src/ui/ScratchpadWindow.h, tests/test_*page.cpp untracked in tree) belong to other waves — never staged by me.

## Git State
- Branch: feature/studio-pages (verified; never switched).
- Code commit (explicit paths only): src/os/FileDialogs.h, src/os/FileDialogs.cpp, src/ui/MainWindow.h, src/ui/MainWindow.cpp — trailers Task: T-023 / Agent: W4-IMPL-A2 / Wave: 4 / Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-A2-T-023.md. SHAs: see below (code + docs-only report commit, code first).
- Docs commit (explicit path only): docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-A2-T-023.md.
- Left uncommitted (never staged by A2): docs/agent-ops/20261107-studio/COMMS.md (A2 START/CLAIM/NEED appended per protocol; orchestrator commits), sibling untracked (briefs W4-IMPL-D/N/S/T, src/ui/pages/, src/ui/ScratchpadWindow.h, tests/test_dictionary_page.cpp, tests/test_transforms_page.cpp), this report's scratch test (temp dir, deleted). No git add -A/stash/reset/checkout/rebase/clean used.

