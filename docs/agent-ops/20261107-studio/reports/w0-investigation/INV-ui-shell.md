# INV-ui-shell — W0-INV-03 (UI shell + settings surface)

## ORCH-DIGEST
- Plan C/D/E verified against code at feature/studio-pages @ 0e655ec; §C claims in plan hold with two corrections below.
- HIGH RISK: `quiet_speech_boost` toggle is a silent no-op (not in BuildSchema; SetBool early-returns, GetBool always false) — A4/D-OP4 must add it to schema before any control works.
- P17/P18 breaks are exact source-scan counts in tests/test_overlay_settings_ui.cpp:252-307 + 458-518 (listed §3); font_sources must grow in same commit or new pages silently escape the SetWindowFontScale ban.
- No modal/toast infra exists (0 BeginPopupModal/OpenPopup in src/); Saved/Copied are dwell labels only.
- A3 recorders need a target-key parameter — learn mode is a single global hardcoded to hotkey_ptt in main.cpp:579-585.

## 1. Question and scope
Verify plan P17-P24/A2-A4 UI-shell facts. Plan sections C/D/E read from external plan file (mission 00-mission.md:4), not in repo — [FACT] that path is the only copy. Branch [RAN git branch --show-current -> feature/studio-pages], HEAD 0e655ec matches mission baseline. No tests run (read-only investigator; baseline per mission: CI 42310536369 green).

## 2. Summary
- [VERIFIED] MainWindow shell, UiBus, schema-49, hotkey-learn, Overlay dirty_ all as plan §C describes. MainWindow.cpp is 996 lines, monolothic; pages are Draw*Page member fns dispatched by RenderPages switch.
- [VERIFIED] 14 distinct settings keys touched in MainWindow.cpp; 13 in schema, 1 (quiet_speech_boost) outside — and that 1 is functionally dead (see §6).
- [INFERENCE, high] A3 recorder reuse needs new state: BeginLearnMode/TryTakeLearnedBinding carry no target-key or role.

## 3. Source-scan test breakage at P17/P18 (exact)
File tests/test_overlay_settings_ui.cpp. REQUIREs that abort the case (vs CHECKs that fail one assertion):

