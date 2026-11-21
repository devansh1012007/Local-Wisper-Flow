<!-- ORCH-DIGEST:START -->
agent: W2-DEEP-01 | domain: token-gap design (T-003 tranche 2) | status: DONE
items: inventory 26 ALLOWLIST comments (6 PushFont + 20 Dummy/SameLine) + 4 SetNextItemWidth caps across 17 BanAllowance entries; 9 additive studio_metrics tokens chosen (4 fonts + 5 gaps), 0 existing values changed
chosen_option: A (additive studio_metrics tokens, same-value) with file-local constants for MainWindow 14 and SetNextItemWidth widths; B and C rejected (B touches frozen ThemePalette.h + leaks studio semantics into native header; C shifts pixels)
token_table_count: 9 global tokens + 2 file-local constants (MainWindow kStatusIndent, Dictionary/Scratchpad width constants)
top_risks: (1) flip-check has_metrics_font pins (test:1292-1327) go red on full studio migration — needs coordinated test flip; (2) negative scanner dummy_safe (12.0) goes red the same way — needs synthetic rewrite; (3) P-006 screenshots still pending — pixel identity proven algebraically, not visually
cross_domain_conflicts: none; palette unification (ApplyStudioStyle) explicitly out of scope per brief S6
out_of_plan_discoveries: none new (Transforms cluster_w/cursor_x literal duplicates and 14 row-insets catalogued as unscanned geometry)
confidence: high (every token value checked against current headers; every allowlisted site re-read at HEAD ea04e1a; no builds per brief S7)
last_comms_line_read: [2026-11-08T04:05:00Z] ORCH -> ALL | DECISION | WAVE-START (full log re-read; tail at W1-IMPR-02 CORRECTION)
<!-- ORCH-DIGEST:END -->
# Deep design — W2-DEEP-01 — token gaps for tranche 2 (T-003)
## 0. Method and ground truth (read-only)

- Brief: `docs/agent-ops/20261108-ui-hierarchy/briefs/W2-DEEP-01.md` read fully. WRITE-SET is this file only; no source edits made or committed.
- Required reading followed in order: STANDING-ORDERS [VERIFIED templates/STANDING-ORDERS.md:1-45], AGENTS.md UI + frozen tokens (`AGENTS.md:135-161`, ThemePalette.h:5-16 "Do not change values here without regenerating the contrast tables"; test pins `tests/test_overlay_settings_ui.cpp:236-240,698-738`), 00-mission D-rail/constraints [VERIFIED 00-mission.md:9-33], W0-INV-02 §§5-10 incl. T-003 [VERIFIED reports/w0-investigation/W0-INV-02.md:369-376], W0-INV-04 mockup mapping [VERIFIED reports/w0-investigation/W0-INV-04.md:26-107], W1-REV-01 §7 DO LATER + §3 long-run [VERIFIED reports/w1-review/W1-REV-01.md:61-68,116-123], W1-IMPL-02 tranche-1 report [VERIFIED reports/w1-impl/W1-IMPL-02-T-002.md:39-50,92-100], BanAllowance 1136-1159 + scanner 1174-1256 + negative test 1330-1372 + flip checks 1292-1327. Template used: report-implementation + deep addendum F (failure/rollout/sequencing); palette-unification is out of scope per brief §6 and is not proposed.
- HEAD ground truth: `ops/20261108-ui-hierarchy` at `ea04e1a..` lineage [VERIFIED git log: b824483 is W2 brief commit on top of ea04e1a Wave-1 close]. All line numbers below are HEAD values re-read this session, not copied from Wave-0 reports. Env was read/grep only per brief §7; no builds, no zig (G3 pending, AGENTS.md:42-45).
- Frozen-token invariant for this design: no existing constant in `src/ui/theme/ThemePalette.h` or `src/ui/theme/StudioPalette.h` changes value; additive constants only; therefore WCAG contrast tables are unaffected (spacing/font-size tokens do not enter `RelativeLuminance`/`ContrastRatio`, ThemePalette.h:141-159) and mockup regen is not required (§9).
- Naming ground truth: native fonts are `font_wordmark_px/page_title_px/card_title_px/body_px/mini_text_px` [VERIFIED ThemePalette.h:103-107]; studio fonts are `font_banner_px/caps_px/page_title_px/helper_px` [VERIFIED StudioPalette.h:55-58]; studio radii are `radius_banner/card/row/input/button/chip/pill` [:42-48]; native radii are `radius_control/card/pill` [ThemePalette.h:91-93]; native spacing is `space_xs/s/m/l/xl` [:85-89]; studio spacing is `page_padding/gap/row_height/hit_size` [StudioPalette.h:50-53]. New names follow the `font_<role>_px` and `gap_<role>` idioms; scale-letter reuse across namespaces is deliberately avoided (§3).

## 1. Inventory — every allowlisted site (BanAllowance caps + ALLOWLIST comments)

Two allowlists exist and must not be conflated. Wave 1 shipped (a) **26 `// ALLOWLIST (Wave 2 T-003)` comments**, one per literal site (W1-REV-01 §1), and (b) **17 `BanAllowance` entries** in `kBanAllowances` [VERIFIED tests/test_overlay_settings_ui.cpp:1136-1159] whose caps are ceilings (scanner comment :1122-1128: "Caps are ceilings, not targets"). The 26 comments cover PushFont + Dummy/SameLine gaps only; the `SetNextItemWidth` caps (4 literals, 3 entries) have **no** ALLOWLIST comment and are structural widths (§6). Counts reconcile at HEAD: PushFont digits 6 [VERIFIED W1-IMPL-02:59-60, grep 6], Dummy/SameLine digits 20 [VERIFIED W1-REV-01:51], total 26; BanAllowance ceilings sum to 32 (6 PushFont + 11 DummyHeight + 9 SameLine + 2 DummyWidth + 4 SetNextItemWidth) because ceilings are per-(file,api,value) maxima, not site counts, and SetNextItemWidth is extra. The table below is the per-site inventory the tranche must clear.

### 1.1 PushFont gaps — 6 sites (4 BanAllowance entries)

| # | File:line [VERIFIED] | API | Literal | Role at site | Allowlist reason |
|---|---|---|---|---|---|
| F1 | InsightsPage.cpp:131 `PushFont(nullptr, 10.0f)` | PushFont | 10 | share-badge ring glyphs (ring_radius, 4 repeats) | no 10px font token in either namespace (12 is `font_caps_px` type-only; 13 is helper) [studio cat: glyph] |
| F2 | InsightsPage.cpp:291 `PushFont(nullptr, 10.0f)` | PushFont | 10 | category icon glyph (row icon, ProgressPill context) | same gap as F1, same role -> same token |
| F3 | InsightsPage.cpp:268 `PushFont(nullptr, 24.0f)` | PushFont | 24 | Usage card heading `kUsageTitle` | no 24px font token (28 page_title, 40 banner; 24 is `metrics::space_l`/`studio gap` spacing) |
| F4 | InsightsPage.cpp:308 `PushFont(nullptr, 24.0f)` | PushFont | 24 | Streak card heading `StreakTitle(...)` | same gap as F3 |
| F5 | ScratchpadPage.cpp:190 `PushFont(nullptr, 17.0f)` | PushFont | 17 | "Recents" heading (DrawRecents) | third heading size (17 vs 24 vs 28); no token |
| F6 | Buttons.cpp:124 `PushFont(nullptr, 24.0f)` | PushFont | 24 | IconButton glyph (hit 32px box, glyph optical size) | same 24 value but **icon role**, not heading role — value-coupled today; see §3 coupling note |

Caps: `{"InsightsPage.cpp","PushFont",10.0,2}`, `{"InsightsPage.cpp","PushFont",24.0,2}`, `{"ScratchpadPage.cpp","PushFont",17.0,1}`, `{"Buttons.cpp","PushFont",24.0,1}` [VERIFIED test:1138-1141]. All 6 sites carry an ALLOWLIST comment immediately above [VERIFIED grep 26 matches].

### 1.2 Section / inline gaps — 20 sites (DummyHeight + SameLine; 9 BanAllowance entries)

