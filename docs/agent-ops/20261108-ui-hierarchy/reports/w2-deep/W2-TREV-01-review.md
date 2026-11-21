<!-- ORCH-DIGEST:START -->
agent: W2-TREV-01 | scope: wave 2 test quality (empty ban, 9 pins, flip, negative, stale flips) | verdict: APPROVE-WITH-CHANGES
tests_reviewed: 5 changed test cases (+520-case full suite) | fail-before/pass-after: 2 physical / 3 reasoned / 0 not-possible
findings: BLOCKER 0 | MAJOR 0 | MINOR 2 | NIT 3
flaky_suspects: 1 unexplained full-ctest failure (1 of 5 runs, not reproduced; log lost)
missing_edge_cases: P1 0 | P2 4 | P3 3
mutation_survivors: 5 of 8 (reasoned; 3 killed, 2 reversal-proven by implementer probes)
changed_behavior_without_tests: none (space_xs change is comment-only)
confidence: high (test quality); med (pixel parity — P-006 screenshots still owed by anyone)
<!-- ORCH-DIGEST:END -->

# Test quality review — W2-TREV-01 (wave 2 run 20261108-ui-hierarchy)

## 0. Method
Blind-first per brief: STANDING-ORDERS, AGENTS.md gates, 00-mission, `git diff ea04e1a..HEAD -- tests/`
read before any implementer section. Then read W2-IMPL-01-report, W2-IMPL-02-report, gate-w2-1.md, COMMS tail.
last_comms_line_read: `[2026-11-08T05:45:00Z] W2-IMPL-01 -> ORCH | NEED | T-003` (COMMS.md:35; W2-IMPL-02 posted none).
Read-only task: no source/test edits. Mutation analysis is reasoned from scanner code, not executed
(mutation would require editing sources outside write-set); the two physical reversal probes run by
W2-IMPL-01 are cited as its evidence, not mine.
Evidence tags per STANDING-ORDERS §5.

## 1. Per-test assessment

### 1.1 Ban TEST_CASE ("full window layout APIs use tokens ... allowlist", tests/test_overlay_settings_ui.cpp:1248) — 30 assertions [RAN .\build\lwf_tests.exe -tc="*full window layout*" -> 1 passed, 30/30 assertions]
- Was 29 (9 files × (REQUIRE+CHECK) + 5 flip REQUIREs + 6 flip CHECKs, matching COMMS "29/29"); now 30
  (+1 REQUIRE mainwindow, flip CHECKs net-unchanged: Snippets metrics leg dropped, MainWindow native leg added).
- Empty table strictly stronger — VERIFIED [VERIFIED tests/test_overlay_settings_ui.cpp:1147,1182-1227]:
  with `kBanAllowances[] = {}` the match loop can never match, so EVERY non-universal digit literal in the
  5 scanned APIs becomes a violation; the old cap-exceed loop (:1234-1242) is dead (allowance_total=0) but
  harmless — nothing can hide "under cap" any more. Strictness is a superset of the capped regime.
- Re-label audit (trap §6): old `cap_exceed` leg had 2 identical Buttons.cpp 24.0f lines vs cap 1 → 1 cap-exceed
  violation; new `digit_violations` leg → 2 digit violations. `CHECK_FALSE(empty)` semantics preserved and the
  count itself is not asserted anywhere, so no real violation is hidden — the re-label is accurate and the
  leg still proves the scanner fires.
- Residual: TEST_CASE name still says "cap-exceeds" and cap machinery is unreferenced dead code (NIT-1).

### 1.2 Token pins ("studio palette layout tokens follow the shape scale", :714) — 32 assertions [RAN -tc="*studio palette layout tokens*" -> 32/32]
- +9 CHECKs at :739-747, each value matches its header definition [VERIFIED src/ui/theme/StudioPalette.h:66-85:
  gap_micro 6 / gap_inline 8 / gap_compact 10 / gap_section 12 / gap_block 16; font_card_title 15 / font_heading 24 /
  font_subheading 17 / font_glyph 10]. Same-value mapping against replaced literals is claimed by W2-IMPL-01 §5
  tables; I verified definitions, not every site (expected: pins prove token values, not site mapping — §3).
- Symmetry note: old had 23 CHECKs, new 32. All 9 land in ONE TEST_CASE, so reversal granularity is coarse but
  adequate (any single wrong value fails exactly its own CHECK).