Font mechanism test (239-307):
- font_sources = 7 files [VERIFIED :252-260]: MainWindow.cpp/.h, Overlay.cpp/.h, ThemePalette.h/.cpp, ThemeStyle.h. REQUIRE(file) per entry [:263]; REQUIRE(main_window) [:272]; REQUIRE(palette_cpp) [:281]; REQUIRE(palette_h) [:303].
- CHECKs broken by P17 rail/page split: PushFont(nullptr, metrics::font_ present in MainWindow.cpp [:275] — moving pages to src/ui/pages/*.cpp removes it. CHECK SetWindowFontScale absent in every font_sources file [:267] — new page/widget files NOT added to list escape silently (must extend same commit per plan §H).
- CHECKs broken by P18 palette/style work (ThemePalette.cpp): g_last_applied_context [:284], g_last_applied_scale [:285], "const bool applies =" [:286], "if (applies)" [:287], "g_last_applied_scale = scale;" [:288], "style.ScaleAllSizes(scale);" [:289], absence of "if (scale != 1.0f)" [:292], WindowRounding-before-ScaleAllSizes ordering [:296-299], "once per actual change" in ThemePalette.h [:306].

Keyboard/commit test (458-518):
- REQUIRE(main_window) [:469]; REQUIRE(nav_active) [:494], REQUIRE(focus_here) [:495], REQUIRE(cleanup="void MainWindow::DrawCleanupPage") [:508], REQUIRE(welcome="void MainWindow::DrawWelcomePage") [:514]. Renaming/moving those fns aborts.
- CHECKs broken by P17: NavEnableKeyboard [:483]; EnableNav count >=2 [:484]; "const bool pressed =" [:487]; "InvisibleButton(item.id" [:488]; IsItemClicked absent [:489]; NavActive<SetKeyboardFocusHere [:492-496]; page_focus_pending_=true count ==2 [:499] (sites MainWindow.cpp:192 SetVisible, :991 Welcome-Finish — any new arming site breaks); copied_until_ [:501]; "\"Copied\"" [:502]; IsItemDeactivatedAfterEdit >=3 [:504] (sites :718,848,859); no "if (ImGui::SliderInt(" at/after DrawCleanupPage [:507-509]; validation_error_.clear() >=2 [:512]; ApplyAndSave() >=2 in Welcome substr [:515] (sites :949,:987); stt_provider present [:517].

Adjacent scans P18 must also respect (Overlay/palette untouched per plan, but widgets share files):
- Palette tokens exact [VERIFIED :176-191] kDarkPalette bg 0x0E1714 etc.; contrast >=4.5 [193-216]; WCAG numbers [218-224] e.g. 14.67/13.23/7.35/13.09; font tokens [226-237] wordmark 18/title 28/card 15/body 13/mini 8.5; mini alphas + saved_dwell==1.5 [309-322]. P18 StudioPalette must not change kDark/kLight or saved_dwell (undo-toast 5000ms in plan P18 is a new dwell, not saved_dwell).
- Overlay scans [348-456]: Pack(kDarkPalette.muted, badge_alpha) [:357]; "* badge_alpha" count==1 [:373]; "badge_alpha * " absent [:374]; PushFont(message_font_,font_body_px) ordering Push<Calc<Add<Pop in DrawAttentionMessage [376-395]; dirty_=true >=3 [:447]; cached_status_valid_=false >=1 [:449]; poll_hover_dot_ [:452-453]; LeftMouseButtonDown [:454]; IsCursorInsideRect >=3 [:455].

## 4. Affected files and flows
MainWindow.h:73 Page enum {Welcome,Home,Speech,Cloud,Cleanup,Privacy,About} (7); :85 page_=Home; :55-63 DrawRail/DrawPageChrome/Draw*Page decls; :65-69 BeginCard/EndCard/ToggleRow/ApplyAndSave/ShowSaved; :88-89 saved_until_/copied_until_; :44 SetDetectedHotkey; :90 page_focus_pending_.
MainWindow.cpp:63-84 ToggleSwitch (file-local, anonymous ns, 52x32px, accent/surface, focus ring); :369-381 NavItem{id,label,page}+items[6] (Welcome not in rail); :351-472 DrawRail (InvisibleButton+EnableNav, R-W2-001 return-value switch :407-416); :325-349 RenderPages switch; :474-480 DrawPageChrome (font_page_title_px + subtitle); :482-496 BeginCard/EndCard (ChildBg surface, radius_card); :498-525 ToggleRow; :527-535 ApplyAndSave {settings_.Save(); bus_.PostCommand(ApplySettings); ShowSaved();} / ShowSaved {saved_until_=GetTime()+metrics::saved_dwell}; :308-315 Saved dwell draw; :690-694 Copied dwell (copied_until_=GetTime()+saved_dwell, cleared :210).
ApplyImGuiStyle: decl ThemePalette.h:169, def ThemePalette.cpp:35 (gated on (context,scale) pair :30-31,42-46); called MainWindow.cpp:159 (Init), :254 (DPI reapply); Overlay.cpp:125,319. kDarkPalette def ThemePalette.h:46-61, used ~34x in MainWindow.cpp (:36 using-decl); kLightPalette :64-79 has zero MainWindow.cpp uses [VERIFIED grep] — light tokens exist only for tests.
UiBus.h:10-19 UiCommandType {SetEnabled,OpenSettings,OpenHistory,OpenLogsFolder,ToggleAutostart,StartLearnHotkey,ApplySettings,Quit} (8, no StartTransform/OpenScratchpad). Consumer main.cpp:540-577 synchronous drain; switch has NO default case [VERIFIED :542-576] — new enum values without cases are silently ignored (and -Wswitch may warn). ReadStatus zero callers [VERIFIED grep src/: only UiBus.h:62 decl + UiBus.cpp:76 def]. Status publish path main.cpp:595+ PublishStatus.
Overlay.h:74-78 dirty_ idle early-out (Ready/Paused skip GL pass while !dirty_); setters [VERIFIED grep Overlay.cpp dirty_=true x10: :157,:162,:167,:175,:188,:199,:308,:533,:557,:578] — SetState/PushLevel/OnContentScale/UpdateDrag/poll. Any new interactive element (note button P16) must set dirty_ on hover/poll change or it freezes.

## 5. Patterns/conventions
Live-ref writes: every control does settings_.SetX + ApplyAndSave() (17 ApplyAndSave() sites [VERIFIED grep: def :527 + 16 calls :666-987]); commit-on-deactivate for text/sliders (IsItemDeactivatedAfterEdit :718,848,859); restart banners via restart_needed_+DrawRestartBanner (:537-543, set :722,790,811). One ImGui context per window; CreateContext return kept (MainWindow.cpp:144-148, AGENTS.md). No code comments rule (AGENTS.md) vs existing explanatory comments — new code must follow no-comments.

## 6. Interfaces/invariants + schema keys
BuildSchema 49 keys [VERIFIED Select-String SettingsStore.cpp count=49, lines 16-64]; unknown keys round-trip raw_unknown_ (:180-196, Save :193-202); SetNumber early-return on unknown key (:257-261); GetBool absent->false (:245-249).
Keys exposed in MainWindow.cpp:662-986 (+ reads elsewhere) — 14 distinct (excluding "local" literal): dictation_enabled (:446,581,662), hotkey_ptt (:588,628,948), stt_provider (:641,782), stt_consent_accepted (:642,808), cleanup_enabled (:668,837), double_tap_handsfree (:673), whisper_model_path (:713,721), whisper_language (:738,741,746), quiet_speech_boost (:760,762), cleanup_min_words (:844,852), cleanup_timeout_ms (:856,860), spoken_commands_aggressive (:863), debug_log_transcripts (:887,890,893), first_run (:218,986). All but quiet_speech_boost are in BuildSchema [VERIFIED grep SettingsStore.cpp quiet_speech_boost = no match].
CORRECTION to plan D-OP4 framing [FACT+INFERENCE high]: quiet_speech_boost is not merely "outside the 49" — SettingsStore::SetBool->SetNumber returns without storing for unknown keys, GetBool returns false, so the toggle at :760-764 never persists and main.cpp:476,561 always reads false into EngineSettings.quiet_speech_boost (EngineTypes.h:25). Adding the key to BuildSchema is a behavior fix, not just honesty. [UNKNOWN] whether any shipped config.json carries it as unknown-key (would still read false; only raw-preserved).
ApplySettings consumer main.cpp:557-569 is the only live engine re-read list (min_hold_ms, quiet_speech_boost, sound_enabled, streaming_*, spoken_commands_aggressive, snippet_mode, reduce_motion + LoadBindings). A4 keys consumed live must be added there or they apply only after restart.

## 7. Hotkey learn mode (A3 reuse)
WindowsHotkeyListener.h:31 BeginLearnMode(void) / :32 TryTakeLearnedBinding(std::string& out)->bool; defs WindowsHotkeyListener.cpp:297-302 (clears learned_keys, learned_ready=false, learn_mode=true) / :304-315 (returns learned_binding string, clears ready). Single global learn state, no target/role param [VERIFIED]. Main loop wiring main.cpp:554-556 StartLearnHotkey->BeginLearnMode(); :579-585 poll each frame -> SetString("hotkey_ptt") + Save + UpdateBindings(LoadBindings) + SetDetectedHotkey. Trigger UI MainWindow.cpp:630-632 "Learn a new chord..." posts StartLearnHotkey; Welcome consumes detected_hotkey_ :941-953. LoadBindings HotkeyBinding.cpp:177-222 reads 5 keys: hotkey_ptt (+legacy hotkey_code fallback :181-193), hotkey_handsfree, hotkey_cancel (default "Escape"), hotkey_command, hotkey_paste_last. No conflict-check API exists (HotkeyBinding only Parse/Serialize/IsSatisfied, HotkeyBinding.h:22-24) — A3 must add one. Reuse verdict: capture mechanism reusable; per-key targeting, pending-target state, and anti-conflict are new work.

## 8. Hidden coupling / risks (ranked)
1. quiet_speech_boost dead toggle (§6) — A4 must schema-add first, else control ships broken. Evidence above.
2. New UiBus commands silently dropped without a case (no default) main.cpp:542-576; and StartTransform must not block the drain (LocalLlamaProvider blocking per plan §C) — route to engine/worker.
3. font_sources silent gap (§3) — P18/P19-P23 files outside the list escape the SetWindowFontScale ban.
4. page_focus_pending_=true ==2 count pin [:499] — P17 new arming sites fail it; update same commit.
5. Overlay dirty_ freeze for new badge button (§4).
6. Modal/toast greenfield: 0 BeginPopupModal/OpenPopup/BeginTooltip/toast in src/ [VERIFIED grep, only tray Shell_NotifyIcon NotifyError WindowsTray.cpp:230 unrelated]; P18 must add infra + style (PopupBg/ModalWindowDimBg already in ApplyImGuiStyle ThemePalette.cpp:80,134 — partial head start).
7. [UNKNOWN] Whether plan's "13 exposed" count excluded first_run (welcome-owned) — my count is 14 incl. first_run+quiet; flag for T-a4 scan definition.

## 9. Test landscape / baseline
Existing: tests/test_overlay_settings_ui.cpp (518 lines, doctest, source-scan + palette + OverlayModel); runs via ctest in build [FACT AGENTS.md]. No baseline run by this investigator (read-only brief; mission records CI green). Missing: any test pinning Page enum count, NavItem list, ApplyAndSave call count (17 today), or per-key control coverage (plan T-a4 is new).

## 10. Unknowns
- Scratchpad screenshot/pixel palette (R-01, plan §L) — owner Devansh, F0/P01.
- IHttpTransport fit for OpenRouter (R-02) — C1 fake-transport test, not this domain.
- Whether SetDetectedHotkey single-string channel suffices for 5 recorders — design decision at A3 (needs pending-target state somewhere; MainWindow.h:95 only holds one detected_hotkey_).

## 11. Out-of-scope observations
None investigated. Adjacent: SettingsStore::AddListener/Notify (:325-342) exists but MainWindow never subscribes — live-ref direct Get/Set is the pattern (D-OP1).

## 12. Scratch files created
None.
