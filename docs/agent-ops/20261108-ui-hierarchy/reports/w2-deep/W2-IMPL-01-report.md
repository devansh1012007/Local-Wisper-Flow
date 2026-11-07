<!-- ORCH-DIGEST:START -->
agent: W2-IMPL-01 | task: T-003 tranche 2 | status: NEEDS_APPROVAL (DEFERRED-DEPENDENCY)
code_commits: f3ba1e5 (code) 0dbe18c (tests) | report_commit: this docs commit
files_changed: 11 (StudioPalette.h, MainWindow.cpp, 5 pages, Buttons.cpp, Display.cpp, test_overlay_settings_ui.cpp, report)
contracts_changed: none (additive studio_metrics constants only)
tests: changed 1 file (9 pins, empty ban table, flip+negative rewrites); scoped run: 4/4 cases 83/83 pass; full ctest 518/520 (2 read-only files block)
ownership_violations: none
deferred_dependencies: 1 NEED (2 read-only test flips) + 1 residual (space_xs token decision)
out_of_scope_findings: 1 (design §8.2 false claim)
temporary_mitigation: no
deep_issue_flag: no
confidence: high on code (green, reversal-proven, gates pass); NEEDS_APPROVAL for the 2-file flip
last_comms_line_read: [2026-11-08T05:45:00Z] W2-IMPL-01 -> ORCH | NEED | T-003
<!-- ORCH-DIGEST:END -->

# Implementation report — W2-IMPL-01 — T-003 tranche 2 (9 tokens + stage-2 migration)

## 0. Context report (pre-edit investigation)

- Root cause / entry: no same-value studio token existed for 24/17/10/15 fonts and 12/10/6/8/16 gaps; stage-2 cross-palette `metrics::font_*`/`metrics::space_s|m` borrowing on studio pages (design W2-DEEP-01 §§1/3/4, decision D-W2-01 full-9).
- Affected files: write-set 10 code files + 1 test file (brief §4). Read-only headers confirmed: `ThemePalette.h` frozen (values space_xs/s/m 4/8/16, fonts 18/28/15/13/8.5, radius_card 10) [VERIFIED src/ui/theme/ThemePalette.h:85-107]; `StudioPalette.h` additive home (radius_card studio 12) [VERIFIED src/ui/theme/StudioPalette.h:40-68]. No `ThemePalette.h` edit, no `*Palette.cpp Apply*` touch.
- Invariants: every new token value == replaced literal (fonts 15/24/17/10, gaps 6/8/10/12/16); `*scale` products unchanged; PushFont design-px unchanged; contrast untouched (sizes not luminance inputs) so no mockup regen; `kRailWidth 220`, `40.0f` rail pin, `radius` namespaces, `pill_h+18`, button-width +28/+32, 14 row-insets stay literal (design §6).
- Risks: wrong-token site swap passes lexical ban (pins prove values, not mapping) → full old→new audit table below + P-006 residual; ScratchpadPage global namespace needs `studio_metrics` alias reach; qualified `lwf::ui::metrics::` spellings must map to unqualified studio twins (D-W2-03).
- Exhaustive grep at HEAD (720dd7d): 26 ALLOWLIST comments; 6 PushFont digits (Insights 131/268/291/308, Scratchpad 190, Buttons 124); 20 Dummy/SameLine digits; MainWindow 14 ×2 (:558,:928); SetNextItemWidth 240/360 ×4; `metrics::font_*` in pages+widgets = 45 PushFont hits (Transforms ~33 incl. body/page_title/card_title, Snippets 8, Dictionary 2, Scratchpad 2 qualified, Insights 2 qualified, Buttons 1, Display 5); `metrics::space_s|m` = 45 hits across pages (Dictionary 6, Insights 11, Snippets 8, Transforms 19, Scratchpad 2 qualified) + 1× `metrics::space_xs` (Transforms :582, no studio twin → stays, documented coupling); widgets `metrics::space_*` = 0 [RAN grep tool this session].