| # | File:line | API/value | Role |
|---|---|---|---|
| G1-G3 | DictionaryPage.cpp:148,162,202 (`Dummy(0.0f, 12.0f*scale)`) | DummyHeight 12 x3 | cadence after header / tabs / toolbar — the universal section gap (W0-INV-02 §P-002). Cap `{"DictionaryPage.cpp","DummyHeight",12.0,3}` |
| G4 | InsightsPage.cpp:44 (`Dummy(...,12.0f)`) | DummyHeight 12 x1 | divider-to-content gap after `DrawDivider` line |
| G5-G6 | SnippetsPage.cpp:116,276 (`Dummy 12`) | DummyHeight 12 x2 | section cadence (2 sites) |
| G7 | ScratchpadPage.cpp:48 (`Dummy 12`) | DummyHeight 12 x1 | header->banner gap |
| G8-G9 | ScratchpadPage.cpp:178,183 (`Dummy 10`) | DummyHeight 10 x2 | mock-note skeleton bars inside banner mock (imitation rows, not page cadence — but same 10 value; lands on same compact token) |
| G10-G11 | TransformsPage.cpp:586,686 (`Dummy 12`) | DummyHeight 12 x2 | modal footer gaps (reset confirm, how-it-works) |
| G12-G13 | InsightsPage.cpp:54,68 (`SameLine(0.0f, 6.0f*scale)`) | SameLine 6 x2 | heatmap legend: "More"/cells/"Less" inline gaps |
| G14-G19 | TransformsPage.cpp:172,208,216,314,538,591 (`SameLine 12`) | SameLine 12 x6 | header cluster (title/BadgeBeta, opt-in cluster, My-Transforms actions, editor/reset footer). Cap 6 |
| G20 | TransformsPage.cpp:199 (`SameLine 10`) | SameLine 10 x1 | Opt-in label -> info IconButton inline gap |
| — (dup, unscanned) | TransformsPage.cpp:189-191 `cluster_w = label_w + 10.0f*scale + ... + 12.0f*scale + ... + (12.0f*scale+view_w)` | width model (assignment, no API) | duplicates G20/G14-G16 in `CalcTextSize` arithmetic; scanner does not scan assignments so no cap exists, but R-W1-001 class requires the model to move with the layout (§6) |
| — (dup, unscanned) | TransformsPage.cpp:307-308 `cursor_x = ... - (12.0f*scale + create_w)` | cursor arithmetic (no API) | duplicates G17 (SameLine action gap); same migration obligation |
| — (dup, fixed Wave 1) | TransformsPage.cpp:105 `width += metrics::space_s *scale`, :107 Keycap font already `studio_metrics::font_helper_px` [VERIFIED current lines] | — | W1-IMPR-01 closed the 8/13 half of the R-W1-001 split; the 10/12 half remains (§6) |

Caps: `DummyHeight 12`: Dictionary 3, Insights 1, Snippets 2, Scratchpad 1, Transforms 2; `DummyHeight 10`: Scratchpad 2; `SameLine 6`: Insights 2; `SameLine 12`: Transforms 6; `SameLine 10`: Transforms 1 [VERIFIED test:1143-1151]. The 26-comment grep shows the same 20 (3+1+2+1+2+2 Dummy + 2+6+1 SameLine + 2 Scratchpad-Dummy10) — reconciled.

### 1.3 Native 14px — 2 sites (1 BanAllowance entry, no studio token)

| # | File:line | API/value | Role |
|---|---|---|---|
| N1 | MainWindow.cpp:558 `Dummy(ImVec2(14.0f*scale, 0.0f))` | DummyWidth 14 | rail status-dot indent: black dot at `+4.0f*scale`, then 14px advance before status word |
| N2 | MainWindow.cpp:928 `Dummy(ImVec2(14.0f*scale, 0.0f))` | DummyWidth 14 | Home status-card dot indent (same construction, dot at `+4.0f`) |

Cap `{"MainWindow.cpp","DummyWidth",14.0,2}` [VERIFIED test:1153]. No 14px spacing token in native `metrics::*` (grid 4/8/16); studio `activity_cell`/`gauge_stroke` are 14 but wrong palette and wrong role (cell size, stroke width). These are the only stave of 14 in scanned APIs; other 14s (Scratchpad note-row `AddText` insets :248/253/259/263, Insights icon `AddCircle` :288, Buttons Chip `pad_x` :179, Display gauge offset :134) are `ImDrawList` geometry, **not** in the 5 scanned APIs [VERIFIED scanner anchors :1234-1238] and stay literal by design (§6).

### 1.4 Structural widths — 4 literals (3 BanAllowance entries, no comments)

| # | File:line | API/value | Role |
|---|---|---|---|
| W1 | DictionaryPage.cpp:173 `SetNextItemWidth(240.0f*scale)` | 240 x1 | search field width (InputTextWithHint) |
| W2-W3 | DictionaryPage.cpp:349,355 `SetNextItemWidth(360.0f*scale)` | 360 x2 | add/edit dialog input widths |
| W4 | ScratchpadPage.cpp:198 `SetNextItemWidth(240.0f*scale)` | 240 x1 | recents search field width |

Caps `{"DictionaryPage.cpp","SetNextItemWidth",240.0,1}`, `{...,360.0,2}`, `{"ScratchpadPage.cpp","SetNextItemWidth",240.0,1}` [VERIFIED test:1156-1158]. No ALLOWLIST comment; these are control-width design decisions, not spacing rhythm (§6). Other `SetNextItemWidth(-1.0f)` fill sentinels (MainWindow :635/649/667/1233/1245/1292) are universal-exempt [VERIFIED scanner :1206-1210] and out of inventory.

**Totals:** 6 + 20 + 2 + 4 = **32 literals** under 17 BanAllowance entries; **26** carry ALLOWLIST comments (32 − 4 widths − 2 MainWindow-14 sites have no Token-Gap comment? — MainWindow 14 in fact carries no ALLOWLIST comment; W1-IMPL-01 left them as capped literals with the comment "rail status-dot indent" living only in `kBanAllowances` :1152-1153. The 26-comment count = 6 fonts + 20 gaps exactly, and MainWindow 14 + widths are cap-only. The tranche treats both uniformly: comments are deleted on migration, caps are deleted on migration.)

### 1.5 What is NOT in the allowlist (so the tranche does not touch it)

- Transforms `KeycapRowWidth` 105 now tokened (fixed Wave 1) — no action. `Keycap` glyph 24 at `Buttons.cpp:124` **is** in (F6). `KeycapRowWidth` `width += text_w + 24.0f*scale` (:110) is keycap chrome arithmetic (unscanned) — §6.
- `pill_h + 18.0f*scale` ScratchpadPage.cpp:124 Dummy expression — unscanned (anchor sees identifier, §6).
- Button-width models `+28/+32` (`TransformsPage.cpp:186,300,304`; `DictionaryPage.cpp:303-305`; `ScratchpadPage.cpp:72`; `Overlays.cpp:130,135` + `Buttons.css` FramePadding `(16,8)` at Buttons.cpp:41,81) — unscanned assignments (§6).
- `40.0f*scale` rail rows (MainWindow.cpp:471) + button/toast heights (Buttons.cpp:58,68; DictionaryPage.cpp:309; ScratchpadPage.cpp:305) — unscanned (not a scanned API), test-pinned (§6).
- `kRailWidth 220.0f` (MainWindow.cpp:58) + `metrics::rail_width 208` (ThemePalette.h:95) — D-rail locked; doc-only (§5).
- 16-vs-20 card padding drift (Insights/Display 16 vs Transforms `kCardPadding` 20) — unscanned `PushStyleVar(WindowPadding)`; documented drift, values frozen (§9).

## 2. Options and tradeoffs

### Option A — Additive `studio_metrics::*` tokens, same-value (CHOSEN)

Add 9 `inline constexpr float` constants to `src/ui/theme/StudioPalette.h` (4 fonts + 5 gaps, §3), migrate the 26 allowlisted sites to them (`push_font` without `*scale`; spacing with `*scale`, identical products), hoist MainWindow 14 and SetNextItemWidth widths to file-local named constants (scanner-exempt by form, no global token), migrate the unscanned `cluster_w`/`cursor_x` duplicates to the new gap tokens for model/layout parity, and migrate studio-page 15px sites from `metrics::font_card_title_px` to the new studio twin to close the type-vocabulary leak (§4). **No existing constant changes; no `ThemePalette.h` edit.**

