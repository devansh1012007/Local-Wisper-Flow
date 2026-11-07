<!-- ORCH-DIGEST:START -->
agent: W3-REV-01 | scope: Wave 3 (P17/P18 + widgets/modal/toast) | verdict: APPROVE-WITH-CHANGES
findings: BLOCKER 0 | MAJOR 1 | MINOR 5 | NIT 2
claims_audit: 16 checked / 2 unsupported or false
band_aids_found: none
top_3_risks: muted #7A776F fails 4.5 (4.00–4.47) will bite at Wave 4 light opt-in; Toast ID/cursor contract carried into Wave 4 wiring (no caller yet); independent Apply*Style gates allow double-scale if both palettes applied per context
optimizations: DO-NOW 1 | DO-LATER 2 | NOT-WORTH-IT 1
reversal_check: done
confidence: med (logic + tests independently rerun green; pixel fidelity / DPI / G3 Windows runtime unverifiable here)
<!-- ORCH-DIGEST:END -->

# Code review — W3-REV-01

## 0. Method
Diff range reviewed: `git diff bd9e790..HEAD -- src/ tests/ CMakeLists.txt` (docs excluded per scope).
[RAN git diff --stat bd9e790..HEAD -- src/ tests/ CMakeLists.txt -> 12 files, 1493 insertions, 28 deletions:
CMakeLists.txt, src/core/SettingsStore.cpp, src/ui/MainWindow.{h,cpp}, src/ui/theme/StudioPalette.{h,cpp},
src/ui/widgets/{Buttons.cpp,Display.cpp,Overlays.cpp,Overlays.h,Widgets.h}, tests/test_overlay_settings_ui.cpp.]
Files read in context (full, not just hunks): MainWindow.{h,cpp}, SettingsStore.{cpp,h}, StudioPalette.{h,cpp},
Widgets.h, Overlays.{h,cpp}, Buttons.cpp, Display.cpp, full test appends (test_overlay_settings_ui.cpp:520–796),
ThemePalette.h (ContrastRatio/Rail metrics), scripts/check_hygiene.sh + check_no_network.sh, main.cpp ApplySettings
drain, vendored imgui.h (PushFont/InvisibleButton/BeginPopupModal/IsKeyPressed signatures).
Order followed: blind pass first (source only), then implementer reports
(docs/agent-ops/20261107-studio/reports/w3-impl/W3-IMPL-S1-T-014.md,
W3-IMPL-S003-T-015.md, W3-IMPL-INT.md, W3-FIX-SCAN.md).
Independent reruns: [RAN bash scripts/check_hygiene.sh -> hygiene: OK, exit 0],
[RAN bash scripts/check_no_network.sh -> no-network: OK, exit 0],
[RAN .\build\lwf_tests.exe --test-case="*studio*,*last_page*,*keyboard nav*,*font mechanism*,*contrast*" -> 13 passed / 261 asserts, exit 0],
[RAN ctest --test-dir build -R "lwf-tests" -> 1/1 Passed 21.15s],
[RAN python contrast calc -> muted/page_bg 4.28, muted/card 4.00, muted/white 4.47, muted/banner_cream 3.80;
ink/page_bg 16.31, white/teal_700 7.49, white/teal_500 4.69 — matches S2 numbers].
No source edits made (read-only review). No Windows visual run, no zig G3, no MSVC build (see §9).

## 1. Blind-pass findings (before reading implementer reports)
- Rail keyboard contract is intact: [VERIFIED src/ui/MainWindow.cpp:156] NavEnableKeyboard preserved,
  [VERIFIED :421-422] `const bool pressed = InvisibleButton(item.id, …, EnableNav)` preserved,
  [VERIFIED :400-406] NavActive-gated `SetKeyboardFocusHere` restore preserved,
  [VERIFIED :434-437] 2px focus ring preserved. `page_focus_pending_ = true` count = 2
  (SetVisible:196 + Welcome-Finish:1178), `= false` = 3 — count pin holds. `if (ImGui::IsItemClicked(` absent
  (only the pre-existing explanatory comment at :416). SliderInt bare calls only at :868/:880 (Cleanup), none added.
- Settings tab move is genuinely unchanged bodies: diff hunks for MainWindow.cpp are only
  kRailWidth, constructor, RunOnce rail_w, RenderPages switch, DrawRail, PersistPage, stubs+General+Settings,
  and 1 line in DrawWelcomePage. [RAN git diff -U0 + grep Draw*Page -> zero hunks inside
  DrawSpeech/Cloud/Cleanup/Privacy/About/Home bodies]. The five bodies are moved only in the sense of
  dispatch indirection (Settings → tabs → dispatch) plus the new General page.