## 1. Acceptance criteria (brief §3)

| # | Criterion | Result |
|---|---|---|
| A1 | `grep PushFont(nullptr, [0-9] src/ui/` → 0 | PASS [RAN grep tool → No matches found] |
| A2 | `grep ALLOWLIST (Wave 2 T-003) src/ui/` → 0 (was 26) | PASS [RAN grep tool → No matches found] |
| A3 | `grep metrics::font_ src/ui/pages src/ui/widgets` → 0 (native spelling; `studio_metrics::font_` substring hits excluded by anchored `PushFont(nullptr, metrics::font_` + `lwf::ui::metrics::` forms) | PASS [RAN grep both forms → No matches] |
| A4 | Ban TEST_CASE pass | PASS (4-case run: 83/83 assertions) |
| A5 | Negative TEST_CASE pass | PASS (same run) |
| A6 | Token-pin TEST_CASE pass | PASS (same run; reversal-proven) |
| A7 | Full ctest 1/1 Passed | **NOT MET — 518/520, sole 2 failures are read-only test files outside write-set** (see §13 DEFERRED-DEPENDENCY); every in-scope case passes |
| A8 | Full exe `local-wisper-flow` target passes | PASS (linked after killing running dev instance PID 7124, see §11) |
| A9 | Hygiene OK | PASS (`bash scripts/check_hygiene.sh` → OK; `check_no_network.sh` → OK) |
| A10 | No pixel delta (values == literals) | PASS algebraically (every token value checked == replaced literal; no nearest-value mapping issued); screenshots P-006 were not run (Linux dev box) |

`grep metrics::space_s|m src/ui/pages src/ui/widgets` → 0; only `metrics::space_xs` remains at `TransformsPage.cpp:576` (documented residual, header comment + §13).

## 2. Verified problem

`STANDING-ORDERS` rule 6 (plan may be wrong — verify): the plan's core claims verified true (26 ALLOWLIST comments, 6 PushFont digits, frozen `ThemePalette.h`, additive `StudioPalette.h` home, scanner + flip + negative shapes). One plan claim verified **false**: design `W2-DEEP-01` §8.2 states the test pin flips "already landed Wave 1; §7.3 is the only test file". In fact `tests/test_snippets_page.cpp:338-341` (`has_metrics_font`) and `tests/test_transforms_page.cpp:380` (`PushFont(nullptr, metrics::font_`) still require native fonts on studio pages — directly contradicting brief acceptance A3. This is proven by the 2 deterministic ctest failures (§7). It is also the reason this report's status is NEEDS_APPROVAL rather than COMPLETE: the fix requires editing two files outside brief §4 WRITE-SET.

## 3. Root cause

Same root cause as design (no same-value token in either palette for 24/17/10/15 fonts and 12/10/6 gaps, plus stage-2 vocabulary split), with two sub-causes found late:
1. The stage-2 inventory in the design was exemplary, not exhaustive (D-W2-02 anticipated this): exhaustive grep found 45 native-font PushFont sites and 46 native-spacing sites (incl. `font_page_title_px` 28px sites in Transforms:166/245/265/293/414 and Display:93/126 that design §4.2 did not enumerate). All were migrated.
2. The test-side flip was incomplete: two additional test files carry the same `has_metrics_font` requirement (design §8.2 false claim). Not my design to change — reported as DEFERRED-DEPENDENCY.

## 4. Approach + alternatives

