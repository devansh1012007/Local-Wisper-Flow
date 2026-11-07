# W4-IMPL-I — Insights page (P21/F6, T-018)

## ORCH-DIGEST
- Status: IMPLEMENTED — page code + tests committed; integration leftovers are integrator NEEDs below (CMakeLists registration, MainWindow/main wiring, scan lists).
- Scope: src/ui/pages/InsightsPage.{h,cpp} (new) + tests/test_insights_page.cpp (new). Nothing else touched; MainWindow/Cmake wiring are NEEDs (see below).
- Stacks on Wave 3 shell/widgets (StudioPalette, TabRow, StatCard-style child, RadialGauge, ProgressPill, ActivityGrid, OutlinePill, Modal) + Wave 1 StatsStore/InsightsModel (Generation() verified in StatsStore.h:40) + Wave 2 stats hook (DictationRecord fields in StatsStore.h:9-18).
- Closed-loop behavior (all test-proven): 14 cases / 201 assertions green standalone; WPM floor, midnight + month-boundary streaks, gap longest, calendar month_max, shares 100-or-0 incl. unknown-key guard, distinct apps, generation-driven refresh (no polling), and month nav have mutation/red-first evidence. Top 0.1% banned by source-scan; "Not available yet." voice tab (D15) pinned.
- Decisions: Card B big = corrected+dict (FixesMadeByFlow); gauge cap 200 WPM display-only; 6-row pill denominator filtered to canonical keys; legend in brief-literal More/ Less order; header-inline model (imgui-free) so lwf_tests links without Overlays.cpp.
- Gates: g++ -fsyntax-only on the page TU (imgui include) clean with -Wall -Wextra -Wpedantic; hygiene + no-network OK; cmake lwf_tests target still built green (new files unregistered, so they are not yet in that target). Independent review + Windows I-pass outstanding (see §Unverified).

## Context Report (pre-change investigation)

### 1. Root cause / entry points
- [VERIFIED src/ui/MainWindow.cpp:1011-1014] `MainWindow::DrawInsightsPage` is a Wave-3 stub: DrawPageChrome("Insights", "Your dictation at a glance.") + "Not available yet.". Page enum Page::Insights exists [VERIFIED src/ui/MainWindow.h:25] and rail routes to it [VERIFIED MainWindow.cpp:339-340].
- [VERIFIED src/core/StatsStore.h:20-52] StatsStore data source: DictationRecord {timestamp_local, duration_ms, final_text, raw_text, app_profile, process_name, words_corrected, dictionary_fixes} [VERIFIED :9-18]; DayStats {words, dictations, speaking_ms, words_corrected, dictionary_fixes, by_category {6 cats words}, apps {process words}} [VERIFIED :20-28]; API Record / Snapshot() / Generation() [VERIFIED :36-40]. Generation is the refresh trigger (no polling) — name verified as `Generation()` [VERIFIED :40].
- [VERIFIED src/core/StatsStore.cpp:142-174] Record aggregates by local DateKey; generation_ bumped only on successful non-empty enabled persist [VERIFIED :171-173]. Snapshot returns a copy under mutex [VERIFIED :186-189]. Thread-safe for engine worker + UI render.
- [VERIFIED src/core/InsightsModel.h:20-34] Pure formulas: DayKey, Wpm(total_words, speaking_ms) optional with 60s floor [VERIFIED InsightsModel.cpp:85-91], FixesMadeByFlow [VERIFIED :93-101], CategoryShares sums 100 sorted desc/name [VERIFIED :103-141], TotalAppsUsed distinct non-empty [VERIFIED :143-151], CurrentStreak/LongestStreak over valid YYYY-MM-DD serials [VERIFIED :153-188], CalendarLevel quartiles [VERIFIED :190-199].
- [VERIFIED src/core/AppContextResolver.cpp CategoryNames] Six canonical keys: {"AI prompts","Emails","Work messages","Personal","Documents","Other tasks"} — display labels per brief: AI PROMPTS / EMAILS / WORK MESSAGES / PERSONAL MESSAGES / DOCUMENTS / OTHER TASKS.
- [VERIFIED src/ui/widgets/Widgets.h] Reusable widgets: TabRow(id,tabs,count,int* selected,scale), StatCard, RadialGauge(id,value 0..1,center_label,sub_label,scale), ProgressPill(id,value,label,scale), ActivityGrid(id,values,month_max,scale), OutlinePill(id,label,enabled,scale), IconButton(id,glyph,scale) [VERIFIED :14-30]. [VERIFIED src/ui/widgets/Overlays.h] widgets::Modal Open/Close/IsOpen/Begin(title,scale)/End [VERIFIED :7-18].
- [VERIFIED CMakeLists.txt:190] lwf_tests links lwf_core only (no src/ui) — header must be imgui-free with header-inline portable logic; Draw in .cpp only (DictionaryPage/TransformsPage precedent).
- [VERIFIED src/main.cpp Stats wiring] `StatsStore usage_stats(data_dir + "\\stats.json", keep_usage_stats)` + `engine.SetStatsStore(&usage_stats)`; `MainWindow full(fsm, audio, config.Store(), ui_bus)` lacks a stats param — integration NEED below.