- `last_page` handling is total and safe: [VERIFIED src/ui/MainWindow.h:47-67] PageForRoute exact-matches
  only Insights|Dictionary|Snippets|Transforms|Scratchpad|Settings, returns Home for everything else
  (empty/lowercase/Welcome/legacy Speech/About/bogus). [VERIFIED src/core/SettingsStore.cpp:67] additive
  String default "Home", hot_apply true. [VERIFIED src/ui/MainWindow.cpp:93] constructor resolves
  `PageForRoute(GetString("last_page"))` before Init's Welcome override, so first-run still wins.
  PersistPage (:551-557) is Save-only (no ApplySettings spam) and no-ops for Welcome; Welcome-Finish (:1177)
  is the second call site. Unknown keys preserved by construction (Save merges strings_ + raw_unknown_).
- Palette is still dark-only in the live window: [VERIFIED src/ui/MainWindow.cpp:163,258] only
  ApplyImGuiStyle(kDarkPalette) is applied; [RAN Get-ChildItem src -Recurse | Select-String ApplyStudioStyle
  -> only definition StudioPalette.{h,cpp}, zero callers]. Tests pin this absence
  (test_overlay_settings_ui.cpp:790,795). MainWindow was NOT restructured by S2 — verified: no
  src/ui/pages/, no StudioPalette/widgets::/PrimaryButton/RadialGauge references in MainWindow.cpp
  (only DrawRestartBanner false positives), stubs live in the monolith.
- Widget facts verified by hand: RadialGauge arc math correct (180° pi→2pi upper half, radius/stroke/dots
  consistent, clamp 0..1); ActivityGrid indexing consistent (row=i%7, col=i/7, total_w/h = last-cell ends);
  HeatLevel quartiles are strict `>` (exact 25/50/75% fall one level down) and exact-boundary cases are untested;
  ProgressPill badge rule is `fill_w >= text_size.x + 24*scale` → centered white on teal_500, else ink label +
  white badge right; Toast dwell is toast_dwell_s=5.0s (Overlays.cpp:107), Modal/Sheet Esc is
  IsKeyPressed(Escape,false) + armed-once OpenPopup; hit targets are 32px (Icon/Chip/Keycap/TabRow/row 52)
  except ToggleSwitch 52×28 (height 28 < 32).
- AGENTS.md/hygiene: no windows.h/winhttp.h, no TODO/FIXME/XXX, no cloud endpoint outside src/stt/cloud/,
  no network APIs, no file-write outside allowlist — both gate scripts OK. New files have zero comment markers.
  Only comment-marker additions are 2 replacement lines in MainWindow.h (stale page list corrected, same
  comment block). No secrets, no Log:: calls, saved_dwell (1.5s) untouched vs Toast 5000ms — distinct tokens.

