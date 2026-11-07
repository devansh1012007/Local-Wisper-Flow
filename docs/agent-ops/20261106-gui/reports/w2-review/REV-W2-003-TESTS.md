<!-- ORCH-DIGEST:START -->
agent: W2-REV-TESTS | scope: wave-2 test changes (d9e86dd A5/A6 + 3aa4329 A1/A3 coverage absence) | verdict: APPROVE-WITH-CHANGES
tests_reviewed: 3 (new in d9e86dd) + 0 for A1/A3 delta
fail-before/pass-after confirmed: 2 / reasoned: 1 / not-possible: 0
findings: BLOCKER 0 | MAJOR 1 | MINOR 3 | NIT 1
flaky_suspects: none (3 repeated scoped runs, identical 3/3 pass, 20/20 assertions)
missing_edge_cases: P1 1 | P2 4 | P3 2
mutation_survivors: 9 of 15 plausible bugs not caught (8 of 9 from the untested A1/A3 delta + A5 early-out)
changed_behavior_without_tests: A1 keyboard-nav opt-in, A3 slider commit-on-deactivate, A3 Copied dwell, A3 Trust cloud-branch, A5 idle early-out (dirty_ gating)
confidence: high (fail-before verified against parent-commit sources; pass verified by 3 scoped runs; no full-suite re-run per scoped-check rule, brief states ctest already green)
<!-- ORCH-DIGEST:END -->

# Test quality review — W2-REV-TESTS (REV-W2-003-TESTS)

## 0. Method

- Read `docs/agent-ops/20261106-gui/templates/STANDING_ORDERS.md` and
  `review-tests.md`; read both wave-2 impl reports
  (`reports/w2-impl/W2-IMPL-A5A6-T-005.md`, `W2-IMPL-A1A3-T-002.md`).
- Diffed `d9e86dd` (6 files, incl. `tests/test_overlay_settings_ui.cpp` +56) and
  `3aa4329` (2 files, `MainWindow.{cpp,h}` only, 0 test files) via `git show`.
- Fail-before verified by inspecting parent-commit sources
  (`git show d9e86dd^:src/ui/theme/ThemePalette.h`, `...:src/ui/Overlay.cpp`,
  `d9e86dd^:src/core/OverlayModel.h`, `3aa4329^:src/ui/MainWindow.cpp`) — no
  checkout/stash used (forbidden in shared tree, and no source edits per brief).
- Pass-after: [RAN `./build/lwf_tests.exe --test-case="*mini badge alphas*,*waveform ring
  SnapshotInto*,*mini status word*"` ...] — 3 cases, 20 assertions, all pass,
  reproduced 3 times total (1 verbose + 2 quiet runs). [RAN from repo root; the
  source-scan case resolves `src/ui/Overlay.cpp` relative to cwd, same as wave-1
  precedent; ctest runs from source root.]
- No temporary worktree (read-only review, no edits to make); no coverage tooling
  (doctest build, no gcov in scope); mutation analysis is by reasoning over the
  diff, not by running a mutation tool — flagged as such in §7.

## 1. Per test/suite assessment

