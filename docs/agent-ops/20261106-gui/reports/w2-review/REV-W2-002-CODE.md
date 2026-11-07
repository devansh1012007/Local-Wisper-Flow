<!-- ORCH-DIGEST:START -->
agent: W2-REV-CODE | scope: wave 2 T-002/T-003 (3aa4329) + T-005/T-006 (d9e86dd) | verdict: APPROVE-WITH-CHANGES
findings: BLOCKER 0 | MAJOR 2 | MINOR 1 | NIT 2
claims_audit: 16 claims checked / 3 unsupported or false
band_aids_found: none (no sleeps/catches/flags; two findings are logic errors, not band-aids)
top_3_risks: keyboard Enter on rail never switches page (return value discarded); mini click after cursor dwells inside badge zone is swallowed by idle early-out; hover brighten to 1.0 never renders when entering dot from inflated ring
optimizations: DO-NOW 1 | DO-LATER 1 | NOT-WORTH-IT 1
reversal_check: reasoned-only (git revert of 3aa4329 / d9e86dd restores prior behaviour; no migration involved)
confidence: med (static + vendored-ImGui trace + scoped test run; no live GUI runtime / display session)
<!-- ORCH-DIGEST:END -->

# Code review — W2-REV-CODE (wave 2, T-002/T-003 + T-005/T-006)

## 0. Method
Diff range reviewed: `git show 3aa4329` (src/ui/MainWindow.{h,cpp}) and `git show d9e86dd` (src/ui/Overlay.{h,cpp}, src/ui/theme/ThemePalette.h, src/core/OverlayModel.{h,cpp}, tests/test_overlay_settings_ui.cpp). Files read in context: src/ui/MainWindow.cpp (ToggleSwitch, Init, SetVisible, DrawRail, DrawHomePage copy/Copied, DrawCleanupPage sliders, DrawWelcomePage), src/ui/Overlay.cpp (Init fonts, SetState, RenderFrame, DrawBadge, UpdateDrag, DrawAttentionMessage), src/ui/Overlay.h, src/core/OverlayModel.{h,cpp}, src/ui/theme/ThemePalette.h, src/os/WindowStyles.cpp IsCursorInsideRect, vendored `_deps-src/imgui/imgui_widgets.cpp` (InvisibleButton, ButtonBehavior nav-activation) and `imgui.cpp` IsItemClicked. Commands run: `git show` for both commits; `grep` for SliderInt/IsItemDeactivated/SetKeyboardFocusHere/At usage; scoped test run `./build/lwf_tests.exe --test-case="*mini badge alphas*,*waveform ring SnapshotInto*,*mini status word*"` → 3 passed, 20 assertions. Order followed: blind pass first (diffs + vendored ImGui source), then implementer reports W2-IMPL-A1A3-T-002.md and W2-IMPL-A5A6-T-005.md, then claims audit.

## 1. Blind-pass findings (before reading implementer reports)
- Rail page switch uses `ImGui::InvisibleButton(...)` return discarded + `IsItemClicked()` check — vendored `IsItemClicked` is `IsMouseClicked && IsItemHovered`, while nav activation returns `pressed=true` from ButtonBehavior with no HoveredId set. Keyboard Enter therefore cannot switch pages. MAJOR.
- Idle early-out polls only the inflated pass-through rect. Entering the exact dot from the ring, or clicking while the cursor is already inside the zone, changes nothing in `poll_near` → no frame → `hovered_` frozen and `IsMouseClicked` never observed. MAJOR.
- `page_focus_pending_` persists until the first `NavActive` frame, so a later Tab press can yank focus to the rail long after a mouse switch/re-show. MINOR.
- `WaveformRing::At` is public but unused in src (only tests). NIT.
- Poll geometry truncates `poll_dot` to int and derives `poll_dot_min_x` from `glfwGetWindowSize`, potentially ±1px from DrawBadge's ImGui-space rect; harmless for wake, but parity is assumed not proven. NIT.