## 2. Claims audit
| Implementer claim | Verified? | Evidence / finding |
|---|---|---|
| S1: Settings bodies moved behind tabs UNCHANGED (zero edits to 5 functions) | Yes | [RAN git diff hunks -> zero intra-body hunks; dispatch only] |
| S1: Rail shell 220px/40px/8px/15px, 7 items + divider, keyboard preserved | Yes | [VERIFIED MainWindow.cpp:39 kRailWidth=220, :381-389 7 ids, :408-412 divider, :391 40.0f, :392 font_card_title, :441-443 0.10 wash + 8px] |
| S1: last_page additive String default Home, invalid→Home, unknown keys preserved, Welcome never persisted | Yes | [VERIFIED SettingsStore.cpp:67, MainWindow.h:47-67, MainWindow.cpp:93,551-557,1177] + code shows Save merges unknown |
| S1: NavEnableKeyboard / focus-ring / restore / page_focus_pending_=true == 2 preserved | Yes | Counts re-verified above; keyboard doctest 1/1 green |
| S1: PersistPage uses Save only (no ApplySettings reload spam) | Yes | [VERIFIED :551-557]; main.cpp ApplySettings (:577-596) ignores last_page so hot_apply=true is harmless |
| S1: UiBus StartTransform/OpenScratchpad already have cases, no main.cpp edit needed | Yes | [VERIFIED src/main.cpp:603,615 cases; drain has no default but both commands covered] |
| S1: No CMake edit needed (stubs in MainWindow.cpp, font_sources unchanged) | Yes | S1 diff adds no src/ui/*.cpp; main build still links (INT gate) |
| S1: "Zero // additions in src/" | No (overstated) | [VERIFIED git diff -> 2 added // lines in MainWindow.h:12-13, but they replace the same existing comment; MainWindow.cpp additions have zero //; new files zero //]. Substance (no new comment topics) holds; literal count is wrong. See NIT. |
| S1: Existing assertions undeleted (rail scans updated only because incorrect) | Yes | [RAN git diff tests -> no `^-` deletions, appends only (include + EOF)] |
| S2: 13 brief hexes verbatim, layout token set | Yes | [VERIFIED StudioPalette.h:24-68 matches; test pins all 13 hexes + 13 layout values. Note: 15 widget functions implemented, brief context says 14 — one extra, not a defect.] |
| S2: muted #7A776F is 4.00–4.47 (fails 4.5), ink/teal pairs pass, border ~1.1–1.3 exempt | Yes (numbers) | [RAN python -> 4.00/4.28/4.47/3.80, ink 16.31, white/teal_700 7.49, white/teal_500 4.69] exactly as reported. Test pins 4.47/4.28/4.00 (tests:728-730) — provisional, see §4. |
| S2: ApplyStudioStyle defined but never called; ApplyImGuiStyle(kDarkPalette) only applied palette | Yes | Caller scan = definition only; test pins absence; ApplyStudioStyle gate mirrors ApplyImGuiStyle (StudioPalette.cpp:19-23) |
| S2: Per-frame string allocs only in Banner/TabRow | No (understates) | StyleButton (Buttons.cpp:42-43) and OutlinePill (:87-88) also build a std::string per button per frame. Trivial cost, but claim is incomplete. See NIT. |
| S2: Self-review fixes (dangling c_str removed, TabRow SameLine, "##" ids, Banner Dummy layout, <cstdio>, no comments, armed-once OpenPopup) | Yes | No dangling temporaries found; TabRow has SameLine (:227); StyleButton/OutlinePill compose "##" (:42,:87); Banner Dummy + SetCursorScreenPos (:241-267); Display.cpp:11 <cstdio>; zero // in new files; Overlays.cpp:28-30,67-69 armed-once |
| S2: ToggleSwitch geometry 52×28, knob 20+pad4 mirrored; Primary/Light 40px; Icon 32px/24px glyph | Yes | [VERIFIED Buttons.cpp:108-146; off x=+pad, on x=+w-pad-knob] |
| INT: CMake registers exactly 4 TUs in WIN32 block, build+ctest+hygiene+no-network GREEN | Yes (independent) | [VERIFIED CMakeLists.txt:360-363 four lines after ThemePalette.cpp; hygiene/no-network OK by rerun; ctest -R lwf-tests 1/1 rerun OK. Full 23/23 + 849s ctest not rerun here — scoped-check rule, see §9] |
| FIX-SCAN: font_sources extended with 4 TUs, all ban-clean, gates green | Yes (with correction) | [VERIFIED tests:262-265 four TUs; grep SetWindowFontScale zero in new files; ctest 1/1 rerun OK] |
| FIX-SCAN: "Headers … remain unscanned" | No (false) | Headers ARE scanned by studio test (tests:757-773 lists all 7 incl. 3 headers). Combined coverage is complete; the note looked only at font_sources. See NIT. |
| S2: FINDING palette-source conflict (STUDIO_PLAN navy vs brief teal) needs ORCH correction | Yes (flag is real, docs out of scope) | Teal set implemented per brief; no code arbitration here. Carried as docs risk. |

## 3. Long-run test (independent answers)
Source: Nav restructure fixes the single Page/NavItem/RenderPages triple at its source (no symptom patch);
PersistPage centralizes write discipline (rail + Welcome only); widgets are pure renderers with no state
beyond Modal/Sheet/Toast instances (no singletons). Good shape.
Load: Rail/tabs are O(7)/O(6) per frame; Save is one small config write per nav click (modeled after
ApplyAndSave minus engine reload). Widgets do CalcTextSize + a few per-frame strings — negligible.
Hostile: malformed last_page (empty/bogus/Welcome/legacy/lowercase/non-string) cannot crash or corrupt —
Load coerces non-strings to default, PageForRoute is total → Home; unknown keys preserved; Toast/Modal
Esc/consume-once are idempotent; TabRow/ActivityGrid guard null/empty. Null id/label is the only
unguarded input (ImGui precondition violation would crash) — see R-W3-004.
Operability: failures land on Home (safe default); Esc closes modal/sheet; focus ring on every
interactive widget; stubs are greppable ("Not available yet." ×5 in MainWindow); visual checkpoints
(F3 Devansh, F4–F8 Wave 4) are explicit, not CI-verifiable.
Reversal: revert is additive-file deletion + 4 CMake lines + test appends + 1 schema line; last_page is
ignorable by older builds (unknown-key preserved). No data migration, no format change.

## 4. Findings
### R-W3-001 [MAJOR] Toast action button reuses raw action_label as ImGui ID — collision risk
- Location: `src/ui/widgets/Overlays.cpp:141-143` · Evidence: `ImGui::Button(action_label_.c_str(), button_size)` with
  no `##` suffix or unique id, while every other widget uses `label + "##" + id` (Buttons.cpp:42,87) or
  `_close` (Display.cpp:262). Two buttons with the same label in the same window share ImGui ID state
  (hover/focus/activation). As soon as Wave 4 wires a Toast on a host page that also has e.g. "Undo",
  the Toast button and the page button collide. Related: Toast::Draw calls SetCursorScreenPos without
  restoring the cursor, so calling Draw anywhere but frame-end disrupts layout. Why it matters: first
  real use will misbehave; cost to fix now is one line. Recommended fix: give the Toast button a fixed
  unique id (`"##toast_action"`), save/restore cursor (GetCursorScreenPos/SetCursorScreenPos) or
  document/enforce frame-end-only calling.
### R-W3-002 [MINOR] last_page constructor wiring has no regression pin
- Location: `tests/test_overlay_settings_ui.cpp:641-642` · Evidence: structure scan only asserts
  `content.find("last_page") != npos` in MainWindow.cpp, which PersistPage alone satisfies. Deleting
  `page_ = PageForRoute(settings_.GetString("last_page"))` (MainWindow.cpp:93) would still pass all
  route + persistence + scan tests. Why it matters: the Home-default/restore chain is the point of the
  key, and the mutation test S1 claims ("revert PageForRoute → fail") does not cover this link.
  Recommended fix: pin the constructor expression specifically (e.g. assert the file contains
  `PageForRoute(settings_.GetString("last_page"))`).
### R-W3-003 [MINOR] HeatLevel quartile edges use strict `>` and exact boundaries are untested
- Location: `src/ui/theme/StudioPalette.h:80-89`, tests:742-748 · Evidence: ratio == 0.75 → level 3 (not 4),
  == 0.50 → 2, == 0.25 → 1. No test covers 0.25/0.50/0.75 exactly. Whether exact-quartile belongs up or
  down is a spec decision, currently implicit. Why it matters: month-max calendars will hit exact edges
  (e.g. 25/100) and render one level dimmer than possibly intended; ActivityGrid day-order has the same
  untested-coupling class. Recommended fix: decide `>` vs `>=` with F4, add exact-boundary CHECKs.
### R-W3-004 [MINOR] Widget null-id/label contract is inconsistently guarded
- Location: `src/ui/widgets/Display.cpp:262` (`std::string(id)`), Buttons.cpp:122 `CalcTextSize(glyph)`,
  Display.cpp:150 `CalcTextSize(label)`, Buttons.cpp:170/230 label/tab nulls · Evidence: some labels are
  null-guarded (Primary/Light via ternary, StatCard/Gauge/Progress for center/sub/label) but ids, Chip/Keycap
  labels, glyph, TabRow elements are not. Passing nullptr to ImGui itself is UB, so this is not a new
  crash beyond ImGui's precondition — but the partial guarding implies safety that does not exist.
  Why it matters: a Wave 4 caller assuming "nullptr ok" from the guarded paths will crash in the
  unguarded ones. Recommended fix: either document "id/label/glyph/tabs must be non-null" in Widgets.h
  or guard consistently (early return / empty fallback).
### R-W3-005 [MINOR] ProgressPill empty-state dot + narrow-width badge overlap
- Location: `src/ui/widgets/Display.cpp:162` · Evidence: `fill_w = (w-4*scale)*clamped + 4*scale` so clamped=0
  still draws a 4px teal dot (looks like "started", not "empty"). Badge mode (else branch) places the
  label at +12*scale and the badge at right; when `w` is narrow the label overflows into the badge
  (no truncation/clip). Why it matters: Insights will show a dot at 0% and overlapping text on narrow
  cards. Recommended fix: `fill_w = w*clamped` (or hide fill at clamped==0) and clamp/ellipsize the
  label when it cannot fit beside the badge.
### R-W3-006 [MINOR] Banner dead close button when open==nullptr
- Location: `src/ui/widgets/Display.cpp:227-229,260-266` · Evidence: `open==nullptr` means "always show",
  yet the close IconButton is still drawn; clicking it does nothing (guarded `if (open != nullptr)`).
  Why it matters: dead interactive control + misleading affordance. Recommended fix: skip the close
  button when open==nullptr, or require open non-null for the close path.
### R-W3-007 [MINOR] toast_dwell_s (live token) unpinned; layout test pins only toast_dwell_ms
- Location: `src/ui/theme/StudioPalette.h:60-61`, `src/ui/widgets/Overlays.cpp:107`, tests:704 · Evidence:
  test asserts `toast_dwell_ms == 5000` but nothing asserts `toast_dwell_s == 5.0f`, while behavior uses
  _s. Changing _s to 3.0 would keep the suite green but break the 5000ms spec. Similarly radius_pill
  (999) is unpinned. Why it matters: the pinned token is not the live one (two tokens, one checked).
  Recommended fix: assert both _s and _ms and keep them consistent (or derive one from the other).
### R-W3-008 [NIT] ToggleSwitch off-knob contrast + 28px height vs 32px hit token
- Location: `src/ui/widgets/Buttons.cpp:130-144` · Evidence: off-track is border #E7E4DE with white knob
  (~1.2:1 knob/track — knob barely visible); height 28px < hit_size 32 (Icon/Chip meet 32, Toggle
  does not). Geometry itself is correct (20px knob + 4px pads mirrored; verified). Why it matters:
  off-state affordance is weak and vertical target is below the kit's own token. Spec may intend this
  (S2 documents 52×28) — confirm at F4 with screenshots rather than redesigning here.
### R-W3-009 [NIT] Dead `metrics::rail_width` (208) vs live `kRailWidth` (220) drift
- Location: `src/ui/theme/ThemePalette.h:95`, `src/ui/MainWindow.cpp:39` · Evidence: [RAN grep rail_width
  -> only the definition remains; zero uses]. S1 could not edit ThemePalette (read-only), so the token
  is now dead and misleading. Why it matters: a Wave 4 reader using it gets 208 vs the shipped 220.
  Recommended fix (ORCH-owned): update the token to 220 or delete it and reference kRailWidth.

No BLOCKER was found. Badge/gauge/toggle math, Esc handling, focus rings, and last_page totality
checked out clean (see §1/§5) — no counterfeit placebo fix needed beyond the items above.

## 5. Attempts to break it
- Route fuzz (reasoned + test evidence): PageForRoute with "", "bogus-route", "Welcome", "Speech",
  "About", "settings" (lowercase) all → Home; only exact TitleCase persisted values map elsewhere.
  Outcome: safe default, no crash. RouteName(Welcome)="Welcome" with PageForRoute=Home is deliberate
  (Welcome never persisted via the guard + fallback double-cover).
- Config fuzz (reasoned from LoadLocked): missing last_page → "Home" fill; numeric JSON → default;
  string "Welcome" → Home; unknown keys round-trip through Save. Outcome: no corruption.
- Persist storm: Save holds the SettingsStore mutex across tmp-write + .bak + rename (SettingsStore.cpp:193-232),
  so UI-thread PersistPage and main-thread ApplySettings Save serialize — no tmp-file race.
- ActivityGrid with values.size() % 7 != 0 (e.g. 30 values): weeks=5, last column partial, total_w/h =
  last drawn cell ends — verified arithmetic, Dummy covers exactly the grid. Empty values: weeks=0,
  labels + 122*scale-tall Dummy still drawn (harmless, arguably wasteful — not filed).
- Gauge at clamped 0/1/negative/>1: 0 draws zero-length value arc + overlapping start/end dots;
  1 draws full semicircle; negatives/>1 clamp via Clamp01. Outcome: no crash, only the 0-case dot
  overlap noted as benign (both dots same spot, end on top).
- TabRow with tabs=nullptr/count=0/selected=nullptr → early return before PushFont (no unbalanced Pop).
  TabRow overflowing row width → SameLine wrap would stack rows and the 1px underline would draw mid-grid:
  long-tab overflow unhandled (minor, Wave 4 tab counts are 6 — noted, not filed as defect).
- Modal::End() after Begin()==false would call EndPopup without a matching BeginPopupModal — ImGui
  would assert. No caller exists yet (ApplyStudioStyle/Modal/Sheet/Toast have zero call sites), so the
  footgun is latent. Recommend R-W3-001's contract fix covers this too (guard End or document the
  `if (Begin()) { …; End(); }` pairing).

## 6. Dimension checklist
Correctness · PASS (no logic bug found; gauge math, grid indexing, badge rule, Esc/consume-once all correct).
Root cause vs symptom · PASS (triple-restructure + centralized PersistPage; no symptom patch).
Regression/compat · PASS (additive schema, bodies unchanged, tests append-only; unknown keys preserved).
Concurrency/idempotency · PASS (SettingsStore mutex serializes Save; Toast consume-once; Persist idempotent).
Error handling · PASS with note (Save failures silent as pre-existing; new Persist inherits it — no new swallowing).
Security/abuse · PASS (no secrets/logs/network/file-write; muted contrast is the only above-floor issue, §4).
Performance/scalability · PASS (O(7)/O(6) rail/tabs, one Save per click; per-frame strings negligible).
Migration/rollback · PASS (additive key, no format change; revert = delete files + 5 lines).
Observability · PASS (stubs greppable ×5, "Not available yet." consistent with RowList empty).
Maintainability/conventions · PASS with notes (dead rail_width + minor guard inconsistency; no comments/TODO/windows violations).
Docs impact · FLAG (STUDIO_PLAN navy vs teal conflict is docs-only and out of scope — ORCH must correct §5 at next docs touch, else Wave 4 readers find two truths).

## 7. Optimization and simplification opportunities
| # | Location | Evidence | Impact | Effort | Risk | Verdict |
|---|---|---|---|---|---|---|
| 1 | Overlays.cpp:143 Toast Button id | Raw label as ID (see R-W3-001) | Correctness, not perf | Tiny (one line + cursor save) | Low | DO NOW |
| 2 | ThemePalette.h:95 rail_width dead token | Zero uses vs kRailWidth 220 | Drift/confusion only | Tiny (ORCH token update) | Low | DO LATER |
| 3 | StudioPalette.h HeatLevel boundary | Strict > + untested edges (R-W3-003) | One-octave visual calibration | Small (decide + ~6 CHECKs) | Low | DO LATER |
| 4 | Buttons.cpp:42,87 / Display.cpp:262 per-frame std::string ids | A few small allocs ×60fps | ~KB/s churn, immeasurable | Medium (caching ids) | Medium (lifetime bugs) | NOT WORTH IT |

## 8. Cross-agent / cross-task interactions observed
- S1↔S2 protocol held: S1 consumed only kDarkPalette + local kRailWidth and stayed off StudioPalette
  [VERIFIED COMMS CLAIM + no StudioPalette reference in MainWindow.cpp]; S2's palette/widgets are
  additive and uncalled, so no cross-contamination. INT wired exactly S2's 4 TUs (CMakeLists.txt:360-363)
  without touching S1's shell. FIX-SCAN completed the font_sources chain (4 TUs) — combined with S2's
  studio test header scan, all 7 files are now ban-covered (contra FIX-SCAN's "headers unscanned" note).
- Remaining cross-wave hazards (not Wave 3 defects): (a) S2's single-palette-per-context rule —
  ApplyStudioStyle and ApplyImGuiStyle have independent (context,scale) gates, so alternating them
  double-scales; Wave 4 must commit per context. (b) ActivityGrid day-0 = Sunday of week 1 must match
  the Insights data order at F5, else the grid rotates. (c) OpenScratchpad still routes to
  Log+SetVisible in main.cpp (verified); real Scratchpad page is Wave 4.

## 9. What I could not verify
- Pixel fidelity / 1.0–1.5 DPI layout of rail (220/40/8/15), gauge arc, activity steps, banner gradient,
  and active wash legibility: no Windows runtime in this environment; screenshots live in orchestrator
  conversation, not the repo. F3 (Devansh) + F4–F8 checkpoints own this — not CI-verifiable here.
- G3 zig type-check of Windows sources (no zig on box per AGENTS.md); MSVC-specific compile of the 4
  new TUs (MinGW full build + S2 -fsyntax-only green; MSVC is CI-lane only).
- Full 23/23 ctest (849s) not rerun under the scoped-check mandate; I reran hygiene, no-network, and
  the doctest suite (1/1 lwf-tests, 13/13 scoped studio cases) instead, all green.
- Whether exact 25/50/75% heat edges should be up or down (spec underspecified) — flagged as decision,
  not asserted as bug.
