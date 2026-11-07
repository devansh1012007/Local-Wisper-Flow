# W4-IMPL-A4 T-025 — All-key settings GUI (plan A4)

## ORCH-DIGEST
- STATUS: COMPLETE (code complete; test-source registration for T-a4 is a NEED for the integrator — tests/ is READ-ONLY in the A4 write-set; integration build blocked on the pre-existing A2 CMakeLinks NEED listed below).
- Branch: feature/studio-pages (verified [RAN git branch --show-current])
- Task: T-025 (plan A4) — every BuildSchema key now has a control (54 keys after adding quiet_speech_boost + llm_provider) or a documented D-OP3 carve-out, across the 6 Settings tabs (General/Speech/Cloud/Cleanup/Privacy/About) per D11/D6; Privacy "Delete all local data" (stats/dictionary/snippets/transforms/notes/api_keys removed after two-step confirmation; config.json kept — spec S-P1 + brief).
- Write-set: src/core/SettingsStore.{h,cpp} (schema-additive only, 2 appended entries), src/ui/MainWindow.{h,cpp} (settings controls only). READ-ONLY: everything else untouched. Forbidden files untouched (CMakeLists.txt, scripts/, src/ui/pages/, widgets/, src/os/).
- Sibling conflict: A3 also edits MainWindow.cpp (General tab recorder rows). CONFLICT declared early; A4 hunks avoid ##card_general_hotkey; sound_volume slider sits inside ##card_general_sound (declared).
- Verified: [RAN cmake --build ... --target local-wisper-flow -> MainWindow.cpp compiles clean, link fails ONLY on A2's FileDialogs symbols pending integrator CMake NEED (same as W4-IMPL-A2 report)] [RAN cmake --build ... --target lwf_tests -> 100% Built] [RAN lwf_tests.exe filtered (settings/schema/clamp/diff/last_page/nav/font cases) -> 21 cases 262 asserts PASS] [RAN validators -> 1 case 7 asserts PASS] [RAN bash scripts/check_hygiene.sh -> OK] [RAN bash scripts/check_no_network.sh -> OK] [RAN every-key scan -> 54/54 quoted literals present in MainWindow.cpp].
- Commits: code first (explicit paths) then docs-only report (explicit path). SHAs appended in Git State. Trailers: Task: T-025 / Agent: W4-IMPL-A4 / Wave: 4.

## Context Report (pre-change investigation)
### 1. Root cause / entry points
- [VERIFIED src/core/SettingsStore.cpp:16-67] BuildSchema = 52 entries. [VERIFIED grep] 14 distinct keys referenced in MainWindow.cpp; of those, quiet_speech_boost is NOT in schema (dead no-op: SetBool early-returns SettingsStore::SetNumber :262-264, GetBool false :248-252), so the toggle at MainWindow.cpp:852-856 never persists. D-OP4 requires schema registration.
- llm_provider is not in schema (workstream C registers it in C1 wave 5 per plan E; brief requires A4 register additive `local` default so C1 consumes it). [VERIFIED grep llm_provider src -> zero matches].
- Missing controls (verified by grepping MainWindow.cpp for settings_.Get/Set strings): hotkey_code, hotkey_handsfree, hotkey_cancel, hotkey_command, hotkey_paste_last, double_tap_window_ms, min_hold_ms, max_recording_ms, hands_free_silence_ms, avoid_altgr_conflict, vad_threshold, cleanup_language_policy, dictionary_fuzzy, snippet_mode, command_timeout_ms, use_remote, remote_endpoint, inject_delay_ms, streaming_stt, streaming_min_segment_ms, streaming_pause_ms, stt_language, stt_timeout_ms, stt_max_clips, sound_volume, history_enabled, keep_raw_in_history, history_max_entries, history_max_age_days, scratchpad_flow_bar, keep_usage_stats.
- snippet_mode consumers: SnippetStore::SetMode accepts "whole" (default) and "contains" [VERIFIED Src SnippetStore.cpp:349]. cleanup_language_policy has NO consumer [VERIFIED grep src -> only schema line]; expose as free text (no invented enum). stt_language: comma-separated ISO tokens per LanguageMap [VERIFIED LanguageMap.cpp:73-88]; expose as free text.
- [VERIFIED src/main.cpp:577-596] ApplySettings re-reads min_hold_ms, quiet_speech_boost, sound_enabled, streaming_*, spoken_commands_aggressive, snippet_mode, reduce_motion, keep_usage_stats, scratchpad_flow_bar. All A4 hot-apply keys are already either re-read or read at startup; no main.cpp edit needed (main.cpp is READ-ONLY for A4).
- Speech pickers (A2) committed; Cloud consent (W2) untouched by design (brief).

### 2. Affected files / consumers
- SettingsStore.cpp additive schema only. Consumers: ConfigManager, main.cpp (LoadBindings + ApplySettings), SettingsUiModel::ClassifyDiff iterates Schema() (two new keys classify hot_apply correctly by construction).
- MainWindow.{h,cpp} settings controls only. Consumers: main loop (RunOnce/ApplySettings posting), source-scan tests (N-OK pins preserved: page_focus_pending_ stays 2; IsItemDeactivatedAfterEdit only grows; no `if (ImGui::SliderInt(` outside helpers; NavActive<SetKeyboardFocusHere intact; PushFont markers intact).
- Delete-all: reads GetDataDir() (os/DataDir.cpp) and removes 6 paths. No config.json write. Stores keep in-memory copies until restart (UI helper text says so).

### 3. Invariants preserved
- Additive-only keys; unknown keys round-trip raw_unknown_ (schema edit is append-only at end of vector).
- ApplyAndSave = Save + PostCommand(ApplySettings) + ShowSaved — preserved for every new control.
- Hot-apply keys commit live; hot_apply=false keys (mic_device only among new controls) set restart_needed_=true -> DrawRestartBanner. Existing Speech/Cloud restart paths untouched.
- No code comments added (AGENTS.md). No TODO/FIXME/XXX in src/. windows.h stays out of src/ui (hygiene).
- Secrets: delete removes api_keys.json from disk; nothing logs keys/transcripts.

### 4. Risks / edge cases
- A3 concurrent MainWindow.cpp edit -> CONFLICT declared; A4 avoids hotkey card hunk; orchestrator sequences commits.
- Strings: InputText buffers seeded once from settings (model_path_buffer_ pattern) so in-progress typing is never clobbered.
- Double sliders: SliderFloat with schema bounds (clamp duplicated in SetNumber anyway).
- Delete-all: remove with error_code, tolerate missing files; config.json never touched.

### 5. Expected behavior / tests
- Every key: control in Settings tab or carve-out (D-OP3: mini_x/mini_y readout-only "set by drag", schema_version readout, first_run write-only). Plus last_page = rail nav (existing control). P1 manual checklist in report.
- Suggested T-a4 scan + clamp + ApplyAndSave tests in §NEED (integrator-owned tests/).

## Conflict declaration (A3)
- A3 owns General tab recorder rows (hotkey cards). A4 defers hotkey chord text inputs to SEPARATE card below General cards (##card_general_other_hotkeys) and never edits ##card_general_hotkey / ##card_general_sound / ##card_general_motion / ##card_general_window block structure except adding sound_volume slider INSIDE sound card (disjoint line range, declared here). A3 must not touch the new sliders/StringField helpers at MainWindow.cpp anonymous namespace.

## Implementation Log
(entries appended as phases complete)

### Phase 0 — Investigation (read-only)
- Read brief order: STANDING-ORDERS, AGENTS.md, 00-mission, 01-work-ledger, plan §§A4/E/H/I + D6/D11/D-OP3/D-OP4, INV-ui-shell, W3-IMPL-S1 report, SettingsStore.{h,cpp}, MainWindow.{h,cpp}, A3 brief, A2 report, SettingsUiModel/ConsentGate/ApiKeyStore/DataDir/Overlays headers, test_overlay_settings_ui.cpp (source-scan pins 467-527, 591-673, 770-839).
- [VERIFIED] Schema = 52 keys; MainWindow referenced 14 distinct keys; quiet_speech_boost outside schema = dead no-op (SetBool early-return, GetBool false).
- [VERIFIED] snippet_mode values: "whole"/"contains" (SnippetStore.cpp:349); cleanup_language_policy has NO consumer anywhere (schema-only), so free-text control, no invented enum; stt_language = comma-separated ISO list (LanguageMap.cpp); use_remote/remote_endpoint = legacy gRPC ProviderRouter (ConfigManager 38-43).
- [VERIFIED] main.cpp:577-596 ApplySettings re-reads quiet_speech_boost (585) → hot-apply live for the revived toggle; all other new hot-apply keys are re-read or read at startup — no main.cpp edit required (read-only).
- Posted COMMS START + CLAIM + CONFLICT (A3) + NEED (tests) before editing.

### Phase 1 — Schema (SettingsStore.cpp ONLY)
- Appended 2 entries at end of BuildSchema vector (additive-only, order-stable): {"quiet_speech_boost", Bool, 0, hot_apply true}, {"llm_provider", String, "local", hot_apply true}. DECISION: llm_provider hot_apply=true (read by C1 provider selection; harmless: whitelist ignore in today's main.cpp reader set — unknown-key round-trip never applies since it's schema now, other builds just ignore an extra key via raw_unknown_ on downgrade). No .h change, no other keys touched; raw_unknown_ preservation untouched by construction.
- NOTE on the plan's D-OP4 framing: INV-ui-shell correction verified — the entry is a behavior fix (toggle dead before), not merely honesty.

### Phase 2 — MainWindow.h member state
- <cstdio> include (snprintf in TextField::Seed).
- delete_data_armed_ (two-step delete confirm); 8 TextField members (hotkey_handsfree/cancel/command/paste_last, remote_endpoint, stt_language, cleanup_language_policy, mic_device) — each a seeded-once 512-byte buffer mirroring model_path_buffer_ pattern so in-progress typing is never clobbered.
- Private helpers: IntRow/DoubleRow/BoolRow/StringFieldRow + DeleteLocalData() decl.
- AMBIGUITY (early build failure, resolved): StringFieldRow was declared with a trailing `float scale` param in the header while the definition took 4 args, and TextField was declared below the .cpp definition's access scope in one TU — fixed by removing the unused param and keeping struct above methods; rebuild clean. Reported honestly: first app build attempt failed with that compile error (no band-aid; arity/declaration-order fix).

### Phase 3 — Shared helpers (MainWindow.cpp, after ApplyAndSave)
- #include <filesystem>.
- IntRow: SliderInt full schema range + IsItemDeactivatedAfterEdit commit + ValidateInt + SetInt + ApplyAndSave. DoubleRow: SliderFloat + commit + SetDouble + ApplyAndSave. BoolRow: ToggleRow + SetBool(!v) + ApplyAndSave. StringFieldRow: Seed-from-settings + InputText + deactivated commit (+restart_needed_=true when hot_apply=false, e.g. mic_device) + ApplyAndSave. DeleteLocalData(): std::filesystem remove() on stats.json, dictionary.json, snippets.json, transforms.json, notes/ (remove_all), api_keys.json under GetDataDir(); error_code, missing files tolerated; config.json never touched.
- INVARIANT: every helper commits through existing ApplyAndSave() (Save + PostCommand(ApplySettings) + ShowSaved) — hot-apply vs restart banner semantics preserved.

### Phase 4 — Speech tab
- ##card_listen extended: vad_threshold (DoubleRow 0.0-1.0 "sensitivity %.2f"), min_hold_ms (50-5000), max_recording_ms (1000-120000), hands_free_silence_ms (0-10000), double_tap_window_ms (100-2000), avoid_altgr_conflict (BoolRow). quiet_speech_boost toggle now schema-backed (no UI change needed).
- New ##card_streaming: streaming_stt (BoolRow), streaming_min_segment_ms (1000-30000), streaming_pause_ms (300-5000). Both are in the main.cpp ApplySettings re-read list (587-589) → hot-apply live.

### Phase 5 — Cloud tab
- Provider card extended (A2 pickers + W2 consent untouched): stt_language (StringFieldRow free text), stt_timeout_ms (1000-60000), stt_max_clips (1-200).
- New ##card_remote_engine: use_remote (BoolRow "Use remote engine", helper "Send audio to a local gRPC engine") + remote_endpoint (StringFieldRow). Legacy path honestly labelled, not hidden.

### Phase 6 — Cleanup tab
- Extended ##card_cleanup: dictionary_fuzzy (BoolRow), command_timeout_ms (500-60000), inject_delay_ms (0-500), cleanup_language_policy (StringFieldRow free text — no consumer exists, enum would be invented), snippet_mode (Combo whole/contains + Selectable parity).
- New ##card_cleanup_backend: llm_provider combo {local, openrouter} + warn line when openrouter ("Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.") — the local-first promise amendment disclosure (plan J). Mirrored on Privacy and About cards.
- Existing min_words/timeout sliders kept byte-identical (scan pins: no `if (ImGui::SliderInt(` anywhere — helpers hold the SliderInt calls).

### Phase 7 — Privacy tab + delete
- Extended ##card_privacy: history_enabled, keep_raw_in_history, history_max_entries (10-100000), history_max_age_days (1-3650), keep_usage_stats (BoolRows/IntRows) + openrouter warn (duplicate per plan J on Privacy).
- New ##card_delete_data: Button "Delete all local data" → armed state (error-colored "This cannot be undone…" + Confirm delete + Cancel). Confirm → DeleteLocalData() + ShowSaved(). Helper text enumerates exactly what is removed (statistics, dictionary, snippets, transforms, notes, saved API keys) and that settings are kept. [MANUAL checklist P1 below.]

### Phase 8 — About tab
- Added openrouter warn (stated on About per plan J) + schema_version readout ("Config version: %d"). first_run + mini_x/mini_y carve-outs documented below (no controls by design D-OP3).

### Phase 9 — General tab
- ##card_general_sound: sound_volume (DoubleRow 0.0-1.0) added; mic_device upgraded from readout to editable StringFieldRow (restart=true — schema hot_apply=false → restart_needed_ set; helper "Empty means the system default device. Applies after restart.").
- ##card_general_window: mini_x/mini_y readout kept + "Set by dragging the mini window." (D-OP3 carve-out surfaced in UI) + BoolRow scratchpad_flow_bar ("Scratchpad bar") — surfaced here because it targets the mini-window surface that card owns (main.cpp:595 re-reads it).
- New ##card_general_other_hotkeys (A3-declared separation): hotkey_handsfree/cancel/command/paste_last StringFieldRows (free-text chords; A3 recorder rows replace this card organically when they land — both read/write the same keys, restart=false since all 4 are hot_apply=true).
- New ##card_general_legacy: hotkey_code IntRow 0-255 ("legacy key code %d").

### Phase 10 — Coverage scan (each key exactly once accounted)
All 54 schema keys verified present as quoted literals in MainWindow.cpp (manual script run: missing=0, total=54). Distribution:
- General (12): hotkey_ptt*, hotkey_handsfree, hotkey_cancel, hotkey_command, hotkey_paste_last, hotkey_code, sound_enabled*, sound_volume, mic_device, reduce_motion*, mini_x (readout), mini_y (readout), scratchpad_flow_bar — (* = also on Home page). Carve-outs: mini_x/mini_y (drag) per D-OP3.
- Speech (14): whisper_model_path*, whisper_language*, llm_model_path*, quiet_speech_boost, vad_threshold, min_hold_ms, max_recording_ms, hands_free_silence_ms, double_tap_window_ms, avoid_altgr_conflict, streaming_stt, streaming_min_segment_ms, streaming_pause_ms, double_tap_handsfree* (Home quick settings — also a "control" for the scan; the key literally lives on Home, docs note in report: it is the hands-free enabling switch).
- Cloud (8): stt_provider*, stt_consent_accepted*, stt_language, stt_timeout_ms, stt_max_clips, use_remote, remote_endpoint.
- Cleanup (11): cleanup_enabled*, cleanup_min_words, cleanup_timeout_ms, cleanup_language_policy, spoken_commands_aggressive*, dictionary_fuzzy, snippet_mode, command_timeout_ms, inject_delay_ms, llm_provider.
- Privacy (9): debug_log_transcripts*, dictation_enabled* (Home; Privacy-relevant switch also read-only status elsewhere; counted via Home control), history_enabled, keep_raw_in_history, history_max_entries, history_max_age_days, keep_usage_stats.
- About (2 + carve-out): schema_version (readout per D-OP3), first_run (write-only carve-out — consumed by FirstRunNeeded + Welcome finish; no control by design).
- last_page (1): rail navigation persistence (existing Wave 3 control).
Double-counting note: dictation_enabled/double_tap_handsfree/cleanup_enabled controls live on the Home page (Quick settings) — they satisfy "has a control" (plan D6 counts any control; brief: "every schema key gets a control or carve-out"), and duplicating them in Settings tabs would create two ToggleRow instances for one key (ImGui ID collision on the same page if co-located, and desync risk). Scan literal-match passes either way.

## NEED (integrator)
- tests/test_overlay_settings_ui.cpp: append T-a4 cases: (1) every schema key has a control or carve-out (list below, exact code in report phase log); (2) clamps honored (SetInt above max clamps, e.g. vad set via SetDouble path); (3) ApplyAndSave path preserved: count ApplyAndSave in each Draw*Page >= 1 where controls exist.
- Carve-out list for scan: mini_x, mini_y (readout "set by drag"), schema_version (readout in About), first_run (write-only; consumed by FirstRunNeeded + Welcome finish; no control by design D-OP3).

## NEED (integrator) — exact test code suggested (T-a4)
Append to tests/test_overlay_settings_ui.cpp (uses file's existing count_occurrences lambda pattern):

```cpp
TEST_CASE("every schema key has a control or a documented carve-out") {
    const std::vector<std::string> carve_outs = {"mini_x", "mini_y", "schema_version", "first_run"};
    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string content((std::istreambuf_iterator<char>(main_window)),
                              std::istreambuf_iterator<char>());
    for (const SettingsStore::SchemaEntry& entry : SettingsStore::Schema()) {
        const std::string key = entry.key;
        INFO(key);
        const bool carve_out =
            std::find(carve_outs.begin(), carve_outs.end(), key) != carve_outs.end();
        CHECK(carve_out || content.find("\"" + key + "\"") != std::string::npos);
    }
}

TEST_CASE("new A4 schema keys default and clamp through the store") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_a4_settings.json").string();
    {
        std::ofstream file(path, std::ios::trunc);
        file << "{}";
    }
    SettingsStore store(path);
    CHECK(store.HasKey("quiet_speech_boost"));
    CHECK(store.GetBool("quiet_speech_boost") == false);
    CHECK(store.HasKey("llm_provider"));
    CHECK(store.GetString("llm_provider") == "local");
    CHECK(store.IsHotApply("quiet_speech_boost"));
    CHECK(store.IsHotApply("llm_provider"));
    store.SetBool("quiet_speech_boost", false);
    store.SetString("llm_provider", "openrouter");
    store.Save();
    SettingsStore reloaded(path);
    CHECK(reloaded.GetBool("quiet_speech_boost") == false);
    CHECK(reloaded.GetString("llm_provider") == "openrouter");
    store.SetInt("min_hold_ms", 999999);
    CHECK(store.GetInt("min_hold_ms") == 5000);
    store.SetDouble("vad_threshold", 9.0);
    CHECK(store.GetDouble("vad_threshold") == 1.0);
    store.SetInt("hotkey_code", -4);
    CHECK(store.GetInt("hotkey_code") == 0);
}
```
Caveat (held): entry order in Schema() — last_page index stable; SetString("llm_provider") needs the schema entry (added) — if the test file is compiled before the schema lands the test fails, which is the desired red→green ordering (same-commit rule). The local scratch scan I ran instead — all 54 literals present, missing=0. The suggested scan asserts each control literal `"key"` appears in MainWindow.cpp source; my added controls place every key as a literal at the call site of IntRow/DoubleRow/BoolRow/StringFieldRow (a neutral grep scanning for the literal passes).

## Review Audit
- Correctness / root cause: dead-toggle root cause is schema absence (SetBool early-return). Fix adds the schema entries (root) + wires controls through existing ApplyAndSave (not a parallel path). Schema-keys completeness by construction: 54 literals present; slider bounds copy schema min/max verbatim (drift both ways bounded by SetNumber clamp + ValidateInt). PASS.
  - Severity check: all new values commit through the same Save + PostCommand(ApplySettings) path; schema hot_apply flags drive banner vs live semantics (mic_device sets restart_needed_). PASS.
- Security / edges / invariants:
  - Delete-all: filesystem removals constrained to GetDataDir() children, no user input; error_code-tolerant; config.json excluded (spec S-P1). Two-step confirm prevents accidental destructive click; cancel resets armed. Stored api_keys.json removed — documented in UI helper + report (brief-implied).
  - Secrets: no new log lines; deletion doesn't log key material.
  - Slider ids all unique (`##key`); ToggleRow labels unique per page (ToggleSwitch uses label as Id); TextField typed values can't be clobbered mid-edit (seed-once).
  - Source-scan pins hold: page_focus_pending_=true ==2; IsItemDeactivatedAfterEdit ==8 (>=3); no `if (ImGui::SliderInt(` anywhere in file; NavActive<SetKeyboardFocusHere intact; PushFont markers intact (new cards use same BeginCard/EndCard pattern).
- Maintainability / conventions:
  - Helpers IntRow/DoubleRow/BoolRow/StringFieldRow centralize the commit-on-deactivate idiom — 28 ApplyAndSave() literals now (was 17 incl. def); one-callsite-per-key mapping stays greppable.
  - No code comments added (AGENTS.md); no TODO/FIXME/XXX; windows.h-free src/ui (hygiene gate OK).
  - MINOR (accepted, documented): chords card is free-text until A3 recorder rows land; recorder rows will replace it organically (both write same keys). Separated card avoids A3 hunk conflict. No further action now.
  - MINOR: cleanup_language_policy is free text because no consumer exists — inventing today would fabricate an enum; documented in report §1. Accepted.
- Test rigor (mutation mentality):
  - Revert schema_add → quiet_speech_boost test fails (HasKey false, GetBool default) + llm_provider fails.
  - Revert any control call → scan literal missing → test fails.
  - Revert slider bounds beyond schema → clamp tests fail.
  Until tests exist as committed code (NEED), a manual equivalent substitutes: 54/54 literals present, bounds spot-verified.
- Final review: git diff confined to SettingsStore.cpp (2 lines) + MainWindow.{h,cpp} (+ COMMS orchestrator lines + this report; untracked sibling files untouched). No debug artifacts (scratch removed), no CMake/scripts/os/pages edits, no half-armed delete buttons. Backwards compat: additive keys only; old configs without the two keys load with defaults (false/"local"); unknown-key preservation path untouched; old build reading new config treats the two new keys as unknown and round-trips them.
- Independent review: High-risk? Change is Medium (internal UI + additive schema; no public API/serial format/DB/auth/billing breaking change). Per brief, self-review suffices; no separate reviewer dispatched by orchestrator yet. Status DEFERRED-DEPENDENCY (tests NEED + A2 CMake NEED for full-app link; same pattern as A2). If orchestrator classifies as High Risk, treat as REVIEW_REQUIRED — otherwise COMPLETE with NEEDs.

### Long-run test (Standing Orders 8)
- Source: root cause is missing schema keys + missing controls. Fixes address source (schema entries + control wiring via the one existing commit path). Good long-run shape (future keys get one IntRow/BoolRow line).
- Load: sliders render O(1); file IO only on edit-commit (same as before). No loops/queues added; delete removes 6 paths. Good.
- Hostile: malformed config (string where int expected) → existing Load defaults + clamp; unknown keys round-trip; delete tolerant of absent files; out-of-range slider values impossible (bounds) + clamped in store anyway. Good.
- Operability: failures surface as validation text / restart banner / ShownSaved; armed delete has explicit Cancel; scan literal list keeps completeness greppable. Good.
- Reversal: revert commit (3 files); new config keys are ignorable by older builds (raw_unknown_ round-trip) and defaulted on fresh load; no migration, no file format change.

## Verification Evidence (verbatim)
- [RAN git branch --show-current -> feature/studio-pages]
- [RAN cmake --build build --config Release --parallel 4 --target local-wisper-flow -> first attempt FAILED [EXIT:1]: MainWindow.cpp:587 error no declaration matches StringFieldRow (header declared trailing `float scale` param, definition took 4) + TextField private-in-context. Fixed arity + declaration order; rebuilt: MainWindow.cpp compiles CLEAN, link fails ONLY on: undefined reference to `FileDialogs::PickModelFile(...)` + `FileDialogs::NormalizeModelPath(...)` — pre-existing A2 CMake NEED (see W4-IMPL-A2 report; FileDialogs.cpp is not yet in CMakeLists). gRPC `empty_bases` warnings only (pre-existing).]
- [RAN cmake --build build --config Release --parallel 4 --target lwf_tests -> [100%] Built target lwf_tests, EXIT:0]
- [RAN build/lwf_tests.exe --test-case="*settings*,*schema*,*clamp*,*diff*,*last_page*,*main window keyboard*,*src ui font*,*studio navigation*" -> [doctest] test cases: 21 | 21 passed | 0 failed | 389 skipped; assertions: 262 | 262 passed | 0 failed; Status: SUCCESS! EXIT:0] (covers keyboard-nav pin, font mechanism, navigation, last_page, diff classification)
- [RAN build/lwf_tests.exe --test-case="validators*" -> 1 passed / 7 asserts SUCCESS]
- [RAN bash scripts/check_hygiene.sh -> hygiene: OK] + [RAN bash scripts/check_no_network.sh -> no-network: OK]
- Every-key scan (scratch script in build/, removed after run): scan done, missing=0 (total keys: 54); ApplyAndSave() literals: 28; page_focus_pending_ = true: 2; IsItemDeactivatedAfterEdit: 8; if (ImGui::SliderInt(: none anywhere.
- Slider bounds vs schema min/max cross-checked (14 IntRows + 2 DoubleRows): all identical.
- NOT RUN: full ctest (scoped mandate); zig G3 type-check (no zig on box, pending per AGENTS.md); visual verification on Windows (requires Devansh — see P1 checklist).

## Manual checklist P1 (Devansh, Windows) — PASS/FAIL placeholders
1. Speech tab: drag vad_threshold slider 0.6 → 0.2; release. "Saved" flashes; config.json vad_threshold becomes 0.2; restart not required banner does NOT appear (hot_apply). Reload tab → still 0.2. [ ] PASS / [ ] FAIL — notes: ____
2. Speech tab: quiet_speech_boost toggle flips; close Settings, reopen → still flipped (persist check: schema bug fix). [ ] PASS / [ ] FAIL — notes: ____
3. General tab: drag sound_volume slider; set mic_device to "Microphone Array"; restart banner "Applies after restart." shows; restart → new device active. [ ] PASS / [ ] FAIL — notes: ____
4. General tab: type "Ctrl+Shift+T" in Hands-free field, Enter → Saved; restart → hand-free hotkey works. [ ] PASS / [ ] FAIL — notes: ____
5. Cloud tab: set cloud language "hi,en"; timeout 15000→20000 sliders move and save. [ ] PASS / [ ] FAIL — notes: ____
6. Cleanup tab: switch backend combo to openrouter → warn line appears in Cleanup, Privacy and About; box back to local → warn disappears. [ ] PASS / [ ] FAIL — notes: ____
7. Privacy tab: toggle "Keep history" off; set history_max_entries 1000→500 (slider), restart not required banner does NOT appear. [ ] PASS / [ ] FAIL — notes: ____
8. Delete: click "Delete all local data" → armed state shows red warning + Confirm/Cancel. Cancel → no files removed. [ ] PASS / [ ] FAIL — notes: ____
9. Delete confirm: click "Delete all local data" → "Confirm delete" → stats.json, dictionary.json, snippets.json, transforms.json, notes/, api_keys.json gone from %APPDATA%\LocalWisperFlow; config.json untouched (same mtime); restart app → empty dictionary/snippets/notes reseeded; voice typing still works. [ ] PASS / [ ] FAIL — notes: ____
10. About tab: "Config version: 1" readout visible. [ ] PASS / [ ] FAIL — notes: ____

## Unverified Assumptions & Remaining Risks
- [ASSUMED] A3 recorder rows replace ##card_general_other_hotkeys organically (same keys, restart=false) — hunk area disjoint by declaration; if A3 lands different surrounding code the chord keys keep working.
- [UNKNOWN] Pixel layout of the new long cards (Speech/General/Privacy) on real hardware — card heights grow; P1 checklist is Devansh's visual pass. All cards use the existing BeginCard/EndCard pattern.
- [UNKNOWN] quiet_speech_boost behavioral effect comes from the pre-existing engine path (main.cpp ApplySettings:585) — untouched; only persistence was dead. P1 item 2 verifies persistence only.
- Full app binary cannot link until the integrator applies the A2 CMakeLists NEED (FileDialogs.cpp). Anything requiring a running binary (P1 manual checks) is unverified until then.
- G3 zig type-check pending (no zig installed) → report as "type-checked" (MinGW g++ build), never "works".

## Git State
- Branch: feature/studio-pages (verified; never switched).
- Code commit (explicit paths only): src/core/SettingsStore.cpp, src/ui/MainWindow.h, src/ui/MainWindow.cpp — SHA 5a2a618baeab0e873d12800d3f1b801b9f7f25c1. NOTE: this commit's message carries the Task/Agent/Wave in the subject text (T-025) but the four trailer lines were omitted from the commit message — git --amend is forbidden by standing orders so the omission stands honestly here; the docs-only report commit below carries full trailers.
- Docs commit (explicit path only): docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-A4-T-025.md (this report; full trailers).
- Left uncommitted (never staged by A4): docs/agent-ops/20261107-studio/COMMS.md (A4 START/CLAIM/CONFLICT/NEED appended per protocol; orchestrator commits), sibling untracked files (briefs W4-IMPL-A2/A3/D/I/N/S/T, reports W4-IMPL-A3-T-024, W4-IMPL-I-T-018, W4-IMPL-S-T-017, tests/test_insights_page.cpp, src/ui/pages/*), src/ui/ScratchpadWindow.*. No git add -A/stash/reset/checkout/rebase/clean used.
- docs/PROGRESS.md NOT updated (read-only write-set; orchestrator records per task, same as W4 siblings).
