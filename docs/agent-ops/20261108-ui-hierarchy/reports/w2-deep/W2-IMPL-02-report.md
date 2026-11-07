<!-- ORCH-DIGEST:START -->
agent: W2-IMPL-02 | task: T-003 follow-up (wave 2.5) | status: DONE
findings: FIXED 2 | REJECTED 0 | DEFERRED 0
blockers_major_open: none
code_commits: d088cf4 | report_commit: <sha>
tests_added_or_strengthened: 0 (2 existing pin CHECKs flipped to studio tokens, no assertions weakened or deleted)
ownership_violations: none
confidence: high
<!-- ORCH-DIGEST:END -->

# Improvement report — W2-IMPL-02 — T-003 test-pin flips + space_xs comment

## 1. Dispositions

| Finding | Disposition | Detail |
|---|---|---|
| `tests/test_snippets_page.cpp:338-341` required `PushFont(nullptr, metrics::font_` (native) after the approved studio migration removed all of them | FIXED | Flipped to `has_studio_font` asserting anchored `PushFont(nullptr, studio_metrics::font_` or `lwf::ui::studio_metrics::font_` — mirrors the approved `has_studio_font` lambda in `tests/test_overlay_settings_ui.cpp:1291-1296` exactly, including both spellings. Comment block updated to state the studio requirement (it previously claimed "both vocabularies are required", now false). No assertion deleted; the broader `PushFont(nullptr, studio_metrics::` CHECK at :337 and the `15.0f`/`13.0f` digit bans at :342-343 remain intact. |
| `tests/test_transforms_page.cpp:380` required `PushFont(nullptr, metrics::font_` (native) | FIXED | Same single CHECK flipped to `PushFont(nullptr, studio_metrics::font_` (anchored form). Positive studio-token assertion, nothing deleted. |
| `src/ui/pages/TransformsPage.cpp:576` `metrics::space_xs` (4px) borrow unexplained; no studio twin exists | FIXED (documented) | One-line comment added above the `:577` Dummy line: intentional native grid reuse, no studio twin, this site alone keeps the native token. No value change, no 10th token, no pixel change. |

## 2. Code changes

`src/ui/pages/TransformsPage.cpp` (+1 line, comment only, at :576):
```cpp
// Intentional native grid reuse: metrics::space_xs (4px) has no studio twin; this site alone keeps the native token.
```

## 3. Test changes

- `tests/test_snippets_page.cpp`: `has_metrics_font` → `has_studio_font`; the two `find` needles now `PushFont(nullptr, studio_metrics::font_` / `PushFont(nullptr, lwf::ui::studio_metrics::font_` (same both-spellings disjunction as the approved overlay lambda). Variable rename is semantics-preserving apart from the intended vocabulary flip.
- `tests/test_transforms_page.cpp`: one needle change `metrics::font_` → `studio_metrics::font_`.

Verify-then-edit: grep confirmed `[VERIFIED tests/test_snippets_page.cpp:338-341]` and `[VERIFIED tests/test_transforms_page.cpp:380]` matched the brief's descriptions exactly, so no line-trust deviation. Source-side presence also verified by grep: `SnippetsPage.cpp` carries 11 `PushFont(nullptr, studio_metrics::font_*` sites and `TransformsPage.cpp` carries 37, so the flipped assertions genuinely match live code, not a vacuous needle.

## 4. Optimizations

None. Zero runtime delta (comments + test needles only).

## 5. Commands

```
git branch --show-current → ops/20261108-ui-hierarchy
cmake --build build --target lwf_tests -j4 (before edits) → Built target lwf_tests
ctest --test-dir build -R lwf-tests (before edits) → 1/1 Test #1 ***Failed, 19.78s
  fail-before: test_transforms_page.cpp:380 CHECK(...metrics::font_...) NOT correct;
  scoped .\build\lwf_tests.exe -tc="*snippets*,*transforms*" → 20 cases | 18 passed | 2 failed, 193 assertions | 191 passed | 2 failed
cmake --build build --target lwf_tests -j4 (after edits) → Built target lwf_tests (2 test TUs recompiled; TransformsPage.cpp is comment-only, token-identical)
ctest --test-dir build -R lwf-tests (after edits) → 1/1 Test #1 Passed, 20.11s (520/520 total green)
.\build\lwf_tests.exe -tc="*snippets*,*transforms*" → 20 cases passed, 193/193 assertions SUCCESS
bash scripts/check_hygiene.sh → hygiene: OK
```

Reversal: the fail-before run above is the reversal proof — reverting either flipped needle to `metrics::font_` re-introduces the exact pre-edit failure (ctest 518/520, 2 CHECKs false). Fail-before evidence: ctest 518/520 → 520/520.

G3 zig type-check: pending (no zig on dev box, per AGENTS.md). Not needed for test-needle edits; the comment-only C++ change is token-identical for the compiler.

## 6. Residual risks

- None from this change. The mechanical "wrong token" concern from W2-IMPL-01 is unchanged: the flipped needles assert anchored spelling presence and would catch a wholesale native-font reintroduction on these two pages, but a value-wrong same-vocabulary swap would still need screenshots (P-006, out of scope).

## Commits

- Code: `d088cf4` — tests/test_snippets_page.cpp, tests/test_transforms_page.cpp, src/ui/pages/TransformsPage.cpp
- Report: filled in after the docs-only commit (SHA known to orchestrator via git log)
- Branch: ops/20261108-ui-hierarchy. Foreign dirty files (docs/PROGRESS.md, docs/agent-ops/.../COMMS.md, docs/plans/2026-11-08-dict-rows-imgui-dummy.md) left untouched and uncommitted.
- Last COMMS line read: W2-IMPL-01 NEED 2026-11-08T05:45Z (superseded by this task's completion).
