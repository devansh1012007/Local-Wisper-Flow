<!-- ORCH-DIGEST:START -->
agent: W2-IMPR | scope: wave 2 T-002/T-003/T-005/T-006 review findings (REV-W2-002-CODE + REV-W2-003-TESTS) | status: DONE
findings: FIXED 10 | REJECTED 0 | DEFERRED 0
blockers_major_open: none
code_commits: 07cc957 | report_commit: <see below>
tests_added_or_strengthened: 3 added, 2 strengthened (69 assertions vs 20 before)
ownership_violations: none
confidence: med (static + vendored trace + scoped test run; live GUI keyboard/hover still needs MANUAL_TEST_WINDOWS.md hardware, same as impl reports)
<!-- ORCH-DIGEST:END -->

# Improvement report — W2-IMPR

## 1. Finding-by-finding disposition
| Finding | Severity | Disposition | What I did / why rejected or deferred (with evidence) | Commit |
|---|---|---|---|---|
| R-W2-001 rail Enter discarded (code) | MAJOR | FIXED | `src/ui/MainWindow.cpp:398-401` now captures `const bool pressed = ImGui::InvisibleButton(...)` and switches on it; `IsItemClicked()` call removed (vendored nav activation returns pressed=true without HoveredId). Verified unfixed in HEAD before editing [VERIFIED MainWindow.cpp:396-397]. | 07cc957 |
| R-W2-002 idle poll freezes hover/click (code) | MAJOR | FIXED | `src/ui/Overlay.cpp:290-301` poll now watches 3 verdicts: inflated zone, exact-dot rect (inflate 0), physical button state (`LeftMouseButtonDown()`); members `poll_hover_dot_`/`poll_button_down_` added [VERIFIED Overlay.h:79-87]. Verified unfixed in HEAD before editing [VERIFIED Overlay.cpp:268-289]. | 07cc957 |
| R-W2-003 stale page_focus_pending_ (code) | MINOR | FIXED | Rail press now sets `page_focus_pending_ = false` (focus already on pressed item) and any left-mouse click in DrawRail clears a stale arm before the restore check [VERIFIED MainWindow.cpp:385-396, :403-408]. Only SetVisible + Welcome-Finish arm it (test pins count==2). Frame-bounded lifetime declined: remaining arms are focus-lost cases where restore-on-first-Tab is correct. | 07cc957 |
| R-W2-004 unused WaveformRing::At (code) | NIT | FIXED | Listening bar loop now uses `waveform_.At(Size()-5+i)` oldest-first read [VERIFIED Overlay.cpp:434-439]; At has a production caller, no dead API. | 07cc957 |
| R-W2-005 poll/displaySize ±1px (code) | NIT | FIXED (by R-W2-002 change) | Exact-dot poll reuses DrawBadge center geometry (window-width centered, top-anchored); assumption + one-frame self-correction documented in comment [VERIFIED Overlay.cpp:276-289]. Reviewer agreed not independently worth fixing; closed as part of the MAJOR. | 07cc957 |
| R-W2-003-001 A1/A3 zero tests (tests) | MAJOR | FIXED | Added P1 source-scan TEST_CASE `main window keyboard nav and commit-on-deactivate wiring` [VERIFIED tests/test_overlay_settings_ui.cpp:469-513] with count/ordering/segment checks; fail-before verified by grepping `3aa4329^:src/ui/MainWindow.cpp` (NavEnableKeyboard 0, copied_until_ 0, EnableNav 0, SetKeyboardFocusHere 0; single pre-existing IsItemDeactivatedAfterEdit). | 07cc957 |
| R-W2-003-002 alpha/width floors (tests) | MINOR | FIXED | Floors changed to exact pins `== 0.55f` / `== 1.0f` / `== 250.0f` [VERIFIED tests:307-317]. | 07cc957 |
| R-W2-003-003 status scan one literal (tests) | MINOR | FIXED | Class-level guard: `* badge_alpha` exactly once (intended `surface_alpha * badge_alpha` base dimming) + `badge_alpha * ` absent (excludes legitimate `*=` lines) [VERIFIED tests:356-366]. | 07cc957 |
| R-W2-003-004 PushFont/PopFont ordering (tests) | NIT | FIXED | Ordering asserts scoped to DrawAttentionMessage segment: PushFont < CalcTextSize < AddText < PopFont [VERIFIED tests:371-384]. | 07cc957 |
| R-W2-003-005 empty/zero-capacity ring (tests) | NIT | FIXED | New TEST_CASE covers empty ring At-throw + SnapshotInto-empty, partial-fill start=0 branch, and pre-filled scratch clear [VERIFIED tests:388-417]. | 07cc957 |