- Correctness: every migration is `token_value == literal` (24/17/10/15 fonts; 12/10/6/8/16 gaps), so `*scale` products and PushFont design-px are algebraically identical at every DPI. Pixel identity without screenshot proof.
- Coupling: studio files gain a single spacing/type vocabulary (`studio_metrics::*`); native files keep `metrics::*`. Residual coupling (studio 8/16 grid via `metrics::space_s/m`) is either fully migrated (if gap_inline/gap_block adopted) or explicitly documented (§4) — no silent mix.
- Palette freeze: complies. `ThemePalette.h` untouched; `StudioPalette.h` is the designated additive header (W0-INV-02 §5 T-003 write-set; ledger T-003 "StudioPalette.h (additive) + tranche2"). No contrast input changes (fonts/spacing do not participate in luminance), so no mockup regen.
- Test impact: additive pins land in the existing `studio palette layout tokens follow the shape scale` TEST_CASE (:714-739); BanAllowance entries delete as sites migrate; scanner/negative tests need the two synthetic rewrites in §7 (mechanical, no weakening).
- Density: preserves current hierarchy exactly (no 12→16, 10→8, 24→28 nearest-value shifts). Gaps are spelled, not resized.

### Option B — Extend `metrics::*` spacing + fonts (REJECTED)

Add `metrics::space_*/font_*` constants (e.g. `space_section=12`, `font_heading=24`) to `ThemePalette.h` and route studio sites to native tokens (or vice versa).

