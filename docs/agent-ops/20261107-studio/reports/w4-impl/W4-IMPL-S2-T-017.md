# W4-IMPL-S2 — Snippets page (T-017) — recovery implementation

## ORCH-DIGEST
- Status: DONE (recovery run; all gates run, see §6; code + docs committed separately).
- Recovery basis: original W4-IMPL-S died mid-report after filing W4-IMPL-S-T-017.md
  (context only, no source, Sessions Log PENDING). This recovery re-derives the
  investigation for the load-bearing claims listed under Provenance below, then
  implements from scratch. Nothing in src/ui/pages/SnippetsPage.{h,cpp} or
  tests/test_snippets_page.cpp exists yet at recovery start (verified by directory
  listing, 4 sibling pages only).
- Scope: exactly 3 new files. No other file is touched by this agent. CMakeLists,
  MainWindow wiring, main.cpp, font_sources scan lists, and PROGRESS.md are NEEDs
  for the integrator (exact lines in §7).
- Architecture: `lwf::ui::pages::SnippetsPage` on a shared `SnippetStore&`,
  header-inline portable model in SnippetsPage.h (SnippetStore + StudioPalette +
  Overlays includes only; no imgui.h), ImGui rendering only in SnippetsPage.cpp.
  DictionaryPage precedent followed exactly (helpers + State struct + page class;
  Draw in .cpp; lwf_tests link-free). TransformsPage precedent followed for editor
  sheet state (editing flag, buffer sync, counter, sheet recovery on Esc-close).
- Validation: every editor save goes through SnippetStore::Validate (exact 4 spec
  messages, spec order) before SnippetStore::Upsert, then SnippetStore::Save().
  The page never re-implements matching; tests pin store matching behavior as the
  page's delegated contract.
- Deliberate deviations from siblings are numbered D-1..D-10 in §4.

## Provenance: what was re-derived vs verified-inherited

Verified-inherited from the recovered report (each re-confirmed cheaply in code
this session by reading the same files; line refs below are my own reads):

1. SnippetStore v2 API and Validate message order/strings.
   [VERIFIED src/core/SnippetStore.h:8-55 this session; src/core/SnippetStore.cpp:397-438
   this session: trim-empty trigger -> "Please enter a word"; code-point cap 4000
   -> "Snippet too long"; trim-empty expansion -> "Please enter a snippet";
   cross-field case-insensitive duplicate -> "Snippet already exists".]
2. Upsert semantics: too-long checked BEFORE the empty-expansion delete path;
   empty expansion on an existing trigger deletes and succeeds; id-alone predicate
   when exclude_id is set; expansion stored verbatim, trigger trimmed.
   [VERIFIED src/core/SnippetStore.cpp:445-504 this session.]
3. Save() with empty path_ returns false; Save(path) writes tmp+rename+.bak.
   [VERIFIED src/core/SnippetStore.cpp:280-290, 236-278 this session.]
4. Widget contracts: LightButton/Chip/IconButton/Banner signatures in
   src/ui/widgets/Widgets.h; Sheet Begin/End and Toast dwell in
   src/ui/widgets/Overlays.{h,cpp}; toast_dwell_s = 5.0s [VERIFIED
   src/ui/theme/StudioPalette.h:60-61 this session]; unselected Chip = 18% glass
   [VERIFIED src/ui/widgets/Buttons.cpp:187-191 this session]; IconButton glyph
   drawn at 24px [VERIFIED Buttons.cpp:107-129 this session]; Banner tolerates
   empty body [VERIFIED src/ui/widgets/Display.cpp:233-275 this session].
5. Sheet Esc closes the widget while Begin's frame continues (pre-existing W3
   finding; page recovers by state check). [VERIFIED src/ui/widgets/Overlays.cpp:71-95
   this session.]
6. Test plan items (4 messages + order, longest match, casing, punctuation,
   undo round-trip incl. persistence) and the "no invented banner headline copy"
   constraint. [INHERITED as acceptance criteria from recovered §5 and brief §1.]
7. Undo design decision (immediate RemoveById + single-slot stash +
   Upsert-restore; id regeneration on undo acceptable, spec does not require id
   stability). [INHERITED decision; rationale re-stated below, not re-litigated.]
8. Save-failure decision (mutations call Save() but keep working silently if it
   fails; in-memory state may be ahead of disk). [INHERITED decision, flagged in §4
   as a deliberate risk with a proposed follow-up.]
9. Pencil U+270E retained per brief wording, font glyph coverage flagged as a
   visual-check item. [INHERITED decision.]