## 2. Claims audit
| Implementer claim | Verified? | Evidence / finding |
|---|---|---|
| A1: NavEnableKeyboard + EnableNav on rail/ToggleSwitch + 2px focus ring | Yes | [VERIFIED src/ui/MainWindow.cpp:152, :67, :396, :404-408] ring uses `Pack(kDarkPalette.focus)`, 2.0f*scale. |
| A1: keyboard-only user can tab through rail + switches (full keyboard operability) | False (partial) | Focus/ring yes [VERIFIED], but rail back-page activation broken — see R-W2-001 (return discarded). |
| A1: NavActive gate prevents mouse-focus steal | Mostly yes | [VERIFIED src/ui/MainWindow.cpp:389] gated; see MINOR stale-pending caveat R-W2-003. |
| A3: "Copied" dwell via copied_until_ + saved_dwell, reset on SetLastTranscript | Yes | [VERIFIED src/ui/MainWindow.cpp:210, :672-678, ThemePalette.h:135 saved_dwell=1.5]. |
| A3: sliders commit on IsItemDeactivatedAfterEdit, not per-tick | Yes | [VERIFIED src/ui/MainWindow.cpp:831-849] both sliders changed; model-path [VERIFIED :703] already used pattern. |
| A3: Welcome saves via ApplyAndSave | Yes | [VERIFIED src/ui/MainWindow.cpp:934, :972]. |
| A3: validation_error_ cleared on page switch / successful edit | Yes | [VERIFIED src/ui/MainWindow.cpp:399, :976]; successful edit assigns empty Validate result [:704,:833]. |
| A3: Trust card cloud-variant when provider!=local + consent | Yes | [VERIFIED src/ui/MainWindow.cpp:626-640] incl. unknown-provider `%s` fallback. |
| A5: early-out preserves dwell/fade bookkeeping | Yes | [VERIFIED src/ui/Overlay.cpp:250-260] bookkeeping precedes early-out; SetState sets dirty_ [:157]. |
| A5: hover entry/exit wakes badge; click cannot be swallowed while mouse elsewhere | False | [VERIFIED src/ui/Overlay.cpp:268-289] poll watches only outer inflated boundary; stationary click inside zone + intra-zone dot enter/exit produce no wake — see R-W2-002. |
| A5: drag/DPI force frames | Yes | [VERIFIED src/ui/Overlay.cpp:268, :297, UpdateDrag dirty_ sets :511,:535,:556]. |
| A5: Listening path zero heap alloc after warm-up; Snapshot delegates | Yes | [VERIFIED src/ui/Overlay.cpp:402, OverlayModel.cpp:44-64] SnapshotInto reuses `levels_scratch_`; Snapshot() delegates; scoped tests pass. |
| A5: CalcTextSize 1-entry cache invalidated on DPI apply | Yes | [VERIFIED src/ui/Overlay.cpp:490-495, :300, SetWorkArea :174]. |
| A6: alphas 0.55/1.0, discount removed, width 250 | Yes | [VERIFIED ThemePalette.h:119,123-124; Overlay.cpp:497 `Pack(muted,badge_alpha)`; tests pass]. |
| A6: second 13px font PushFont/PopFont balanced | Yes | [VERIFIED Overlay.cpp:106-113, :590/:600 balanced, no early return between]. |
| A6: saved_dwell resolved to 1.5 for both Saved and Copied | Yes | [VERIFIED ThemePalette.h:135] MainWindow consumes same token. |

Claims checked 16, unsupported or false 2 rows (3 counting both halves of row 2: stationary-click + intra-zone hover). Reports honestly flagged no live-GUI verification; they did not code-inspect the two MAJOR paths.

## 3. Long-run test (independent answers)
- Source: fixes must be root-cause: use InvisibleButton's pressed return for rail activation; make idle poll observe exact-dot hover and click state (e.g. poll exact dot rect + `LeftMouseButtonDown()`), not just inflated-boundary crossing. Patching only alpha or only click masks part of the same freeze.
- Load: both fixes are O(1) per frame / one call; SnapshotInto already bounds allocation. No new growth. Poll must stay cheap (one OS rect + button state).
- Hostile: `At()` throws `std::out_of_range` past newest — callers must not catch-and-continue; Trust fallback uses `%s` (no format injection); `levels_scratch_`/`out.clear()` cannot overflow (writes `filled_` elements) [VERIFIED OverlayModel.cpp:44-64].
- Operability: keyboard-only page switch and mini Paused-resume click must be verified with real input (docs/MANUAL_TEST_WINDOWS.md); code-only verification cannot confirm runtime focus/hover pixels.
- Reversal: each commit reverts independently; reverting d9e86dd restores unconditional rendering + 0.35/0.65 alphas (tests 1 & 3 then fail, as implementer noted). No data migration.

## 4. Findings
### R-W2-001 [MAJOR] Rail keyboard activation discarded — Enter/Space on focused rail item does nothing
- Location: `src/ui/MainWindow.cpp:396-397` · Evidence: code discards the `InvisibleButton` return and tests `ImGui::IsItemClicked()`. Vendored [_deps-src/imgui/imgui.cpp:6392] `IsItemClicked = IsMouseClicked && IsItemHovered`, and [imgui_widgets.cpp:846,700-767] nav activation sets `pressed=true` via ButtonBehavior without setting HoveredId ("we don't set g.HoveredId to not interfere with mouse"). Mouse still works (hovered+clicked), keyboard does not. ToggleSwitch correctly returns `toggled` [:67,:83], so switches work — rail alone is broken. Why it matters: A1's core acceptance (keyboard can drive page switch) is half-delivered; focus ring moves but activation is dead. Recommended fix: capture `const bool pressed = ImGui::InvisibleButton(...)` and switch on `pressed` (or `pressed || IsItemClicked()` if preserving mouse semantics; `pressed` already covers mouse). Root cause is discarding the activation return, not missing flags.