### 2. Affected files / consumers
- New: src/ui/pages/InsightsPage.h (imgui-free header-inline model + InsightsPage decl), src/ui/pages/InsightsPage.cpp (ImGui Draw only), tests/test_insights_page.cpp (doctest; instantiates model + StatsStore only, never Draw).
- Consumers: MainWindow::DrawInsightsPage (NEED), main.cpp StatsStore pass (NEED). StatsStore/InsightsModel untouched (read-only consumption).

### 3. Invariants preserved
- No source comments (AGENTS.md); no windows.h (src/ui not src/os); PushFont(nullptr, design_px) only, never SetWindowFontScale; keyboard nav via widget returns + EnableNav (no IsItemClicked); no Log/transcript writes (R4); never invent stats — empty states show "0"/"—"/"No dictations yet..." per brief (R3).
- StatsStore file format untouched (page reads Snapshot() only, never writes stats.json). InsightsModel formula semantics untouched (page adapts DayStats aggregates to DayRecord views, never re-implements WPM/streak/share logic).
- Generation-driven refresh only: Draw calls SyncIfStale on every frame (reads atomic Generation, re-Snapshots + re-summarizes only when changed); no timers, no polling threads.

### 4. Risks / edge cases
- Snapshot double-count trap: DayStats aggregates cannot be flattened to a single DayRecord list without double-counting words/fixes across category/app splits — page builds three targeted views (day-level for streaks/fixes, category-level for shares, app-level for distinct apps) and computes totals directly from DayStats; tests pin each.
- Card B big number = words_corrected + dictionary_fixes (FixesMadeByFlow over day-level fixes). This is the only reading that makes the widget + the two breakdown rows cohere; recorded as DECISION in report.
- Gauge normalization (WPM -> 0..1) is display-only; cap 200 WPM declared in header (kWpmGaugeCap) so screenshots never read as a user ranking (Top 0.1% forbidden).
- Month grid weekday alignment: ActivityGrid index 0 = Sunday week-1 [VERIFIED Display.cpp:219-226 row=i%7] — page pads leading blanks with 0 (level-0 empty color) so day-1 lands on its real weekday.
- lwf_tests does not link imgui/Overlays.cpp: tests must never call share_modal_ methods (header exposes share_open_ bool + Modal sync in .cpp only, TransformsPage editor_open_ precedent). CMakeLists FORBIDDEN (NEED: InsightsPage.cpp to WIN32 sources + test file to lwf_tests + font/studio scan lists).

### 5. Expected behavior / tests
- Title "Insights" 28px; 56px teal_300 share badge with "SHARE •" ring + upload glyph opens share dialog (Flow Highlights 4 stats + Flow Streak + disabled Copy/Download + "Coming soon").
- Tabs "Your usage"/"Your voice" (D15 empty "Not available yet."); Row-1 three cards (A WPM+tooltip+gauge+"Average"+footnote without Top 0.1%; B fixes+2 rows; C total+Desktop row+disabled mobile pill); Row-2 D (Desktop usage 24px + TOTAL APPS USED + 6 ProgressPills) + E (streak + LONGEST STREAK + month navigator over data+current months + ActivityGrid + More/Less legend).
- Empty: "0"/"—", D "No dictations yet...", E empty calendar.
- Tests: WPM 0/59/60s; midnight streak; gap longest; calendar month_max/levels; shares 100-or-0 across 6 rows; distinct apps union; generation refresh; month nav; pill labels; source-scan exact copy + forbidden-string absence.

