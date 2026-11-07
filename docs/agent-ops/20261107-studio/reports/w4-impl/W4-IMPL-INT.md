# W4-IMPL-INT — Wave 4 integration (T-016..T-025)

## ORCH-DIGEST
- Status: COMPLETE (code + full gate GREEN; Snippets deferral is an
  upstream missing-page blocker owned by W4-IMPL-S, not an integration
  failure; independent review recommended for the UI seams — see below).
- Branch: feature/studio-pages. Wave 4 code landed in 67c955b (T-016),
  fc96356 (T-019), acfaf21 (T-020), 96fb22c (T-018), acc7c5c (A2),
  5a2a618 (A4); A3 code was uncommitted and is now committed as a2cbf64
  (this pass, explicit paths). Integrator code is 76cf674 (explicit
  paths). Gate: docs/agent-ops/20261107-studio/gates/gate-w4-1.md GREEN
  (configure + build + ctest 23/23 in 679.66s + hygiene + no-network).
- Write-set respected: CMakeLists.txt, src/ui/MainWindow.{h,cpp},
  src/main.cpp, tests/test_overlay_settings_ui.cpp (font_sources + T-a4
  only), gate + this report. Forbidden paths untouched except the
  explicit A3 commit in step 1.

## 1. Audit

- [RAN git status --porcelain + git show --stat] Wave 4 commits present:
  67c955b src/ui/pages/DictionaryPage.{h,cpp} + tests/test_dictionary_page.cpp;
  96fb22c src/ui/pages/InsightsPage.{h,cpp} + tests/test_insights_page.cpp;
  fc96356 src/ui/pages/TransformsPage.{h,cpp} + tests/test_transforms_page.cpp;
  acfaf21 src/ui/pages/ScratchpadPage.{h,cpp} + src/ui/ScratchpadWindow.{h,cpp}
  + tests/test_scratchpad_page.cpp; acc7c5c src/os/FileDialogs.{h,cpp} +
  MainWindow Browse wiring; 5a2a618 (A4) SettingsStore schema + MainWindow.
- Snippets page MISSING: `git log --all -- src/ui/pages/SnippetsPage.*`
  is empty; no SnippetsPage.* in the tree, no test_snippets_page.cpp.
  W4-IMPL-S-T-017.md is still IN PROGRESS with no committed code
  (Sessions Log PENDING). `Page::Snippets` routing/rail already exist
  from Wave 3, so the stub stays reachable. Owning agent: W4-IMPL-S.
  No CMake reference was added (would break configure).
- A3 uncommitted files: YES at start — src/core/HotkeyBinding.{h,cpp},
  src/core/UiBus.h, src/os/WindowsHotkeyListener.{h,cpp},
  src/ui/MainWindow.cpp (DrawGeneralHotkeyRow + 5-row rewrite only).
  Per A3 report Git State, A4's explicit-path commit 5a2a618 already
  swept A3's MainWindow.h decls/setter defs under T-025 trailers;
  verified present in HEAD (SetLearnedHotkey/DrawGeneralHotkeyRow/
  learn_target_). The remaining uncommitted A3 hunks are committed
  here, explicitly, as a2cbf64 (6 files, trailers Task: T-024 /
  Agent: W4-IMPL-A3 / Wave: 4). No A3 file was left uncommitted.

## 2. CMakeLists.txt

- WIN32 `target_sources(local-wisper-flow)`: added
  src/os/FileDialogs.cpp, src/ui/ScratchpadWindow.cpp,
  src/ui/pages/DictionaryPage.cpp, src/ui/pages/InsightsPage.cpp,
  src/ui/pages/ScratchpadPage.cpp, src/ui/pages/TransformsPage.cpp.
- `add_executable(lwf_tests)`: added src/os/FileDialogs.cpp,
  tests/test_dictionary_page.cpp, tests/test_insights_page.cpp,
  tests/test_scratchpad_page.cpp, tests/test_transforms_page.cpp.
- Skipped with cause: tests/test_snippets_page.cpp and
  tests/test_filedialogs.cpp do not exist (see Audit / A2 §Verification;
  registering a missing path would break configure). A3 needs no new
  test file (cases append to existing tests/test_hotkey.cpp, which is
  outside the integrator write-set — deferred to orchestrator).