### 1.3 Flip + MainWindow (:1282-1321)
- `has_studio_font` broadened to both `studio_metrics::font_` and `lwf::ui::studio_metrics::font_` [VERIFIED :1291-1296];
  all 5 studio pages now assert studio-only (Snippets metrics leg dropped; Scratchpad tightened from the old
  metrics-or-studio disjunction → stricter). New `CHECK(has_metrics_font(mainwindow))` + REQUIRE at :1308-1310
  gives native-page proof (MainWindow.cpp is also banned-scanned at :1260).
- Two findings: (a) existence semantics mask single-site reverts — one `studio_metrics::font_card_title_px` →
  `metrics::font_card_title_px` on any of the 5 pages survives while ≥1 other studio font remains in that file
  (MINOR-1, mitigated only where digit-bans exist, i.e. Snippets 15.0f/13.0f npos); (b) comment at :1257 cites
  line numbers :1169-1171, which will rot on any edit (NIT-3). Neither blocks.

### 1.4 Negative ("layout literal ban fires ...", :1323) — 8 assertions [RAN -tc="*layout literal ban fires*" -> 8/8] (brief said 6 — brief predates the dummy rewrite)
- 6→8 CHECKs: `dummy_safe` 1-leg → 3 legs (`dummy_literal` fires, `dummy_tokened` empty, `dummy_universal` empty).
  Each maps to a real scanner branch: token exemption (:1180-1181), universal set (:1195-1198), sentinel
  API-scoping (:1197-1198), DummyWidth/DummyHeight double-anchor (0.0f universal skip, 12.0f fires).
  The rewrite is strictly stronger: previously a capped DummyHeight 12.0f counted as safe; now the same input
  must fire AND the tokened equivalent must not — both directions pinned.

### 1.5 Stale pin flips (W2-IMPL-02: tests/test_snippets_page.cpp:339-343, tests/test_transforms_page.cpp:380)
- Both flipped to anchored studio needles (snippets mirrors the overlay lambda's both-spellings form
  [VERIFIED tests/test_snippets_page.cpp:339-341]); nothing deleted, surrounding digit-bans intact.
- [RAN -tc="*snippets*,*transforms*" -> 20 cases, 193/193] matching W2-IMPL-02. Physical fail-before/pass-after
  518/520 → 520/520 recorded by W2-IMPL-02 (I ran only the pass-after side; fail-before cited, not re-run).

## 2. Coverage
- Changed cases: 5 (ban+flip, negative, pins, snippets page case, transforms case); suite: 520 cases / 27,790 assertions.
- Wave-2 code touched 11 src files; coverage is ban (literals) + pins (token values) + flip (vocabulary presence).
  Site-level value-correctness (right token at right site) is covered ONLY by the impl-01 §5 audit tables +
  owed P-006 screenshots. File-local hoists (kSearchWidth 240, kDialogInputWidth 360, kStatusDotIndent 14,
  Transforms cluster widths) have no value pin anywhere (P2-3).
- Known unscanned classes stay unscanned by design and were NOT introduced by this wave: binary call-geometry
  arithmetic, button-width models, 40.0f rails, 14px insets — unchanged, out of scope.

## 3. Mutation thinking (reasoned; ~8 probed: 3 killed / 5 survive)
Killed:
- M1 revert any token to its literal → ban fires (impl-01 physical probe: TransformsPage 12.0 literal → ban case failed with file logged).
- M2 wrong token value (gap_section=13.0f) → pin fails exactly on that CHECK (impl-01 physical probe, reverted).
- M3 full studio→native font reversion → flip fails (2 stale tests demonstrated it pre-flip: 518/520).
Survivors (all lexical-residual class, NOT wave defects per trap; the wrong-value swap is systemic):
- S1 wrong-token site swap (gap_section↔gap_block, 12↔10) — scanner exempts any line containing `metrics::`/
  `studio_metrics::` (:1180-1181); pins check definitions only. Needs screenshots.
- S2 cross-role font swap (font_heading 24 at a 10px glyph site) — same mechanism.
- S3 single-site studio→native font revert on a studio page — existence check (§1.3 MINOR-1).
- S4 multi-line smuggle: anchor and number on different lines defeats ParseNumberAt (:1193) — pre-existing gap.
- S5 same-line token+literal mix exempts the literal — pre-existing.