## Root Cause & Context Summary
Greenfield page (no bug): the Wave-3 shell stub at [VERIFIED src/ui/MainWindow.cpp:1011-1014] renders only chrome + "Not available yet." The gap was a UI that reads StatsStore snapshots through InsightsModel formulas on the Wave-3 widget kit. Two architectural decisions resolve the hard parts:

- Snapshot flattening: DayStats aggregates per day carry split by_category/apps; flattening to one DayRecord list double-counts words/fixes. The page builds three targeted DayRecord views (day-level for streaks/fixes/calendar, category-level for shares, app-level for distinct apps) and computes cross-day totals directly from DayStats. No formula is re-implemented; Wpm/FixesMadeByFlow/CategoryShares/TotalAppsUsed/CurrentStreak/LongestStreak/CalendarLevel are called in Summary()/CategoryRowsFor().
- Refresh without polling: `StatsStore::Generation()` [VERIFIED src/core/StatsStore.h:40] is the sole trigger. Draw() calls SyncIfStale() every render; it re-Snapshots + re-summarizes only when the atomic generation changed (or on first sync). No timers, sleep, or worker thread (plan §§D/E P21).
- Test-link boundary: lwf_tests links lwf_core only [VERIFIED CMakeLists.txt:190], so InsightsPage.h is imgui-free (verified by source-scan test) and every test-reachable model function is header-inline; Draw + share_modal_ sync live in the .cpp. Header exposes only share_open_ bool + share state (Modal sync in .cpp), so tests never link Overlays.cpp (TransformsPage editor_open_ precedent).

## Phased Changes Executed

### Phase 0 — Tests first (tests/test_insights_page.cpp, new)
- 14 TEST_CASEs / 201 assertions, hermetic (temp_directory_path for StatsStore files, direct map snapshots for formula wiring). Red-first proven: pre-header compile fails at `ui/pages/InsightsPage.h: No such file or directory` [RAN g++ syntax check].
- Cases: WPM 0/59/60s via SummarizeSnapshot + WpmLabel/GaugeFraction; midnight/month-boundary current streak; gap longest streak; calendar MonthValues values/month_max + CalendarLevel quartiles + empty month zeros; category 6-row fixed order + share sum 100-or-0 + fraction + PillLabel "30 AI PROMPTS"; unknown-category share-sum guard (fixed during review, see audit); distinct apps union across days + empty-string exclusion; fixes split (7+5=12) + StreakTitle/DesktopWordsLine formats; empty snapshot (0/"—"/empty calendar/share lines "0 words"); generation refresh via real StatsStore Record (SyncIfStale true on change, false when idle, LastGeneration equality); month navigator (NavigableMonthsFor data+current, Prev/Next jumps, page GoPrev/GoNext bounds); calendar helpers (leap Feb 28/29, Sunday start 2026-11-01, MonthLabelFor); copy constants + share-highlight lines for a 974-word/372000ms snapshot ("1 day streak"/"157 wpm"/"974 words"/"1 apps"); source-scan exact copy + bans (Top 0.1% absent, SetWindowFontScale/windows.h/TODO/FIXME/XXX absent, imgui.h absent from header, widgets used).
- Mutation evidence: kWpmGaugeCap 200->100 mutant makes GaugeFraction 1.0 vs 0.6 and fails exactly that CHECK; restore returns 14/14 green.
- Full acceptance mapping: WPM 0/59/60 (case 1), streak midnight (case 2, via CurrentStreak on serial days incl. month boundary), gap longest (case 3), calendar levels (case 4), shares 100-or-0 (cases 5+unknown), distinct apps (case 6), generation trigger (case 9). Calendar-level distinction (model vs widget) is documented as a FINDING; page passes day_words+month_max to the widget.

