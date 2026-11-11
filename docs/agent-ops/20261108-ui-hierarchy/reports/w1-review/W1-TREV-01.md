<!-- ORCH-DIGEST:START -->
agent: W1-TREV-01 | scope: Wave 1 test changes (T-004: tests/test_overlay_settings_ui.cpp, test_scratchpad_page.cpp, test_snippets_page.cpp) | verdict: APPROVE-WITH-CHANGES
tests_reviewed: 3 (1 new TEST_CASE + 2 flipped TEST_CASEs) | fail-before/pass-after: 0 live / reasoned 3 (HEAD-blob counts, Deviation 4) / not-possible 0
findings: BLOCKER 0 | MAJOR 1 | MINOR 4 | NIT 1
flaky_suspects: none
missing_edge_cases: P1 1 | P2 3 | P3 2
mutation_survivors: 7 of 10
changed_behavior_without_tests: none (value-pinned tokens at test_overlay_settings_ui.cpp:238,730; site-to-token mapping unpinned - residual)
confidence: high (counts re-verified; fail-before has no live RED run)
<!-- ORCH-DIGEST:END -->

# Test quality review — W1-TREV-01 (wave 1, T-004)

Blind pass first: read STANDING-ORDERS, AGENTS.md gates, 00-mission, the full
`git diff 2c06061..HEAD -- tests/` (3 files, +332/-14), then the three test
files in context. Read the implementer report (W1-IMPL-03-T-004.md) and COMMS.md
only after forming independent findings. READ-ONLY on source/tests; writes only
this report.

## 0. Method

- Diff review at line level; no existing TEST_CASE modified in
  test_overlay_settings_ui.cpp (single +272 append hunk after F9 copy-lint) —
  A2 verified by diff shape. Scratchpad/snippets diffs are helper + pin flip only.