- FINDINGS sweep: A4's link blocker (undefined FileDialogs::
  PickModelFile/NormalizeModelPath) is resolved by this edit — the
  full `local-wisper-flow` target links in gate-w4-1.

## 3. MainWindow wiring

- Page enum + rail + last_page already contained Dictionary/Snippets/
  Transforms/Scratchpad/Insights from Wave 3 (MainWindow.h:26,
  RouteName/PageForRoute); no enum/rail change needed. Snippets
  routing likewise untouched (stub).
- DECISION (setter injection, not the ctor extensions proposed in
  D/I NEEDs): the briefs proposed extending the MainWindow ctor with
  shared stores. That couples the shell to 4 stores + an engine diff
  adapter in one ctor and breaks the existing 4-arg call site for no
  isolation gain. Integrator adds additive setters
  (SetDictionaryPage/SetInsightsPage/SetTransformsPage/SetScratchpadPage)
  taking std::unique_ptr; MainWindow.h forward-declares the pages and
  keeps the 4-arg ctor. INVARIANT: the same store instances main.cpp
  owns are injected (D-OP2 shared dictionary; stats/transforms/notes
  singletons), so pipeline/engine observations are shared by
  construction. Forward-declare keeps MainWindow.h imgui-free for
  lwf_tests.
- Draw routes delegate whole-page: DrawDictionaryPage ->
  `dictionary_page_->Draw(scale)`, etc. (ctor signatures verified:
  DictionaryPage(DictionaryStore&, PersonalDictionary*),
  InsightsPage(StatsStore&),
  TransformsPage(TransformStore&, UiBus*, ITransformDiffSource*),
  ScratchpadPage(NotesStore&, SettingsStore&, UiBus&)).
  Fallback preserved: when the page pointer is null, each route draws
  its original chrome + "Not available yet." — the Wave-3 stub count
  test (== 5) stays green and null pages degrade instead of crashing.
  Snippets stub intentionally unchanged (no page exists).