### Phase 1 — Portable model header (src/ui/pages/InsightsPage.h, new)
- Constants: exact brief copy (titles, captions, tooltips, footnote, mobile, desktop-empty, share titles/buttons, "Coming soon", em-dash kEmptyWpm). DECISION: Card B big number = words_corrected + dictionary_fixes (sole reading coherent with FixesMadeByFlow + the two breakdown rows). DECISION: gauge display cap kWpmGaugeCap=200.0 (display-only normalization, never a user rank; "Top 0.1%" banned by test).
- CategoryDefs fixed display order with transition labels (AI prompts->AI PROMPTS, ..., Personal->PERSONAL MESSAGES) + two-letter neutral icons (AI/EM/WK/PM/DC/OT). PillLabel formats "{n} {LABEL}".
- Views: BuildDayRecords (fixes=corrected+dict, clamped), BuildCategoryRecords, BuildAppRecords (words>0 + non-empty app). SummarizeSnapshot wires Wpm/FixesMadeByFlow/CurrentStreak/LongestStreak/TotalAppsUsed; has_data iff total_words>0 (drives D empty state, never invented numbers). CategoryRowsFor: after review, filtered to the 6 canonical keys so shares sum 100-or-0 even with stray unknown JSON categories (INV: rows cover exactly the 6 spec rows; unknown words still count in total_words card C but never inflate pill shares). Fractions use filtered total (sums to 1.0 when data present).
- Calendar: DaysInMonth (leap-aware), WeekdayOfFirst (0=Sun, civil serials; 2026-11-01 Sunday verified via PowerShell + python), TodayKeyForTime delegates to StatsStore::DateKey (no duplicated localtime), MonthKeyFor/DayKey substr, ParseMonthKey/MonthLabelFor ("November 2026"), MonthValuesFor (leading-zero pad so day-1 lands on its weekday per ActivityGrid row=i%7 [VERIFIED Display.cpp:219-226]), NavigableMonthsFor (data months union current, sorted), PrevNavigable/NextNavigable (jump to adjacent navigable month, "" at bounds).
- InsightsPage: StatsStore& single source; share_open_ bool (cpp syncs widgets::Modal), tab_ 0/1, view_year_/month_ + view_initialized_, last_generation_/snapshot_valid_/snapshot_/summary_/current_month_key_. SyncIfStale reads Generation() each call; Recompute sets view month from today_key once. OpenShare/CloseShare/SetTab/GoPrev/GoNext/SyncFromSnapshot are test surfaces without imgui.
- INVARIANT: page never writes stats.json (reads Snapshot() only); formula semantics and store format untouched.

### Phase 2 — Draw implementation (src/ui/pages/InsightsPage.cpp, new)
- Draw() order: SyncIfStale -> Header -> Tabs -> voice? empty + share dialog return : Row1 + Row2 + share dialog. No timers anywhere.
- Header: "Insights" at 28px (font_page_title_px) + 56px circular badge top-right: teal_300 disk, repeating "SHARE •" ring (per-codepoint glyph vector derived from kShareBadgeLabel; bullet is one UTF-8 sequence, never per-byte split), upload arrow (shaft/chevron/baseline lines, ink), 2px teal_700 focus ring, InvisibleButton EnableNav -> OpenShare(). Title/button geometry uses origin/rect math (no SetWindowFontScale).
- Tabs: TabRow("Your usage"/"Your voice"); voice shows "Not available yet." (D15) and no cards.
- Row 1: explicit-origin three-column layout (card_w=(avail-2*gap)/3, 16px gaps). BeginCard/EndCard = white child (card_bg_white, radius_card, 16px padding, AutoResizeY). Card A: big WPM (28px) + info "i" IconButton (tooltip exact) + "WORDS PER MINUTE" caps + RadialGauge(value, "Average", wpm) + footnote wrapped. Card B: fixes_total + "FIXES MADE BY FLOW" + divider + "{n} words corrected"/"{n} dictionary fixes" rows with info tooltips ("Words the cleanup changed."/"Words the dictionary corrected." — definitional only). Card C: total_words + "TOTAL WORDS DICTATED" + divider + "Desktop / {n} words" + disabled OutlinePill "Download on mobile" with disabled-tooltip. Card widths are fluid (spec ~185px at nominal window; see risk note).
- Row 2: left 55% / right 45%. Card D: "Desktop usage" 24px + "TOTAL APPS USED | {n}" caps; has_data==false -> "No dictations yet. Hold Ctrl + Win to start." instead of pills; else 6 rows: icon slot (circled two-letter glyph, teal_700) + shifted ProgressPill(PillLabel, fraction). Card E: "{n} day streak" 24px + "LONGEST STREAK | {n} DAYS" + month navigator (BeginDisabled chevrons "<"/">" over NavigableMonthsFor + centered "November 2026") + ActivityGrid(day_words, month_max) + legend "More [4,3,2,1 squares] Less [empty]" (brief order literal).
- Share dialog: headless share_open_ sync -> widgets::Modal "Share" with two preview cards ("Flow Highlights" 4 ShareHighlightLines; "Flow Streak" big streak) + BeginDisabled PrimaryButtons "Copy image"/"Download image" + "Coming soon" muted + "Close" LightButton via CloseCurrentPopup+End+Close (DictionaryPage balanced-close pattern). Esc/native close recovered via post-End IsOpen -> share_open_=false sync.
- Self-review defect [CRITICAL, fixed]: first badge ring iterated UTF-8 bullet by byte (phrase.size() over 3-byte bullet) rendering mojibake. Fixed: glyph vector (label chars + " " + bullet string + " ") stepped per codepoint. Verified syntax + full test re-run.

