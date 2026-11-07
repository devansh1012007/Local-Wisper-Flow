<!-- ORCH-DIGEST:START -->
agent: W2-REV-DELTA | scope: wave 2 delta 07cc957 (+44542e2 docs) | verdict: APPROVE-WITH-CHANGES
findings: BLOCKER 0 | MAJOR 0 | MINOR 1 | NIT 2
claims_audit: 14 claims checked / 1 unsupported (LeftMouseButtonDown literal is not fail-before)
band_aids_found: none
top_3_risks: live Enter/hover/click/drag paths unverified without display session; rail click edge moved press→release (standard button semantics, needs hardware sanity check); source-scan guards are cwd-relative and marker-brittle by design
optimizations: DO-NOW 0 | DO-LATER 0 | NOT-WORTH-IT 1
reversal_check: reasoned-only (single-commit revert; new test markers fail loudly on revert)
confidence: med (static diff + full-context reads + scoped test run; live GUI behavior needs MANUAL_TEST_WINDOWS.md hardware, same limit as implementer)
<!-- ORCH-DIGEST:END -->

# Delta review — W2-REV-DELTA (REV-W2-004-DELTA)

## 0. Method
Blind pass first over `git show 07cc957` (4 files) against full surrounding
context in `src/ui/MainWindow.cpp`, `src/ui/Overlay.cpp/.h`,
`src/os/WindowStyles.cpp`, `src/core/OverlayModel.cpp`; fail-before markers
checked against `07cc957^` and `3aa4329^` trees; then the improver report
`W2-IMPR-T-002.md` read and claim-audited. Read-only; no source edits.
Commands run: scoped doctest selection (see §9).

## 1. Blind-pass findings (before reading implementer reports)

- Rail: `MainWindow.cpp:407-416` switches page on the `InvisibleButton`
  return instead of `IsItemClicked()`. This is correct and consistent with
  the pre-existing precedent in the same file (`ToggleSwitch` at
  `MainWindow.cpp:67` already switches on the return value) [VERIFIED
  MainWindow.cpp:67]. Side effect worth noting: the mouse activation edge
  for the rail moves from press (`IsItemClicked` = press + hovered) to
  release (ImGui `ButtonBehavior` pressed), i.e. press-then-drag-off no
  longer switches pages. That is standard ImGui button semantics and the
  right call, but it is a behavior change that hardware should sanity-check
  (click vs click-drag-off).
- `MainWindow.cpp:391-393`: global `IsMouseClicked` clears
  `page_focus_pending_` at the top of `DrawRail`. `DrawRail` runs every
  Full-window frame, so the clear also covers clicks in the content area,
  not just the rail — coherent with the stated intent. Ordering (clear
  before the `NavActive` restore check at `:394-397`) means a same-frame
  click beats a pending restore, which is the correct priority.
- `page_focus_pending_ = true` has exactly two armers post-fix
  [VERIFIED MainWindow.cpp:192 SetVisible, :991 Welcome-Finish], matching
  the new test's `== 2` pin.
- Idle poll [VERIFIED Overlay.cpp:281-309]: the triple-verdict condition
  (`poll_near`, exact-dot `poll_hover_dot`, physical `poll_button_down`)
  covers the three wake cases: pass-through-zone crossings, intra-zone
  dot entry/exit (hover alpha change), and parked-cursor button press.
  Dwell/fade bookkeeping runs before the early-out [VERIFIED
  Overlay.cpp:250-260] and dwell-expiry goes through `SetState`, which sets
  `dirty_` [VERIFIED Overlay.cpp:157], so the Done/Attention dwell still
  wakes. Dragging bypasses the early-out (`!dragging_`), and the drag
  release path is OS-polled in `UpdateDrag` [VERIFIED Overlay.cpp:563], so
  drag start/continue/release are all preserved. `dismissal` of
  `dirty_` only happens after a real frame renders [VERIFIED
  Overlay.cpp:343].