- Settings General entry points: page/rail navigation is the existing
  Wave-3 rail (##nav_dictionary etc. + Settings tabs); no new Settings
  entry was specified for these pages, and none was invented.

## 4. main.cpp

- A3 target-key routing applied verbatim from W4-IMPL-A3 §§NEED:
  StartLearnHotkey resolves the payload via
  UiBus::ParseStartLearnTarget, drops unknown targets with a Log::Warning
  (never redirects to ptt), and calls hotkey.BeginLearnMode(target).
  The learned poll uses TryTakeLearnedBinding(chord, target), validates
  via HotkeyBinding::ValidateRecorderChord against the pre-save
  LoadBindings registry (self-role excluded), and on success saves,
  refreshes the hook + TransformStore, and calls
  full.SetLearnedHotkey(target, chord); failures surface via
  full.SetRecorderError(target, error). Clear already flows through
  ApplySettings — no extra wiring.
- Store seams (each NEED honoured): DictionaryStore constructed next to
  the engine PersonalDictionary (dictionary_store on the same
  dictionary.json; DictionaryPage::Draw loads once, no Load call);
  StatsStore usage_stats passed to InsightsPage (Generation()-driven
  refresh needs no polling seam); TransformsPage receives the shared
  TransformStore, &ui_bus (StartTransform post), and an
  lwf::ui::EngineDiffSource over the engine (Has/ Snapshot forwards,
  null-safe).
- Scratchpad lifecycle per W4-IMPL-N NEED-3 exact lines: ScratchpadWindow
  constructed after NotesStore and Init'ed (warning-only on failure);
  ScratchpadRouteInjector wraps inject- -> buffer/flag BEFORE engine
  construction; DictationEngine takes scratchpad_router (CommandMode
  controller stays on the raw injector per NEED); OpenScratchpad drains
  to OpenNewNote/OpenNote (full.SetVisible no longer called);
  per-tick SetScratchpadTarget(IsDictationActive()) + RenderFrame +
  TakeCloseRequest after the full->overlay render order; Shutdown after
  overlay.Shutdown() and before full.Shutdown(). Early Init-failure
  return is safe (ScratchpadWindow dtor calls Shutdown). ScratchpadPage
  injected via SetScratchpadPage.
- RunEngine verified dead: single definition at main.cpp:203, zero call
  sites after this pass (grep for `RunEngine(`). No engine thread
  started by the integrator.

## 5. Tests

- font_sources (tests/test_overlay_settings_ui.cpp:254) extended with
  the 10 new UI files (4 pages x .h/.cpp + ScratchpadWindow x2),
  W3-FIX-SCAN pattern. studio_sources deliberately NOT extended (outside
  the font_sources + T-a4 write-set); coverage for the same ban is
  retained via font_sources. Recorded as a deferred follow-up, not a
  failure.
- T-a4 appended verbatim in substance from the A4 NEED (every-schema-key
  control/carve-out scan + new-keys default/clamp case). One mechanical
  fix required: doctest forbids `||` inside CHECK (build failure at
  test_overlay_settings_ui.cpp:863), so the scan was rewritten as a
  manual carve-out loop with a conditional CHECK. Added <algorithm>
  for completeness (no std::find remains — harmless). Mutation
  intent preserved: removing any carve-out or control literal fails.
- A3 source-scan suggestion (DrawGeneralHotkeyRow / Record## / Clear##
  markers) and A3 test_hotkey.cpp cases NOT added — outside the
  integrator write-set (test_overlay_settings_ui.cpp font_sources +
  T-a4 only; tests/test_hotkey.cpp read-only). A3's scratch harness
  evidence (44 checks) stands; committed-test registration is deferred
  to the orchestrator.

## 6. Gate + review audit

- Full gate GREEN — see gate-w4-1.md for verbatim commands, part/whole
  times, and ctest (23/23, 679.66s within the 40-min budget).
- Self-review before commit: diff re-read from scratch (5 code files,
  no forbidden paths staged; COMMS.md left for ORCH); null-fallback
  Draw routes keep stub count at 5; one fix during the gate (doctest
  || rewrite) was rebuilt and re-run to green, not assumed. No
  `//`/TODO/windows.h/SetWindowFontScale/page_focus/validation_error_
  additions in integrator hunks. Hygiene + no-network OK.
- Verification evidence: [RAN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  -> EXIT 0] [RAN cmake --build build -j4 -> BUILD_EXIT 0, 100% Built
  local-wisper-flow + lwf_tests; only pre-existing gRPC empty_bases
  warnings] [RAN ctest --test-dir build --output-on-failure -> 100%
  tests passed out of 23, 679.66s] [RAN bash scripts/check_hygiene.sh ->
  hygiene: OK] [RAN bash scripts/check_no_network.sh -> no-network: OK].
  NOT RUN: zig G3 (unavailable); Windows visual/manual checklists
  (Devansh hardware); cold full build (incremental, cached deps).
- Unverified assumptions: MinGW build implies MSVC/CI link (A-001);
  Scratchpad dictation routing/drain race and Modal Esc imbalance are
  the pre-existing W4/W3 findings, not introduced here; page Draw
  geometry needs the manual screenshot pass.
- Independent review: recommended (fresh-context read of MainWindow
  setters, main.cpp scratchpad context ordering, and the 4 page Draw
  delegations). Status is COMPLETE for the integrator scope with the
  Snippets deferral owned upstream; if ORCH classifies the shell
  wiring as High Risk it should be treated as REVIEW_REQUIRED.

## 7. Git state

- Branch feature/studio-pages. Commits by this agent:
  a2cbf64 A3 (explicit 6 A3 paths, trailers Task: T-024 /
  Agent: W4-IMPL-A3 / Wave: 4) — A3 MainWindow.h decls were already in
  5a2a618 (A4 sweep), not re-committed;
  76cf674 integrator code (explicit 5 paths, trailers
  Task: T-016..T-025-integration / Agent: W4-IMPL-INT / Wave: 4).
  Docs commit (this report + gate) follows as the second commit.
  COMMS.md (42-line A3 append) and untracked briefs/reports
  (W4-IMPL-S-T-017.md etc.) intentionally NOT staged (ORCH-owned).