| Test | Asserts behavior? | Fails without fix? | Deterministic/isolated? | Over-mocked? | Notes |
|---|---|---|---|---|---|
| `mini badge alphas meet the mockup contrast floors` (test_overlay_settings_ui.cpp:307) | Yes — reads the actual `metrics::` tokens the renderer consumes; these are the contract values, not copies of the code | Yes, confirmed: parent has 0.35/0.65/140/1.6 [VERIFIED `git show d9e86dd^:ThemePalette.h`]; all five CHECKs fail pre-fix (0.35<0.55, 0.65<1.0, 140<250, 1.6!=1.5) | Yes — pure constexpr reads, no state, no I/O | No | Only token-level test in the batch. Weakness: floors not exact pins (see R-W2-003-002). |
| `waveform ring SnapshotInto and At match Snapshot ordering` (:321) | Yes — true unit test of the new portable API: wraparound order (70 pushes into cap 64 → front 6), Snapshot==SnapshotInto sample-for-sample, At() endpoints, `std::out_of_range` past newest, buffer reuse after another push | Yes, compile-fail-before: parent `OverlayModel.h` declares only `Snapshot()` [VERIFIED `git show d9e86dd^:OverlayModel.h`]; new members absent. Implementer's "correct fail-before signal" claim holds | Yes — stack-local ring, no shared state, no clock | No | Strongest test in the batch. Gaps: partial-fill (`filled_ < capacity`, the `start=0` branch) and pre-filled-scratch clear unproven (§6 P2). |
| `mini status word is drawn at badge alpha without a discount` (:345) | Implementation detail (source-scan) — but justified: `lwf_tests` does not link ImGui (comment cites this, :346), and wave 1 established exactly this guard pattern for the same constraint (T-001 font scan, fda449e) | Yes, confirmed: parent Overlay.cpp contains `Pack(kDarkPalette.muted, 0.75f * badge_alpha)` and no `PushFont(message_font_, ...)` [VERIFIED `git show d9e86dd^:Overlay.cpp`]; CHECKs 1, 3, 4 fail pre-fix, CHECK 2 fails pre-fix too (old string present) | Yes — read-only file read; fails as REQUIRE if cwd ≠ source root (pre-existing suite-wide trait, noted by A5A6 §13; ctest cwd is correct) | No (no mocks at all in this suite) | Ordering not asserted (PushFont before CalcTextSize, PopFont after) unlike the wave-1 gate test which asserts `find < find`. `PopFont()` appears exactly once in Overlay.cpp (:600) so its removal is caught. A re-introduced discount with a different constant survives (R-W2-003-003). |
| (A1/A3 — no test) | — | — | — | — | 62-line MainWindow delta ships with zero regression guard. See R-W2-003-001 (MAJOR) and §6 P1. |

## 2. Coverage of changed lines/branches, error paths, boundaries

**d9e86dd (A5/A6).** Portable (testable) surface changed: 4 token values in
`ThemePalette.h`, `WaveformRing::SnapshotInto/At` in `OverlayModel.cpp`.
Coverage: tokens 4/4 asserted; `SnapshotInto` order/reuse asserted, `At`
endpoints + throw asserted. Overlay.cpp-only changes (idle early-out `dirty_`
gating, status-word 1-entry cache, second-font PushFont scope) are covered only
by one lexical scan (status word) — the early-out and cache have **no test at
all**, not even a scan: a regression that drops a `dirty_ = true` setter (stale
badge, swallowed hover/drag wakeup) or forgets `cached_status_valid_ = false`
on DPI re-apply (stale text size) passes everything. Error path `At` throw is
covered; `SnapshotInto` has no error path (clear+reserve+push, cannot fail
except bad_alloc).

**3aa4329 (A1/A3).** All changes are inside ImGui draw functions
(`MainWindow.cpp`), so no portable-logic unit test is possible without an
ImGui-linked harness — but the run's own established pattern for exactly this
constraint is the source-scan guard (wave-1 T-001). Every A1/A3 behavior has
an unambiguous lexical marker: `NavEnableKeyboard` (:152),
`ImGuiButtonFlags_EnableNav` (2 sites, :67/:396), focus ring
`Pack(kDarkPalette.focus)` gated on `IsItemFocused()`,
`SetKeyboardFocusHere` + `NavActive` (:389-390), `copied_until_` + `"Copied"`
(:675-678), `IsItemDeactivatedAfterEdit` on both cleanup sliders (:833/:844),
`ApplyAndSave()` in `DrawWelcomePage` (:934/:972), `validation_error_.clear()`
on rail click (:399), `stt_provider != "local"` Trust branch. None is
asserted anywhere. Fail-before for such a scan is available for free: parent
`MainWindow.cpp` contains none of `NavEnableKeyboard`/`copied_until_` and only
the one pre-existing `IsItemDeactivatedAfterEdit` (model-path site)
[VERIFIED]. This is the same situation wave 1 closed for A2 via gate markers
(fda449e) — wave 2 left A1/A3 open.