10. SnippetEditorModel (SettingsUiModel) carries non-spec messages and stays
    bypassed; page validates through the store only.
    [VERIFIED src/core/SettingsUiModel.h:43-56 this session; INHERITED decision.]

Re-derived by me this session (not present in the recovered report, own evidence):

11. Namespace and wiring: `lwf::ui::pages::SnippetsPage`; fwd-decl/setter/member
    exact lines read at src/ui/MainWindow.h:18-20,106,193, src/ui/MainWindow.cpp:257-267
    and 1307-1310, src/main.cpp:392-411 and 533-541; CMakeLists insertion points
    read at CMakeLists.txt:163,183,366-369. [VERIFIED this session.]
12. Banner headline "Reusable phrases." = the existing MainWindow.cpp:1308 stub
    subtitle (zero invented copy); empty body. [RE-DERIVED.]
13. Sheet title copy "Add new snippet" (brief button copy) / "Edit snippet";
    empty-state copy "No snippets yet." (Dictionary family shape); trigger hint
    "What to say", expansion hint "What it types" (field labels "Trigger" /
    "Expansion"); sheet width 440 (Transforms editor precedent); counter
    "N / 4000"; toast copy 'Deleted "trigger"' with action "Undo".
    [RE-DERIVED from brief + sibling precedent.]
14. Row truncation implementation: code-point budget binary search over
    ImGui::CalcTextSize sub-ranges in the .cpp (no allocation per measure);
    "trigger → expansion" via U+2192 (Dictionary precedent for the glyph/arrow).
    [RE-DERIVED.]
15. Buffer capacities: trigger 1024 (Dictionary 256 was judged tight for
    sentence-length triggers), expansion 16384 (= 4000 max code points x 4-byte
    UTF-8 + slack, so a valid-length expansion never truncates).
    [RE-DERIVED from SnippetStore::kMaxExpansionChars.]
16. Example chips are display-only (pressed result ignored; no invented action).
    [RE-DERIVED; alternative (chip opens the editor) noted in §4.]

## 1. Root cause / entry points

There is no defect; this is a missing-page implementation. Entry points:

- [VERIFIED src/ui/MainWindow.cpp:1307-1310 this session] `MainWindow::DrawSnippetsPage`
  is still the Wave-3 stub (DrawPageChrome("Snippets", "Reusable phrases.") +
  "Not available yet."). Page enum Page::Snippets exists under `lwf::ui::pages`
  forward-decl scope conventions [VERIFIED src/ui/MainWindow.h:38,68-69 this session].
- [VERIFIED src/main.cpp:392-411 this session] production SnippetStore `snippets`
  is Loaded (path set), SetMode(snippet_mode), and handed to the pipeline by
  reference. The page must hold a `SnippetStore&` to that shared instance and
  `Save()` after mutations, so the pipeline observes edits by construction.
  No pipeline change.
- [VERIFIED src/ui/MainWindow.h:193-196 this session] sibling page pointers are
  `std::unique_ptr` members injected by additive setters; main.cpp:534-541
  constructs them after MainWindow construction. Snippets wiring mirrors this
  exactly in §7.

## 2. Affected files / consumers

- New: src/ui/pages/SnippetsPage.h (portable state + helpers + page class decl;
  NO ImGui in the header), src/ui/pages/SnippetsPage.cpp (Draw only),
  tests/test_snippets_page.cpp (doctest; instantiates the page class and
  SnippetStore only — no ImGui link, same as Dictionary/Transforms siblings).
- Consumers: MainWindow::DrawSnippetsPage (NEED), main.cpp SnippetStore wiring
  (§7 NEEDs). TextPipeline reads the same SnippetStore instance (no change).
- SnippetStore JSON shape untouched (page never writes snippets.json directly;
  all writes via store Save() = tmp+rename+.bak). Matching semantics untouched
  (page calls store Expand/Normalize semantics only through the store).

## 3. Invariants preserved

- SnippetStore v2 JSON shape + whole/contains matching semantics untouched.
- No code comments in new sources (AGENTS.md); no windows.h/winhttp.h; no cloud
  endpoints; PushFont(nullptr, ...) only, never SetWindowFontScale; keyboard nav
  via widget return values + EnableNav items (InvisibleButton rows, IconButton,
  Chip, LightButton) and input fields — no IsItemClicked anywhere.
- No secrets in code or logs (page logs nothing; not even Log::).
- 4000 cap counts code points in the store; the page never hardcodes a byte cap
  (counter uses SnippetStore::kMaxExpansionChars).
- Sheet Esc recovery: if the Sheet closes (Esc) while editing, DrawSheet closes
  the editor state (Transforms precedent), so editor state never desyncs from
  the widget.