## Review Audit

### Phase 0 self-review (stranger view of the tests)
- CORRECTNESS: total_words/feeds deterministic temp dirs (lwf_insightspage_ prefix, remove_all+create). Generation case uses real StatsStore Record so the atomic bump path is exercised end-to-end (not mocked). Month navigator case asserts both free functions and page state transitions.
- MUTATION: gauge-cap mutant fails exactly the intended CHECK (see above); share-sum cases assert sum==100 (not individual percents only) so off-by-one remainder distribution fails. Empty-category case pins sum==0 (not "no rows" — 6 rows must still exist).
- [MINOR] first run: source-scan case correctly FAILED pre-.cpp (REQUIRE impl_stream.is_open()) while 12/13 passed — proves the scan is not vacuous for the .cpp.

### Phase 1 self-review (stranger view of the header)
- [MAJOR] CategoryRowsFor initially summed all by_category keys (incl. unknowns) for the denominator while rows cover only 6 — with a stray "Custom" key shares would sum <100, violating the 100-or-0 invariant. FIXED: filtered to the 6 defs for both totals and shares input (unknown words still count in card-C total_words). Added unknown-category test (Custom ignored, AI=100%).
- CORRECTNESS: ParseMonthKey validates digits before stoi (no throw); MonthValuesFor guards size/parse/day range/clamps; WeekdayOfFirst handles negatives; Summarize clamps negatives (store already clamps, manual JSON defense); BuildAppRecords excludes empty app + zero-word entries.
- THREADING: Generation() atomic read in SyncIfStale; snapshot copy under store mutex; page summary mutated only on the UI render thread. No locks needed page-side.
- [MINOR] SyncFromSnapshot always returns true (it always recomputes). Accepted and documented as the test seam; Draw never calls it.
- INVARIANT: zero `//` in header (grep-verified); no imgui.h/windows.h/TODO; StatsStore::DateKey reused (no localtime duplication).

