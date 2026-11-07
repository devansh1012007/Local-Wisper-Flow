<!-- ORCH-DIGEST:START -->
agent: W1-REV-A2T | scope: wave 1 / task T-001 (GUI-A2) | verdict: APPROVE-WITH-CHANGES
tests_reviewed: 1 | fail-before/pass-after confirmed: 1 / reasoned: 0 / not-possible: 0
findings: BLOCKER 0 | MAJOR 1 | MINOR 2 | NIT 1
flaky_suspects: none
missing_edge_cases: P1 1 | P2 2 | P3 1
mutation_survivors: 3 of 6 plausible token/mechanism bugs not caught
changed_behavior_without_tests: SetWindowFontScale removal (AC1); scoped PushFont + single-DPI wiring; token rename mini_text_px -> font_mini_text_px
confidence: high (token-value layer verified; mechanism-level coverage absent by design until ImGui is linkable into lwf_tests)
<!-- ORCH-DIGEST:END -->

# Test quality review — W1-REV-A2T (T-001 / GUI-A2)

## 0. Method

- Read-only audit on the shared tree; branch `fix/gui-typography-dpi`; no source edits, no test edits, no commits, no worktree used (per brief ownership).
- Required reading followed in order: (1) `docs/agent-ops/20261106-gui/templates/STANDING_ORDERS.md`; (2) `AGENTS.md`, `docs/plans/2026-11-06-gui-fix-improvements.md`, `docs/agent-ops/20261106-gui/00-mission.md`; (3) `docs/agent-ops/20261106-gui/reports/w0-investigation/INV-tests-font-theme.md`; (4) `docs/agent-ops/20261106-gui/reports/w1-impl/W1-IMPL-A2-T-001.md` (after blind pass over diff + context); (5) `tests/test_overlay_settings_ui.cpp` (L171-228) and `git diff 42a7ea0f..b21e40f -- tests/test_overlay_settings_ui.cpp`.
- Blind-first honored in execution order: the added diff, the current test file, `src/ui/theme/ThemePalette.h`, `CMakeLists.txt` (lwf_tests linkage), and `docs/design/mockup.html` type scale were all read before the implementer report was weighed.
- Coverage tooling: not used (no gcov/llvm-cov coverage config surfaced for the `lwf_tests` target in this repo); verification was behavioral via the doctest binary and source-state via git.

## 1. Per test/suite assessment

Scope of review: the only test added/changed for T-001 is the single new `TEST_CASE("design px font tokens follow the mockup type scale")` in `tests/test_overlay_settings_ui.cpp:221-228`.

| Test | Asserts behavior? | Fails without fix? | Deterministic/isolated? | Over-mocked? | Notes |
|---|---|---|---|---|---|
| `design px font tokens follow the mockup type scale` (added; `tests/test_overlay_settings_ui.cpp:221-228`) | Partially. It asserts token constants (`lwf::ui::metrics::font_*_px`) rather than rendered UI behavior. For the agreed ImGui-free pattern this is the sanctioned proxy, and the brief itself framed the bar as "assert behavior/values not 'doesn't throw'". Values are hardcoded expectations, so impl drift fails the test. | Confirmed via source state: at base `42a7ea0` the token family `font_*_px` did not exist (only legacy `metrics::mini_text_px = 8.5f`, `ThemePalette.h:113`), so the added case cannot compile -> fails before; at `b21e40f` the tokens exist and the case passes -> fails-after logic holds as a compile-fail-before. | Yes. Pure `inline constexpr float` comparisons, no I/O, no global state, no threads, no randomness. | No. No mocks involved (constants only). | Exact float `==` against exactly-representable literals (18.0f, 28.0f, 15.0f, 13.0f, 8.5f) is valid; no epsilon needed. No `INFO()` on failure paths (token tests are value-pin tests; cosmetic only). |

Neighbouring cases in the same file (`theme palettes keep the mockup token values`, contrast policy, WCAG math) were reviewed as context only — they are unchanged by T-001 and out of the added/changed diff.