- Result of Save() is deliberately ignored (inherited decision 8); in-memory
  state stays authoritative for the session.

## 4. Technical decisions (numbered for review)

- D-1 Namespace `lwf::ui::pages` (Dictionary verbatim), not a new
  `lwf::ui::snippets` — integrator fwd-decl/setter/member lines in §7 mirror
  Dictionary's.
- D-2 Banner headline "Reusable phrases." (existing stub subtitle) with empty
  body. Zero invented copy. Chips below the banner box, Dictionary-chip-row
  shape: Chip("my booking link") + arrow + Chip("https://example.com/book"),
  both unselected (glass).
- D-3 Undo model: immediate RemoveById + Save on delete; single-slot stash holds
  the removed Entry; Undo = Upsert(trigger, expansion) + Save (new id + fresh
  timestamps; spec does not require id stability). Second delete replaces the
  stash; the first entry stays deleted. Expiry (5s) clears the stash only —
  the delete itself is already committed. Alternative (Dictionary pending-delete
  hide-without-removing) was considered and rejected to stay faithful to the
  recovered report's accepted design (item 7 above); id churn is the known cost.
- D-4 Save-failure silence (inherited): mutations honor Save() as the last step
  but keep working if it fails (e.g. no Load first -> empty path). Risk: user
  believes a snippet was saved when only memory changed. Mitigation: in production
  the store is Loaded (path set) before the page exists; a test pins Save(Path)
  round-trip. Follow-up candidate: surface "Could not save snippets." like
  Dictionary's SubmitAdd — proposed as an integrator follow-up, not implemented
  this run because it would change the accepted helper contract.
- D-5 Pencil U+270E retained for the row edit action (brief wording, inherited).
  Default-font glyph coverage is unverifiable in this environment -> flagged in
  the S1+S2 manual checklist (§8) for Devansh. Fallback if tofu: ASCII glyph or
  frontend font addition (out of write-set).
- D-6 Editor submit order: SnippetStore::Validate first (exact messages);
  on non-empty, show it and keep the sheet open (never reach Upsert's
  empty-expansion-deletes path). On empty, Upsert then Save; close on success.
- D-7 Expansion counter "N / 4000" under the multiline field (Transforms
  precedent) so the cap is visible without trial and error.
- D-8 No search box (Dictionary has one; the brief does not ask for search and
  no Snippets-page search copy is specified — not invented).
- D-9 Chips are display-only: Chip press results are ignored (no invented
  action). Feels inert but invents nothing; alternative (chip pre-fills the
  editor) needs copy/behavior spec first.
- D-10 Delete lives in the sheet (edit mode) AND on the row X button. Row X
  and sheet Delete share RequestDelete + toast + CloseEditor.

## 5. Expected behavior / tests

- Banner: headline "Reusable phrases."; glass chips
  "my booking link" -> "https://example.com/book"; close X hides the banner
  (banner_closed).
- LightButton "Add new snippet" opens the sheet empty.
- Rows: "trigger -> expansion" (U+2192), truncated with "..." at the row width;
  hover/focus shows pencil edit + X delete IconButtons; row press opens the
  editor for that snippet; pressing Enter on the trigger field submits.
- Sheet: trigger single-line, expansion multiline with 6 visible rows; counter
  "N / 4000"; validation errors shown exactly (4 spec messages); Cancel closes
  without mutating; Save persists through the shared store; sheet Delete
  deletes+undo-toasts; Esc closes and discards.
- Delete (row X or sheet Delete): entry removed immediately + Save; toast
  'Deleted "trigger"' with "Undo" for 5s; Undo restores trigger+expansion;
  after 5s the stash is dropped.
- Tests (18 cases planned): banner copy; row text + code-point truncation;
  4 exact validation messages via SubmitSnippet and via the editor SaveEdit
  flow; validation order (4001 spaces = "Snippet too long"; empty trigger beats
  empty expansion); editor open/prefill/close + add/edit through the page class;
  duplicate exact + edit-self exempt + rename onto another trigger rejected;
  longest match (contains); casing preserved + case-insensitive trigger;
  punctuation rules (whole "My Email!!" expands, contains "my, email" does not);
  delete undo round-trip incl. persisted file content; expiry 5s; second delete
  replaces the stash; visible-banner default-true; source-scan hygiene
  (SetWindowFontScale/windows.h/imgui.h/Log:: ban + copy presence pins).
- Manual checklist (S1+S2, Devansh hardware): see §8.