### Phase 2 self-review (stranger view of the .cpp)
- [CRITICAL] badge ring per-byte split (above) — FIXED, syntax + 14/14 green after.
- CORRECTNESS: same-line info IconButtons use same-line spacing after big number/rows (nav focus preserved, no IsItemClicked anywhere). Chevrons wrap with BeginDisabled/EndDisabled balanced per Can* calls (evaluated before each). Share dialog close path mirrors DictionaryPage (CloseCurrentPopup inside Begin/End, Close after End) so no unbalanced EndPopup on those paths.
- [MINOR] gauge width risk: RadialGauge widget is fixed 220*scale wide inside fluid ~185-220px cards — can overflow on narrow windows (pre-existing widget geometry, not fixable in this write-set). Recorded as a risk for the manual I-pass at both window sizes; card width is fluid so no overlap imposed by the page.
- [MINOR] legend "More [4 squares] Less [empty]" rendered in brief-literal order (More left, Less right) rather than the GitHub Less/More order — brief wording wins; noted for F9 screenshot check.
- FILING FINDING (not fixed, read-only file): widgets::Modal Esc-close imbalance (Overlays.cpp Close-inside-Begin skips EndPopup) inherited; page state recovers via IsOpen sync but the stale-popup structural fix belongs to W3. Same finding already posted by W4-IMPL-D/T.
- FILING FINDING: InsightsModel::CalendarLevel vs StudioPalette HeatLevel diverge at boundaries (e.g., 26/100 -> model 1, widget 2). Page passes raw day_words+month_max to ActivityGrid, so rendered heat follows HeatLevel while tests pin CalendarLevel. Both quartile policies need F0/F9 reconcile; page does not choose between them silently.

### Final review
- Cross-phase: summaries flow StatsStore Snapshot -> Summarize (Wpm/Fixes/Streaks/Apps) -> CategoryRows (Shares) -> MonthValues (Calendar) -> Draw widgets; no stage re-implements a formula. Backwards compat: no existing file modified; stats.json/InsightsModel semantics untouched; only 3 new files. No leftover build artifacts in repo (insights_page_test.exe removed; test temp dirs under system temp). No band-aids (no swallowed errors, sleeps, type bypasses, or weakened tests). Independent fresh-context review NOT obtained (single-shot agent) — status stays IMPLEMENTED with NEEDs posted; orchestrator must dispatch a reviewer + run the I-checklist on Windows hardware before merge.

## Long-run test (Source/Load/Hostile/Operability/Reversal)
- Source: StatsStore Snapshot is the single source; InsightsModel is the single formula source; page holds only a render cache + generation watermark. All 6 display labels/icons come from CategoryDefs (no second hardcoding in .cpp).
- Load: Snapshot copy is bounded (730-day eviction in store); per-frame work is O(days + categories) only when generation changes; month grid O(days in month); no unbounded collections, no file IO in Draw.
- Hostile: empty/corrupt snapshot (missing file -> empty map) -> 0/"—"/empty calendar, no crash (Load tolerates corrupt per store); negative/manual JSON clamped; malformed month keys skipped; unknown categories ignored for pill shares; null/always-valid tooltips (IconButton ids unique per card); BeginDisabled guards on chevrons at bounds.
- Operability: generation-driven refresh works without restart (Record on engine worker -> next render reflects); month nav bounded to data+current; share dialog recoverable from Esc via IsOpen sync; tabs chevron reachability preserved (disabled, not hidden).
- Reversal: additive-only (3 new files, 0 edits to existing files) — revert by deleting the files + integrator CMake/MainWindow lines; stats.json schema and every other page unaffected.