- Live runs (scoped, brief section 7):
  - [RAN cmake --build build --target lwf_tests --parallel 4 -> Built target lwf_tests, zero warnings]
  - [RAN lwf_tests --test-case="full window layout APIs use tokens not digit literals outside the token-gap allowlist" --success -> 1 passed, 29 assertions passed]
  - [RAN lwf_tests --test-case="snippets page sources stay off the font scale ban and carry the brief copy" --success -> 1 passed, 41 assertions passed]
  - [RAN lwf_tests --test-case="scratchpad page source wires..." --success -> 1 passed, 36 assertions passed]
  - [RAN ctest --test-dir build -R lwf-tests -> 1/1 Passed 19.82s] x3 consecutive runs (flakiness probe)
  - [RAN new ban + flipped pins with workdir build/ -> 29/29 passed] (A4, touched tests only)
  - [RAN --test-case="*source*" with workdir build/ -> 7 cases, 3 failed: dictionary (test_dictionary_page.cpp:317), insights (test_insights_page.cpp:311), studio-rail (test_overlay_settings_ui.cpp:624) — all pre-existing bare-path CWD failures, not T-004 regressions]
  - [RAN python census of 9 scoped files: 26 capped literals + 2 universal zeros; PushFont digit literals = 6, all allowlisted; every allowance exactly at cap, zero slack]
  - [RAN git show 2c06061:<9 files> census -> PushFont digit total 37; Snippets/Dictionary/Insights lack PushFont(nullptr, metrics::font_ at base; Scratchpad 15.0f x2] — independently reproduces the fail-before counts.
  - [RAN python scan for digits in Overlay.cpp/ScratchpadWindow.cpp/theme -> Overlay 0; ScratchpadWindow 4 live SameLine literals (documented second-window exclusion); headers 0 PushFont digits]
- Mutation analysis: reasoned by tracing scanner anchors (no live source edits — READ-ONLY), each marked FIRES/SURVIVES. A scanner re-implementation was NOT run, so these are reasoned, not executed.

## 1. Per-test assessment

### 1a. New ban TEST_CASE (test_overlay_settings_ui.cpp:1260) — "full window layout APIs use tokens not digit literals outside the token-gap allowlist"
- Behavior: line-based lexical scan of 9 hardcoded files; 5 anchors (PushFont(nullptr,, Dummy(ImVec2(0.0f,, Dummy(ImVec2(, SameLine(0.0f,, SetNextItemWidth(); ParseNumberAt; universal {0,0.5,1,2} + -1.0f (SetNextItemWidth only); 17-entry capped allowlist per (file,api,value); anchored token-font requirements for Dictionary/Insights/Snippets/Transforms (metrics::font_) + Snippets (studio) + Scratchpad (either).
- Strengths: single `CHECK(violations.empty())` with file:line:api + full line on failure — operable error text. Anchored `PushFont(nullptr, metrics::font_` (and qualified spelling) correctly avoids the studio_metrics:: substring trap. REQUIRE_FALSE(content.empty()) per file prevents silent-pass on missing file. Caps are ceilings (tokenizing a gap site stays green — good direction). DummyWidth/DummyHeight double-anchor interaction is safe: `Dummy(ImVec2(0.0f,` DummyWidth parse yields 0.0 -> universal -> skipped, no double-count [verified by tracing test_overlay_settings_ui.cpp:1234-1236].
- Weaknesses: scanner logic (~115 lines incl. ParseNumberAt) has zero direct tests — only end-to-end green-on-current-tree (R-W1-001). Token requirements are presence-only per file, not per-site (R-W1-002). 9-file hardcoded list (R-W1-003). Lexical loopholes (R-W1-004).
- Boundary correctness verified by tracing: `PushFont(nullptr, -1.0f)` -> violation (sentinel exemption is SetNextItemWidth-only, correct). Integer `13` and fractional parsing both handled. `Dummy(ImVec2(14.0f * scale, 0.0f))` parses 14.0 and stops at space — matches allowance cap 2, current count exactly 2 [verified against MainWindow.cpp:558,928]. Computed `pill_h + 18.0f` (ScratchpadPage.cpp:124) correctly exempt as non-number (deliberate).
- Answer to brief "try to break": (i) literal in unlisted file (e.g. new src/ui/pages/Foo.cpp, or excluded ScratchpadWindow.cpp — 4 live literals already proven at SameLine 4.0f/6.0f) -> ESCAPES; (ii) literal in allowlisted file at capped (file,api,value) triple -> FIRES (cap-exceed); literal in allowlisted file but different value -> FIRES (non-allowlisted violation); (iii) literal in widget file Display/Overlays (in list, no allowances) -> FIRES; Buttons.cpp PushFont 24.0 second instance -> FIRES (cap 1). The ban holds on all in-list cases; only the out-of-list case escapes, as documented.

### 1b. Scratchpad pin flip (test_scratchpad_page.cpp:353-363)
- Replaces PushFont(nullptr,17)/15 presence with has_token_font (any of studio/metrics/lwf::ui::metrics) + 15.0f == npos + CountHits(17.0f) <= 1.
- Not weakened beyond the documented deviation: the 17.0f site stays covered by the overlay ban cap=1. Removing the presence check loses "at least one 17 remains" — acceptable since no token exists and the ban caps it. Token requirement is looser (any of 3 spellings, no font_ anchor, unlike overlay) — consistent because scratchpad legitimately uses banner/helper tokens; but `PushFont(nullptr, metrics::garbage` would satisfy it. Minor.

### 1c. Snippets pin flip (test_snippets_page.cpp:331-343)
- Old `PushFont(metrics` ban reversed -> requires PushFont(nullptr, studio_metrics:: (loose prefix, any token) AND PushFont(nullptr, metrics::font_ (anchored). The old `PushFont(nullptr,` presence check is subsumed by the stronger anchored checks. 15.0f/13.0f absence added. SetWindowFontScale/windows.h/TODO/FIXME/Log::/Wispr checks untouched. No token/contrast CHECK weakened anywhere in the wave: overlay file's contrast tests intact (diff is append-only); token values pinned at test_overlay_settings_ui.cpp:238 (font_card_title_px==15.0f), 730 (font_helper_px==13.0f).

### 1d. ReadSource helpers (all 3 files)
- LWF_SOURCE_DIR-first with bare-relative fallback, matching test_transforms_page.cpp:30-44 idiom. Live-verified from build/ for the 3 touched files. Fallback silent-mode issue: if LWF_SOURCE_DIR is stale and a rogue relative file exists in CWD, the scan reads wrong bytes with only REQUIRE_FALSE(non-empty) as guard — NIT, unrealistic in CI (ctest cwd is source dir).
- `ReadSourceFrom(".", relative)` when macro undefined produces "./src/..." — correct.

## 2. Coverage

| Area | Status |
|---|---|
| Ban end-to-end green on landed tree | Covered [RAN 29 assertions] |
| Dummy/SameLine/width caps (at count) | Covered implicitly — any +1 fires (counts exactly at caps) |
| -1.0f sentinel exemption | Covered (6 hits in MainWindow) |
| Tokened-line exemption | Covered (all token sites) |
| Universal 0.5/1/2 branches | DEAD today — no such literals in tree; branches unexercised |
| Comment-line branch | Effectively untested (only anchor-less prose hit) |
| ParseNumberAt no-digit / sign / frac branches | Partially (identifiers + -1.0f + ints); no synthetic cases |
| Violation + cap-exceed firing paths | NEITHER exercised by any test (R-W1-001) |
| CWD from repo root and build/ | Covered for 3 touched files [RAN]; still failing for 3 sibling tests (R-W1-005) |
| Token VALUES (15/13/28 pinned) | Covered globally (overlay 238/730) |
| Site-to-token mapping | NOT covered (R-W1-002, systemic residual) |

## 3. Mutation thinking (10 considered, 7 survivors, 3 killed)

FIRES (ban kills): revert-any-token-to-digit (non-allowlisted -> violation) [FIRE]; add PushFont 24.0 #2 in Buttons.cpp (cap-exceed) [FIRE]; add any digit literal in Display/Overlays (no allowances) [FIRE].
SURVIVE:
1. Mapping 15->13: replace PushFont(nullptr,15.0f) with metrics::font_body_px (13.0f). Presence-only `metrics::font_` still satisfied (7 other card_title sites exist in Snippets). Visual -2px, test green. SYSTEMIC lexical-only residual (brief section 6) — value pinned globally but not per-site.
2. Mapping 16->8 class: replace PushFont 24.0 with metrics::font_page_title_px (28.0f) or font_helper_px (13.0f). Same mechanism — any token satisfies any site. SURVIVES.
3. Token-value drift in palette (font_card_title_px 15->13 in ThemePalette.h): CAUGHT — value pinned at overlay:238. (Not a survivor; documented to distinguish which half of the mapping is pinned.)
4. Allowlist abuse via slack: no slack today (all 17 caps exactly at current counts [RAN]) — adding a capped-value literal fires. SURVIVES only in the sense that cap allows up to current count forever (baseline creep blocked only beyond cap). OK.
5. File glob escape: new src/ui file with PushFont(nullptr, 99.0f) — no test reads it. SURVIVES (acknowledged Wave 2 glob follow-up; live precedent: ScratchpadWindow.cpp).
6. Named-constant hoisting: `constexpr float kX = 13.0f; PushFont(nullptr, kX)` — identifier exempt. Deliberate per code comment. SURVIVES (by design).
7. `Dummy(ImVec2(14.0f, 12.0f))` — two-component: width 14 counted (cap 2), height 12 invisible (DummyHeight anchor needs first component 0.0f). SURVIVES.
8. Multi-line split `PushFont(nullptr,\n13.0f)` / `PushFont( nullptr,` — anchor mismatch. SURVIVES. No instances today.
9. Trailing-comment token smuggle: `PushFont(nullptr, 13.0f); // metrics::font_body_px` — whole-line tokened exemption. SURVIVES. Contrived, but defeats the ban with one comment.
10. Revert whole T-001/T-002 swap (37 digits): FIRES (31 non-allowlisted at base — counted). Judge: ban + caps do their job inside scope.

## 4. Flakiness

None. Deterministic pure string scan, no time/random/threading/IO beyond source reads. [RAN ctest 3x consecutive -> 1/1 Passed each, 19.8s, same order as baseline 23.98s] and single-case re-runs from root and build/ identical (29/29, 41/41, 36/36). Only theoretical flap: concurrent sibling edits to src/ui during a ctest run — operational, not a test defect.

## 5. Findings

- R-W1-001 (MAJOR): No negative test for the scanner. A regression making ScanLayoutLiterals always return empty passes forever; the violation and cap-exceed firing paths are never exercised by any TEST_CASE, and there are no synthetic-content unit tests for ParseNumberAt/ScanLayoutLiterals. Fail-before is proven only by HEAD-blob counts (Deviation 4, honest and reproducible — I re-verified the counts) — no live RED run was captured. For a ban whose entire value is "fires when someone cheats", the firing direction needs a test. Sketch: `TEST_CASE("layout literal ban fires on violations") { const auto v = ScanLayoutLiterals(path, "ImGui::PushFont(nullptr, 13.0f);\n"); CHECK(!v.empty()); ... cap-exceed case: repeat PushFont 24.0 x2 in Buttons content -> cap violation; universal cases: SameLine(0.0f, 0.5f) -> empty; -1.0f PushFont -> violation }`. Not a wave blocker (counts prove the scanner fires on the real base tree), but the single largest test-quality gap.
- R-W1-002 (MINOR): Per-site token/value mapping unpinned (mutations 1-2 survive). Token values ARE pinned globally (overlay:238,730) so palette drift is caught; only the site->token choice depends on screenshots (P-006, manual). This is exactly the brief-section-6 systemic residual (lexical-only, no ImGui link). Not a blocker; record as residual risk. A cheap partial mitigation exists: per-file exact-token presence/count pins (e.g. Snippets requires font_card_title_px present) — would still not bind sites, but narrows the survivor set.
- R-W1-003 (MINOR): Hardcoded 9-file list escapes future files. Live precedent already in tree (ScratchpadWindow.cpp, documented exclusion). Implementer disclosed; Wave 2 glob planned. The ban is sound inside its list.
- R-W1-004 (MINOR): Lexical loophole class — headers unscanned (0 PushFont digits today, no scan), Dummy two-component height escape, multi-line/space-split anchors, tokened-line comment smuggle. None has a live instance; all are single-edit escapes. Each could be a P2 follow-up; none blocks wave 1.
- R-W1-005 (MINOR): A4 CWD fix is partial within touched files. The 3 flipped/new tests pass from build/ [RAN], but 3 pre-existing tests in the same wave's files still fail from build/ (dictionary, insights, studio-rail; evidence in section 0). Pre-existing trap, untouched by T-004 (its write-set append-only convention kept old tests as-is), not a regression — but the report's claim "run exe from repo root AND build/ both green" holds only for the touched subset. Recommend a follow-up NEED migrating the remaining bare-relative readers to ReadSource.
- NIT (R-W1-006): ReadSourceFrom silent fallback (rogue relative file could be scanned if LWF_SOURCE_DIR is stale). No realistic CI path.

## 6. Missing edge cases

- P1 (1): Direct scanner tests — violation, cap-exceed, universal-vs-nonuniversal boundaries (0.5/1/2 currently dead; -1.0f PushFont vs SetNextItemWidth), comment-line and tokened-line exemption. Sketch in R-W1-001.
- P2 (3): (a) new-file/glob escape canary (a test that enumerates src/ui/**/*.cpp and asserts the ban's file list covers them, or scans the glob directly); (b) header scan for PushFont digits (defense before first header literal lands); (c) migrate dictionary/insights/overlay bare-path readers to ReadSource so build/-cwd runs are genuinely green.
- P3 (2): (a) per-file exact-token presence pins to shrink wrong-value-mapping survivors (Snippets: font_card_title_px) — cheap, no palette change; (b) two-component Dummy height + multi-line anchor robustness (match spanning line, or scan whole file not line-by-line).

## 7. Unverified / out of scope

- No live RED run exists (by necessity: ban and flips landed atomically with the source swaps); fail-before is count-proven, re-verified by me, but not by a captured red process. Confidence med on that claim alone.
- Mutation survivors 1/5-9 are reasoned by tracing the scanner, NOT executed (READ-ONLY barred source edits; a temp-copy re-implementation would not be the real test).
- Pixel/same-value correctness cannot be established by lwf_tests (no ImGui link) — screenshots P-006 remain the gate for that; per standing orders honestly flagged, not claimed.
- Full build/clang-tidy/hygiene beyond scoped ctest: not run (brief section 7 scope); implementer reported hygiene OK; G3 zig pending (no zig on dev box).
- Existing token/contrast CHECKs: verified intact by diff (append-only overlay hunk; scoped flip hunks) — no weakening.
- Last COMMS line read: 29.

(End of review — not executed: source/test edits, git commit of this report is orchestrator-owned.)