## Sessions Log
- [2026-11-07T20:05Z] START+CLAIM on COMMS. Report created (this file).
- [2026-11-07T20:14Z] Implementation written (3 files, this session, verified inherited
  against SnippetStore/Widgets/Overlays/StudioPalette this session).
- [2026-11-07T20:16Z] Standalone doctest: 18/18 cases, 159/159 assertions PASS.
- [2026-11-07T20:18Z] cmake --build build --config Release --parallel 4 --target lwf_tests:
  built (50% lwf_core, 100% lwf_tests) unchanged by this task (test not registered - NEED N-1).
- [2026-11-07T20:19Z] bash scripts/check_hygiene.sh: hygiene: OK.
- [2026-11-07T20:21Z] Code committed (3 files, explicit paths); NOTE: code commit
  095ef47 message lacks the Task:/Agent:/Wave:/Report: trailers (omit was my error;
  amend is forbidden by Standing Order 13, so left as-is and recorded here; docs
  commit below carries the trailers).

## 6. Verification Evidence

- [RAN standalone doctest, MinGW g++ 15.2, MSYS2] `g++ -std=c++17 -Isrc
  -I"D:\lwf\_deps-src\doctest" -I"D:\lwf\_deps-src\json\include" <main TU with
  DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN> tests/test_snippets_page.cpp
  src/core/SnippetStore.cpp src/core/Log.cpp -o test_snippets_page.exe -> compiled, exit 0`
  then `.\test_snippets_page.exe` from repo root:
  `[doctest] test cases: 18 | 18 passed | 0 failed | 0 skipped`
  `[doctest] assertions: 159 | 159 passed | 0 failed`
  `[doctest] Status: SUCCESS!`
  First link attempt without doctest impl TU failed (undefined doctest::String etc.);
  resolved by compiling doctest 2.4.12 header impl (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
  in a temp TU outside the repo) - no repo file changed for this. exe removed after run.
- [RAN cmake --build build --config Release --parallel 4 --target lwf_tests ->
  exit 0: `[ 50%] Built target lwf_core`, `[100%] Built target lwf_tests`] Shared
  build unaffected; new test NOT in this build (NEED N-1). SnippetsPage.cpp likewise not
  yet in any target (NEED N-2) - its ImGui TUs are therefore compile-unverified here.
- [RAN bash scripts/check_hygiene.sh -> `hygiene: OK`] (G4 green).
- NOT-RUN (needs shared env + integrator wiring): in-target lwf_tests run of
  test_snippets_page.cpp (needs N-1), Windows app build with SnippetsPage.cpp (needs N-2),
  G3 zig type-check (no zig on box), MSVC compile (CI only).
- [VERIFIED this session, read] widget signatures
  [src/ui/widgets/Widgets.h:14-31] match every call in SnippetsPage.cpp; Sheet/Toast API
  [src/ui/widgets/Overlays.h:20-45] match; SnippetStore API
  [src/core/SnippetStore.h:18-41] (kMaxExpansionChars, Validate/Upsert/RemoveById/Save/SetEntries/SetMode/Expand)
  matches all call sites. Transforms sheet pattern
  [src/ui/pages/TransformsPage.cpp:449-452,542-548] matches DrawSheet open/Begin/End/recovery.

## 7. NEEDs (integrator, outside write-set - nothing here was edited by me)

- N-1 CMakeLists.txt tests: add `tests/test_snippets_page.cpp` after line 163
  (`tests/test_dictionary_page.cpp`, alphabetical neighborhood:
  163 test_dictionary_page, 164 test_insights_page, 165 test_scratchpad_page,
  166 test_transforms_page - insert after 165 to keep _page grouping).
- N-2 CMakeLists.txt WIN32 sources: add `src/ui/pages/SnippetsPage.cpp` after line 369
  (`src/ui/pages/TransformsPage.cpp` at :369; pages alphabetical block :366-369).
- N-3 src/ui/MainWindow.h: add fwd-decl `class SnippetsPage;` next to line 19
  (`class DictionaryPage;`); add setter
  `void SetSnippetsPage(std::unique_ptr<lwf::ui::pages::SnippetsPage> page);`
  next to line 106 (`SetDictionaryPage`); add member
  `std::unique_ptr<lwf::ui::pages::SnippetsPage> snippets_page_;`
  next to line 193 (`dictionary_page_`).