## 2. Coverage of changed behavior, error paths, boundaries

Changed behavior under review (T-001):
1. New design-px token family in `lwf::ui::metrics` (`font_wordmark_px=18`, `font_page_title_px=28`, `font_card_title_px=15`, `font_body_px=13`, `font_mini_text_px=8.5`) replacing magic ratios via `ImGui::SetWindowFontScale`.
2. Removal of obsolete `SetWindowFontScale` from `src/ui/` (AC1: zero references in src/ui).
3. Scoped `ImGui::PushFont(nullptr, design_px)` + `PopFont` wiring in `MainWindow.cpp`/`Overlay.cpp`; single DPI application via `style.FontScaleMain`.

Coverage assessment:
- Token values: covered by the added case (all five asserted with hardcoded literals).
- Boundaries: asserted values include fractional design px (`8.5f`) and integer design px; exact equality is appropriate.
- Error paths: N/A for compile-time constants (no runtime input path).
- Changed behavior NOT covered by any test (see Findings R-W1-002T-A2-01): AC1 obsolete-API removal, scoped mechanism wiring, DPI single-apply, token-set completeness. The test cannot cover these without ImGui linkage, which `lwf_tests` lacks (`CMakeLists.txt:145` links `lwf_core` + `doctest` only).

## 3. Mutation thinking

| Plausible bug (off-by-one, inverted condition, wrong default, …) | Caught? | By which test |
|---|---|---|
| Off-by-one on an asserted token value (e.g. wordmark 18 → 18.5/17, mini 8.5 → 8/9) | Yes | Design-px token case (exact `==` vs exactly-representable literals) |
| Wrong token value chosen for an asserted size (e.g. body 13 → 14) | Yes | Design-px token case (hardcoded expectations) |
| Missing token / broken rename (`font_mini_text_px` absent, legacy `mini_text_px` shape wrong) | Yes (compile-level) | Design-px token case (new-name references) |
| Call site wired to the wrong token (e.g. card-title token pushed for wordmark) | No | — (mechanism-level; token test cannot see call sites) |
| Double DPI application (scoped design px combined with style scaling twice) | No | — (needs ImGui context) |
| Unbalanced `PushFont`/`PopFont` at a call site | No | — (needs ImGui context) |
| Token set incompleteness (a mockup size such as s4 = 14 left untokenized/untested) | No | — (test asserts only the five constants; no mockup-scale completeness check) |

**Mutation survivors: 3 of 6** plausible token/mechanism bugs not caught by this test set (call-site token wiring, scoped-font balance, DPI single-apply). All other mutations in the asserted surface are caught.

## 4. Flakiness evidence and test-suite health

- `[RAN] ctest --test-dir build -R lwf-tests` -> `1/1 Test #1: lwf-tests ... Passed 16.13 sec`; `100% tests passed out of 1`; Total Test time (real) = 16.15 s.
- `[RAN] build/lwf_tests.exe` -> `[doctest] test cases: 216 | 216 passed | 0 failed | 0 skipped`; `[doctest] assertions: 9568 | 9568 passed | 0 failed`; `[doctest] Status: SUCCESS!`.
- Flakiness: none suspected — deterministic compile-time constant comparisons, no I/O, no time/random/state dependence. Two full-binary runs produced the same green summary.
- Suite health: scoped `lwf-tests` green; suite duration ~16 s. Full `ctest` not run (standing orders: scoped checks only).

## 5. Findings