- `WaveformRing::At` is genuinely oldest-first and identical in convention
  to `SnapshotInto` order [VERIFIED OverlayModel.cpp:44-59]; the Listening
  loop change [VERIFIED Overlay.cpp:437-441] indexes `At(Size-5+i)` where
  the old code read `levels[levels.size()-5+i]` — same index math, and the
  `Size()>=5` guard replaces an equal-strength `levels.size()>=5` guard
  since `levels` is the snapshot of the same ring. Not a behavior change.
- One test-spec gap found (→ R-W2-004-001): the early-out guard's
  `LeftMouseButtonDown()` substring check passes on the pre-fix tree
  because `UpdateDrag` already contained that call [VERIFIED
  Overlay.cpp:563]; it does not discriminate the poll wiring on its own.
- Poll verdicts are stored only on the early-out path; when the app exits
  and re-enters the early-out with stale stored values there can be one
  spurious wake frame. Harmless, bounded.

## 2. Claims audit

| Implementer claim | Verified? | Evidence / finding |
|---|---|---|
| R-W2-001 (rail Enter) fixed in `MainWindow.cpp:398-401` via `const bool pressed` | Yes | [VERIFIED MainWindow.cpp:407-416 at HEAD] diff shows capture + `if (pressed)`; `IsItemClicked` survives only in a comment [VERIFIED grep] |
| "Verified unfixed in HEAD before editing [MainWindow.cpp:396-397]" | Yes | `07cc957^` tree contains `if (ImGui::IsItemClicked())` and no `const bool pressed` [RAN git show 07cc957^] |
| R-W2-002 (idle poll triple verdict) at `Overlay.cpp:290-301`, members in `Overlay.h:79-87` | Yes | [VERIFIED Overlay.cpp:295-307, Overlay.h:79-87] diff + HEAD |
| "Verified unfixed in HEAD before editing [Overlay.cpp:268-289]" | Yes | Pre-fix poll had a single `IsCursorInsideRect(` call (poll) + one in `DrawBadge` [RAN git show 07cc957^]; post has 3 [RAN git show HEAD] |
| R-W2-003 (stale `page_focus_pending_`) — mouse clear + rail-press disarm | Yes | [VERIFIED MainWindow.cpp:391-393, :412-415]; armers exactly 2 [VERIFIED MainWindow.cpp:192, :991] |
| Frame-bounded lifetime declined because remaining arms are focus-lost cases | Reasonable | [ASSUMED] matches `SetVisible`/Welcome-Finish semantics; no artifact contradicts |
| R-W2-004 (`WaveformRing::At` used in Listening loop, no dead API) | Yes | [VERIFIED Overlay.cpp:434-441] + oldest-first convention [VERIFIED OverlayModel.cpp:44-59] |
| R-W2-005 closed by R-W2-002 change (comment documents ±1px self-correction) | Yes | Comment exists [VERIFIED Overlay.cpp:275-279]; one-frame overshoot claim is consistent with per-frame re-poll — [ASSUMED] hardware-confirmable |
| R-W2-003-001 (P1 source-scan test) fail-before vs `3aa4329^` | Yes | `3aa4329^:MainWindow.cpp` lacks `NavEnableKeyboard`, `copied_until_`, `EnableNav`, `SetKeyboardFocusHere`; has `IsItemClicked` + exactly one `IsItemDeactivatedAfterEdit` [RAN git show 3aa4329^] — matches the test's own comment |
| R-W2-003-002 (exact pins `==0.55f/1.0f/250.0f`) | Yes | [VERIFIED tests/test_overlay_settings_ui.cpp:307-318] |
| R-W2-003-003 (status-scan class guard) | Yes | `* badge_alpha` occurs exactly once [VERIFIED Overlay.cpp:408]; `badge_alpha * ` absent [VERIFIED grep]; old `0.75f * badge_alpha` literal remains forbidden by construction (count would be 2) — no coverage loss |
| R-W2-003-004 (PushFont ordering scoped to `DrawAttentionMessage`) | Yes | Order `PushFont(612) < CalcTextSize(613) < AddText(620) < PopFont(622)` [VERIFIED Overlay.cpp:604-623]; test scopes to `substr(fn)` |
| R-W2-003-005 (partial/empty/scratch ring test) | Yes | New TEST_CASE exists [VERIFIED tests:388-417]; semantics match `SnapshotInto`/`At` impl [VERIFIED OverlayModel.cpp:44-59] |
| Early-out guard "also pins R-W2-002 markers" (P2.3) | Partially false | `poll_hover_dot_` checks and `IsCursorInsideRect >= 3` ARE fail-before, but the `LeftMouseButtonDown()` substring check passes pre-fix (pre-existing use in `UpdateDrag` [VERIFIED Overlay.cpp:563]). See R-W2-004-001 |
| "7 passed, 69 assertions" + `ctest 1/1 Passed` + hygiene OK | Yes (re-run scoped) | [RAN `.\build\lwf_tests.exe --test-case="*mini badge alphas*,*waveform ring*,*mini status word*,*idle early-out*,*keyboard nav*"` -> 7 passed, 69 assertions, 0 failed] Binary contains new cases (224+ test-case list lines). Full `ctest` not re-run (shared-env scope); improver's ctest claim [ASSUMED] still accurate given the scoped suite passes on the same binary |
| "Assertion count for the touched cases: 69 total (was 20)" | Plausible | Scoped run confirms 69 now [RAN above]; "was 20" not independently re-derived — [ASSUMED] |
| "Ownership violations: none" / write-set discipline | Yes | `git show 07cc957 --stat`: exactly `MainWindow.cpp`, `Overlay.cpp/.h`, `tests/test_overlay_settings_ui.cpp`; `44542e2` is docs-only single file. Branch `fix/gui-typography-dpi` [RAN git branch] |
| "Code commit 07cc957; this report follows in a separate docs-only commit" | Yes | `44542e2` = report only [RAN git show 44542e2 --stat] |
| Residual risk: "Poll runs before NewFrame in RenderFrame, so yes structurally" | Yes | [VERIFIED Overlay.cpp:301-309 (poll) precedes :336 `ImGui::NewFrame()`] |
| `IsMouseClicked` at `DrawRail` top is global by design | Yes | [VERIFIED MainWindow.cpp:391] |

