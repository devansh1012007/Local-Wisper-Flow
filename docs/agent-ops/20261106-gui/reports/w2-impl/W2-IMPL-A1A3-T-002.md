<!-- ORCH-DIGEST:START -->
agent: W2-IMPL-A1A3 | task: T-002/T-003 | status: DONE
code_commits: 3aa4329 | report_commit: pending
files_changed: 2 (src/ui/MainWindow.cpp, src/ui/MainWindow.h)
contracts_changed: none
tests: added 0, changed 0; scoped run: build + hygiene green, ctest not run (out of scope per brief)
ownership_violations: none
deferred_dependencies: saved_dwell 1.6->1.5 in src/ui/theme/ThemePalette.h (forbidden to this agent; sibling W2-IMPL-A5A6 owns ThemePalette) — MainWindow references metrics::saved_dwell so Copied dwell follows automatically once bumped; NEED posted in COMMS
out_of_scope_findings: 0
temporary_mitigation: no
deep_issue_flag: no
least_confident_about: exact focus-target choice for page-switch restore (first rail button; see section 4 alternatives)
confidence: med (no runtime GUI verification possible here; compile + hygiene only)
last_comms_line_read: WAVE-START 2026-11-06T19:45:00Z
<!-- ORCH-DIGEST:END -->

# Implementation report — W2-IMPL-A1A3 — T-002/T-003 GUI-A1 keyboard nav + GUI-A3 copy/state

## 1. Task and acceptance criteria (restated)
- NavEnableKeyboard on Full window IO; rail + ToggleSwitch InvisibleButtons pass ImGuiButtonFlags_EnableNav and draw 2px focus ring (kDarkPalette.focus) when focused.
- Page-switch focus restore via SetKeyboardFocusHere gated on IO.NavActive (no mouse-focus stealing); re-show refocus.
- Copy button shows "Copied" for saved_dwell via copied_until_; reset on SetLastTranscript.
- Welcome saves route through ApplyAndSave/ShowSaved; sliders commit on IsItemDeactivatedAfterEdit, not per-tick.
- validation_error_ cleared on page switch and on successful edit.
- Trust card branches on stt_provider!=local + consent to cloud-variant copy.
- saved_dwell 1.6->1.5: token lives in ThemePalette.h, FORBIDDEN to this agent — deferred to sibling (see section 12). The spawn brief said it was "ALREADY IN ThemePalette" — it is NOT: `saved_dwell = 1.6f` still at src/ui/theme/ThemePalette.h:135 [VERIFIED].
- Build green, hygiene green. No test edits (sibling W2-IMPL-A5A6 owns tests/).

## 2. Verification that the problem exists
Evidence gathered myself [VERIFIED src/ui/MainWindow.cpp pre-change]:
- No `NavEnableKeyboard`, no `ImGuiButtonFlags_EnableNav`, no `SetKeyboardFocusHere`, no `NavActive` anywhere in src/ [grep, 0 matches].
- ToggleSwitch was `ImGui::InvisibleButton(id, ImVec2(w,h))` with no flags and no focus ring; same for rail items.
- Copy button was label-fixed "Copy" with no dwell state; `copied_until_` did not exist.
- Cleanup sliders wrapped `if (SliderInt(...)) { Set+ApplyAndSave }` = save on every drag tick.
- Welcome steps 2 and 4 called `settings_.Save()` directly (no ApplySettings post, no Saved flash).
- Trust card was hard-coded local-only copy with a "when cloud speech lands" comment.
- `validation_error_` was never cleared on page switch.
No investigation report for A1/A3 existed on disk (only A2 wave-0/wave-1 reports); I derived requirements directly from the brief + mockup §5 tokens [VERIFIED docs/design/mockup.html section 5; palette section gives `focus` token + 2px `focus-sample` outline].

## 3. Root cause
The Full window never opted into ImGui keyboard navigation (no NavEnableKeyboard, custom InvisibleButtons without EnableNav = explicitly non-navigable), so keyboard users could not drive the rail or switches, and there was no focus indicator token wired in. Separately, several A3 copy/state behaviours used raw persistence (`settings_.Save()`) or per-tick slider saves instead of the shared ApplyAndSave/deactivate-commit pattern already used by the model-path input, so UI feedback (Saved, Copied) was inconsistent across pages.

