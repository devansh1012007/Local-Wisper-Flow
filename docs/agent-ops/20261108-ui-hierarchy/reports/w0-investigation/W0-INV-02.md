<!-- ORCH-DIGEST:START -->
agent: W0-INV-02 | domain: hierarchy/density patterns (spacing, cards, typography, page layout) | status: DONE
items: 3 verified EXISTS (P-001 literals in MainWindow; P-002 literals in studio pages + dual-palette split; P-003 widget density drift) / 37 literal PushFont sites counted / 64 Dummy + 62 SameLine sites in src/ui
proposed_tasks: 4 (SURFACE 3, DEEP 1) — T-001 MainWindow normalize, T-002 studio pages normalize, T-003 widget density, T-004 source-scan test (DEEP: token-gap decisions for 24/17/10px fonts + shared-BeginCard extraction)
top_risks: (1) dual-palette rendering inside single MainWindow ImGui context — studio pages get dark style, hand-draw studio colors; (2) 24/17/10px fonts have no token in either palette — normalization needs new tokens or value changes; (3) visual regressions invisible to source-scan tests — screenshot gate is the only proof
cross_domain_conflicts: none identified from this domain; P-004 rail drift (kRailWidth 220 vs metrics::rail_width 208) matches mission D-rail default (leave + document)
out_of_plan_discoveries: 5 (TransformsPage uses metrics:: fonts on studio palette; DictionaryPage mixes kDarkPalette.err into studio page; Display Widget fonts 15/13 literals predate page-level pattern; 13px = font_body_px == font_helper_px numeric duplicate; ScratchpadWindow is second OS window outside Full-window scope but same tokens on second context)
confidence: high (every count from grep over src/ui this session; three key claims re-checked against source before writing)
last_comms_line_read: 2026-11-08T04:05:00Z (WAVE-START)
<!-- ORCH-DIGEST:END -->

# Investigation report — W0-INV-02 — hierarchy/density patterns

## 0. Scope and method

Question: how spacing, cards, typography, and page layout are already solved, and where
spacing/typography literals bypass the token systems, so P-001..P-003 can follow existing
conventions (scoped PushFont design-px, metrics vs studio_metrics, BeginCard, shared widgets).

Method (read/grep only, no builds — brief §7):
- Read project guidance first: AGENTS.md UI section, 00-mission.md, 01-work-ledger.md,
  templates/STANDING-ORDERS.md, templates/report-investigation.md, COMMS.md.
- Read every in-scope token/layout source named in brief §2: src/ui/theme/ThemePalette.h,
  src/ui/theme/StudioPalette.h, src/ui/widgets/Widgets.h, Buttons.cpp, Display.cpp,
  DictionaryPage.cpp:115-237, InsightsPage.cpp:85-107 (spot-read rest in full).