P2 add-ons from REV-W2-003 §6 folded in: partial-fill (P2.1), pre-filled scratch (P2.2), early-out/cache dirty_ scan (P2.3, new TEST_CASE `mini idle early-out wakeup wiring is guarded`, also pins R-W2-002 markers). P3 cwd-robustness left alone (pre-existing, suite-wide, out of brief scope).

## 2. Code changes (per file, with rationale; root-cause level, no band-aids)
- `src/ui/MainWindow.cpp` — (R-W2-001) rail loop captures the InvisibleButton activation return. The return is true for both mouse click and keyboard activation, so one check replaces the mouse-only IsItemClicked with no mouse regression. (R-W2-003) mouse-click clear at DrawRail top + rail-press disarm. Both are cause-level: the bug was treating a navigation-capable button as mouse-only and arming focus restore from a button that already holds focus.
- `src/ui/Overlay.cpp` — (R-W2-002) idle poll triple verdict. Root cause was wrong poll domain (pass-through zone ≠ interaction zone), so the fix polls the exact dot rect and the OS button state rather than rendering unconditionally. Cost stays O(1) (2 GetCursorPos + 1 GetAsyncKeyState per idle frame). (R-W2-004) At() used in the Listening bar loop; identical ordering to the levels snapshot it replaces, guarded by Size()>=5 using the same invariant (SnapshotInto filled == Size).
- `src/ui/Overlay.h` — two new idle-poll verdict members with INVARIANT comment tying them to the repaint decision.
- Long-run test answers: Source — root-cause fixes as above, no alpha patch or single-path special case. Load — 3 OS calls/frame, no allocs. Hostile — At() throw guarded by Size() check; no new inputs. Operability — rail keyboard switch and parked-click resume need MANUAL_TEST_WINDOWS.md hardware verification (no display session here). Reversal — single commit 07cc957 reverts cleanly; tests 7 cases assert the new behavior so a revert fails loudly.

## 3. Test changes (added edge cases from the test review, strengthened assertions, flake fixes, removed redundancy)
- Added 3 TEST_CASEs: MainWindow P1 scan (17 CHECKs incl. count/ordering/segment checks); early-out wakeup wiring scan (R-W2-002 markers + dirty_/cache counts); waveform partial/empty/scratch edges.
- Strengthened: alpha/width floors → exact pins; status-word scan literal → class + ordering asserts (existing checks preserved).
- Removed nothing; no test weakened. Assertion count for the touched cases: 69 total (was 20).

## 4. Optimizations applied and evidence of benefit (and ones declined, with reason)
- Applied: At() in bar loop removes one indexed read path through a copied vector (levels still needed for the avg fold); no measured perf claim — correctness is the point (closes dead-API nit).
- Declined: unconditional render for the idle path (would destroy the A5 idle win); frame-bounded pending lifetime (unneeded once mouse-clear + rail-disarm land); poll/displaySize unification beyond comment (needs cross-window DisplaySize plumbing, reviewer-rated not-worth-it); cwd-robust source-scan (P3, suite-wide, out of scope).

## 5. Commands run and results (scoped)
- [RAN `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` -> configure done] (PowerShell exit 1 from stderr streaming; gRPC pre-existing header warnings only)
- [RAN `cmake --build build -j4 --target lwf_tests local-wisper-flow` -> both Built target, link OK]
- [RAN `./build/lwf_tests.exe --test-case="*mini badge alphas*,*waveform ring*,*mini status word*,*idle early-out*,*keyboard nav*"` -> 7 passed, 69 assertions, 0 failed] (run 2x, incl. after final comment edit)
- [RAN `ctest --test-dir build -R lwf-tests --output-on-failure` -> 1/1 Passed, 17.42s]
- [RAN `bash scripts/check_hygiene.sh` -> hygiene: OK]
- Fail-before checked by grepping `git show 3aa4329^:src/ui/MainWindow.cpp` (all behavioral markers absent; stt_provider pre-exists, noted in test comment).

## 6. Residual risks and anything the delta reviewer should check
- Live behavior NOT verified: real Enter/Space page switch, ring→dot hover brighten, parked Paused click, drag start from parked cursor — all code-trace conclusions against vendored ImGui; needs hardware run per MANUAL_TEST_WINDOWS.md. Check: does the button-edge wake land in the same tick as ImGui's NewFrame mouse state? Poll runs before NewFrame in RenderFrame, so yes structurally.
- `ImGui::IsMouseClicked` at DrawRail top is global (not item-scoped) by design — any click cancels; if a future UX wants click-to-work-then-Tab to keep rail focus, that needs rethinking (currently correct per MINOR scenario).
- COMMS.md last read: before commit (through WAVE-2 START lines + W2 lines to line 30 header; file had later W2-IMPR-A5A6 DONE lines).
- Branch: fix/gui-typography-dpi (per ORCH DECISION, not ops/<run-id>). Code commit 07cc957; this report follows in a separate docs-only commit.