**Boundary note.** `saved_dwell == 1.5f` (exact) is the one indirect A1/A3
guard: the Copied dwell reads `metrics::saved_dwell` (:678), so the token
value is pinned — but its *use* (dwell deadline, reset in `SetLastTranscript`)
is untested.

## 3. Mutation thinking

| Plausible bug | Caught? | By which test |
|---|---|---|
| `mini_idle_alpha` reverted to 0.35 | Yes | alpha floors |
| `saved_dwell` back to 1.6 | Yes | alpha floors (exact `==`) |
| SnapshotInto off-by-one (`start = head_` when not full, wrong modulo) | Yes — wraparound front==6/back==69 + At endpoints pin both branches' outputs for the full case | waveform test |
| `At` bounds `>` instead of `>=` (index==filled_ reads garbage) | Yes | `CHECK_THROWS_AS(At(64))` |
| SnapshotInto forgets `out.clear()` | Yes — second call would leave size 128, `REQUIRE(size==64)` fails | waveform test (reuse half) |
| Status-word discount reintroduced as `0.8f * badge_alpha` | **No** — only the literal `"0.75f * badge_alpha"` is forbidden; presence CHECK passes on the new code regardless | — (R-W2-003-003) |
| `PushFont(message_font_, ...)` deleted (message at 8.5px) | Yes | status-word scan |
| `PopFont()` deleted (font stack leak) | Yes — single `PopFont()` site in Overlay.cpp | status-word scan |
| `NavEnableKeyboard` removed / `EnableNav` flags dropped | **No** | — (R-W2-003-001) |
| Sliders re-wrapped in `if (SliderInt(...))` (per-tick save returns) | **No** | — (R-W2-003-001) |
| `copied_until_` dwell deleted / reset removed | **No** | — (R-W2-003-001) |
| Trust cloud-branch removed (hard-coded local copy) | **No** | — (R-W2-003-001) |
| `dirty_` setter dropped in `SetState`/`PushLevel` (stale idle badge) | **No** | — (§6 P2) |
| `cached_status_valid_ = false` dropped on DPI apply (stale size) | **No** | — (§6 P2) |
| `PushFont`/`PopFont` order swapped or PushFont moved before CalcTextSize… actually after: `font_body_px` must wrap both Calc and AddText | **No** — scan checks presence only, not ordering | (R-W2-003-004) |

9 of 15 survive; 8 of the 9 survivors come from the untested A1/A3 delta plus
A5's early-out/cache. The A5/A6 *portable* changes are well covered; the
*ImGui-wiring* changes are not.

## 4. Flakiness evidence and suite health

- [RAN scoped 3×] `./build/lwf_tests.exe --test-case="*mini badge alphas*,*waveform
  ring SnapshotInto*,*mini status word*"` → 3/3 cases, 20/20 assertions, three
  consecutive runs (1 verbose, 2 quiet), zero flakiness signal. No timing,
  randomness, or shared state in any new test; the scan tests are pure file
  reads.
- Suite-health trait (pre-existing, not introduced): source-scan cases require
  repo-root cwd (`REQUIRE` on open) — running the exe from `build/` fails
  them; ctest runs from the source root and passes (brief: `ctest -R lwf-tests`
  already green; not re-run here per scoped-check rule). A5A6 already
  recorded this as a FINDING; I concur it is a MINOR/P3 hygiene item, not a
  wave-2 regression.
- No order dependence introduced: WaveformRing tests use local instances;
  token tests are constexpr reads.

## 5. Findings