### R-W2-002 [MAJOR] Idle early-out polls only the inflated zone — intra-zone hover and stationary click are frozen
- Location: `src/ui/Overlay.cpp:268-289` (early-out) + `DrawBadge:373-374` (hovered_/UpdateDrag) · Evidence: early-out returns when `poll_near == poll_near_dot_`. `poll_near` is `IsCursorInsideRect(..., inflate)` [WindowStyles.cpp:42-58, inflate expands by `mini_hover_inflate`]. Sequence: (a) cursor in inflated ring but outside dot → wake (pass-through off, hovered=false, alpha=0.55); move inward onto exact dot → `poll_near` unchanged → `hovered_` stays false, badge never brightens to 1.0, and `IsMouseClicked` is never re-polled; (b) cursor parked over dot → left-click → `poll_near` unchanged → no `NewFrame`, `UpdateDrag` never sees click, Paused-resume/drag-start swallowed. Exit from dot to ring likewise freezes alpha at 1.0. Why it matters: hover feedback + click-to-resume/drag — the two interactions the idle path must preserve — are broken in normal approach-from-outside use (ring entry always precedes dot entry). Recommended fix (root cause): poll the exact dot rect for hover/alpha transitions AND click state for wake (e.g. wake when exact-dot hover changes OR `LeftMouseButtonDown()` becomes true); or drop the frozen-`hovered_` design and force a frame whenever `near_dot` bin changes between exact and inflated. Do not special-case one path.

### R-W2-003 [MINOR] Stale page_focus_pending_ can steal focus on a later keyboard press
- Location: `src/ui/MainWindow.cpp:389-391` (+ set sites :192,:400,:976) · Evidence: pending is only cleared on the first `NavActive` frame, never on mouse use. After a mouse rail-click or tray re-show, the flag waits; the first later Tab press then fires `SetKeyboardFocusHere()` at DrawRail, redirecting that Tab's navigation to the first rail button even if the user intended a content control. NavActive may also linger true across hide/show (no reset in SetVisible(false)). Why it matters: unexpected focus yank; violates least-surprise for mouse-first users. Recommended fix: clear `page_focus_pending_` on mouse click of a rail item / mouse input, or bound its lifetime (clear after N frames without NavActive), or only arm restore when the switch itself was keyboard-initiated.

### R-W2-004 [NIT] Unused public `WaveformRing::At` widens API without a caller
- Location: `src/core/OverlayModel.h:44`, `src/core/OverlayModel.cpp:57-64` · Evidence: `grep` in src finds only tests referencing `At`; Overlay uses `SnapshotInto`. Why it matters: dead contract surface invites divergent use; throwing accessor with no production caller. Recommended fix: either use `At` in the Listening bar loop (indexed reads it was designed for) or remove it until a caller exists (DO-LATER).

### R-W2-005 [NIT] Poll dot geometry assumes GLFWinSize == ImGui DisplaySize, ±1px truncation
- Location: `src/ui/Overlay.cpp:276-283` vs `DrawBadge:356-359` · Evidence: poll centers dot from `glfwGetWindowSize` with int casts; DrawBadge centers from `ImGui::GetIO().DisplaySize` float. Fractional DPI rounding can differ by a pixel. Imp implementer flagged this; impact is at most a one-frame hover mismatch *when combined with* R-W2-002 (today it contributes to the frozen-hover window). Not independently worth fixing beyond R-W2-002.