- Correctness: same-value is achievable, but the header edited is the **frozen mockup header** (ThemePalette.h:5-16; hotspot per 00-mission.md:43-45; CI hygiene relevance). Any edit there invites "just change the value" drift and requires contrast-table diligence even for spacing (the header comment's regen rule covers *values here* generally).
- Palette purity: studio headings/gaps would live in the native mockup namespace, inverting the intended boundary: studio semantics (Usage/Streak headings, legend gaps) become native tokens. Mockup §5 has no 24/17/10 type step or 12/10/6 grid step; adding them to `metrics::*` falsely claims mockup provenance for screenshot-derived sizes (W0-INV-04: studio tokens come from 2026-11-07 screenshots, STUDIO_PLAN §5 provisional, not mockup).
- Coupling: reduces file-local mix (one namespace) but increases cross-palette coupling (studio split would depend on native header for every heading). Future palette unification would then have to *move* these tokens, a second churn.
- Test impact: pins would land in the native `design px font tokens follow the mockup type scale` test (:230-241), whose comment says "Approved T-001 design mapping: mockup type scale s1-s5". Adding non-mockup sizes there corrupts the mockup↔token mapping the test documents. Separate test would be needed.
- Verdict: rejected for this run. It solves the same spelling problem with the worse header, worse provenance claim, and worse coupling direction.

### Option C — Map to nearest existing token with pixel shift (REJECTED)

Route 12→16 (`space_m`/`gap_card`), 10→8 (`space_s`), 6→4/8, 24→28 (`page_title`), 17→15 (`card_title`), 10→12 (`caps`).

- Correctness: fails by construction. 12→16 shifts every section cadence +4px (7 Dummy sites across 5 files compound vertically; at 960x640 the page bottoms move ~28-48px); 24→28 enlarges Usage/Streak headings 16.7%; 10→13 enlarges glyphs 30%; 6→8 widens legend 33%. These are not renames — they are density changes requiring mockup sign-off and screenshot approval, explicitly forbidden by brief §3 ("never forced to 16/8/4") and ledger conflict note ("avoids 24→28 visual shift", 01-work-ledger.md:54).
- Silent failure: the lexical ban cannot catch a wrong-value swap (W1-REV-01 §3: "a wrong-value token swap passes every automated check"). Option C would ship green with changed pixels and no red signal — the highest-risk trap (W0-INV-02 §9 risk 1). The LONG-RUN reversal test (revert token → literal) cannot detect it either.
- Verdict: rejected unconditionally. No pixel-shift mapping is proposed anywhere in §3; structural literals that have no same-value token are hoisted or kept (§6), never nearest-mapped.

### Tradeoff summary

| Dimension | A additive studio (chosen) | B extend metrics | C nearest-token shift |
|---|---|---|---|
| Pixel correctness | exact, provable | exact, provable | wrong by +2..+6px / +4..+5px font |
| Header touched | `StudioPalette.h` (additive, designated) | `ThemePalette.h` (frozen/mockup) | none, but values wrong |
| Mockup provenance | screenshot-derived, honest (no mockup claim) | false mockup claim | none (changes require mockup sign-off) |
| Coupling after | one namespace per file team; 8/16 decision explicit (§4) | studio depends on native header | fewer namespaces but wrong sizes |
| Test impact | additive pins; caps delete; 2 synthetic rewrites | native pin pollution; caps delete | caps delete but pixels move silently (undetectable) |
| Palette-freeze / contrast | complies (no contrast input) | literal compliance, spirit risk (frozen header) | violates "keep tokens" (D3) |
| Density change | none | none | yes (every gap) |
| Reversibility | `git revert` restores literals 1:1 | same | reverts shift too, but shifted state was never approved |

## 3. Chosen tokens — exact namespace, name, value, justification

All 9 constants are `inline constexpr float` in `namespace lwf::ui::studio_metrics` in `src/ui/theme/StudioPalette.h`, placed after `font_helper_px` (:58) and `gap` (:51) respectively, with design-px comments. Values are **identical to the literals they replace**; no existing constant is modified, renamed, or aliased (§9).

**Fonts** — names follow `font_<role>_px` (banner/caps/page_title/helper). Roles encode hierarchy position, not pixel position, so the type scale reads banner 40 > page_title 28 > heading 24 > subheading 17 > card_title 15 > helper 13 > caps 12 > glyph 10:

| New token | Value | Sites landed | Justification (why this name, this namespace) |
|---|---|---|---|
| `studio_metrics::font_card_title_px` | 15.0f | Dictionary :245,286; Snippets :32,151 (+5 more in-file 15s already mapped Wave 1); Scratchpad :218,246; Display RowList :35,55,65; Insights dialog :357,369; Buttons TabRow :226 precedent | Completes the studio type scale at the exact step that today borrows `metrics::font_card_title_px` (Wave 1 found only 15-token; W0-INV-02 §1.4 "15px literals ... almost certainly mean card title"). Studio row/card titles are 15 by screenshot intent, not mockup `s3`; the twin names the role honestly. Migrating these sites to the twin closes the type-vocabulary leak without resizing (15==15). |
| `studio_metrics::font_heading_px` | 24.0f | Insights :268,308 (Usage/Streak headings); Buttons :124 (IconButton glyph, dual-use, see coupling) | Single optical step between page_title 28 and subheading 17. `heading` matches existing `font_page_title_px`/`font_banner_px` role vocabulary; `font_24`-style numeric names are rejected (they encode pixels, not hierarchy, and would silently survive a future resize). Dual-use note: IconButton glyph at 24 shares the value accidentally (icon box 32, glyph 24). Shipping one token couples glyph size to heading size; splitting into `font_glyph_lg_px=24` + `font_heading_px=24` would create two tokens for one optical step with independent drift. **Decision: one token, coupled, documented in header comment** ("also used by IconButton glyph at the same optical size; split if glyph sizing diverges"). If ORCH prefers split, add `font_icon_px=24` as a second constant with identical value — zero pixel difference, only symbol count changes. |
| `studio_metrics::font_subheading_px` | 17.0f | Scratchpad :190 (Recents) | Third heading size between 24 and 15; mockup has no 17 step (W0-INV-04 UNKNOWN). `subheading` is the hierarchy-generic name (Recents is a section subhead, not a card title). Alternatives (`font_recents_px`) rejected as single-use over-specific. |
| `studio_metrics::font_glyph_px` | 10.0f | Insights :131,291 (ring + category glyphs) | Smallest type step; sits below caps 12. `glyph` names the actual rendering job (single-character icons drawn on a ring, not readable text) and prevents future confusion with `font_caps_px` (12) or body/helper (13). `font_caption_px` rejected (10px is not caption text — no line wraps at 10). |

**Spacing** — names are semantic (`gap_<role>`), **not** scale letters. Reusing `gap_xs/s/m` across namespaces with different values (metrics `space_xs=4` vs proposed studio `gap_xs=6`, metrics `space_s=8` vs studio `gap_s=8`) would let a wrong-namespace swap pass the lexical ban silently. Semantic names make every migration self-auditing (a `12→gap_card(16)` swap is visible on the line).

| New token | Value | Sites landed | Justification |
|---|---|---|---|
| `studio_metrics::gap_micro` | 6.0f | Insights :54,68 (legend) | Legend cells (14px `activity_cell`) with 6px optical gap; distinct from row insets. `micro` vs `inline/compact` orders the ladder 6<8<10<12 without implying grid decimals. |
| `studio_metrics::gap_inline` | 8.0f | SameLine 8 sites on studio pages currently `metrics::space_s` (Transforms :95,203,210, plus Dict/Snippets/Insights SameLines) **if spacing migration adopted (§4)**; otherwise kept on `metrics::space_s` | Inline control-to-control gap (button/pill/icon neighbours). Value equals native `space_s` by grid coincidence (both derive 8 from the 4/8/16 card grid); the studio name marks the inline role. If §4 defers spacing migration, this token is **not added** and the 8-sites stay on `metrics::space_s` (documented grid reuse). |
| `studio_metrics::gap_compact` | 10.0f | Transforms :199 + cluster_w 10; Scratchpad :178,183 (mock-note bars), plus header `gap=10` var Scratchpad :73 | Compact inline/mini-row gap (header cluster glue, skeleton-bar rhythm). Distinct from `gap_section` (12) — the 10/12 pair is adjacent cadence; semantic names prevent swapping them (a 10↔12 swap moves the header cluster by 2px/scale, visible only in screenshots). |
| `studio_metrics::gap_section` | 12.0f | Group G1-G7, G10-G11 (Dummy 12 x7 + SameLine 12 x6 + cursor_x/cluster_w duplicates) | The universal section cadence (header→tabs→toolbar→cards). W0-INV-02 infers deliberate from 4-file consistency. `section` names the macro rhythm; it is **not** `space_m` (16) nor `gap` (24) — those are card-block and page-section respectively, a larger establishment. |
| `studio_metrics::gap_block` | 16.0f | Dummy 16 sites currently `metrics::space_m` (Insights :96,167; Scratchpad :50) **if spacing migration adopted**; otherwise kept | Block-to-block gap (tabs→cards, banner→recents). `block` avoids `gap_card` confusion: 16 in Dummies is air between blocks, while card *padding* 16 lives in unscanned `PushStyleVar` (never migrated). If §4 defers, token not added. |

**Minimal (fonts-only) vs full (fonts + gaps) variants.** Tokens `font_card_title_px`, `font_heading_px`, `font_subheading_px`, `font_glyph_px`, `gap_section`, `gap_compact`, `gap_micro` (7) clear all 26 comments *without* touching the 8/16 grid-reuse sites. Adding `gap_inline`/`gap_block` (9) additionally migrates the ~35 Wave-1 cross-palette spacing sites to studio vocabulary. This design recommends the **full 9-token set with staged commits** (§8): commit 1 adds all 9 + clears allowlisted gaps/fonts; commit 2 migrates 15-font sites to `font_card_title_px` and (optionally) 8/16 sites to `gap_inline`/`gap_block`. The two stages are independently revertible; ORCH may descope stage 2 to the 7-token minimal set without invalidating stage 1. The report's token count (9 + 2 file-local) is the full set; the minimal set is a strict subset and follows the same naming.

**File-local constants (not global tokens, §6):**
| Constant | Value | Defined in | Justification |
|---|---|---|---|
| `kStatusDotIndent` (MainWindow.cpp, file-local `constexpr float`, near `kRailWidth` :58) | 14.0f | MainWindow.cpp | Dot-advance advance shared by N1/N2; mirrors `kRailWidth` precedent (file-local drift documented, `rail_width` dead). Scanner-exempt (identifier arg). Global `metrics::` token rejected (single-file semantic; pollutes frozen header). |
| `kSearchWidth` / `kDialogInputWidth` (DictionaryPage.cpp), `kSearchWidth` (ScratchpadPage.cpp) | 240.0f / 360.0f | respective `.cpp` | Control design widths, not spacing scale. Scanner-exempt. Global width tokens rejected (§6). |

**No `metrics::*` addition.** `ThemePalette.h` is read-only for this tranche. If ORCH later wants a global 14/240/360 vocabulary, that is a separate DEEP item touching the frozen header + mockup regen — not folded into T-003.

## 4. Palette-split fix — `metrics::*` usage inside studio pages

### 4.1 Current split (evidence)

Studio pages/Wave-1 files mix vocabularies today [VERIFIED grep `metrics::` over src/ui/pages; W1-REV-01 §1, §5]:
- **Type:** `metrics::font_card_title_px` (15) on Dictionary/Snippets/Scratchpad/Display/Insights-dialog; `metrics::font_body_px` (13) on Transforms (`TextLink` :116,124, header measure :176,195,211, My-Transforms :299,303) — values coincide with studio `font_helper_px` (13) but names assert native semantics.
- **Spacing:** `metrics::space_m` (16), `metrics::space_s` (8), `metrics::space_xs` (4) across all 5 pages + widgets (e.g. Dictionary :143,156,178,194; Insights :79,96,167; Snippets :111,132,136; Transforms :95,203,210,430,530,582,677; Scratchpad :50,215).
- **Radii:** studio pages use `studio_metrics::radius_card` (12) while native uses `metrics::radius_card` (10) — same member name, different numbers; both namespaces are reachable in Transforms/Insights files (§5).

The split is load-bearing in one place: `ScratchpadPage.cpp` lives in the **global namespace** — unqualified `metrics::` does not resolve there; W1-IMPL-02 hit this and qualified `lwf::ui::metrics::` (W1-IMPL-02:62). New studio tokens sidestep this trap (all studio files already alias/reach `studio_metrics`; verify `ScratchpadPage.cpp:18` alias still applies).

### 4.2 Chosen fix — migrate type fully, spacing optionally (staged)

**Type (required in tranche 2):** all studio-page/widget `PushFont` sites move to `studio_metrics::font_*`:
- 15s → `font_card_title_px` (§3), 13s → existing `font_helper_px` (Transforms `font_body_px` → `font_helper_px` for measure/render parity, 6 sites: :116,124,176,195,211,299,303 — values 13==13, and Keycap measure fix W1-IMPR-01 :107 already proves the parity direction).
- Allowlisted fonts (F1-F6) → `font_glyph_px`/`font_heading_px`/`font_subheading_px`/`font_heading_px`.
- After: **zero `metrics::font_*` in the 8 files** (Dictionary, Insights, Snippets, Transforms, Scratchpad, Buttons, Display, Overlays). `metrics::font_*` remains the native vocabulary (MainWindow). Rationale: type tokens carry hierarchy semantics; a Native-helper on a studio page misstates intent even when the number matches.

**Spacing (staged, recommended):** stage 1 migrates only the allowlisted gaps (Section G) to `gap_micro/compact/section` — these are *new* values with no native twin, so studio vocabulary is unambiguously correct. Stage 2 migrates the Wave-1 8/16 grid sites to `gap_inline`/`gap_block` **or** documents them as intentional shared-grid reuse on `metrics::space_s/m` (both acceptable; the design's default is to **migrate**, because W0-INV-02's principle — "route each literal to the same-value token in its own palette's namespace" — is the stated rule, and post-Wave-1 studio files would otherwise permanently import native spacing for every future grid edit). If ORCH descopes stage 2, the tranche header comment in `StudioPalette.h` must record: *"8/16 spacing on studio pages intentionally reuses `metrics::space_s/m` grid values; do not edit either side expecting the other not to move."* That sentence is the coupling documentation — it converts silent coupling into a recorded dependency.

**What is NOT in the palette-split fix:** `ApplyStudioStyle` call sites (ScratchpadWindow.cpp:91,591) and `ApplyImGuiStyle` (MainWindow :180,336; Overlay :125,339) are untouched (brief §6; W0-INV-04 dual-palette deferral). No color, style, or context change. The tranche fixes *spelling vocabulary*, not the dark-context + teal-cards render split — that split needs the separate palette-unification plan (ledger:32).

## 5. `kRailWidth` doc + `radius_card` 10-vs-12 collision — document only, no value change

- **`kRailWidth=220` (MainWindow.cpp:58) vs `metrics::rail_width=208` (ThemePalette.h:95):** LEAVE + DOCUMENT per mission D-rail (00-mission.md:31) and W0-INV-04 §P-004 (evidence: introduced a4ab50a as spec; test pins `kRailWidth` at test:663; 220→208 shrink risks label clipping at 760x520). Wave-1 already added the accurate comment [VERIFIED MainWindow.cpp:56-58: "Rail is 220 on purpose: metrics::rail_width (208) is intentionally unused until the rail/token drift is resolved (mission D-rail)"]. Tranche 2 **does not edit** this constant or its comment further (comment already satisfies P-004 doc requirement); the design records the decision so the tranche does not reopen it. Future retoken remains DEEP (mockup regen + test :663 rewrite + re-screenshot), out of this run.
- **`radius_card` collision (metrics 10 vs studio 12):** DOCUMENT ONLY. Both headers carry the warning (header comment only, no value change):
  - In `StudioPalette.h` above `radius_card`: `"Native metrics::radius_card is 10 (mockup). Studio radius_card is 12 (screenshot-derived). They are not interchangeable — studio sites must qualify studio_metrics::radius_card; native sites metrics::radius_card."`
  - In `ThemePalette.h` above `radius_card`: *no edit in tranche 2* (frozen header; adding a comment is a header touch that risks hotspot review — the StudioPalette.h side alone is sufficient because the hazard is studio writers importing the wrong 10). If ORCH wants both sides, the ThemePalette edit is comment-only and still requires no value pin change, but it is **not** in the tranche write-set (§8).
  - Usage rule for the tranche: every radius site in the 8 files is re-checked by eye (no automated guard exists). No radius literal is migrated (radii were already tokened; the collision is nominal, not literal-bypass). The `has_metrics_font` flip-check unaffected.

## 6. Where structural literals stay — and why (NOT tokenized)

The 5-API lexical scanner [VERIFIED test:1174-1256] deliberately scopes to `PushFont(nullptr, N)`, `Dummy(ImVec2(0.0f, N))`, `Dummy(ImVec2(N, ...))`, `SameLine(0.0f, N)`, `SetNextItemWidth(N)`, exempting universal factors `{0,0.5,1,2}`, the `-1.0f` width sentinel, token-carrying lines, and identifier / named-constant arguments [:1206-1210,1165-1169]. Literals **outside** those anchors are unscanned by design ("Generic ImVec2 geometry outside Dummy ... is deliberately not scanned" :1169-1173). The tranche respects that boundary:

| Literal (still literal after tranche) | Locations | Why it stays (not a missing token) |
|---|---|---|
| `SetNextItemWidth(240.0f*scale)` / `(360.0f*scale)` | Dictionary :173,349,355; Scratchpad :198 | Control width design decisions (search 240, dialog 360), not spacing/radius. No spacing token encodes "field width"; `content_max=640` is a page maximum, not a field size. **Resolution: hoist to `kSearchWidth`/`kDialogInputWidth` file-local constants** (scanner-exempt, BanAllowance entries delete, §7) — named, not globally tokened. Promoting to a global `metrics::field_width_*` is rejected (single-use widths would pollute the mockup header for non-mockup sizes). |
| `pill_h + 18.0f*scale` (Dummy height) | ScratchpadPage.cpp:124 `Dummy(ImVec2(0.0f, pill_h + 18.0f*scale))` | Expression form: `pill_h` (32*scale) is computed; `18` is banner-to-recents clearance inside the Scratchpad block. Scanner anchor sees `pill_h` (identifier → `ParseNumberAt` fails → no flag) [VERIFIED scanner :1205]. 18 has no spacing token (font_wordmark 18 is type). **Resolution: stay literal**, optionally hoist to `kBannerClearance=18.0f` for spelling hygiene; no global token (single-use clearance). Documented here so the tranche does not "fix" it to `gap_block` (16, −2px shift). |
| `cursor_x` + `12.0f*scale` | TransformsPage.cpp:307-308 | Cursor-offset arithmetic mirroring the rendered `SameLine 12` at :314. **Resolution: migrate the 12 to `gap_section`** (same value) with the SameLine, even though the scanner never enforced it — this is the R-W1-001 model/layout parity fix (W1-REV-01 R-W1-001; Wave-1 delta fixed only the 8-half [W1-DELTA-01:6]). Same for `cluster_w` 10/12 (:189-191): migrate both to `gap_compact`/`gap_section` so the centered cluster cannot misalign the header by 2px if a future token ever moves. Value-identical today. |
| Button-width models `+28.0f` / `+32.0f` *scale | Transforms :186 (+28), :300/:304 (+32); Dictionary :303-305 (+32); Scratchpad :72 (+28); Overlays :130,135 (+32) | `CalcTextSize(label).x + chrome*scale` approximations of button widths. +32 = FramePadding 16×2 (Buttons.cpp:41,81 `(16,8)` literal — itself unscanned `PushStyleVar`) ; +28 is a distinct chrome tolerance. **Resolution: stay literal.** These model `StyleButton` chrome, not layout gaps; mapping +28/+32 to `space_*` would conflate padding with gaps and mislead (also +28≠32 proves chrome ≠ one token). Centralizing from FramePadding is the W1-REV-01 DO LATER item (needs a `button_padding` token + widget API decision) — explicitly deferred, not folded into T-003. No scanner cap applies (assignment arithmetic). |
| `40.0f*scale` rail rows (and button/toast heights) | MainWindow.cpp:471 `item_h = 40.0f*scale`; Buttons.cpp:58,68; Dictionary :309; Scratchpad :305 | Rail row height 40 is deliberate drift from mockup nav 36 (P17 spec; W0-INV-04 §P-001 documents 28px vertical delta for 7 rows, pinned by test:664 `CHECK(content.find("40.0f * scale")...)`). `control_min_height=32` is a floor, not a row size (W0-INV-02 §11.4). **Resolution: stay literal.** Hoisting to `kRailItemHeight` would break the test pin string and needs a pin update for zero visual gain; leaving it keeps the pin green and the intent discoverable. Do **not** map to `font_banner_px` (40, wrong role) or `control_min_height` (32, −8px shrink). The coincidence (40 == banner) is documented here to prevent future "deduplication". |
| 14px row insets / `pad_x` / gauge offsets | Scratchpad DrawNoteRow :248/253/259/263; Insights icon :288; Buttons Chip :179; Display :134; plus `cursor_x` word-wrap :623-655 (token_size arithmetic) | `ImDrawList::AddText`/`AddCircle` geometry, outside the 5 APIs. **Resolution: stay literal.** They are absolute in-row offsets (8/30 pair, W0-INV-02 §3), not inter-item gaps; tokenizing them as gaps would mislabel and invite grid edits that break rows. File-local `kRowIndent` hoisting is an optional hygiene follow-up, not required for caps-zero. |
| `kSheetWidth=440`, `kCardPadding=20`, `kCardGap=16` (Transforms/Snippets named constants) | Transforms :37-38, :462 `440.0f`; Snippets :22 | Already named — the "good local pattern" (W0-INV-02 §P-002). **Stay as-is.** 16-vs-20 card-pad drift is documented known drift (values frozen; equalizing needs mockup sign-off). |

**Rule for the tranche:** a literal is tokenized iff (a) it sits in a scanned API, (b) a same-value token exists or is added in §3, and (c) its role matches the token's role. Everything else stays literal or is hoisted to a file-local name — never nearest-mapped.

## 7. Test plan — exact updates

All edits are in `tests/test_overlay_settings_ui.cpp` (T-004 file, single-writer per ownership/W1.tsv pattern; tranche 2 code and test must land together or test-only first as red-by-design — see §8 sequencing).

### 7.1 BanAllowance — 17 entries → 0 (caps 32 → 0, comments 26 → 0)

**Decision: all entries delete; the allowance table itself ships empty** (`kBanAllowances` retained as an empty array with a comment, or removed entirely if the struct is unused — prefer **retain empty** so a future token-gap can be re-added without resurrecting the type, and so the scanner's count logic stays live).

| Entry group (current) | Count | Tranche disposition |
|---|---|---|
| PushFont 10/24/17 (4 entries, caps 2+2+1+1=6) | 6 | **DELETE** after §3 fonts land (F1-F6 migrated; grep `PushFont(nullptr, [0-9]` over the 8 files must return 0) |
| DummyHeight 12 (5 entries, caps 3+1+2+1+2=9) | 9 Dummy | **DELETE** after `gap_section` migration (G1-G7,G10-G11) |
| DummyHeight 10 (Scratchpad, cap 2) | 2 | **DELETE** after `gap_compact` (G8-G9) |
| SameLine 6 (Insights, cap 2) | 2 | **DELETE** after `gap_micro` (G12-G13) |
| SameLine 12 (Transforms, cap 6) | 6 | **DELETE** after `gap_section` (G14-G19) |
| SameLine 10 (Transforms, cap 1) | 1 | **DELETE** after `gap_compact` (G20) |
| DummyWidth 14 (MainWindow, cap 2) | 2 | **DELETE** after `kStatusDotIndent` hoist (N1-N2 become `Dummy(ImVec2(kStatusDotIndent*scale,0))` — identifier anchor → no flag) |
| SetNextItemWidth 240/360 (3 entries, caps 1+2+1=4) | 4 | **DELETE** after `kSearchWidth`/`kDialogInputWidth` hoists (W1-W4 become `SetNextItemWidth(kSearchWidth*scale)` etc. — anchor sees identifier → no flag) |

Result: `grep -c "ALLOWLIST (Wave 2 T-003)"` over `src/ui/` → 0; `ScanLayoutLiterals` over the 9 `layout_sources` → 0 violations at HEAD; `kBanAllowances` empty. This is the **"caps 26→0"** answer: the 26 comments go to 0 **and** the 4 SetNextItemWidth literals go to 0 via hoisting; the only literals that remain in the 5 APIs are the universal `{0,0.5,1,2}` and `-1.0f` sentinel forms, which were never counted. A reviewer verifying "caps→0?" checks both greps plus a green ban TEST_CASE.

**Cap-semantics note (must ship with the code):** caps are ceilings [VERIFIED :1122-1128], so the empty-table green state is robust to *removal* (a site already tokened stays green) and sensitive to *addition* (any new digit literal fires immediately as a violation, no cap to hide behind). That is the intended fail-before/pass-after: reintroducing a digit literal after tranche 2 fails the ban TEST_CASE; reverting any single §3 token to its literal also fails it. The empty table is therefore strictly stronger than the Wave-1 table.

### 7.2 Token value pins — `studio palette layout tokens follow the shape scale` (:714-739)

Append 9 CHECKs (full set; 7 if minimal) in the same `namespace metrics = lwf::ui::studio_metrics;` block, after :738 `progress_height`:

```
CHECK(metrics::font_card_title_px == 15.0f);
CHECK(metrics::font_heading_px == 24.0f);
CHECK(metrics::font_subheading_px == 17.0f);
CHECK(metrics::font_glyph_px == 10.0f);
CHECK(metrics::gap_micro == 6.0f);
CHECK(metrics::gap_inline == 8.0f);      // full set only
CHECK(metrics::gap_compact == 10.0f);
CHECK(metrics::gap_section == 12.0f);
CHECK(metrics::gap_block == 16.0f);      // full set only
```

These pin the *same-value* half of the proof (token == replaced literal). The lexical-ban half (no digit literal remains) is pinned by the ban TEST_CASE itself. No `ThemePalette.h` pin changes (native test at :230-241 untouched — Option B's pollution avoided). Contrast pins (:741-769) untouched (no contrast input).

### 7.3 Flip checks — `has_metrics_font` → `has_studio_font` (REQUIRED, easy to miss)

The ban TEST_CASE tail (:1292-1327) asserts each studio page carries a `PushFont(nullptr, metrics::font_` (or qualified). After §4 type migration those files carry **zero** `metrics::font_` pushes, so the checks fail **by design**. The test tranche must flip them:

- Keep `has_metrics_font` for native surfaces only (MainWindow continues to push `metrics::font_*`; if the layout_sources loop is extended, MainWindow remains the metrics-font proof).
- For Dictionary, Insights, Snippets, Transforms, Scratchpad: assert `has_studio_font` (`PushFont(nullptr, studio_metrics::font_`) instead. Snippets already asserts both (:1320-1321) — drop the metrics leg there.
- Scratchpad's `has_token_font` (:1325-1327) becomes `has_studio_font` alone (it already passes via studio, but the metrics-or-studio disjunction would silently pass on a regressed cross-palette site — tighten it).
- Display/Buttons/Overlays are not in the flip block today; no change needed (their coverage is via the ban + pins).

This flip is the *palette-split proof*: a future regression that reintroduces `metrics::font_card_title_px` on a studio page fails the flipped check, whereas today it would pass. Do not ship the code migration without this test flip.

### 7.4 Negative scanner test — `layout literal ban fires on violations and cap-exceeds` (:1330-1372)

Two legs go red on the empty-table state and must be rewritten synthetically (W1-IMPR-02 created them; W1-REV-01 MAJOR required them; they must survive tranche 2):

1. **Cap-exceed leg** (:1339-1344: Buttons `PushFont 24` x2 vs cap 1): with the entry deleted, two hits become two *violations* rather than a *cap-exceed* — the CHECK (`!empty`) still passes, but the comment/category lies. **Fix:** re-label as `digit_violation` (rename variable + comment) **or** introduce a synthetic allowance for the test (e.g. a test-only `BanAllowance` table override parameter). Preferred: re-label and assert the violation message contains both line numbers? The scanner's violation strings include line numbers (:1223-1227); asserting their presence strengthens the test. Do **not** keep a dummy cap to preserve the leg's wording — that would defeat caps→0.
2. **Dummy-safe leg** (:1368-1371: Dictionary `Dummy(...,12.0f)` expecting *empty* "within cap 3"): with the cap deleted, a single 12.0f is a *violation* — CHECK fails. **Fix:** replace the synthetic with a universal/tokened form that is genuinely safe (e.g. `Dummy(ImVec2(0.0f, 1.0f))` universal, or `Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale))` tokened — both must be empty). Preserve the test's intent ("a single gap line is not a violation") by making the synthetic actually tokened, and add a companion `CHECK` that the *literal* 12.0f now violates (proves the caps→0 state). That companion is the tranche's fail-before in miniature.
3. **Universal + sentinel legs** (:1346-1363): untouched — they prove the exemptions that remain. After caps→0 these legs are the only remaining exemptions; they must stay green.

### 7.5 Glob / file-list expansion — KEEP the explicit list (deferred glob)

`layout_sources` hardcodes 9 files (:1269-1279) [VERIFIED]. W1-TREV-01 R-W1-003 deferred a `src/ui/**/*.cpp` glob to Wave 2. **Decision: keep the explicit list in tranche 2.** Rationale: (a) the scanner's exclusions (:1169-1171) are Overlay (Mini), ScratchpadWindow (second window), ThemePalette.cpp/StudioPalette.cpp (frozen internals) — a naive glob would either include them (false violations on frozen internals / out-of-scope windows) or need exclusion filtering + `<filesystem>` sorting, churn disproportionate to tranche 2's 9 files; (b) tranche 2 adds no new source files, so the list is complete; (c) the escape risk ("a new page file silently escapes") is mitigated by adding a one-line comment at :1269: *"New Full-window sources must be added here (see exclusions :1169-1171); Windows type-check (G3) runs per-file."* A glob + exclusion filter is a standalone test-improvement item, not a token-gap dependency. If ORCH insists on glob now, the filter must preserve the 4 exclusions and the test must still fail-before on a synthetic new-file insertion — i.e. a 4th synthetic leg.

### 7.6 Scoped verification commands for the tranche (must all be recorded in the impl report)

- `grep -rn "PushFont(nullptr, [0-9]" src/ui/` → 0 (fonts); `grep -rn "ALLOWLIST (Wave 2 T-003)" src/ui/` → 0; ban TEST_CASE (`full window layout APIs use tokens not digit literals outside the token-gap allowlist`) → pass; negative TEST_CASE (`layout literal ban fires on violations and cap-exceeds` after §7.4 rewrite) → pass; token-pin TEST_CASE (`studio palette layout tokens follow the shape scale`) → pass; full `ctest -R lwf-tests` → 1/1 (needs code+test landed together; test-only is red-by-design, code-only leaves pins absent — sequence in §8).
- G1: `cmake --build build --target local-wisper-flow -j4` (compiles src/ui; lwf_tests alone does not [VERIFIED W1-IMPL-02:62, CMakeLists sources]); G3 zig pending; G4 hygiene (comments contain no TODO/windows.h); P-006 screenshots (§9).

## 8. Rollout, rollback, sequencing — and the implementation tranche write-set

### 8.1 Sequencing after Wave 1 HEAD (`ea04e1a..`)

Wave 1 closed at `ea04e1a` (gate w1-2 green, delta APPROVE [VERIFIED 01-work-ledger.md:44-45]; `b824483` is the W2 brief docs commit on top). Tranche 2 is sequenced as a **single dedicated agent, two commits (code then tests, or scoped test-first red-by-design per W1 pattern)**:

1. **Commit 1 (code + forward-refs):** `StudioPalette.h` (§3 constants + §5 radius comment) + 9 `.cpp` files: `src/ui/pages/DictionaryPage.cpp`, `InsightsPage.cpp`, `SnippetsPage.cpp`, `TransformsPage.cpp`, `ScratchpadPage.cpp`, `src/ui/widgets/Buttons.cpp`, `Display.cpp`, `Overlays.cpp`, `src/ui/MainWindow.cpp` (14 hoist only). This commit is **red-by-design** on the flipped pins/ban until commit 2 (same pattern as T-002 → T-004: ctest 517/518 transient, sole failures owned by the pin flip, required to cite in the report).
2. **Commit 2 (tests):** `tests/test_overlay_settings_ui.cpp` (§7.1 empty table, §7.2 9 pins, §7.3 flipped checks, §7.4 synthetic rewrites). At commit 2, `ctest -R lwf-tests` → 1/1, `local-wisper-flow` target green, gate G1/G2/G4 green.
3. **Gate + screenshots (P-006)** before merge: Windows 960x640 + 760x520, DPI 1.0 + 1.5, Home/Settings/Welcome + 5 studio pages (+ Scratchpad editor window for Buttons/Display/Overlays churn — W1-IMPL-02:55/70 note). Expected diff is nil by §3 value proof; the gate is the visual backstop for the false-value-swap residual (lexical tests cannot see a 15→13 swap, W1-TREV-01 R-W1-002 deferred to screenshots).

If ORCH splits the agent: **Agent A** owns `StudioPalette.h` + fonts/fonts-migration; **Agent B** owns gaps + 14/width hoists + `cluster_w`/`cursor_x`; **Agent C** owns `tests/test_overlay_settings_ui.cpp` alone (C5 disjoint per ledger). Agents A/B must not both edit the same `.cpp` (Transforms/Insights/Scratchpad are shared) — serialize on those files or assign pages by file (Dictionary/Insights A, Snippets/Transforms B, Scratchpad/Buttons/Display/Overlays split). Single-agent is preferred (§8.1 default) precisely to avoid this.

### 8.2 Write-set (tranche 2)

- **Edited:** `src/ui/theme/StudioPalette.h` (additive constants + radius comment only; no existing value touched); the 9 `.cpp` above (spelling only); `tests/test_overlay_settings_ui.cpp` (§7).
- **Not edited (explicitly forbidden for this tranche):** `src/ui/theme/ThemePalette.h`, `src/ui/theme/ThemePalette.cpp`, `src/ui/theme/StudioPalette.cpp` (style applicator internals), `src/ui/MainWindow.h`, `src/ui/widgets/Widgets.h` (widget API signatures — `RowList` deletion is R-W1-002 deferred, §9), `src/ui/Overlay.cpp`, `src/ui/ScratchpadWindow.cpp` (out-of-scope Windows; Scratchpad editor shares widgets so its pixels ride along, but its file is never edited), `src/ui/pages/*.h`, `tests/test_scratchpad_page.cpp` / `test_snippets_page.cpp` (pin flips already landed Wave 1; §7.3 is the only test file).
- **Contract-set (headers):** `StudioPalette.h` additive-only. Downstream risk: any `#include`r of `StudioPalette.h` gains new constants — additive, no ABI/API break (`inline constexpr` floats; no signature change). `ThemePalette.h` contract unchanged.

### 8.3 Rollback

- **Roll back §3 tokens:** `git revert <code-commit>` restores digit literals 1:1 (values never moved, so revert is value-neutral). No migration, config, or data involved (compile-time spelling only). Roll back tests with the same revert (empty table → Wave-1 caps restore).
- **Roll back stage 2 only (§4 spacing migration):** `git revert` the second code commit restores `metrics::space_s/m` on studio pages; `gap_inline`/`gap_block` then become unused-but-harmless (compiler does not warn on unused `constexpr`). Do not delete the stage-1 tokens on a stage-2 revert.
- **Forward-fix trap to avoid:** never "fix" a red ban by lowering a cap or re-adding an allowance entry — the empty table is the contract; a red ban after tranche 2 means a digit literal was reintroduced and must be migrated or hoisted. This is the reversal test (§9).

### 8.4 Observability

- **What to watch in screenshots:** section cadence (12px Dummies after headers/tabs/toolbars), header clusters (Transforms opt-in cluster: title↔BadgeBeta 12, label↔info 10, toggle↔keycaps 12), Usage/Streak heading sizes (24), Recents heading (17), ring/category glyphs (10), legend gaps (6), status-dot indent (14), search/dialog widths (240/360). All must be pixel-identical to Wave-1 baseline at both window sizes / both DPIs.
- **What cannot be observed automatically:** a wrong-value token swap (e.g. `gap_section`(12) used where `gap_compact`(10) was meant) passes the ban and pins but shifts 2px/scale. Mitigation is review-time value audit (this design's Tables §1/§3) + P-006. This residual is inherited from the lexical approach (W1-REV-01 §3) and is not introduced by tranche 2.

## 9. Failure modes, mockup/contrast, risks

### 9.1 Failure-mode table

| Failure mode | Detection | Consequence if missed |
|---|---|---|
| Wrong token at one site (12↔10, 24↔28, 15↔13) | **None automated** (ban exempts tokened lines; pins only prove token values, not site mapping) | Silent density shift (2-5px/scale); P-006 screenshot is the only catch. Mitigation: impl report must list every site's old-literal→new-token pair (cf. W1-REV-01 §1 audit) for blind review. |
| Token added with wrong value (e.g. `gap_section=16`) | Pin CHECK (`== 12.0f`) fails — **good**, loud | Compile-time green, test red. Mitigation: pins land with tokens (§7.2); do not land code without pins. |
| Existing token value edited (e.g. `gap=24` → 12) | Native/studio pin suite fails (contrast unaffected but shape-scale test red) | Hierarchy collapse across all gap sites. Mitigation: diff review must show **zero** changes to existing lines in both headers (pure addition). |
| Cross-palette regression (`metrics::font_*` reappears on studio page) | Flipped §7.3 checks fail — **good** (new guard) | Type-vocabulary leak returns silently otherwise. |
| New digit literal smuggled into a file not in `layout_sources` | None (explicit list, §7.5) | Escape. Mitigation: comment at :1269 + G3 per-file listing; glob deferred. |
| `cluster_w`/`cursor_x` left literal while SameLines migrated | No scanner flag (unscanned) — needs review to see | Header cluster mis-centers by the drift amount if a token ever moves. Mitigation: §6 migration obligation (not optional). |
| `pill_h+18` "fixed" to `gap_block` (16) | No scanner flag (expression) | −2px banner clearance, Recents creep. Mitigation: §6 stay-literal (§6 table) + impl report must state it was left. |
| `40.0f` rail "deduped" to `font_banner_px` or `control_min_height` | Test :664 needs `"40.0f * scale"` — a hoist breaks it (loud) | −8px rail rows or wrong-role coupling. Mitigation: §6 stay-literal; pin stays. |
| `radius_card` crossed (studio 12 used on native card) | No automated check | 2px rounding delta on cards. Mitigation: §5 comment + eye check; no radius migration in tranche. |

### 9.2 Why mockup regen + contrast are not required

New constants are **spacing and font-size geometry only**. Contrast policy is computed exclusively from `Palette` colors via `RelativeLuminance`/`ContrastRatio` (ThemePalette.h:141-159) and enforced on color pairs (:741-769), never on sizes. Adding `font_heading_px=24` cannot change any contrast ratio or any existing pair's pass/fail. Mockup regen is triggered by *palette value* changes (ThemePalette.h:5-16: "Do not change values here without regenerating the contrast tables") — this tranche changes no palette value and no existing token value, only adds screenshot-derived sizes. The tranche records (header comment) that the 9 constants are **not mockup tokens** so a future mockup regen does not mistake them for D0 content.

### 9.3 Open risks (ordered)

1. **P-006 screenshots still pending** (01-work-ledger.md:45; gate-w1-1/w1-2 note them pending). Tranche 2 multiplies the evidence burden: ~60 spelling sites must be visually nil. Do not declare hierarchy proof on green tests alone.
2. **RowList dead code** (W1-REV-01 R-W1-002): `widgets::RowList` (Display.cpp) has zero callers; its Wave-1 15→token edits are inert and its future `gap_*` edits will be too. Tranche 2 migrates it uniformly (same tokens) so it stays consistent if ever called, but screenshots cannot cover it — delete-or-exercise remains a separate follow-up (write-set: Display.cpp + Widgets.h — API decision, not token decision).
3. **G3 zig pending** (no zig on dev box): `local-wisper-flow` link is the MSVC-compat proxy; per-file zig syntax-only is unproven for the 9 `.cpp` edits. Record pending in impl report + `docs/QUESTIONS.md` per AGENTS.md.
4. **Card padding 16-vs-20 drift** (Transforms `kCardPadding` 20 vs house 16): unscanned, values frozen, documented. Equalizing would be a density change needing mockup sign-off — not tranche 2.
5. **Dual-palette render split** (dark context + teal cards): unchanged by design; if P-006 shows the split as unacceptable, it goes to the palette-unification plan (separate DEEP), not fixed inside T-003.
6. **Single-token dual-use coupling** (`font_heading_px` for headings + IconButton glyph): if glyph sizing ever diverges from headings, the coupled token forces a split edit. Mitigated by the header comment; split cost is one new constant + one site.
7. **Wave-1 palette-split lesson for `ScratchpadPage.cpp` global namespace:** unqualified `metrics::` fails there. New `studio_metrics` tokens are reachable via existing alias, but the tranche must rebuild `local-wisper-flow` (not just `lwf_tests`) to prove it — the W1-IMPL-02 build lesson.

## 10. Long-run test (standing orders §8) — source / load / hostile / operability / reversal

- **Source / deterministic:** ban TEST_CASE (empty allowlist) + 9 value pins + flipped `has_studio_font` checks + negative scanner TEST_CASE (rewritten synthetics). Deterministic, no ImGui link, runs in ms. Fails on any reintroduced digit literal in the 9 files (fail-before demonstrated by the companion "literal 12 now violates" assertion in §7.4 — the impl must cite this as the fail-before evidence).
- **Load (100x / concurrency / partial failure):** N/A — pure compile-time spelling (`inline constexpr float`, zero runtime delta). State explicitly in the impl report (cf. W1-IMPL-02 §8). Partial-failure mode is missing source → `REQUIRE_FALSE(content.empty())` aborts loudly [VERIFIED :1283], never silent pass.
- **Hostile input:** N/A — no user-input/format/path handling touched. The only "hostile" case is a future literal smuggled into an unlisted file; §7.5 comment + G3 listing is the mitigation; glob is the systemic fix (deferred, not tranche 2).
- **Operability / observability / rollback:** §8 (revert code, revert tests, P-006 visual contract). Observe: pixel-identical screenshots across 4 conditions (960x640/760x520 × DPI 1.0/1.5) + editor window. Roll back: revert order tests→code (or both together); no data migration.
- **Reversal (which test fails if the fix is reverted?):** revert **any** §3 token to its literal (e.g. `font_heading_px` → `24.0f`) → ban TEST_CASE fails (violation, no cap left); revert **any** pin (`gap_section == 12.0f` → `== 13.0f` in a scratch edit) → token-pin TEST_CASE fails; revert **§4 type migration** (one `studio_metrics::font_card_title_px` → `metrics::font_card_title_px`) → flipped §7.3 check fails (proving the palette-split guard is load-bearing); revert **hoists** (`kStatusDotIndent` → `14.0f`) → ban fails on the DummyWidth site. That four-way reversal matrix is the tranche's test justification; the impl report must run/record the three scripted reversals (or at least the ban and pin legs) as fail-before evidence.

## 11. Out-of-scope findings / F-findings (no action in tranche 2)

- F1: `TransformsPage.cpp:110` `width += text_w + 24.0f*scale` (KeycapRowWidth chrome) shares the 24 value with `font_heading_px` but models keycap padding, not type — stays literal ([VERIFIED :110]). Do not migrate.
- F2: Scratchpad header `gap = 10.0f*scale` var (ScratchpadPage.cpp:73) and `+10.0f*scale` title offset (:64) are assignment/SetCursor geometry — `gap` var migrates to `gap_compact` for model parity (same var feeds `controls_w = flow_size + gap + ...`), offsets stay (role: title/BadgeBeta glue vs header-cluster glue — same token accepted, same value).
- F3: `Overlay.cpp` / `ScratchpadWindow.cpp` literal clusters remain (out of Full-window scope, 00-mission.md:13). Their `SameLine 4/6` and widths are not cleared by this tranche's caps-zero claim — the claim scopes to the 9 `layout_sources` only.
- F4: No new files are introduced by tranche 2 (no new tokens need a new header; `StudioPalette.h` is the home).

## 12. Least-confident / shared-decision items for ORCH

1. **Full (9) vs minimal (7) spacing set** (§3/§4): the `gap_inline`/`gap_block` pair (8/16 migration) is the one genuinely judgmental choice. The design recommends full — staged, worth the churn for the own-namespace rule — but ORCH may descope to minimal and have stage 1 alone clear the 26 comments. Either is pixel-identical; the difference is whether studio files ever stop importing `metrics::space_s/m`.
2. **Single vs split 24 token** (heading + IconButton glyph): single is fewer symbols, split is cleaner semantics. Design defaults single with a split-ready comment; ORCH split costs one constant, zero pixels.
3. **Glob vs list** (§7.5): list kept for tranche 2; glob-first ORCH would need the exclusion-filter + synthetic new-file leg in this tranche. Design recommendation is list, to keep the token-gap tranche free of test-architecture churn.

## 13. Commits (this task)

Docs-only design. No code commit. Report commit trailers per brief §8: `Task: T-003 Agent: W2-DEEP-01 Wave: 2 Report: docs/agent-ops/20261108-ui-hierarchy/reports/w2-deep/W2-DEEP-01-design.md` (docs-only commit; empty code SHA list is intentional — no source edits in this task).

## 14. Self-review

- Every count in §1 was reconciled against the scanner table (17 entries) and the comment grep (26) — 26 = 6 + 20; +2 (14) +4 (widths) = 32 under caps. A reader who counts only one allowlist will under-count by 6; §1 makes the split explicit.
- Every migration in §3 was checked for same-value (token == literal) against current headers; no nearest-value mapping survives anywhere in the design (Option C excluded, §6 stay-literals listed to block drive-by nearest mapping).
- Review traps re-run: (a) no `ThemePalette.h` value change proposed; (b) no palette unification (`Apply*Style` untouched); (c) no `40.0f`/`220`/`kRailWidth` value change (pin-safe); (d) `has_metrics_font` flip flagged as mandatory test edit (§7.3) — the trap most likely to be missed if the design only listed BanAllowance; (e) negative-test dummy_safe red-flagged before it bites (§7.4).
- What was **not** verified (honest limits): no builds (brief §7, read/grep only); no screenshots (P-006 manual Windows); no zig (G3 pending). Ground-truth claims are header/source reads at HEAD `b824483`/`ea04e1a` lineage, not build results.