**R-W2-003-001 (MAJOR) — A1/A3 behavior ships with zero tests, against the
wave-1 established guard pattern.** Location: `3aa4329` (MainWindow.cpp,
62 added lines); no owning test file. The A1A3 agent's lack of test
ownership explains the absence but the run closed A2 with source-scan gate
markers (fda449e, closing R-W1-003-001) for the identical constraint
("lwf_tests does not link ImGui"). The sibling test-owner (A5A6) did not add
the MainWindow markers the A1A3 report §7 explicitly handed over as
"mutation rationale for the sibling". Highest-risk unguarded behaviors:
slider commit-on-deactivate (a refactor re-wrapping `if (SliderInt(...))`
silently restores per-tick saves — the actual A3 bug), NavEnableKeyboard/
EnableNav opt-in (keyboard nav silently dead again), Copied dwell, Trust
branch. Recommended fix: append a source-scan TEST_CASE to
`tests/test_overlay_settings_ui.cpp` asserting, over `src/ui/MainWindow.cpp`:
`NavEnableKeyboard`, two `ImGuiButtonFlags_EnableNav` sites (count occurrences),
`SetKeyboardFocusHere` + `NavActive` with ordering (`NavActive` before
`SetKeyboardFocusHere`), `copied_until_` + `"Copied"`,
`IsItemDeactivatedAfterEdit` count ≥ 3 (model-path + 2 sliders) and absence of
`if (ImGui::SliderInt(` on the cleanup sliders, `validation_error_.clear()`
count ≥ 2, `stt_provider`. All markers verified present post-fix and absent
pre-fix, so fail-before is available. Not a BLOCKER: changes are additive
ImGui wiring, compile+hygiene green, no portable logic touched, and no
current defect is evidenced — the risk is silent regression, not shipped
breakage.

**R-W2-003-002 (MINOR) — Alpha/width asserts are floors, not mockup pins.**
`mini_idle_alpha >= 0.55f` (test :312) and `mini_message_w >= 250.0f` (:316)
pass for any larger value, so silent drift (e.g. 0.9, or a window 2× too wide)
stays green while violating the mockup pins (55%/100%, 250) the comment
cites. `mini_active_alpha` is effectively pinned (≥1.0 ∧ ≤1.0) and
`saved_dwell` is exact `==`, so the file already mixes both styles.
Recommended: `== 0.55f` / `== 250.0f` (or keep floors with a comment stating
the floor is intentional headroom). A5A6's acceptance text said "alpha floor
assertions", so this is arguably by-brief; flagged MINOR, not MAJOR.

**R-W2-003-003 (MINOR) — Status-word scan forbids one literal, not the
class.** Forbidding `"0.75f * badge_alpha"` (:355) lets any other sub-1.0
multiplier (`0.8f * badge_alpha`, `badge_alpha * 0.75f`) through. Recommended:
also forbid `* badge_alpha` occurrences other than the bare
`Pack(kDarkPalette.muted, badge_alpha)` site — e.g. count occurrences of
`badge_alpha)` with a preceding multiplier, or simply assert the scan region
between the status-word comment and `DrawAttentionMessage` contains exactly
one `Pack` call. Small, cheap hardening consistent with wave-1 gate style.

**R-W2-003-004 (NIT) — PushFont/PopFont ordering not asserted.** The scan
checks both strings exist anywhere in Overlay.cpp, not that PushFont precedes
CalcTextSize and PopFont follows AddText in `DrawAttentionMessage`. Wave 1's
gate test asserted the analogous ordering (`WindowRounding` assignment <
`ScaleAllSizes`). Recommended: index comparisons like the pre-existing
`palette_content.find(...) < palette_content.find(...)` pattern.

**R-W2-003-005 (NIT) — `WaveformRing` empty/zero-capacity SnapshotInto/At
untested.** `At` on an empty ring throws (correct, by inspection:
`0 >= filled_=0`); `SnapshotInto` on empty writes nothing (`filled_==0` skips
the loop, no modulo-by-zero). Behavior is safe, and capacity-0 is unreachable
in production (`WaveformRing{64}` in Overlay; `Push` on a 0-capacity ring
would be UB but pre-exists d9e86dd and is out of scope). P3 only.