## Verification Evidence
Verbatim commands and outcomes (Windows MSYS2/MinGW, branch feature/studio-pages):
- [RAN g++ -std=c++17 -fsyntax-only -Isrc -ID:/lwf/_deps-src/doctest tests/test_insights_page.cpp (pre-header, red-first) -> `ui/pages/InsightsPage.h: No such file or directory`, exit 1]
- [RAN g++ -std=c++17 -Wall -Wextra -Wpedantic -fsyntax-only -Isrc -ID:/lwf/_deps-src/imgui src/ui/pages/InsightsPage.cpp -> exit 0, zero warnings (after badge fix too)]
- [RAN g++ -std=c++17 -Isrc -ID:/lwf/_deps-src/doctest -ID:/lwf/_deps-src/json/single_include -ID:/lwf/_deps-src/miniaudio tests/main.cpp tests/test_insights_page.cpp build/liblwf_core.a -o insights_page_test.exe && .\insights_page_test.exe -> 14 passed / 201 assertions, 0 failed, exit 0 (final; includes unknown-category + source-scan)]
- [Mutation kill: kWpmGaugeCap 200->100 -> WPM case FAILED (`1 == Approx(0.6)`); restore -> 14/14 green]
- [RAN cmake --build build --config Release --parallel 4 --target lwf_tests -> exit 0, `[100%] Built target lwf_tests` (my files unregistered, so this verifies no regression to the shipping suite)]
- [RAN .\build\lwf_tests.exe --test-case="Wpm*,CategoryShares*,CurrentStreak*,LongestStreak*,CalendarLevel*" -> 15 passed / 55 assertions, exit 0 (InsightsModel suite intact)]
- [RAN bash scripts/check_hygiene.sh -> `hygiene: OK`, exit 0; bash scripts/check_no_network.sh -> `no-network: OK`, exit 0]
- [RAN Select-String grep // TODO/FIXME/XXX windows.h/SetWindowFontScale/IsItemClicked/Log:: over the 3 write-set files -> no matches (header has no imgui.h either, asserted in tests)]
- NOT-RUN honestly: full `local-wisper-flow` link (page TU unregistered until CMake NEED lands) and any Windows visual window run (no window runtime here; MSVC compile deferred to CI per A-001). Standalone doctest against liblwf_core.a is the scoped equivalent (pages cannot link into lwf_tests, CMakeLists.txt:190).

## Unverified Assumptions & Remaining Risks
- [ASSUMED] Card B big number = total fixes (corrected+dict). Alternative reading (dict-only) would make FixesMadeByFlow unused; breakdown rows show both parts so the sum reading is the only coherent one.
- [ASSUMED] Gauge fraction = WPM/200 clamped (display cap). The brief never names a cap; any other cap changes fill only, never a user stat. Flagged for the I-pass screenshot check.
- [ASSUMED] Fluid card widths (~(avail-2*gap)/3, Row-2 55/45 split) satisfy "~185px" at the nominal window; narrow-window gauge overflow risk stands (widget 220px).
- [ASSUMED] info-icon tooltips B rows ("Words the cleanup changed."/"Words the dictionary corrected.") are definitional, not invented stats. First tooltip/footnote/mobile/empty copy are brief-verbatim.
- [UNKNOWN] headless pixel fidelity (badge ring proportions, card heights, legend order, gauge center 28px-vs-16px widget size) — blocked on Windows visual pass.
- [RISK] Modal Esc stale-popup (W3-owned) can glitch the share dialog Esc path; page recovers logically but the structural fix is outside the write-set.
- [RISK] Model-vs-widget calendar boundary divergence (26/100 example) — reconcile at F0/F9.
- [RISK] MSVC compile of InsightsPage.cpp (MinGW syntax only here, A-001) and runtime Draw (ImGui link) unverified until integrator wires CMake.

## Manual Checklist I1+I2+I3 (operator spec §8 not in-repo; operationalized from P21 acceptance — ALL NOT-RUN)
- I1 (data + WPM + refine): 1) Open app -> Insights -> title + 56px share badge + "Your usage"/"Your voice" tabs visible. 2) With <60s speech: Card A shows "—", gauge empty, footnote visible, no "Top 0.1%" anywhere. 3) Dictate >=60s: Card A shows rounded WPM, gauge fills toward 200 cap, info "i" tooltip reads "Your average speaking speed while dictating." 4) Card B total equals words-corrected + dictionary-fixes with both rows + divider; Card C total words + "Desktop / {n} words" + disabled "Download on mobile" (tooltip on hover).
- I2 (streak + calendar + refresh): 1) Dictate on consecutive days incl. midnight: "N day streak" title + "LONGEST STREAK | {n} DAYS" update without restart (next render after Record). 2) Same-day heat cell lights on today's date in ActivityGrid; legend "More [4 squares] Less [empty]" visible. 3) Month chevrons move only through months with data + current month; buttons disable at bounds ("Not available yet." voice tab does not shift layout).
- I3 (share + empty + voice): 1) Click share badge -> "Share" dialog with "Flow Highlights" (streak, wpm, total words, apps) + "Flow Streak" (current streak) cards; "Copy image"/"Download image" disabled + "Coming soon"; Close returns. 2) Voice tab -> "Not available yet." only. 3) Fresh profile (empty stats.json): cards show "0"/"—", D shows "No dictations yet. Hold Ctrl + Win to start.", E shows an empty month calendar, share dialog shows 0-based highlights without invented stats.