- Grepped src/ui for: PushFont( (156 hits incl. comments), PushFont literal class
  `PushFont(nullptr, [0-9]` (37), Dummy( (64), SameLine( (62), ApplyStudioStyle /
  ApplyImGuiStyle (13), metrics:: / studio_metrics:: usage by file, kStudioPalette /
  kDarkPalette / kLightPalette, raw literals (40.0f/32.0f/24.0f/16.0f/14.0f/10.0f/17.0f etc.),
  BeginCard/BeginChild, kRailWidth/rail_width.
- Cross-read MainWindow.cpp (rail 436-564, BeginCard 574-588, page chrome 566-572,
  RenderPages 407-434, Draw*Page delegations 1399-1442), ThemePalette.cpp,
  StudioPalette.cpp, ScratchpadWindow.cpp (tabs/toolbar 439-536), docs/design/mockup.html
  (presence only — full token verification is W0-INV-04's job), and the existing
  source-scan tests in tests/test_overlay_settings_ui.cpp:243-330, 698-738.
- All numeric spacing claims below are design px at 1.0 scale, multiplied by window DPI
  scale (ThemePalette.h:81-82).

No scratch files created. Only write is this report (brief §4). COMMS.md was read but not
appended (not in WRITE-SET; no NEED outside scope arose).

## 1. The two token systems (ground truth)

### 1.1 Native metrics (Fullscreen window — MainWindow, Overlay)
[VERIFIED src/ui/theme/ThemePalette.h:83-137]
- Geometry: space_xs 4, space_s 8, space_m 16, space_l 24, space_xl 32; radius_control 6,
  radius_card 10, radius_pill 999; rail_width 208, content_max 640, control_min_height 32;
  page 960x640 default, 760x520 min.
- Type scale: wordmark 18, page_title 28, card_title 15, body 13, mini_text 8.5.
- Style application: ApplyImGuiStyle maps geometry to ImGuiStyle (WindowPadding =
  space_m 16, ItemSpacing = space_s 8 — ThemePalette.cpp:60-62), sets FontScaleMain =
  scale exactly once per (context, scale) change, ScaleAllSizes once (ThemePalette.cpp:35-73).
- Palette: kDarkPalette (Hunter/Brass) + kLightPalette (Ivory/Navy), frozen mockup-exact
  (ThemePalette.h:5-16; AGENTS.md UI section). Tests pin every value
  (test_overlay_settings_ui.cpp:236-240).

### 1.2 Studio metrics (Studio pages + ScratchpadWindow)
[VERIFIED src/ui/theme/StudioPalette.h:40-69]
- Geometry: page_padding 32, gap 24, row_height 52, hit_size 32; radii banner 16, card 12,
  row 10, input 10, button 8, chip 6, pill 999; toast_dwell 5s; activity cell 14 / gap 4 /
  corner 3; gauge_stroke 14, progress_height 28.
- Type scale (partial): banner 40, caps 12, page_title 28, helper 13. There is NO studio
  equivalent of wordmark(18), card_title(15), body(13 under that name), mini_text, or any
  24/17/10 size — this gap is the root of many literal sites (see §1.4).
- Style application: ApplyStudioStyle — WindowPadding = page_padding 32 (StudioPalette.cpp:38),
  FontScaleMain = scale, same once-per-change gate.
- Palette: kStudioPalette (light-only map: page_bg FAFAF8, teal scale, etc.), test-pinned
  including contrast policy (test_overlay_settings_ui.cpp:698-768).

### 1.3 Established solved patterns (what P-001..P-003 should reuse)
- Fonts are scoped design px: `ImGui::PushFont(nullptr, metrics::font_*_px)` + PopFont,
  DPI applied once via FontScaleMain (ThemePalette.h:99-107, 161-168). The obsolete
  SetWindowFontScale API is guarded against reappearing
  (test_overlay_settings_ui.cpp:243-291 over all src/ui files).
- Cards are children with explicit rounding + WindowPadding + Borders|AutoResizeY, then a
  trailing Dummy for the inter-card gap (MainWindow::BeginCard 574-583 + EndCard 585-588
  uses metrics::space_m; Insights::BeginCard 25-36 uses studio radius_card 12 + literal
  16px padding; Display::StatCard 78-102 uses studio radius_card + literal 16px padding;
  Transforms::DrawCard 356-398 uses studio radius_card + named kCardPadding 20).
- Rows are custom draw lists: thin 1px separators, explicit text offsets, hover fill
  teal_100 0.35, focus ring 2px (DictionaryPage.cpp:232-332; Display::RowList 25-76 twin).
- Page cadence: header → tabs → toolbar/rows → Dummy gap (DictionaryPage::Draw 115-137;
  InsightsPage::Draw 85-100). Native cadence: DrawPageChrome 566-572 (title + subtitle +
  space_s Dummy) then a stack of max-640-wide cards (MainWindow.cpp:889,1020,1141,1214,1313,1380,1447,1624).

### 1.4 Value overlaps between the two systems (no rename needed, but must not fight)
| design px | native token | studio token | verdict |
| --- | --- | --- | --- |
| 13 | font_body_px | font_helper_px | same value, two names [VERIFIED ThemePalette.h:106; StudioPalette.h:58] |
| 28 | font_page_title_px | font_page_title_px | same value (only duplicate name; namespace-qualified, unambiguous) |
| 15 | font_card_title_px | — | no studio title token → 15px literals on studio pages almost certainly mean card title |
| 16 | space_m (card pad) | literal 16 everywhere (no studio card-pad token) | consistent value, inconsistent expression |
| 32 | space_xl / control_min_height | page_padding / hit_size | same value, different roles |
| 24 | space_l | gap | same value, different roles |
| 8 | space_s | literal 8 everywhere | no studio small-gap token |
| 12 | — | font_caps_px (type only) | Dummy(…,12) has NO spacing token in either system |
| 10 | radius_card (radius only) | radius_row/radius_input | Dummy/text-offset 10s have no spacing token |
| 17 / 24 / 10 (fonts) | — | — | NO type token in either system (see §3.2) |

## 2. Per plan item (P-xxx)

### P-001 — MainWindow native pages (Home/Settings/Welcome) spacing/typography literals → metrics::*

Verdict: EXISTS (partially tokenized; densest literal cluster is rail, switches, tabs, chips).
Root cause: tokens were defined centrally but several hand-drawn controls were written
before/without routing their geometry through metrics::*
[VERIFIED src/ui/MainWindow.cpp:56,83-101,469,523,555,595-596,806-807,1563-1564,925].
Blast radius: MainWindow.cpp has 27 PushFont sites, all tokenized [VERIFIED grep] — fonts
are clean; geometry literals are not.

Every bypassing literal in MainWindow.cpp (line → value → token it should be):
- kRailWidth = 220.0f (:56) vs metrics::rail_width = 208 (:374,:437) — 12px drift, the
  only site where a token AND a literal compete for the same slot. Locked default: leave
  220 local, document dead token (00-mission.md:31). [matches mission D-rail]
- Rail item height 40 (:469), rail text x-offset +19 (:523), status dot indent Dummy
  14x0 (:555, :925), "Saved" y-offset 10 (:395), rail active fill rounding 8 (:521),
  rail bottom 8 + 32 (:531).
- Switch geometry duplicated in two places: anonymous ToggleSwitch (w 52, h 32, knob 24,
  pad 4 — :83-98) AND ToggleRow recomputing switch_w 52 / switch_h 32 (:595-596).
  widgets::ToggleSwitch (Buttons.cpp:131-149) is the studio twin with the same 52x32/20
  knob — three hand-drawn implementations of one control, two on native palette.
- Chips: chip_h 36, pad_x 14, step 18 (:806-824). Settings tabs: tab_h 36, tab_pad 16,
  focus/active rounding 8 (:1563-1593). SameLine(0, 8*scale) x2 (:862,:1568); 10 bare
  SameLine() relying on style ItemSpacing (space_s 8 via apply, ThemePalette.cpp:62).
- Fully tokenized already (do not touch): content padding space_xl (:380-381),
  BeginCard/EndCard (574-588), DrawPageChrome (566-572), full_default/min (139-149),
  content_max caps, saved_dwell (:626), all 27 font pushes.

Classification: SURFACE (spacing/typography only; no palette, no contract).
Fix options:
 A) Normalize in place (recommended for this run): replace each literal with the metrics::
    member of identical value; the two switch implementations keep their 52/32 numbers
    but source them from one named constant (or accept as bespoke — values already match
    widgets::ToggleSwitch).
 B) Extract a shared switch/geometry helper for native + studio (unify the 3
    implementations): DEEP — touches widget API surface and both palettes, out of
    surface-fix scope.
WRITE-SET: src/ui/MainWindow.cpp. CONTRACT-SET: none (no public headers).
DEPENDS-ON: none. ENV-SET: none.
Test plan: source-scan extension (P-005) must cover MainWindow.cpp; fail-before on this
file's literal count (currently ~18 literal geometry sites), pass-after zero new literals
while kRailWidth stays (exception-listed, see §5).
Acceptance: diff touches only scale-multiplied constants; screenshots at 960x640 +
760x520, DPI 1.0/1.5 identical to baseline (values unchanged).

### P-002 — Studio pages (Dictionary/Insights/Snippets/Transforms/Scratchpad) → studio_metrics::*

Verdict: EXISTS (the worst literal density of the run; fonts are the worst offender).
Root cause: studio_metrics:: has no row_text/card_title/small-gap tokens, so
implementers wrote 15.0f/13.0f/24.0f/10.0f and 8/12/16-literally at each row site
[VERIFIED DictionaryPage.cpp:242,283,290,299; SnippetsPage.cpp:32,132,150,190,233,237,
248,251,260,268; ScratchpadPage.cpp:186,214,242; InsightsPage.cpp:127,263,285,301;
Display.cpp:35,55,65,153,160,166; Buttons.cpp:123,153,165,175,195; Overlays.cpp:127,132,142,148].
Total: 37 PushFont literal sites in src/ui [RAN grep 'PushFont(nullptr, [0-9]' -> 37],
plus Dummy/SameLine literals (64 Dummy / 62 SameLine total in src/ui [RAN grep]).

Per-file literal census (typography):
- Display.cpp (RowList 15x3, ProgressPill 13x3) — shared widget, so row text on EVERY
  studio page that reuses it. 15.0f with no studio token; row text is neither title nor
  helper — needs a decision (map to metrics::font_card_title_px 15, or add
  studio_metrics::font_row_px / font_title_px = 15).
- DictionaryPage.cpp: row/empty 15x2 (:242,283), detail/actions 13x2 (:290,299).
- SnippetsPage.cpp: 10 of 11 font pushes are literals (worst file) — same 15/13 pattern,
  plus named kRowTextPadding 16 / kRowButtonGap 8 / kSheetWidth 440 (a good local pattern
  to imitate, but the names don't map to tokens).
- InsightsPage.cpp: 10.0f badge-ring glyphs (:127) + 10.0f category icons (:285) +
  24.0f Usage/Streak headings (:263,301). 24 and 10 have no token anywhere.
- ScratchpadPage.cpp: 17.0f Recents heading (:186) — third heading size (17 vs 24 vs 28),
  no token. Row title 15 (:214,242).
- TransformsPage.cpp: near-clean — 36/37 font pushes tokenized (metrics::!), 1 literal
  (KeycapRowWidth 13 at :107). The exception is buried in a measuring helper.
- Buttons.cpp widgets: IconButton glyph 24 (:123), Keycap/Chip 13x4 (:153,165,175,195).
- Overlays.cpp Toast: 13x4 (:127,132,142,148) — and Modal/Sheet WindowPadding literal 24
  x2 (:37,83).

Per-file literal census (spacing):
- Cadence 12.0f Dummy after every header/tabs/toolbar block: DictionaryPage (:147,160,199),
  SnippetsPage (:115,274), ScratchpadPage (:47), TransformsPage (:577-579,677), Insights
  divider (:43). 12 = studio font_caps_px (type!) — no 12px spacing token exists in either
  system [VERIFIED ThemePalette.h:85-89; StudioPalette.h:50-53]. A new token decision is
  required (e.g. studio_metrics::gap_s = 12, or accept 12 as the page-section gap).
- 16.0f: header SameLine offsets (Dict :143, Snippets :111) → metrics::space_m / studio
  has no 16-gap token; card WindowPadding literals 16 (Insights BeginCard :28,
  Display StatCard :84, ScratchpadPage banner text offsets :139,143,146,155,
  Overlays Toast :144,151) vs Transforms kCardPadding 20 (:37) — card padding is
  16 in 5 places, 20 in one: real density drift, not just spelling.
- 8.0f: SameLine + Dummy everywhere (Dict toolbar, Snippets sheet, Transforms cards
  :376,380,464,475,489,518,525,602,669; ScratchpadPage :211). Native space_s 8 has no
  studio counterpart → recommend studio_metrics::gap_s = 8 (new token) OR cross-reference
  metrics::space_s from studio pages (acceptable numerically; breaks palette purity).
- Card/row geometry already tokenized (do not touch): radius_card/row (12/10),
  row_height 52, hit_size 32, activity_*, gauge_stroke, progress_height, toast_dwell,
  kStudioPalette colors, font_page_title (studio) / font_caps / font_helper / banner.

Dual-palette split — PROOF that this is structural, not a style nit:
1. Two style applicators, two OS windows, never mixed in one context:
   - ApplyImGuiStyle(kDarkPalette) — MainWindow.cpp:180,336; Overlay.cpp:125,339.
   - ApplyStudioStyle(kStudioPalette) — ONLY ScratchpadWindow.cpp:91,591. [RAN grep ->
     2 studio call sites, both in ScratchpadWindow.cpp; 01-work-ledger.md:35 corroborated]
   - MainWindow NEVER calls ApplyStudioStyle [VERIFIED grep: 0 hits in MainWindow.cpp].
2. All five studio pages render INSIDE the MainWindow ImGui context
   (RenderPages :407-434 → Draw*Page :1399-1442 → page_->Draw(scale)), so the live
   ImGuiStyle (rounding, paddings, item spacing, colors) is the NATIVE dark one
   (ThemePalette.cpp:49-65) while every studio pixel is hand-drawn with
   kStudioPalette colors + studio_metrics radii [VERIFIED e.g. DictionaryPage.cpp:234-275;
   InsightsPage.cpp:25-32; Display.cpp:27-31]. Style-driven studio controls (plain
   ImGui::Button/InputText) therefore inherit dark geometry while custom widgets use
   studio values — P-002 doc-only normalization cannot fix this split (see option B).
3. Mixed usage inside single files proves the split is ungoverned:
   - DictionaryPage.cpp:363 error text uses kDarkPalette.err inside a studio page.
   - TransformsPage.cpp: 36 metrics:: font tokens + kStudioPalette colors (a studio page
     speaking the native font vocabulary; values coincide: 28==28, 13==13, only
     font_card_title_px 15 has no studio twin).
   - InsightsPage share dialog mixes metrics::font_card_title_px (:350,362) with
     studio_metrics fonts (:355,365,376).
   - Buttons.cpp mixes studio radii with one metrics::font_card_title_px (TabRow :226)
     and zero native colors; Display.cpp mixes metrics::font_page_title_px (:93,126)
     into studio widgets; Overlays.cpp uses NO metrics:: at all.
4. Radius split: native radius_card 10 vs studio radius_card 12; native radius_control 6
   vs studio radius_button 8 / radius_chip 6 / radius_row 10 / radius_input 10 —
   identical names, different numbers (ThemePalette.h:91-93 vs StudioPalette.h:42-48).
   radius_pill 999 matches both. Any shared widget must declare which radius world it
   lives in; today it does so only implicitly per file.

Classification: SURFACE for literal→token spelling where a same-value token exists
(direction: studio page → studio token; the 15px sites need one new-token decision —
see below, classified DEEP as a separable task, not a blocker for the rest).
Fix options:
 A) Normalize both vocabularies, keep the two palettes (RECOMMENDED for this run):
    route each literal to the same-value token in its own palette's namespace
    (studio pages → studio_metrics::*, MainWindow/widgets-native → metrics::*);
    where no token exists (15px row text, 12px gap, 8px gap on studio side),
    first add ONE additive token set to StudioPalette.h (e.g. font_row_px 15,
    gap_s 8, gap_m 16) with new CHECKs in test_overlay_settings_ui.cpp.
    TransformsPage metrics:: fonts: leave OR alias (values identical for 28 and 13;
    only 15 differs — no pixel change either way). No palette rebrand.
 B) Unify to one palette sooner: make ApplyStudioStyle the Full-window style, or port
    studio pages to native tokens. REJECT for this run: mission D3 (keep tokens),
    D4 (src/ui/ only), out-of-scope "palette unification" (00-mission.md:9-13); it
    changes every color and needs mockup regen + contrast re-table.
WRITE-SET: src/ui/pages/*.cpp (5 files), src/ui/widgets/Buttons.cpp, Display.cpp,
  Overlays.cpp, src/ui/theme/StudioPalette.h (additive tokens only).
CONTRACT-SET: src/ui/theme/StudioPalette.h (test-pinned header — additive constants only,
  no value changes; existing pins at test_overlay_settings_ui.cpp:698-738 must stay green).
DEPENDS-ON: T-004 token decision (which new studio tokens exist) for the 15/12/8 gaps;
  no dependency for sites with existing tokens.
Test plan: source-scan (P-005) fail-before on current 37 literal PushFonts + per-file
  Dummy/SameLine literal counts; pass-after zero. Screenshot gate mandatory (dark
  context + studio colors interact).
Acceptance: zero PushFont/Dummy/SameLine literals in the five pages + three widget files
  (except a small allowlist for inherently computed sizes like text_size-derived
  InvisibleButton extents and the kRailWidth-class named constants).

### P-003 — Widgets density consistency (cards/rows/chips/toggles)

Verdict: EXISTS (values are close but not governed by one density scale).
Root cause: widget geometry was hand-tuned per widget; only radii/hit_size/row_height/
  progress_height/activity_* reached tokens [VERIFIED Widgets.h:14-31 declares no geometry
  contract by design; sizes live inside Buttons.cpp/Display.cpp/Overlays.cpp].

Density census (design px, all x scale):
- Heights: Primary/LightButton 40 [Buttons.cpp:58,68] vs OutlinePill 32 (:90,93) vs
  IconButton/Keycap/Chip hit 32 (:110,157,179) vs BadgeBeta 24 (:209) vs TabRow
  max(32, line+12) (:228) vs Toast 32 [Overlays.cpp:126] vs Modal/Sheet pad 24
  (:37,83) vs native rail item 40 [MainWindow.cpp:469], settings tab 36 (:1563),
  hotkey chip 36 (:806), switch 32 (:84,595).
- Widths/padding: button FramePadding (16,8) [Buttons.cpp:41,81] — no token on either
  side; Keycap pad_x 12 (:156); Chip pad_x 14 (:178); BadgeBeta pad_x 10 (:208);
  toggle 52x32 knob 20 [Buttons.cpp:133-143] vs native switch 52x32 knob 24
  [MainWindow.cpp:83-98] — same track, different knob (20 vs 24) and pad (4 vs 4).
- Cards: padding 16 (Insights/Display) vs 20 (Transforms kCardPadding) vs native
  space_m 16 [MainWindow.cpp:577-578]; radius 12 studio vs 10 native
  [StudioPalette.h:43; ThemePalette.h:92]; card gaps: space_m 16 native (EndCard)
  vs Insights manual 16 (:168,247) vs Transforms kCardGap 16 (:38) vs studio gap 24
  (used only for banner-chip cadence in Dict/Snippets).
- Rows: RowList row_height 52 tokenized [Display.cpp:53]; Dictionary/Snippets rows
  52 [Dict :262; Snippets :169]; ScratchpadPage note rows 68 literal (:225) — a
  different row density on the same Full window with no token; pin-hit 28 (:258)
  vs hit_size 32.
- Visual extremes to preserve as intended hierarchy (INFERENCE from consistent use, not
  flag as defects): banner font 40 (heaviest), page_title 28, card_title 15,
  helper/body 13, caps 12 — strictly descending emphasis already; the density problem
  is spacing, not type order. DictionaryPage rows put the 52px rows back-to-back
  inside one child (no per-row Dummy) while every page adds Dummy cadence BETWEEN
  sections — that pattern is the house style and needs no change, only 12→token.

Classification: SURFACE (tokenize literals, preserve all numeric values); DEEP only if
  unifying heights (40 vs 32 vs 36) or card padding (16 vs 20) — that is a visual
  design change requiring mockup sign-off, explicitly out of palette/token scope.
Fix options:
 A) Govern-by-tokens, values frozen (RECOMMENDED): add only spelling tokens
    (studio_metrics small gaps; widget-local named constants where two widgets share a
    value, e.g. switch track); do NOT equalize 40/32/36 or 16/20.
    16-vs-20 card padding becomes a documented known-drift (like P-004 rail) with a
    screenshot to decide later.
 B) Unify density (one height ladder, one card pad): DEEP design change, needs
    mockup + screenshots + W0-INV-04 input; do not fold into P-003 surface task.
WRITE-SET: src/ui/widgets/Buttons.cpp, Display.cpp, Overlays.cpp; same contract-set
  as P-002 (StudioPalette.h additive; no value changes).
DEPENDS-ON: T-004 token decision (shared with P-002). CONFLICTS: none if values frozen;
  conflicts with any future palette-unification task (separate plan per 01-work-ledger.md:32).
Test plan: source-scan counts per widget file + screenshot proof that every widget
  renders unchanged at both window sizes / DPI 1.0 + 1.5.
Acceptance: every widget size/padding in the three widget files is written as either
  an existing token, an approved new token, or a named file-local constant with a
  comment citing the mockup section (mockup values are never invented — AGENTS.md rule).

## 3. Duplicates / shared root causes

1. Four private BeginCard implementations with the same shape (push ChildBg + rounding +
   padding, BeginChild Borders|AutoResizeY, pop): MainWindow::BeginCard (574-583),
   Insights::BeginCard (25-36), Display::StatCard inline (78-102), Transforms::DrawCard
   inline (356-398), plus RowList/Dictionary/Snippets row-children with WindowPadding 0.
   They agree on outline semantics but disagree on radius world (10 native / 12 studio)
   and padding (16 / 16 / 20). Shared root cause of P-002+P-003 card drift. INFERENCE:
   a shared helper can't span the radius worlds until P-004-class decisions are taken;
   recommend documenting the four sites, normalizing spellings only (option A in P-002).
2. Row rendering duplicated 3x with same layout math: DictionaryPage::DrawRows (232-332),
   SnippetsPage::DrawRows (140-221), Display::RowList (25-76). Same 52px height, 16px
   text inset, 8px/30px title/detail offsets — that 8/30 pair is why "12" never got a
   token (offsets are absolute inside a row, not gaps). Tokenize as named row offsets.
3. InvisibleButton-as-row pattern everywhere (Dict/Snippets/Transforms/Scratchpad note
   rows, TabRow, ToggleSwitch x2, Keycap/Chip) — focus ring 2px + radius choice is the
   repeated density decision; no behavioral bug, but every literal is a future drift site.
4. ToggleSwitch 3x (MainWindow anonymous, MainWindow::ToggleRow resizes, widgets).
   Same 52x32; knobs 24 (native) vs 20 (studio). Only true shared-control duplication.

## 4. Test landscape and long-run test (required by standing orders §8)

What exists today [VERIFIED tests/test_overlay_settings_ui.cpp]:
- Token VALUES pinned by CHECK (fonts :236-240; studio palette + all studio_metrics
  :698-738; contrast :743-768; heat :775-805).
- Source-scan guards only: SetWindowFontScale absence over 27 src/ui files (:256-291),
  one positive marker MainWindow contains `PushFont(nullptr, metrics::font_` (:298),
  ApplyImGuiStyle gate markers (:300-322), header text marker (:329), Overlay-specific
  literal-class guards (:371-408). There is NO literal-bypass scan for PushFont/Dummy/
  SameLine/raw float sizes, no per-file wrap of the font mechanism (the check asserts
  presence in MainWindow only, and only for the happy-path marker).
- No pixel/render tests possible (lwf_tests links no ImGui; GUI proof is manual
  screenshots — 00-mission.md:52-58, A-003).

Long-run test (to be implemented as P-005/T-003 by the test owner; design here):
- Source / deterministic + fast: extend the existing font_sources loop (same file-list
  idiom, same ifstream style) with one TEST_CASE asserting ZERO matches in each listed
  .cpp for `PushFont(nullptr, [0-9]` (the exact grep class used in this report), plus
  per-file counts for `SameLine(0.0f, [0-9]` and `Dummy(ImVec2(0.0f, [0-9]` above an
  allowlist of named/structural sizes. Fails on the current tree (37 literal PushFonts
  alone), passes after P-001..P-003 spelling normalization. Deterministic, no ImGui
  link, mirrors the badge-alpha "forbid the class" precedent (:381-386).
- Load (100x / concurrency / partial failure): N/A beyond what exists — this work is
  pure compile-time spelling; no runtime objects. State explicitly in the task so
  reviewers don't mistake it for covered.
- Hostile input: N/A (no user input surface). The closest hostile case is a future
  literal smuggled into a one-off file not on the scan list — mitigate by globbing
  src/ui/**/*.cpp instead of the hardcoded list (test file today hardcodes 27 paths;
  a new page file added later would silently escape — W0-INV-03 should confirm the
  glob approach against the build).
- Operability: if a future change intentionally introduces a new size, the test failure
  text must name the new token decision (standing-order style: fail message points at
  StudioPalette.h/ThemePalette.h, not a bare count).
- Reversal: revert any P-001..P-003 literal→token edit → its file's literal count rises
  → the new TEST_CASE fails. That is the fail-before/pass-after proof; no other test in
  the suite would catch it today.
- Screenshot proof (the real acceptance for this run, per mission D5): before/after at
  960x640 default and 760x520 min, DPI 1.0 + 1.5, Home/Settings/Welcome + all five
  studio pages + widgets in place; difference must be nil for option A (values frozen).
  Recorded as manual checklist (AGENTS.md honest-limits rule).

## 5. Recommended task breakdown and ordering

| Task | Scope | Class | Write-set | Contract-set | Depends |
| --- | --- | --- | --- | --- | --- |
| T-001 | P-001 MainWindow literals → metrics::* | SURFACE | src/ui/MainWindow.cpp | none | — |
| T-002 | P-002+P-003 studio pages + widgets (existing-token sites only) | SURFACE | src/ui/pages/*.cpp, src/ui/widgets/{Buttons,Display,Overlays}.cpp | none for this tranche | — |
| T-003 | Token gaps: 15px row text, 8/12/16 studio gaps, 24/17/10 font headings; Transforms font-namespace; kRailWidth doc | DEEP (decisions + additive header, values only — no visual change) | src/ui/theme/StudioPalette.h (+ doc), then T-002 tranche 2 | StudioPalette.h additive; new CHECKs in test_overlay_settings_ui.cpp:698-738 region | T-002 (spelling first) |
| T-004 | P-005 source-scan test | SURFACE | tests/test_overlay_settings_ui.cpp | none | T-001..T-003 (write after, so it passes) |

Ordering rationale: token decisions (T-003) gate only the gap sites, not the ~90%
  same-value sites in T-001/T-002 — run T-001 and T-002 in parallel (disjoint files),
  then T-003, then T-004 LAST so the suite is green at each gate (work-ledger wave-1
  pre-mortem already warns that C5/test task can collide with T-001/T-002 edits — file
  ownership keeps them disjoint).

## 6. Interfaces, invariants, and contracts that must be preserved

- ThemePalette.h / StudioPalette.h token VALUES are frozen (mockup-exact; AGENTS.md UI
  section; test-pinned). No numeric change without mockup regen.
- Palette split itself (dark native / light studio) is out of scope
  (00-mission.md:9-13); P-001..P-003 may normalize spelling only.
- The once-per-(context,scale) apply gate (g_last_applied_context/scale in both
  *Palette.cpp) and the "derive, never lighten" contrast rule (ThemePalette.h:10-16).
- ImGui context discipline: two windows, two contexts, CreateContext return value kept
  (AGENTS.md UI section) — no layout fix may touch context handling.
- widgets/Widgets.h function signatures (PrimaryButton/LightButton/OutlinePill/
  IconButton/ToggleSwitch/Keycap/Chip/BadgeBeta/TabRow/RowList/StatCard/RadialGauge/
  ProgressPill/ActivityGrid/Banner) — all scale-parameterized; naming only.
- Row/card child semantics: Borders|AutoResizeY + trailing Dummy gap + 1px separators.

## 7. Hidden coupling and downstream effects

- One MainWindow ImGui context styles ALL pages; plain-ImGui controls on studio pages
  (InputText, buttons, modals, sliders) inherit native geometry (WindowPadding 16,
  ItemSpacing 8) while custom widgets use studio radii 12/10 — a P-002-only fix still
  leaves two densities on one screen. This is the correct motivation for screenshots.
- Both *Palette.cpp applicators mutate shared ImGuiStyle state and keep per-context
  statics (g_last_applied_context, g_studio_last_context) — any future palette
  unification must solve the 2-window/2-context split first (AGENTS.md UI section).
- Tests that pin tokens (test_overlay_settings_ui.cpp) read files by path; renaming
  tokens or files breaks CI (00-mission.md:43-45 hotspot note matches the file's own
  comments).
- history.jsonl/logs/models/ live under %APPDATA%/LocalWisperFlow; unrelated to UI
  tokens — no coupling found.
- ScratchpadWindow (editor window) shares RowList/Toast/Banner widgets and studio
  palette with ScratchpadPage — widget changes in T-002 alter the editor window too
  (same values, re-spelled; still needs a look in screenshots).

## 8. Baseline results

No commands run (brief §7: grep/read only, no builds). Pre-existing baseline from
00-mission.md:52-58 (HEAD 8a6ea22, Phase 0): lwf_tests build OK; full build timed out
at 120s (slow link, not failure); ctest 1/1 passed (23.98s); hygiene OK; no-network OK;
G3 zig pending (no zig on dev box). I ran none of these; they are cited, not re-verified.

## 9. Risks (ranked, with evidence)

1. Visual regression without visual proof — highest risk. Source-scan tests cannot see
   pixels, and option-A changes are value-preserving ONLY if every mapping is same-value;
   a single wrong mapping (e.g. 15px → helper 13, or 20 → 16) silently changes density
   and no test fails. Evidence: 13px/15px literal ambiguity (§1.4); 16-vs-20 card pad
   (§P-003). Mitigation: screenshots gate (P-006) before merge; fail-before counts.
2. Token-gap decisions creep into value changes. 24/17/10 headings and 12px gap have no
   token; a sloppy "normalize" maps them to 28/13/13. Evidence: InsightsPage 24s,
   ScratchpadPage 17, Dividers 12 (§P-002). Mitigation: keep those sites allowlisted
   until T-003 decides explicit new tokens mirroring the mockup.
3. Transforming the palette split under the hood. Touching ApplyStudioStyle call sites
   (ScratchpadWindow.cpp:91,591) or "fixing" dark-context studio pages would recolor
   the Full window and violate "keep tokens" (00-mission.md:9). Evidence: single-context
   proof (§P-002 point 2). Mitigation: forbid edits to *Palette.cpp / Apply* call
   sites in the Wave-1 task briefs.
4. Test-name churn breaks CI (test_overlay_settings_ui.cpp pins strings).
   Evidence: existing marker-based checks (:298-329). Mitigation: new test follows the
   same idiom; no renaming.

## 10. Assumptions and unknowns

- UNKNOWN: whether 24.0f headings (Insights Usage/Streak) and 17.0f (Recents) are
  intentional sub-title sizes from the mockup or drift (mockup section 5 authoritative —
  for W0-INV-04 to confirm). UNTIL THEN: do not normalize those values.
- UNKNOWN: whether 12px section gap and 8px gaps are deliberate (12 appears as the
  universal section cadence in 4 files — INFERENCE: deliberate, needs a token
  gap_s = 12 / gap_sm = 8 in studio_metrics or a named constant).
- UNKNOWN: TransformsPage 172px card height / 440px sheet width / 220px share-cards —
  design constants or placeholders? Out of P-001..P-003 normalization unless W0-INV-04
  ties them to the mockup.
- ASSUMED: studio pages' metrics:: font use (Transforms, Insights dialog) is
  intentional reuse (values coincide) rather than a bug — normalizing vocabulary
  first is safe only under value-identity, which holds for 28 and 13.
- UNKNOWN: kRailWidth 220 vs rail_width 208 intent (brief/mission locked default:
  leave + document — adopted as-is, not re-investigated).

## 11. Out-of-scope observations

1. ScratchpadWindow.cpp:40,68,126 etc. literal-heavy (tabs 32, toolbar SameLine 4/6,
   undo toast 1.5s hard-coded at :436 vs metrics::saved_dwell 1.5) — second OS window,
   outside Full-window scope; left untouched. [VERIFIED read 420-539]
2. ThemePalette.cpp:55,61,63,65 has literals (ScrollbarSize 10, FramePadding y 6,
   ItemInnerSpacing y 4, GrabMinSize 14) — token-system internals, not spacing bypasses.
3. Overlay (Mini) surface is out of scope (00-mission.md:13) — its Dummy/PushFont are
   tokenized already (Overlay.cpp:661-671); one comment reference at :104 is prose.
4. Brief mentions "40.0f" as the cue literal — in current code 40.0f encodes rail item
   height (MainWindow:469) and Primary/Light button heights (Buttons:58,68); those are
   hierarchy (actions vs pills) not a missing button-height token (control_min_height
   32 is a floor, not a button size). Unifying them is a DEEP design decision, not a
   normalization.
5. The mission's P-004 (rail) and W0-INV-01's MainWindow architecture findings were
   not re-investigated here; the rail drift is cited only as a boundary fact.

## 12. Scratch files created

none (read-only investigation; no /tmp scripts were needed — grep/read suffice).

--- Re-checks of the three most important claims (method step 11, done before writing):
1. "ApplyStudioStyle fires only at ScratchpadWindow.cpp:91,591; MainWindow never calls it"
   — re-verified by grep 'ApplyStudioStyle|ApplyImGuiStyle' over src/: exactly those
   2 studio sites + 4 native sites (MainWindow:180,336; Overlay:125,339). HOLDS.
2. "37 literal PushFont sites" — recount: Buttons 5 (123,153,165,175,195) + Dictionary 4
   + Overlays 4 + Snippets 10 + Insights 4 + Transforms 1 + ScratchpadPage 3 + Display 6
   = 37. HOLDS (TransformsPage is the control case: 35/36 tokenized).
3. "kRailWidth 220 (MainWindow.cpp:56) vs metrics::rail_width 208 (ThemePalette.h:95)"
   — re-verified by grep + both reads. HOLDS; leave+document per mission.