## 4. Approach chosen and why
- `ImGuiConfigFlags_NavEnableKeyboard` OR-ed into IO.ConfigFlags at Init (one line, next to existing IO config).
- Both custom InvisibleButtons (rail items, ToggleSwitch) pass `ImGuiButtonFlags_EnableNav`; when `IsItemFocused()` each draws an outward `AddRect` with `Pack(kDarkPalette.focus)` at `2.0f*scale` thickness (2px at 1.0 scale, matching the `focus` token's "focus ring, 2 px" contract [VERIFIED src/ui/theme/ThemePalette.h:41]).
- Focus restore: `page_focus_pending_` flag set on rail click, Welcome Finish, and `SetVisible(true)`; consumed once in DrawRail immediately before the first rail button via `SetKeyboardFocusHere()` ONLY if `ImGui::GetIO().NavActive`. Mouse clicks never steal focus; the flag simply waits until the user drives with the keyboard.
- Copy: `copied_until_` deadline uses `metrics::saved_dwell` (same token as the Saved indicator, so the dwell value changes in one place); `SetLastTranscript` resets it.
- Sliders: return value ignored; `IsItemDeactivatedAfterEdit()` gates validate+save (mirrors the existing model-path input pattern).
- Welcome: `settings_.Save()` -> `ApplyAndSave()` at both save sites.
- Trust card: `provider != "local" && stt_consent_accepted` branches to cloud copy using provider display names Gnani/ElevenLabs/Muse (raw provider string as fallback), with fallback-to-local helper text reused from the Cloud page.

Alternatives considered and rejected:
- Focus the first content control of the new page instead of the first rail button: rejected. Content focus targets vary per page and the content child is created after the rail; anchoring to the first rail button keeps one predictable landing spot and Tab order starts at the rail regardless.
- Clear the pending flag on the first frame even when NavActive is false: rejected — that would drop the restore for keyboard-after-switch and contradicts the "gated on NavActive" requirement.
- Per-site saved-dwell constant: rejected — brief requires the shared token so the 1.6->1.5 bump needs no MainWindow edit.

## 5. What changed (per file)
| File | Change | Why |
|---|---|---|
| src/ui/MainWindow.h (+2 lines) | `copied_until_` (-1 default), `page_focus_pending_` (false) members | Copy dwell + deferred focus restore state |
| src/ui/MainWindow.cpp ToggleSwitch | EnableNav flag + 2px focus ring | GUI-A1 switch nav |
| src/ui/MainWindow.cpp Init | NavEnableKeyboard | master keyboard nav switch |
| src/ui/MainWindow.cpp SetVisible | pending flag on show | re-show refocus |
| src/ui/MainWindow.cpp SetLastTranscript | reset copied_until_ | stale "Copied" guard |
| src/ui/MainWindow.cpp DrawRail | NavActive-gated SetKeyboardFocusHere; EnableNav; focus ring; click clears validation_error_ + sets pending | GUI-A1 + A3 validation scoping |
| src/ui/MainWindow.cpp DrawHomePage Trust | local/cloud branch | GUI-A3 trust copy |
| src/ui/MainWindow.cpp DrawHomePage Copy | "Copied" for saved_dwell | GUI-A3 |
| src/ui/MainWindow.cpp DrawCleanupPage | two sliders commit on IsItemDeactivatedAfterEdit | GUI-A3 per-tick save fix |
| src/ui/MainWindow.cpp DrawWelcomePage | two sites via ApplyAndSave; Finish clears validation + sets pending | GUI-A3 |

## 6. Blast radius and contracts
- Write-set: only src/ui/MainWindow.{h,cpp} [VERIFIED git diff --stat: 2 files + report + COMMS line]. No API/contract changes (private members only).
- Greps: `settings_.Save()` remains only inside ApplyAndSave itself; all three `IsItemDeactivatedAfterEdit` sites (model path + 2 sliders) confirmed; both EnableNav sites confirmed; no other `page_` assignments besides Init/Welcome/rail.
- Downstream consumers: none external (MainWindow owns rendering; SettingsStore/UiBus interfaces untouched).
- Backward compat: no config/schema/serialized changes.

## 7. Tests
Added/changed tests: none — forbidden (sibling W2-IMPL-A5A6 owns tests/ per brief). Mutation rationale for the sibling: source-scan tests should look for `NavEnableKeyboard`, `ImGuiButtonFlags_EnableNav` (2 sites), `kDarkPalette.focus`, `SetKeyboardFocusHere` + `NavActive`, `copied_until_` + `"Copied"`, `IsItemDeactivatedAfterEdit` on both sliders, `ApplyAndSave` in DrawWelcomePage, `validation_error_.clear()` on rail click, and the `stt_provider != "local"` Trust branch.
Commands run: see section 11. ctest NOT RUN (per brief: scoped verification only; full suite is orchestrator-owned).

## 8. Long-run test
- Source: every change uses existing tokens/patterns (focus token, saved_dwell token, model-path deactivate pattern, Cloud-page fallback copy); no invented constants.
- Load: focus ring is a single rect when one item is focused; pending flag is one bool; no per-frame allocation added.
- Hostile: Trust fallback prints provider string via `%s` (no format-injection); transcript text rendered via existing `%s` patterns; Copied label switch does not alter clipboard content.
- Operability: keyboard-only user can now tab through rail + switches with visible ring; mouse users see zero behaviour change.
- Reversal: all changes are additive/local; revert is a single-file git revert with no migration.

## 9. Edge cases and abuse scenarios
- Handled: NavActive false at switch time (pending waits, no steal); hide-then-show (pending re-arm); new transcript mid-dwell (reset); slider drag with invalid value (error shown via existing validation_error_ render, no save); cloud provider variants incl. unknown provider string (falls back to raw string display).
- Not handled (out of scope): actual Tab-trap or arrow-key nav between content controls beyond ImGui defaults; the Welcome step-1/3 buttons rely on default Button nav; saved_dwell value bump itself (sibling-owned ThemePalette).
- Unknown: no runtime GUI verification was possible on this box (no display session exercised) — behaviour verified by compile + source inspection only.

## 10. Performance and security considerations
- No new loops, allocations, or per-frame work beyond one conditional + one rect draw for the focused item.
- No secrets/PII touched (provider display uses settings enum strings only).

## 11. Commands run and results (verbatim summaries)
- [RAN cmake --build build --config Release --parallel 4 --target local-wisper-flow] -> Built target local-wisper-flow (100%). Only warnings are pre-existing vendor grpc `empty_bases` notes from main.cpp; zero warnings from MainWindow.cpp.
- [RAN bash scripts/check_hygiene.sh] -> `hygiene: OK`.
- [RAN git diff --stat] -> only MainWindow.{h,cpp} + COMMS line + report.

## 12. Deviations from the plan/brief; ownership issues; deferred dependencies
- DEFERRED-DEPENDENCY: saved_dwell is still 1.6f [VERIFIED src/ui/theme/ThemePalette.h:135] contrary to the spawn note "ALREADY IN ThemePalette". ThemePalette.h is forbidden to me; NEED posted to COMMS for sibling W2-IMPL-A5A6. MainWindow consumes `metrics::saved_dwell` so "Copied" dwell automatically becomes 1.5 once the token is bumped — no MainWindow change needed.
- No other deviations. Tests untouched per brief (sibling-owned).

## 13. Out-of-scope findings
- None new (default ImGui nav cursor colour already maps to focus token [VERIFIED src/ui/theme/ThemePalette.cpp:133]).

## 14. Known limitations, risks, rollback notes
- Focus-restore landing spot is the first rail button (design choice, section 4) — a reviewer preferring first-content-control should ask for a follow-up; changing it later is a small local edit.
- If NavActive is already true when a mouse click switches pages, the next frame places focus on the first rail item (harmless, matches Tab-start position).
- Rollback: `git revert` of the code commit; no data migration involved.

## 15. Where I am least confident / what the reviewer should scrutinize
- Least confident: that `SetKeyboardFocusHere()` called before the rail loop (after the wordmark Text) reliably lands on the first InvisibleButton rather than being swallowed by a non-focusable item. ImGui's nav model skips non-focusable items, but I could not runtime-verify on this box. Reviewer: confirm with real keyboard input, or move the call to just before the first button creation if needed.
- Also verify the 2px ring offset/rounding on the pill switch at hi-DPI (rounding = h/2 + 2px, thickness 2*scale).

## 16. Self-review checklist
- [x] Root cause, not symptom (IO opt-in + EnableNav flags are the actual nav blockers; per-tick slider save was the actual spam source)
- [x] No band-aid patterns (no sleeps/catches/flags; existing patterns reused)
- [x] Tests fail without the fix — N/A (sibling owns tests; source-scan targets listed in section 7)
- [x] Only write-set files changed
- [x] No secrets/PII
- [x] Conventions from AGENTS.md followed (ImGui context discipline preserved, build + hygiene green, no full ctest)

## 17. Commits
code: 3aa4329 | report: pending