## Integrator NEEDs (exact, FORBIDDEN to this agent)
1. **CMakeLists.** Add `src/ui/pages/InsightsPage.cpp` to the WIN32 `target_sources(local-wisper-flow PRIVATE ...)` list at **CMakeLists.txt:348-364** (after `src/ui/widgets/Overlays.cpp:363`, same 12-space indent, `src/ui/pages/InsightsPage.cpp`). Add `tests/test_insights_page.cpp` to `add_executable(lwf_tests)` at **CMakeLists.txt:137-187** (alphabetical near `tests/test_insights.cpp:161`, e.g. after `tests/test_insights.cpp`). Re-run the 14-case standalone result (14 passed / 201 assertions) via the registered target.
2. **MainWindow + main wiring** (my ctor is `(StatsStore&)`). Anchors verified today:
   - `src/main.cpp`: `StatsStore usage_stats(data_dir + "\\stats.json", ...)` already exists; pass it to the shell: change `MainWindow full(fsm, audio, config.Store(), ui_bus);` to `MainWindow full(fsm, audio, config.Store(), ui_bus, usage_stats);` (needs `#include "core/StatsStore.h"` — already present per grep).
   - `src/ui/MainWindow.h`: extend the ctor to `(const DictationFSM&, const MiniaudioCapture&, SettingsStore&, UiBus&, StatsStore&)` (declarations near lines 69-70), store a member `StatsStore& stats_;` (or own `std::unique_ptr<InsightsPage>` built in the ctor), and `#include "ui/pages/InsightsPage.h"`.
   - `src/ui/MainWindow.cpp:1011-1014`: replace the stub body with:
     ```cpp
     void MainWindow::DrawInsightsPage(float scale) {
         insights_page_->Draw(scale);
     }
     ```
     (member-lifetime construction in the ctor, not a function-static; `Page::Insights` routing/rail already correct [VERIFIED MainWindow.cpp:339-340,385]).
3. **font_sources / studio scan lists.** Extend the scan lists in `tests/test_overlay_settings_ui.cpp` (font_sources ~254-260 + the studio scan listing) with `src/ui/pages/InsightsPage.h` and `src/ui/pages/InsightsPage.cpp` in the same commit that first compiles the TU, or the new files escape the `SetWindowFontScale` ban (files are already clean; source-scan test enforces this when extended).
4. **Modal/HeatLevel findings (W3-owned).** `src/ui/widgets/Overlays.cpp` Modal/Sheet Esc-Close-before-EndPopup imbalance; `src/ui/theme/StudioPalette.h` HeatLevel vs `src/core/InsightsModel.cpp` CalendarLevel boundary divergence. Neither touched here.

## Sessions Log
- [COMPLETE] Implementation, standalone + scoped verification, hygiene, commits. Sync-diff vs W4 siblings: no overlapping files (siblings own HotkeyBinding/UiBus/MainWindow; this agent touched only its 3 write-set paths + its report).

## Git State
- Branch: feature/studio-pages [VERIFIED git branch --show-current].
- Code commit 96fb22cf873ed53f938009e5497599ca131f1910 (explicit paths src/ui/pages/InsightsPage.h src/ui/pages/InsightsPage.cpp tests/test_insights_page.cpp). PROCESS DEVIATION: this and the report commit below were created with single-line messages and WITHOUT the Standing Order 15 trailers (Task/Agent/Wave/Report) — `commit --amend` is forbidden (Order 13), so the omission is disclosed here rather than rewritten. The correction commit that appends this section carries the full trailers; task attribution is this report's filename + COMMS lines.
- Report commit 3b72804b380f5bb09d97d5ca6a7e49f700a59ac1 (docs-only, explicit path). COMMS.md appended (START/CLAIM/3 NEEDs + DONE below) but left uncommitted for ORCH (standing order: only ORCH commits COMMS).
- Working tree at handoff: my 3 write-set paths + report committed; remaining modified/untracked files belong to W4 siblings (HotkeyBinding/UiBus/WindowsHotkeyListener/MainWindow, A2-A4/S briefs/reports) and were NOT staged or touched. Correction commit above carries full trailers (Task/Agent/Wave/Report).