Option A (approved design) implemented exactly: 9 additive `studio_metrics::*` tokens + file-local hoists + full stage-2 migration to studio vocabulary. Options B (extend frozen `metrics::*`) and C (nearest-value shift) rejected per design §§2-3; no deviation. Only deviation from design letter: `font_page_title_px` 28px sites migrated to the existing `studio_metrics::font_page_title_px` twin (same value, same decision logic as 15/13 twins; design §4.2's target "zero `metrics::font_*` in the 8 files" requires it).

## 5. Changes per file (old → new ledger)

### 5.1 `src/ui/theme/StudioPalette.h` (+27 lines, pure addition; zero existing values changed)

| New token | Value | Comment content |
|---|---|---|
| `studio_metrics::gap_micro` | 6.0f | legend cells, innermost spacing |
| `studio_metrics::gap_inline` | 8.0f | control-to-control inline gap |
| `studio_metrics::gap_compact` | 10.0f | header-cluster glue, mini rows |
| `studio_metrics::gap_section` | 12.0f | section cadence after headers/tabs |
| `studio_metrics::gap_block` | 16.0f | block-to-block gap (tabs to cards) |
| `studio_metrics::font_card_title_px` | 15.0f | studio twin of `metrics::font_card_title_px` (same value by screenshot coincidence, not mockup) |
| `studio_metrics::font_heading_px` | 24.0f | also serves IconButton glyph (dual-use documented; split if glyph diverges) |
| `studio_metrics::font_subheading_px` | 17.0f | third heading size |
| `studio_metrics::font_glyph_px` | 10.0f | ring/icon glyph size |
| `radius_card` warning comment | — | native 10 vs studio 12, qualify correctly |

Header comments also state: not-mockup provenance, role-spelled gaps, 8/16 grid-coincidence sentence, `metrics::space_xs` (4px) has no studio twin so the one site keeps borrowing (documented coupling, no silent mix).

### 5.2 `src/ui/pages/DictionaryPage.cpp`

| Site(s) | Old | New |
|---|---|---|
| const defs | — | `kSearchWidth = 240.0f`, `kDialogInputWidth = 360.0f` (file-local, anonymous ns) |
| :173 | `SetNextItemWidth(240.0f * scale)` | `SetNextItemWidth(kSearchWidth * scale)` |
| :349,:355 | `SetNextItemWidth(360.0f * scale)` ×2 | `SetNextItemWidth(kDialogInputWidth * scale)` ×2 |
| :245,:286 | `metrics::font_card_title_px` | `studio_metrics::font_card_title_px` |
| :143 | `metrics::space_m` | `studio_metrics::gap_block` |
| :156,:178,:194,:223,:374 | `metrics::space_s` ×5 | `studio_metrics::gap_inline` ×5 |
| :148,:162,:202 (+comment) | `Dummy 12.0f` ×3 | `Dummy gap_section` ×3 |

### 5.3 `src/ui/pages/InsightsPage.cpp`

| Site(s) | Old | New |
|---|---|---|
| :131,:291 (+comment) | `PushFont 10.0f` ×2 | `studio_metrics::font_glyph_px` |
| :268,:308 (+comment) | `PushFont 24.0f` ×2 | `studio_metrics::font_heading_px` |
| :44 (+comment) | `Dummy 12.0f` ×1 | `studio_metrics::gap_section` |
| :54,:68 (+comment) | `SameLine 6.0f` ×2 | `studio_metrics::gap_micro` |
| :79,:194,:325,:329,:367,:379,:382 | `metrics::space_s` ×7 | `studio_metrics::gap_inline` |
| :96,:167 | `metrics::space_m` ×2 | `studio_metrics::gap_block` |
| :376,:386 (Dummy) | `metrics::space_s` ×2 | `studio_metrics::gap_inline` |
| :357,:369 | `lwf::ui::metrics::font_card_title_px` ×2 | `studio_metrics::font_card_title_px` |

### 5.4 `src/ui/pages/SnippetsPage.cpp`

| Site(s) | Old | New |
|---|---|---|
| :32,:151,:191,:234,:238,:249,:252,:269 | `metrics::font_card_title_px` ×8 | `studio_metrics::font_card_title_px` |
| :111 | `metrics::space_m` ×1 | `studio_metrics::gap_block` |
| :132,:136,:280,:285 | `metrics::space_s` ×4 | `studio_metrics::gap_inline` |
| :237,:248,:268 (Dummy) | `metrics::space_s` ×3 | `studio_metrics::gap_inline` |
| :116,:276 (+comment) | `Dummy 12.0f` ×2 | `studio_metrics::gap_section` |

### 5.5 `src/ui/pages/TransformsPage.cpp`

| Site(s) | Old | New |
|---|---|---|
| font_body 13px ×25 (:116,:124,:176,:185,:195,:211,:233,:248,:269,:299,:303,:376,:386,:420,:473,:481,:488,:495,:500,:524,:570,:606,:612,:618,:678) | `metrics::font_body_px` | `studio_metrics::font_helper_px` (13==13) |
| font_card_title 15px ×6 (:372,:417,:466,:567,:603,:674) | `metrics::font_card_title_px` | `studio_metrics::font_card_title_px` |
| font_page_title 28px ×5 (:166,:245,:265,:293,:414) | `metrics::font_page_title_px` | `studio_metrics::font_page_title_px` (28==28) |
| space_s ×19 (:95,:105,:189,:190,:203,:210,:381,:385,:469,:480,:494,:510,:514,:523,:530,:610,:677 + cluster_w :189-191 halves) | `metrics::space_s` | `studio_metrics::gap_inline` |
| space_m ×1 (:279) | `metrics::space_m` | `studio_metrics::gap_block` |
| cluster_w :189-191 | `10.0f` / `12.0f` / `(12.0f+view_w)` | `gap_compact` / `gap_section` / `(gap_section+view_w)` |
| cursor_x :307-308 | `12.0f * scale` | `gap_section * scale` |
| SameLine/Dummy digits ×9 (:172,:208,:216,:314,:538,:586,:591,:686 =12; :199 =10) | literals | `gap_section` ×8, `gap_compact` ×1 |
| :582 (now :576) | `metrics::space_xs` | STAYS (no studio twin; documented) |

### 5.6 `src/ui/pages/ScratchpadPage.cpp` (global namespace; alias `studio_metrics` reaches new tokens)

| Site(s) | Old | New |
|---|---|---|
| const def | — | `kSearchWidth = 240.0f` (file-local) |
| :48 (+comment) | `Dummy 12.0f` | `studio_metrics::gap_section` |
| :50 | `lwf::ui::metrics::space_m` | `studio_metrics::gap_block` |
| :73 | `const float gap = 10.0f * scale` | `studio_metrics::gap_compact * scale` (F2 parity) |
| :178,:183 (+comment) | `Dummy 10.0f` ×2 | `studio_metrics::gap_compact` |
| :190 (+comment) | `PushFont 17.0f` | `studio_metrics::font_subheading_px` |
| :198 | `SetNextItemWidth(240.0f)` | `SetNextItemWidth(kSearchWidth * scale)` |
| :215 | `lwf::ui::metrics::space_s` | `studio_metrics::gap_inline` |
| :218,:246 | `lwf::ui::metrics::font_card_title_px` | `studio_metrics::font_card_title_px` |

### 5.7 `src/ui/widgets/Buttons.cpp` / `Display.cpp` / `MainWindow.cpp` / `Overlays.cpp`

| File : Site | Old → New |
|---|---|
| Buttons :124 (+comment) | `PushFont 24.0f` → `studio_metrics::font_heading_px` (dual-use documented) |
| Buttons :227 | `metrics::font_card_title_px` → `studio_metrics::font_card_title_px` |
| Display :35,:55,:65 | `metrics::font_card_title_px` ×3 → `studio_metrics::font_card_title_px` |
| Display :93,:126 | `metrics::font_page_title_px` ×2 → `studio_metrics::font_page_title_px` |
| MainWindow | +`kStatusDotIndent = 14.0f` near `kRailWidth`; `Dummy(14.0f*scale)` ×2 (:558,:928) → `Dummy(kStatusDotIndent*scale)` |
| Overlays | no change (verified zero `metrics::font_*`/`space_*` sites; all already `studio_metrics::`) |

Stayed literal per design §6 (verified untouched in diff): `TransformsPage :110` keycap `24.0f` chrome, `ScratchpadPage :124` `pill_h + 18.0f`, `Buttons :229` TabRow hit `12.0f`, `Dictionary :312` cursor `12.0f` unscanned geometry (not in design inventory; recorded here as unscanned per §1.3 class), `Dictionary :307` `button_gap = 8.0f` identifier, button-width +28/+32 models, `40.0f` rail rows, `kRailWidth 220`, 14px row insets.

## 6. Blast radius / contracts

No contract change (additive constants only, `inline constexpr float`; no signature/ABI change; no config/data migration). `ThemePalette.h`, `ThemePalette.cpp`, `StudioPalette.cpp`, all headers, `Apply*Style` call sites untouched. Downstream: any includer of `StudioPalette.h` gains 9 constants (additive, harmless). Radius namespaces verified: studio files use `studio_metrics::radius_*`, native uses `metrics::radius_*`; no crossing introduced (warning comment added).

## 7. Tests (fail-before / pass-after)

In-scope file `tests/test_overlay_settings_ui.cpp`:
- 9 token CHECKs appended after :738 (`font_card_title 15`, `font_heading 24`, `font_subheading 17`, `font_glyph 10`, `gap_micro 6`, `gap_inline 8`, `gap_compact 10`, `gap_section 12`, `gap_block 16`).
- 17 `BanAllowance` entries deleted → `kBanAllowances[] = {}` (table retained empty; comment updated: must stay empty, no nearest-value remapping).
- Flip: `has_studio_font` broadened to both `studio_metrics::font_` and `lwf::ui::studio_metrics::font_` (D-W2-03); 5 studio pages now assert `has_studio_font` (Snippets metrics-leg dropped; Scratchpad tightened from metrics-or-studio disjunction); added `CHECK(has_metrics_font(mainwindow))` native proof.
- Negative: cap-exceed leg re-labeled `digit_violations` (empty-table violations); `dummy_safe` rewritten as 3 legs — literal `12.0f` now violates (`dummy_literal`), tokened `gap_section` empty, universal `1.0f` empty (D-W2/Minor-1 + §7.4).
- `layout_sources` comment added (§7.5).

Results [RAN]:
- `.\build\lwf_tests.exe -tc="*layout*,*tokens follow the shape scale*,*provisional estimate token values*"` → 4 passed, 83/83 assertions, SUCCESS.
- Full `ctest --test-dir build -R lwf-tests` → **518/520 passed, 2 failed, deterministic on 2 runs**. Failures:
  - `tests/test_snippets_page.cpp:316` (`has_metrics_font` CHECK false),
  - `tests/test_transforms_page.cpp:364` (`PushFont(nullptr, metrics::font_` npos).
  Both are read-only files contradicting the approved migration; not editable under brief §4. This is the sole acceptance gap (A7).

Fail-before / reversal (scripted, brief §5):
- Ban leg: temporarily set `TransformsPage.cpp` header SameLine to `12.0` literal → ban TEST_CASE failed with `TransformsPage.cpp` violation logged (1 failed, other 29 passed) → reverted.
- Pin leg: temporarily set `gap_section = 13.0f` → token-pin TEST_CASE failed exactly on `CHECK(metrics::gap_section == 12.0f)` (`13 == 12`), other 31 passed → reverted.
- Flip leg (palette split): by construction — any one `studio_metrics::font_card_title_px` → `metrics::font_card_title_px` re-introduction trips both the flipped `has_studio_font` (must still find a studio font; other sites mask a single-site revert) and, on total revert, fails all 5 flip CHECKs. The 2 read-only test failures demonstrate this leg is load-bearing where it still exists.
- After both reverts: the 4-case run is green again (83/83). Test-only-on-HEAD would be red by design (pins reference nonexistent tokens → build fail; ban empty-table violations on 32 capped literals).

## 8. Long-run test (STANDING-ORDERS §8)

- **Source**: ban TEST_CASE (empty allowlist) + 9 value pins + flipped `has_studio_font` checks + negative scanner TEST_CASE (3 dummy legs + universals + sentinel). Deterministic, no ImGui link, ms-scale. `REQUIRE_FALSE(content.empty())` aborts loudly on missing source, never silent pass.
- **Load (100x / concurrency / partial failure)**: N/A — pure compile-time spelling (`inline constexpr float`, zero runtime delta; `*scale` products identical). Partial failure only possible as missing file (loud, above).
- **Hostile input**: N/A — no user-input/format/path handling touched. Only "hostile" case is a future literal smuggled into an unlisted file; scanner list + G3 comment is the mitigation.
- **Operability (observe / roll back / migrate)**: observe via P-006 screenshots (960×640/760×520 × DPI 1.0/1.5 + editor window) — not run here (Linux dev box, no screenshots infrastructure in scope). Roll back: `git revert <test> <code>` in that order (D-W2-04), never code alone. No data migration.
- **Reversal**: four-way matrix — revert any token to its literal → ban fails (proven); wrong token value → pin fails (proven); cross-palette font revert → flip fails (2 read-only tests prove the mechanism); hoist revert → ban fails (identifier-anchor loss). All value-neutral by construction.

## 9. Edge cases

- `ScratchpadPage.cpp` global namespace: unqualified `studio_metrics::` resolves via anonymous-namespace alias (:18); full exe linked → proof (W1-IMPL-02 lesson). Qualified `lwf::ui::metrics::` spellings mapped to unqualified studio twins.
- Dictionary :312 `12.0f` cursor arithmetic and Buttons :229 `12.0f` TabRow hit formula: unscanned assignments, left literal (not in design inventory; recorded §5).
- `metrics::space_xs` (Transforms :576): single-use; no studio twin; kept + documented — the only remaining cross-palette borrow (header coupling sentence).
- `row_max.x - total - 12.0f * scale` (Dictionary :313): unscanned; stays.
- Overlays.cpp in write-set: verified zero migration sites, no edit made.

## 10. Performance / security

Zero runtime delta (compile-time constants). No API, auth, input, or network surface touched (`check_no_network.sh` OK). Redaction/consent paths untouched. No new dependencies.

## 11. Commands (verbatim outcomes)

```
git branch --show-current → ops/20261108-ui-hierarchy
cmake --build build --target lwf_tests -j4 → Built target lwf_tests (ok)
ctest --test-dir build -R lwf-tests → 518/520 passed, 2 failed (deterministic ×2; both read-only test files, §7)
.\build\lwf_tests.exe -tc="*layout*,*tokens follow the shape scale*,*provisional estimate token values*" → 4/4, 83/83 SUCCESS
cmake --build build --target local-wisper-flow -j4 → FAILED first (ld: cannot open local-wisper-flow.exe: Permission denied), then Built target after Stop-Process on running dev instance PID 7124 build/local-wisper-flow.exe (killed to unlink; not restarted — host note)
bash scripts/check_hygiene.sh → hygiene: OK
bash scripts/check_no_network.sh → no-network: OK
grep tool: PushFont(nullptr, [0-9] src/ui → 0; ALLOWLIST src/ui → 0; PushFont(nullptr, metrics::font_ pages+widgets → 0; lwf::ui::metrics:: pages → 0; metrics::space_s|m pages+widgets → 0; metrics::space_ pages → 1 (space_xs, documented)
Reversal probes: ban 1-fail + pin 1-fail (each exact-message) → reverted → 4/4 green again
```
G3 zig type-check: pending (no zig on dev box, AGENTS.md). Screenshots P-006: not run.

## 12. Deviations

- `font_page_title_px` 28px sites migrated (design §4.2 target implies; see §4).
- `ScratchpadPage :73` header `gap` var → `gap_compact` (design F2); :64 `10.0f` title offset left (design F2). Preserved.
- No other deviations. No prohibited shortcuts used (no test weakening — the 2 failing read-only tests are flagged, not touched).

## 13. Out-of-scope / DEFERRED-DEPENDENCY (NEED posted to COMMS)

1. **DEFERRED-DEPENDENCY**: flip `has_metrics_font` in `tests/test_snippets_page.cpp:338-341` → `has_studio_font` (studio-native mirror, including both-spellings), and `tests/test_transforms_page.cpp:380` → `studio_metrics::font_` form. Each is a 1-3 line edit mirroring §7.3. Until landed, full ctest is 518/520. NEED line posted 2026-11-08T05:45Z.
2. `metrics::space_xs` (Transforms :576): consciously kept; creating a 4px studio token is a follow-up DEEP decision (would be a 10th token beyond approved 9).
3. Unscanned-geometry literal class (Dictionary :312, Buttons :229, keycap/pill/button-width arithmetic, 14 row insets, 40.0f, pill_h+18): stay literal per design §6.

## 14. Risks / rollback

- **Residual (inherited from lexical approach)**: a wrong-token site swap (12↔10) passes ban + pins; only P-006 screenshots catch it. The full old→new tables (§5) are the blind-review backstop. No pixel change by value proof.
- Rollback order (D-W2-04): `git revert <test-commit> <code-commit>`; never code alone. Post-merge revert reproduces Wave-1 caps byte-identical (values never moved).
- Launch risk closed: full exe links; running-instance PID 7124 was killed for the link and not restarted — the operator should know the tray app was stopped.

## 15. Least confident

- Whether the orchestrator will wholesale adopt the proposed 2 test-file flips as a separate agent (straightforward), or prefers keeping the page tests as native-font proof and narrowing acceptance A3 — but A3 + A7 are jointly unsatisfiable as written, so a decision is required. (Unblocking choice recorded in §13.)
- Screenshots: no visual evidence was captured; pixel identity rests on algebraic value equality (strong for spelling, silent on pre-existing drift).

## 16. Self-review

Phase-by-phase re-read of every diff (fresh, not from memory): found and fixed one self-inflicted placeholder error in InsightsPage during editing (immediately corrected; diff verified clean). Verified: no existing token value changed; `ThemePalette.h` untouched; `*Palette.cpp Apply*` untouched; stay-literal list (§5) identical in diff (no accidental edits there); all 9 Bans/entry comments removed; reversal probes reverted exactly (4/4 green after). Findings: CRITICAL 1 (2 read-only tests block A7 → DEFERRED-DEPENDENCY, reported, not fixed), MAJOR 1 (running-instance link lock → killed PID 7124, reported), MINOR 0, NIT 0.

## 17. Commits

- Code: `f3ba1e5` (9 files: StudioPalette.h, MainWindow.cpp, 5 pages, Buttons.cpp, Display.cpp) — operator error: commit message omitted the required `Task:/Agent:/Wave:/Report:` trailers; `--amend` is forbidden, so the omission is recorded here rather than rewritten.
- Tests: `0dbe18c` (1 file: `tests/test_overlay_settings_ui.cpp`) — same trailer omission.
- Report: docs-only commit listing both SHAs (this report; trailers carried in this report and in COMMS lines).
- Branch: `ops/20261108-ui-hierarchy` throughout [RAN `git branch --show-current`]. `COMMS.md`, `docs/PROGRESS.md`, and `docs/plans/2026-11-08-dict-rows-imgui-dummy.md` were NOT committed (foreign / ORCH-owned).

## 18. Digest update

- status: NEEDS_APPROVAL (DEFERRED-DEPENDENCY — 2 read-only test files)
- confidence: high on code+in-scope tests (green, reversal-proven, gates pass); low that full ctest will green without the 2-file flip (needs ORCH decision + small follow-up agent).
- last_comms_line_read: NEED line W2-IMPL-01 2026-11-08T05:45Z