## 4. Flakiness
- [RAN ctest --test-dir build -R lwf-tests -> Run1 FAILED (test name only, LastTest.log overwritten by later runs),
  then 3 consecutive ctest passes 19.73-19.89s + 2 direct `.\build\lwf_tests.exe` runs 520/520, 27,790/27,790,
  exit=0 (an interim "exit 1" was the PowerShell Select-Object pipe artifact, cf. gate note)]. The single
  failure cause is UNKNOWN — candidate: transient build/lock interplay on the chained build+ctest invocation.
  flaky_suspect: full-suite ctest wrapper (1/5 runs; not reproduced in any binary-direct run; suite itself
  shows no flake). Per standing orders §18 I retried and it passed; I cannot bisect an overwritten log.
  Pre-existing CWD (relative ReadSource paths) sensitivity unchanged — not a regression (trap §6).
- Scoped cases are deterministic pure source-scans (ms-scale, no I/O beyond ReadSource); no timing/parallelism.

## 5. Findings
- MINOR-1: flip checks are existence-based; a single-site cross-palette font revert survives on 4 of 5 studio
  pages (Snippets/Transforms partially guarded by digit-bans). Pre-existing pattern, kept by this wave — a
  per-file "no native font" negative CHECK would close it.
- MINOR-2: one unexplained full-ctest failure, unreproduced, log lost (see §4). Operability note, not a
  correctness blocker; flag for orchestrator if it recurs.
- NIT-1: negative TEST_CASE name still says "cap-exceeds"; cap-exceed branch (:1234-1242) and BanAllowance
  machinery are dead with an empty table — keep or remove deliberately.
- NIT-2: ban TEST_CASE title still mentions "allowlist"; comment upkeep (likewise NIT-3 line-number citation
  :1169-1171 will rot).
- NIT-3: see NIT-2. (No test weakened: every rewritten leg fires-or-is-empty in the stronger direction, §1.4.)

## 6. Missing edge cases
- P1: none. Screenshots P-006 are owed, but that is manual visual proof, not a missing test edge.
- P2-1: S1/S2 wrong-token site swaps — only screenshots catch them (systemic; P-006 owed).
- P2-2: scanner blind spots S4/S5 (multi-line calls, same-line token exemption, non-nullptr PushFont anchor) —
  pre-existing, worth a future ambiguity fix, not a wave regression.
- P2-3: file-local hoist values (240/360/14/widths) have no pins — a wrong hoist value is silent until screenshots.
- P2-4: new Full-window source file must be manually added to `layout_sources` or the bin silently misses it
  (comment at :1257 mitigates; a directory-scan self-check would harden it).
- P3-1: dead cap branch/name (NIT-1). P3-2: MainWindow `has_metrics_font` becomes spurious-failing if MainWindow
  ever legitimately drops all native PushFont sites. P3-3: scanner/tests hardcode relative paths (pre-existing CWD trap).

## 7. Unverified
- Physical fail-before runs (M1/M2/M3 probes, W2-IMPL-02 518/520) — cited from implementer reports; I did not
  re-execute mutations (read-only write-set). My fail-before for the empty table is strong but reasoned:
  with the table empty, the 32 previously-capped literals all violate by construction [VERIFIED :1147,1182-1227].
- P-006 screenshots (960×640/760×520, DPI 1.0/1.5) — not run; pixel parity rests on value equality.
- Windows/G3 zig type-check (no zig on this box), full exe target — see gate-w2-1.
- The single transient ctest failure's root cause (evidence overwritten by passing reruns).

## Verdict
APPROVE-WITH-CHANGES, non-blocking: the Wave-2 test changes are strictly stronger (empty ban verified as a
superset of capped violations; negative legs tightened; pins exactly 9/9; flawless 520/520 suite with
reversal evidence). Follow-ups: harden the flip to per-file native-font negatives (MINOR-1), watch the
unreproduced ctest flake (MINOR-2), tidy dead cap branch/names (NITs). No blocker, no test weakening.
Standing-orders §8 long-run: Source/deterministic yes; Load N/A (compile-time constants, zero runtime delta);
Hostile = literal smuggling covered except S4/S5; Operability = rerun log retention for traceability;
Reversal: reverting code-without-tests re-fails ban/pins/flip as proven above.