## 5. Attempts to break it
- Keyboard Enter on rail (traced vendored): ButtonBehavior pressed=true, IsItemClicked=false → page_ unchanged. Breaks A1; confirmed by code trace, not runtime.
- Mouse still switches pages (hovered=true + IsMouseClicked=true) → no regression for mouse.
- Idle: cursor approaches dot from outside → ring entry wakes once, dot entry does not (poll unchanged) → alpha stuck 0.55; click parked over dot → swallowed. Traced both state-transition sequences; verified `hovered_`/`UpdateDrag` only run inside `DrawBadge` after `NewFrame`, both skipped by early-out return.
- Dwell expiry (Done→Ready) while idle: SetState sets dirty_ → next frame full-renders → OK. Fade increment while `fade_alpha<1`: `fade_settled` false → full render each tick → OK. `SetState` same-state re-assert: returns early without dirty_ (correct, no repaint needed). `SetState(Attention)` from Attention re-arms dwell + dirty_ → OK.
- SetVisible: hide/show leaves pending armed (see MINOR). `copied_until_` after time wrap: `GetTime()` float + 1.5s; reset on new transcript; long dwell across UI pause — benign (dwell simply expires).
- `SnapshotInto` order across wraparound: scoped doctest `*waveform ring SnapshotInto*` passed (front 6.0/back 69.0 after 70 pushes, At bounds + throw). [RAN ./build/lwf_tests.exe → 3 passed, 20 assertions].
- `At(64)` after 64 filled throws out_of_range [VERIFIED test + OverlayModel.cpp:57-64]; negative/huge index: index is size_t, huge value fails `index >= filled_` → throw, no OOB read.

## 6. Dimension checklist
- Correctness: FAIL — R-W2-001 (rail nav activation), R-W2-002 (idle hover/click freeze). A3 copy/state, A6 tokens, SnapshotInto ordering, PushFont pairing otherwise correct.
- Root cause vs symptom: Rail bug is a cause-level error (ignored activation return); early-out bug is cause-level (poll domain wrong: inflated zone instead of exact dot + click). Fixes above address causes.
- Regression/compat: Mouse rail click, toggle, sliders, Welcome, Trust, Copy, dwell/fade, DPI, drag-while-dragging all preserved. No config/contract change; Snapshot() behavior unchanged.
- Concurrency/idempotency: Single-threaded main-loop contract; `dirty_`/`poll_near_dot_` non-atomic is consistent [VERIFIED Overlay.h contract comment]. No idempotency risk.
- Error handling: `At` throws (documented, tested); fallback font `Fonts[0]` guarded [VERIFIED Overlay.cpp:109-113]. No swallowed errors.
- Security/abuse: No auth/network/secrets surface; Trust `%s` safe; transcript `%s` safe. No new injection.
- Performance/scalability: Idle skip + scratch reuse + 1-entry cache are genuine wins; poll adds one OS call/frame. No new allocations beyond warm-up.
- Migration/rollback: No migration; single-commit reverts restore behaviour.
- Observability: No logging needed; regressions would show as dead rail keyboard / frozen badge — recommend manual keyboard + mini-hover checks in MANUAL_TEST_WINDOWS.md before re-merge.
- Maintainability/conventions: Comments (DECISION/INVARIANT) match project style; `At` unused (NIT); poll geometry assumption deserves a comment tying it to DrawBadge's near_dot.
- Docs impact: None required beyond manual-test checklist updates for keyboard-rail and idle-mini-click cases.

## 7. Optimization and simplification opportunities
| # | Location | Evidence | Impact | Effort | Risk | Verdict |
|---|---|---|---|---|---|---|
| 1 | `src/ui/Overlay.cpp:268-289` poll exact dot + click | R-W2-002 trace | Restores hover/click; idle cost unchanged (2 OS calls) | Small | Low | DO NOW |
| 2 | `src/ui/MainWindow.cpp:396` use InvisibleButton return | R-W2-001 trace | Restores keyboard page switch | One line | Low | DO NOW |
| 3 | `src/core/OverlayModel.h:44` use or remove `At` | grep: no src caller | Less dead API | Small | Low | DO LATER |
| 4 | Poll/displaySize unification | R-W2-005 ±1px | Negligible | Small | Low | NOT WORTH IT alone |

## 8. Cross-agent / cross-task interactions observed
- `saved_dwell` NEED correctly crossed the A1A3→A5A6 boundary: A1A3 deferred, A5A6 applied 1.5s to the shared token, so "Copied" + "Saved" dwells are consistent [VERIFIED project reports + ThemePalette.h:135]. No conflict.
- ThemePalette metrics widened window geometry (`mini_message_w` 140→250) read uniformly by Init/ApplyWindowGeometry/UpdateDrag — no call-site divergence [VERIFIED]. Overlay font additions do not touch MainWindow context discipline (separate context established at Init). MainWindow Confirm/validation patterns mesh with existing model-path commit path.

## 9. What I could not verify
- Live GUI behaviour (no display session here): actual ImGui focus ring pixels, Copied dwell timing, badge alpha visually, hover brightening, drag latency, 150% DPI poll parity, and tray re-show focus — all flagged as manual-hardware items per AGENTS.md. Findings R-W2-001/R-W2-002 are code-trace conclusions against vendored ImGui source, not runtime reproductions.
- Full ctest suite (scoped run only: 3 overlay test cases). Pre-existing source-scan CWD sensitivity (running exe from build/ fails) is pre-existing, not introduced here.