## 6. Missing edge cases and tests (prioritized)

**P1 (must add — the R-W2-003-001 scan).** Scenario: any A1/A3 behavior
regresses during a future MainWindow refactor. Case table: for each marker in
R-W2-003-001, expected = present (post-fix) / absent (pre-fix `3aa4329^` — a
reviewer can verify fail-before by grepping the parent). Where: append one
TEST_CASE `main window keyboard nav and commit-on-deactivate wiring` to
`tests/test_overlay_settings_ui.cpp`, modeled on the wave-1 gate block
(:275-304) including ordered `find < find` checks. Sketch: open
`src/ui/MainWindow.cpp`, `REQUIRE` open, then `CHECK(find("NavEnableKeyboard")
!= npos)`, count `EnableNav` ≥ 2 via repeated `find` from an advancing offset,
`CHECK(find("NavActive") < find("SetKeyboardFocusHere"))`,
`CHECK(find("if (ImGui::SliderInt(") == npos` for the cleanup sliders (careful:
the model-path input may legitimately differ — scope the absence check to the
segment after `DrawCleanupPage`), `CHECK(find("copied_until_") != npos &&
find("\"Copied\"") != npos)`, `CHECK(find("validation_error_.clear()") != npos)`.

**P2 (should add).**
1. Waveform partial-fill branch: push 5 values into cap-64, assert
   `SnapshotInto` == {0,1,2,3,4} order and `At(4)`==4, `At(5)` throws. The
   `start = 0` branch (filled < capacity) is currently exercised by *no* test
   in the file (both old and new waveform tests push past capacity).
2. `SnapshotInto` onto a pre-filled scratch (e.g. 100 sentinels): assert size
   becomes exactly `filled_` and no sentinel survives — pins `out.clear()`
   directly rather than via the size-after-reuse proxy.
3. A5 early-out/cache scan (same pattern as P1, Overlay.cpp): assert
   `dirty_ = true` appears in `SetState`, `PushLevel`, `UpdateDrag`;
   `cached_status_valid_ = false` appears at the DPI-apply sites. Weaker than
   a render test but matches the run's guard standard and catches the
   stale-badge class.
4. Alpha test: switch floors to exact pins per R-W2-003-002 (or document
   floors as intentional).

**P3 (nice to have).**
1. Empty-ring `SnapshotInto`/`At(0)` throw test (R-W2-003-005) — documents
   the zero-sample contract.
2. cwd-robustness for source-scan tests: resolve `src/ui/...` relative to a
   compile-time source dir macro or `__FILE__`-derived root so running the
   exe from `build/` also passes (pre-existing, suite-wide; do not fix here,
   note for ORCH backlog).

## 7. What I could not verify

- Fail-before by *execution* (running the new tests against a pre-fix tree):
  impossible without checkout/stash, forbidden in the shared tree. Fail-before
  is instead verified by parent-commit source inspection (all markers/values
  absent-or-different pre-fix) — strong for lexical/token tests, and the
  waveform test's compile-fail-before is likewise structural. Counting: 2
  confirmed (alpha by parent values, status-word by parent strings), 1
  reasoned (waveform compile-fail).
- Live GUI behavior of any w2 change (focus landing spot, Copied dwell,
  early-out wakeups, 13px message font) — no display run in this environment;
  same limitation recorded in both impl reports.
- Full `ctest -R lwf-tests` — not re-run (scoped-check rule; brief states it
  is already green). I ran only the 3 new cases, 3 times.
- Mutation results are reasoned, not tool-run (no mutation tooling in the
  repo). §3's "survives" verdicts mean "no existing test would fail", verified
  by reading every CHECK in the file, not by injecting the mutation.