- N-4 src/ui/MainWindow.cpp: define setter body next to line 257-258
  (`dictionary_page_ = std::move(page);` inside SetDictionaryPage) as
  `snippets_page_ = std::move(page);`; replace DrawSnippetsPage body (lines 1307-1310)
  with the Transforms/Dictionary shape:
  `if (snippets_page_) { snippets_page_->Draw(scale); return; }` followed by the
  existing DrawPageChrome("Snippets", "Reusable phrases.") + TextDisabled fallback
  (keep fallback verbatim). Include "ui/pages/SnippetsPage.h" next to the other page includes.
- N-5 src/main.cpp: after line 541 (SetScratchpadPage) add
  `full.SetSnippetsPage(std::make_unique<lwf::ui::pages::SnippetsPage>(snippets));`
  where `snippets` is the existing SnippetStore at main.cpp:392 (Loaded, path set,
  handed to pipeline at :411). Include "ui/pages/SnippetsPage.h" next to page includes.
- N-6 tests/test_overlay_settings_ui.cpp source-scan list (if it enumerates page files):
  add SrcPages entries for SnippetsPage.{h,cpp} if that scan covers them (check before
  assuming; sibling pages are covered there per AGENTS.md hotspot note).
- RISK follow-up (also D-4): Save() failure is silent. If integrator prefers surfacing it,
  add a Save-failure toast/error line (Dictionary SubmitAdd precedent) - spec change, not done here.

## 8. Manual checklist (S1+S2, Devansh on Windows hardware - NOT claimed verified)

- S1: Open Settings > Snippets. Banner shows headline "Reusable phrases." with glass chips
  "my booking link -> https://example.com/book"; banner X closes it for the session.
- S1: Click "Add new snippet"; sheet opens (440px) with Trigger hint "What to say" and
  Expansion hint "What it types"; expansion field shows 6 lines; counter under it shows
  "0 / 4000". Type trigger "my email", expansion "addr@example.com", press Enter in the
  trigger field -> saved, sheet closes, "Saved" toast, row "my email -> addr@example.com".
- S1: Row hover shows pencil (check glyph renders - U+270E tofu check, D-5) and X;
  pencil reopens the sheet prefilled; X deletes the row immediately and shows
  'Deleted "my email"' toast with Undo; Undo within 5s restores the row; after 5s Undo is gone.
- S1: Edit a snippet; clear the expansion and Save -> "Please enter a snippet" shown, sheet stays open.
- S1: Add a duplicate trigger (different casing) -> "Snippet already exists". Long paste >4000
  chars -> "Snippet too long" before any other message.
- S2: Restart the app with snippets.json present; rows reload; dictating "my email" types the expansion.
- S2: Keyboard-only: Tab to the row (focus ring), Enter opens the editor; Esc closes the sheet
  and discards edits; Tab reaches Add/Save/Cancel/Delete (no IsItemClicked anywhere).

## 9. Review audit (self-review of the 3 new files)

- Re-read all 3 files from scratch post-hoc against the brief and sibling precedents.
- CRITICAL: none. The ImGui TU (SnippetsPage.cpp) is not yet in any CMake target (NEED N-2),
  which is the single largest unverified surface; flagged, not hidden.
- MAJOR (1, accepted + documented): D-4 Save() result ignored - in-memory state can run ahead
  of disk if Save fails. Mitigated in production by the store being Loaded before the page
  exists; round-trip pinned by test. Follow-up proposed in N-6/RISK above.
- MINOR (1): D-9 example chips are display-only (pressed results ignored) - invents no action;
  needs spec before any chip behavior.
- MINOR (2): row pencil glyph U+270E font coverage unverifiable here - S1 checklist item;
  fallback is an ASCII glyph or frontend font (out of write-set).
- MINOR (3): `TruncateToWidth` PushFont uses hardcoded 15.0f design px matching the row text
  size (never scale-dependent beyond the font call itself); `scale` param voided - the truncation
  budget already accounts for scale at the call site. Visual width check is in the S1 checklist.
- Mutation mental check: deleting/renaming any of the 4 validation messages in
  SnippetStore::Validate fails the exact-string tests; breaking RowText's arrow, the 5s stash
  expiry, the second-delete-replaces-stash rule, or Save() persistence each fails a dedicated
  test. Matching behavior (longest/casing/punctuation) is pinned through the store, not re-implemented.
- Leftovers: test_snippets_page.exe (standalone build artifact) deleted before commit;
  temp doctest main TU lives outside the repo (TEMP) and is not committed. No debug prints,
  no TODO/FIXME (source-scan test pins this), no IsItemClicked (source-scan pins this).
- Independent review: the ImGui TU has not had a fresh-context reviewer in this environment;
  recommend the integrator's review pass cover SnippetsPage.cpp rendering paths (hover actions,
  sheet Esc recovery, toast timing) on Windows hardware per §8.