## 3. Long-run test (independent answers)

- **Source:** Rail fix is at the cause (activation return, same as the
  file's own `ToggleSwitch` precedent), not a keyboard special-case; idle
  poll fixes the wrong-domain poll rather than rendering unconditionally.
  No band-aid constructs found (no sleeps, no test weakening — the literal
  scan was widened to a class check, floors became pins).
- **Load:** Poll adds one `GetCursorPos` + one `GetAsyncKeyState` per idle
  frame; O(1), no allocations. The `At()` change trades a snapshot-index
  read for an accessor call; immaterial. No new scaling hazard.
- **Hostile:** `At()` is bounds-checked (throws at `index >= filled_`)
  and guarded by `Size()>=5`; truncation in `poll_dot_px` can only
  overshoot by a frame, per the comment. No new untrusted input surface.
- **Operability:** Press→release rail semantics change and all live
  input paths (Enter/Space, hover brighten, parked click, drag-from-parked)
  still need a MANUAL_TEST_WINDOWS.md hardware pass; there is no display
  session in this environment [NOT-RUN — needs shared env/hardware].
- **Reversal:** Reverting 07cc957 removes the markers the new source-scan
  tests pin (so the tests fail loudly on revert), and the poll members
  revert with the header. [reasoned-only — revert not executed, read-only brief]

## 4. Findings

### R-W2-004-001 [MINOR] Early-out guard's `LeftMouseButtonDown()` marker is not fail-before
- Location: `tests/test_overlay_settings_ui.cpp` ("mini idle early-out wakeup wiring is guarded") · Evidence: the guard asserts `content.find("LeftMouseButtonDown()")`, but pre-fix `Overlay.cpp` already contains that call in `UpdateDrag`'s release check [VERIFIED Overlay.cpp:563]. Removing the poll's button verdict would not fail this assertion. · Why it matters: the improver report claims the guard (P2.3) "also pins R-W2-002 markers"; one of those markers silently lacks discriminating power. The test case as a whole still fails pre-fix via `poll_hover_dot_` / `IsCursorInsideRect >= 3`, so this is a spec-precision gap, not a silent pass. · Recommended fix: assert the poll-scoped form instead, e.g. `poll_button_down = WindowStyles::LeftMouseButtonDown()` (the assignment site), so the marker exists only in the poll block.

### R-W2-004-002 [NIT] Rail mouse activation moved from press-edge to release-edge
- Location: `src/ui/MainWindow.cpp:407-416` · Evidence: old `IsItemClicked()` fires on mouse-down over the item; `ButtonBehavior`'s `pressed` fires on release-inside. Same as pre-existing `ToggleSwitch` [VERIFIED MainWindow.cpp:67], and standard ImGui guidance. · Why it matters: press-then-drag-off no longer switches pages (arguably a fix), but it is a behavior delta the hardware checklist should cover (click vs click-drag-off while keyboard-focused cases) so nobody later "fixes" it back to press-edge.

### R-W2-004-003 [NIT] Button verdict wakes on any desktop click, not only near the dot
- Location: `src/ui/Overlay.cpp:300-308` · Evidence: `LeftMouseButtonDown()` is global; pressing the button anywhere (e.g. in another app) flips the verdict and costs one frame render, then settles (held button doesn't retrigger since state is stored). · Why it matters: trivial extra-frame cost, bounded; gating on `poll_near` would narrow it but would reintroduce the parked-over-dot race the fix exists to close. Leave as-is; noted only so the over-eager wake is a recorded decision, not an accident.

## 5. Attempts to break it

- A1/A3+P1 fail-before: re-grepped `3aa4329^:src/ui/MainWindow.cpp` — all behavioral markers absent, pre-existing `IsItemClicked` + single `IsItemDeactivatedAfterEdit` present [RAN]. The new P1 test fails pre-fix on multiple independent checks.
- R-W2-002 fail-before: pre-fix tree has 2 `IsCursorInsideRect(` sites and no `poll_hover_dot_` [RAN git show 07cc957^] — early-out guard fails pre-fix via those checks (and only those; see R-W2-004-001).
- Stale-arm interleavings reasoned: same-frame click+arm → click-clear runs first (ordered before restore) [VERIFIED MainWindow.cpp:391-397]; mouse-click in content area → `DrawRail` still runs every frame so the global clear catches it; keyboard-only flow → `IsMouseClicked` false, no interference.
- Parked-click interleaving reasoned: press edge wakes the same-tick frame (poll precedes `NewFrame` [VERIFIED Overlay.cpp:301-336]), drag-bypass keeps it rendering until release, release path is OS-polled [VERIFIED Overlay.cpp:563].
- Intrazone hover: ring→dot crossing flips exact-dot verdict → wake → `hovered_` recomputed via `IsItemHovered` [VERIFIED Overlay.cpp:393]; leaving the dot while still inside the inflated zone flips it back. No stuck-alpha path found.
- `badge_alpha * ` false-positive check: legitimate `badge_alpha *=` lines use `*=` without the trailing-space form, so the guard doesn't collide [VERIFIED Overlay.cpp:402-404 + grep].
- `At()` ordering equality verified index-for-index against `SnapshotInto` (oldest-first, `start = filled < cap ? 0 : head_`) [VERIFIED OverlayModel.cpp:44-59].

## 6. Dimension checklist

| Dimension | Verdict | Notes |
|---|---|---|
| Correctness | Pass | All four product fixes are semantically sound; marker counts (`page_focus_pending_ = true` ×2, `IsItemDeactivatedAfterEdit` ×3, `* badge_alpha` ×1, `IsCursorInsideRect` ×3) independently confirmed |
| Root cause vs symptom | Pass | Wrong poll domain; wrong activation API; stale-arm lifetime — all addressed at the invariant level |
| Regression / compat | Pass (with nit) | No weakened tests (literal→class widening + pins are strengthenings); press→release edge change is standard semantics, recorded as NIT |
| Concurrency / idempotency | Pass | Poll verdicts are plain per-frame bools on the render thread; stored state can't wedge (worst case one spare frame) |
| Error handling | Pass | `At()` throw is guarded; no new fallible calls |
| Security / abuse | Pass | No new inputs, no logs, no secrets |
| Performance / scalability | Pass | O(1) poll, no new allocs |
| Migration / rollback | Pass | Single-commit reversal; tests fail loudly on revert (reasoned) |
| Observability | Reasonable for scope | Comments cite finding IDs (R-W2-001/002/003/005) so future readers can trace rationale |
| Maintainability / conventions | Pass (1 minor) | Source-scan guards are the established repo pattern (wave-1 T-001 gate); R-W2-004-001 tightens one marker; cwd-relative paths are a known suite-wide trait (P3, out of scope) |
| Docs impact | Pass | `44542e2` docs commit matches the code commit |

## 7. Optimization and simplification opportunities

| # | Location | Evidence | Impact | Effort | Risk | Verdict |
|---|---|---|---|---|---|---|
| 1 | `Overlay.cpp:295-299` (two `IsCursorInsideRect` calls doing 2× `GetCursorPos`) | Each call re-reads the cursor via `GetCursorPos` [VERIFIED WindowStyles.cpp:42-58, Applied ×2 in poll +1 in DrawBadge]; a single sampled point could be reused | ~1 redundant syscall per idle frame | Small refactor touching tested geometry | Low | NOT-WORTH-IT — idle-path cost is already negligible; churn against a just-landed fix isn't justified |

## 8. Cross-agent / cross-task interactions observed

- The P1 guard pins wave-1 A1/A3 markers (`NavEnableKeyboard`, `copied_until_`, commit-on-deactivate), coupling this delta's test to earlier wave work — intentional per the report, and the fail-before evidence was re-verified against `3aa4329^`, not just assumed.
- The exact-dot poll depends on `WindowStyles::LeftMouseButtonDown` + `IsCursorInsideRect` [VERIFIED WindowStyles.cpp:42-82]; both read live OS state, so the fix only matters on Windows — consistent with the rest of the overlay design (Linux stubs return `false`). No cross-task breakage seen.
- `SetState`/`PushLevel`/`UpdateDrag` remain the `dirty_ = true` producers [VERIFIED lines 157,162,167,175,188,199,533,557,578]; the `>= 3` count guard in the test is weak but directionally correct (deletion of any one setter could still leave ≥3; the polling verdicts are the real bug class here, and the exact-dot pins cover them).

## 9. What I could not verify

- Live GUI behavior: real Enter/Space rail page switch, ring→dot hover brighten, parked Paused click resume, drag start from parked cursor, click-vs-click-drag-off on the rail. No display session in this environment; needs MANUAL_TEST_WINDOWS.md hardware run [NOT-RUN — needs shared env/hardware]. The improver report discloses the same limit.
- Full `ctest` suite (only the 7 scoped cases were re-run; improver's `ctest 1/1 Passed` 17.42 s claim is [ASSUMED] accurate — the binary passes the scoped selection which includes all 3 new + strengthened cases, on a binary built from current sources).
- `bash scripts/check_hygiene.sh` green (improver-reported; not re-run — my diff-adjacent files were only read).
- Exact "20 assertions before" figure for the touched cases (69 now confirmed [RAN]).