- **R-W1-002T-A2-01 — MAJOR (confirmed) — changed behavior without tests:** the core of T-001 (removal of obsolete `SetWindowFontScale` across `src/ui/`, AC1; scoped `PushFont`/`PopFont` wiring; single-DPI application) has **no automated test coverage**. Grep of `tests/` for `SetWindowFontScale|ApplyImGuiStyle|FontScaleMain|PushFont|FontSizeBase|FontWindowScale` returns zero test hits; the W0 investigation (`INV-tests-font-theme.md:18`) and the implementer both noted this gap. The impl verified AC1 only via manual `git grep` post-change. Consequence: reintroducing the obsolete API (or a stale font-scale path) would not fail CI. Recommended fix: add an ImGui-free regression test (e.g., source scan of `src/ui/` for the obsolete token / or a new ImGui-free header-level assertion set). Severity for the *test change itself* is minor, but for the *task's acceptance coverage* it is MAJOR.
- **R-W1-002T-A2-02 — MINOR (confirmed) — mockup type-scale conformance gap:** the test is named `"design px font tokens follow the mockup type scale"` but asserts only five token values; the mockup type scale (`docs/design/mockup.html:137-141`) defines s1=28, s2=18, s3=15, s4=14, s5=13 plus mini 8.5. The mid-scale body token (s4, 14) is not represented in the asserted set — `font_body_px = 13` merges mockup body (s4) and helper (s5), a discrepancy already recorded in the impl report §14 as known. The test therefore cannot detect a token-set drift at the 14-px step. Recommended fix: encode the approved token-to-mockup mapping explicitly (e.g., add/adjust assertions for body vs helper and the s4 scale step).
- **R-W1-002T-A2-03 — MINOR (confirmed) — token-family completeness:** the case asserts the new `font_*_px` names, so a rename slip is compile-detected, but absence of the legacy `metrics::mini_text_px` cannot be asserted behaviorally in the same translation unit; a stale alias re-added alongside the new family would not be caught. Recommended fix: static-assert/compile-time negative check or a source-level token-set test.
- **R-W1-002T-A2-04 — NIT (confirmed) — token test ergonomics:** the case uses bare `CHECK(... == ...)` with no `INFO()` context and could pin constness via `static_assert`; cosmetic, non-blocking.

## 6. Missing edge cases and tests (prioritized)

- **P1 (must add):** Obsolete-API regression guard — scenario: any `SetWindowFontScale` reintroduction or stale font-scale state under `src/ui/`. Input/case: source scan / grep-style assertion over `src/ui/**`. Expected: zero occurrences. Where: `tests/`. Ready-to-write: doctest source-scan over `src/ui` headers/impls (filesystem read) or a new ImGui-free token-guard test.
- **P2 (should add):** Mockup type-scale completeness — scenario: mid-scale (s4=14) / helper-vs-body token mapping drift. Case table: wordmark 18, page title 28, card title 15, body/helper 13, mid-scale 14. Expected: per approved design decision. Where: same token TEST_CASE.
- **P2 (should add):** Scoped mechanism / DPI single-apply behavior — blocked on ImGui linkage for `lwf_tests` (agreed pattern keeps token tests ImGui-free); needs a follow-on ImGui-context test or a source-level check of PushFont arg values.
- **P3 (nice to have):** Compile-time constness assertions (`static_assert`) for the design-px token family and explicit negative check for the legacy token name.

## 7. What I could not verify

- Executing a pre-fix build to observe the compile failure directly (standing orders forbid checkout/stash; the fail-before was derived from git source state at `42a7ea0` instead). Treated as a source-verified compile-fail-before.
- Vendored ImGui source (`_deps-src/imgui`) re-read directly by this reviewer (only via the impl/investigation reports).
- Runtime pixel measurement of effective scaled sizes (deferred; impl report §15).
- Hygiene / full-suite gates beyond the scoped `lwf-tests` run (standing orders: scoped checks only).

## Final verdict

The added test is deterministic, correctly pinned to hardcoded design-px expectations, covers all five token values including the fractional mini-text size, and its fail-before/pass-after logic holds via source-verified token absence/presence. The suite is green (`216/216` cases, `9568/9568` assertions). Residual quality issues are coverage gaps rather than defects in the added test: (a) no automated coverage for the AC1 obsolete-API removal / scoped mechanism (MAJOR for task acceptance), (b) incomplete mockup type-scale encoding (MINOR), (c) token-family completeness (MINOR), plus small NITs. Verdict: **APPROVE-WITH-CHANGES**.
