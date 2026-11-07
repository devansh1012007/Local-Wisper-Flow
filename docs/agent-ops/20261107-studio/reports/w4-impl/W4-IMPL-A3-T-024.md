# W4-IMPL-A3 T-024 — In-app hotkey recorders (A3)

## ORCH-DIGEST
- STATUS: COMPLETE (code + scoped tests green; main.cpp integration + committed tests are integrator NEEDs below; manual Windows checklist open for Devansh)
- Branch: feature/studio-pages. Task: T-024 (plan A3/D10) — generalized WindowsHotkeyListener learn mode for ALL 5 config hotkeys + studio shortcuts.
- Write-set: src/os/WindowsHotkeyListener.{h,cpp}, src/core/HotkeyBinding.{h,cpp}, src/core/UiBus.{h,cpp} (target-key additive), src/ui/MainWindow.{h,cpp} (General tab recorder rows ONLY). COMMS CLAIM + CONFLICT (A4 shares MainWindow) + NEEDs (main.cpp consumer, CMakeLists, tests) to follow before editing.
- Key facts verified: learn mode today single global hardcoded to hotkey_ptt (WindowsHotkeyListener.cpp:157-179 HandleLearnMode role PushToTalk; main.cpp:574-576 BeginLearnMode no-arg; main.cpp:622-628 TryTakeLearnedBinding->hotkey_ptt). UiBus StartLearnHotkey exists but payload unused (MainWindow.cpp:688-690,1048-1050). LoadBindings reads 5 keys (HotkeyBinding.cpp:177-222). TransformStore already has conflict detector (ShortcutsConflict subset semantics, RecomputeConflicts) + TransformsPage recorder (ImGui polling, ValidateShortcut modifier-required). See Context Report below.

## Context Report (pre-change investigation)

### 1. Root cause / entry points
- [VERIFIED src/os/WindowsHotkeyListener.h:31-32] `BeginLearnMode(void)` / `TryTakeLearnedBinding(std::string&)->bool` carry no target-key or role. [VERIFIED src/os/WindowsHotkeyListener.cpp:157-179] `HandleLearnMode` collects `learned_keys` on the hook thread, hardcodes `BindingRole::PushToTalk`, and serializes via `HotkeyBinding::Serialize`. Cell: `DispatchKey` swallows every key while `learn_mode` is true (`:123`), so dictation bindings cannot fire during learn (capture preserved by suspension, not by filtering).
- [VERIFIED src/main.cpp:574-576] `StartLearnHotkey` consumer calls `hotkey.BeginLearnMode()` with no payload. [VERIFIED src/main.cpp:622-628] learned poll does `SetString("hotkey_ptt", learned)` + `Save` + `UpdateBindings(LoadBindings)` + `SetDetectedHotkey(learned)`. Hardcoded to ptt — the exact W0 finding (brief §1). main.cpp is READ-ONLY for A3; integration is NEED.
- [VERIFIED src/core/UiBus.h:10-21] `UiCommandType::StartLearnHotkey` already has `UiCommand{type,payload}`. [VERIFIED src/ui/MainWindow.cpp:688-690 (Home), :1048-1050 (General)] both post with empty payload, so the channel is plumbed but unused. No validation helper exists.
- [VERIFIED src/core/HotkeyBinding.h:22-24] only `Parse/Serialize/IsSatisfied`. [VERIFIED src/core/HotkeyBinding.cpp:177-222] `LoadBindings` reads `hotkey_ptt` (+ legacy `hotkey_code` fallback 165=RightAlt else `0x..` else `Ctrl+Win`), `hotkey_handsfree`, `hotkey_cancel` (default `Escape`), `hotkey_command`, `hotkey_paste_last`. No conflict API, no modifier rule, no per-key registry helper.
- [VERIFIED src/core/HotkeyBinding.cpp:34-71] `ParseSingleKey` accepts named keys + F1-24 + `0x..` only — single letters/digits (`1`,`O`,`S` in studio `Win+Alt+1/O`) do NOT parse. [VERIFIED src/core/TransformStore.cpp:74-94] studio conflict logic is therefore string-token based (`TokenSet` + subset `TokenSetsCollide`), not vk-based. Any vk-based registry check that must cover studio chords requires a vocabulary extension in `HotkeyBinding` or string-token fallback.
- [VERIFIED src/core/TransformStore.h:37-42,54] `SetHotkeyBindings` + `Conflicted` + `ShortcutsConflict` + `DiffShortcut/SetDiffShortcut` (default `Win+Alt+O`, :70). [VERIFIED src/core/TransformStore.cpp:222-257] `RecomputeConflicts` disables a slot when it collides with config bindings or another slot (subset semantics) and logs `transform slot N shortcut 'X' conflicts with an existing hotkey; disabled`. That is the plan copy for studio-side disabling (config wins, studio disabled).
- [VERIFIED src/ui/pages/TransformsPage.h:192-230] `transforms::ValidateShortcut` already enforces modifier-required (`A shortcut needs a modifier key`), key-required (`A shortcut needs a key`), diff-chord, other-slot, and `ShortcutsConflict` (`That shortcut is already in use`) via string tokens. [VERIFIED src/ui/pages/TransformsPage.cpp:425-442] `PollRecording` uses ImGui key polling (in-sheet), not the WH_KEYBOARD_LL hook. [VERIFIED src/ui/pages/ScratchpadPage.h:22,190-191] scratchpad shortcut is in-memory `Win+Alt+S` pill (no store, no persistence, no Press handling) — nowhere for General to persist a scratchpad chord. Diff chord IS persisted (transforms.json). Conclusion: page recorders are self-contained per-page; General must NOT duplicate them (brief §3). A3 shares validation logic, not capture.
- [VERIFIED src/ui/MainWindow.cpp:1036-1100] `DrawGeneralPage` hotkey card holds single ptt chips + Learn button. [VERIFIED src/ui/MainWindow.h:90,152-153] `detected_hotkey_` is a single string with no target — Welcome step 2 consumes it ([VERIFIED MainWindow.cpp:1198-1209]). Multi-row armed state needs new per-target state; `SetDetectedHotkey` alone is insufficient (INV-ui-shell §10).
- [VERIFIED tests/test_overlay_settings_ui.cpp:467-527,529-552] scans pin `page_focus_pending_=true ==2`, `IsItemDeactivatedAfterEdit >=3`, no `if (ImGui::SliderInt(` after Cleanup, `validation_error_.clear() >=2`, `StartLearnHotkey` presence is implied by General markers in S1 scan (`##card_general_hotkey`, `StartLearnHotkey`, `sound_enabled`, `reduce_motion`, `mini_x` — see W3-IMPL-S1 §Phase 4). New rows must preserve all.

### 2. Affected files / consumers
- HotkeyBinding.{h,cpp}: portable core, consumed by HotkeyTracker, WindowsHotkeyListener, TransformStore (for `Binding`/`Parse`/`Serialize`), SettingsStore via `LoadBindings`, tests (test_hotkey.cpp, test_engine.cpp, test_transformstore.cpp via Parse). Additive-only changes required.
- UiBus.{h,cpp}: consumed by MainWindow (producer), main.cpp drain (consumer), tray, Overlay handlers. Keep enum + `PostCommand/PollCommand` signatures stable; payload helpers only.
- WindowsHotkeyListener.{h,cpp}: Windows-only TU (not in lwf_tests). Consumed by main.cpp only. Keep no-arg `BeginLearnMode()` + single-arg `TryTakeLearnedBinding` source-compatible (default args / overload) so tree compiles before main.cpp NEED lands.
- MainWindow.{h,cpp}: General tab only. Consumers: main.cpp (`SetDetectedHotkey`, RunOnce), Welcome step 2 (ptt commit via `detected_hotkey_`). New `SetLearnedHotkey`/`SetRecorderError` API is additive; `SetDetectedHotkey` must keep Welcome working.
- SettingsStore schema: NO change needed (design finding — 5 keys already String hot-apply). `SetString("")` clearing works; `LoadBindings` fallback governs effective bindings. No NEED for schema.
- TransformStore / pages: READ-ONLY (forbidden). General hosts config 5 only; diff/scratchpad recorders NOT hosted in General (would need TransformStore ref in MainWindow ctor + Scratchpad has no store). Document as deliberate "may host" decline.

### 3. Invariants that must be preserved
- Dictation capture: hook swallows all keys while learning; tracker fed `LoadBindings` on `ApplySettings`; engine never sees learn keys. Adding a chord (studio or config) must never steal ptt capture — subset-conflict semantics + ptt-first loader dedup + recorder reject are the three layers.
- Round-trip: `Parse(Serialize(b)) == b` for every chord the listener can emit (named + F + hex + new digit/letter extension). `LoadBindings` must still derive ptt from `hotkey_code` and default `Ctrl+Win`, and load `Escape`-only cancel (recorder strictness is recorder-only, never loader).
- Source-scan pins (font mechanism, keyboard nav/commit, studio routes) must stay green. No new `SetWindowFontScale`, no new `page_focus_pending_=true`, no new `validation_error_.clear()`, no `if (ImGui::SliderInt(`.
- No code comments in new code (AGENTS.md); no `windows.h` outside src/os (hygiene); no `TODO/FIXME/XXX`; no file-write/network APIs in core/os changes.

### 4. Technical risks and edge cases
- Empty-payload backward compat: Home Learn + pre-NEED retained. Empty payload MUST resolve to `hotkey_ptt` everywhere (listener default, UiBus parse, MainWindow). Invalid non-empty payload must NOT silently redirect to ptt (would steal ptt) — main.cpp NEED validates via `IsConfigHotkeyKey` and drops unknowns.
- Escape exception: schema default `hotkey_cancel=Escape` is a single non-modifier key. Recorder modifier-required must exempt exactly `{Escape}` for the Cancel role only; all other roles reject it. `RightAlt`-only ptt (legacy) and hand-edited `F1` stay loadable but not re-recordable (loader permissive, recorder strict). Documented, not a bug.
- ptt cannot be disabled: `LoadBindings` falls back to `Ctrl+Win` when `hotkey_ptt` is empty (even after Clear). Clear-for-ptt is therefore "restore fallback" not "disable". Rows show the saved string; effective binding after ApplySettings is the fallback. No way to make ptt empty-active exists today (hotkey_code=0 still yields Ctrl+Win).
- Digit/letter Parse extension: additive (previously-rejected single-char chords now parse). `NotAKey` (multi-char) stays rejected, preserving test_hotkey pin. `CanonicalName` displays `0x41` as `A` etc. so chips stay friendly. Single-char `f` is the F key, not an F-key index (F-check requires 3 chars).
- Concurrent learned state: `learned_keys`/`learned_binding`/target are touched from hook thread (`HandleLearnMode`) and UI thread (`Begin/TryTake`). Today only `learned_ready/learn_mode` are atomic and `HandleLearnMode` mutates the vectors without `tracker_mutex` while `Begin` holds it — data race. Fix is to guard all learned state with `tracker_mutex` inside `HandleLearnMode`.
- Sibling A4 edits MainWindow general cards concurrently. Hunks must be confined to `DrawGeneralPage` hotkey card + recorder rows + setter API; CONFLICT declared in COMMS.
- Tests live outside write-set (integrator NEED): doctest registration for new HotkeyBinding helpers needs CMake/test file ownership. Verify via standalone scratch build; committed tests NOT-RUN until integrator wires.

### 5. Expected behavior and required test cases (T-024 acceptance)
- Generalized recorder: `BeginLearnMode(target="hotkey_ptt")` stores pending target; first key-up serializes chord with role for that target; `TryTake(chord,target)` returns both; UiBus payload carries target (`StartLearnPayload`/`ParseStartLearnTarget`, empty→ptt); main.cpp routes by target (NEED exact lines).
- All 5 config hotkeys recordable from Settings > General (Keycap + Record + Clear per row); main.cpp `ApplySettings` already re-loads bindings, so saved rows survive restart (manual checklist). Transform slots keep page recorder (shared validation only, no duplication); diff/scratchpad in General explicitly declined.
- Conflict: candidate colliding (subset/superset/exact, role-agnostic) with any other registry entry is rejected at record time (`That shortcut is already in use`, not saved) + current-row warning + loader dedup disables later duplicates (ptt first) + TransformStore disables studio slots on recompute.
- Modifier-required: `ValidateRecorderChord` rejects modifier-less chords (`A shortcut needs a modifier key`); Cancel+Escape exempt as the single case where a non-modified chord is the schema default and must stay recordable.
- Tests (NEED payload for integrator): (a) ptt-vs-custom conflict detection, (b) modifier requirement, (c) Serialize/LoadBindings round-trip for all 5 keys, (d) registry-wide uniqueness via loader dedup + FindConflict, (e) engine/tracker regression (existing test_hotkey + test_engine green), (f) UiBus payload round-trip, (g) General source-scan markers. Manual checklist (record+persist+restart; conflict disables; clear) listed in report.

## Implementation Log (atomic phases)

### Phase 0 — Investigation (read-only)
- Read brief order: STANDING-ORDERS, AGENTS.md, 00-mission, 01-work-ledger, STUDIO_PLAN (D10, conflict copy, recorder rows), INV-ui-shell (learn wiring, LoadBindings :177-222, single global learn state), INV-datastore (registry shape), W2-IMPL-E2 (TransformStore shortcut model + conflict-warning field, StartTransform consumer), W3-IMPL-S1 (General tab layout, Home chips), WindowsHotkeyListener.{h,cpp} fully, HotkeyBinding.{h,cpp} fully, main.cpp learn section (:574-576, :622-628) fully, TransformsPage.{h,cpp} (validate + PollRecording), ScratchpadPage.{h,cpp} (Win+Alt+S pill, no store), SettingsStore schema, HotkeyTracker (subset safety), existing tests (test_hotkey 14 cases) and overlay source-scans.
- Posted COMMS START + CLAIM + CONFLICT (A4) + NEED (main.cpp, CMake/tests) before editing. Created this report early.
- Design findings locked before coding: (1) no schema change needed; (2) hook capture vs ImGui capture are different mechanisms — share validation, not capture; (3) modifier+key rule cannot copy TransformsPage verbatim (would reject Ctrl+Win default); (4) single-letter/digit + F1-F9 vocabulary gaps block vk-based studio conflict checks and Serialize round-trip.

### Phase 1 — src/core/HotkeyBinding.{h,cpp} (ordinal: portable first, tests-first logic)
- `ParseSingleKey`: added single `a-z0-9` handling (studio `Win+Alt+1/0/O/S`, `ctrl+o`) mapping to the ASCII vk (`toupper`). Additive: `NotAKey` (multi-char) still rejected, preserving test_hotkey pin. Added `f1-9` (`size==2, f + 1-9`) so `Serialize` output `F1..F9` round-trips (pre-existing: `CanonicalName(0x70)=F1` emitted unparseable `F1`, only `F10+` parsed). `F13` path unchanged.
- `CanonicalName`: if vk is `0-9/A-Z` return the single character (chips show `1/O` not `0x31/0x4F`). Digit/letter range has no named-key overlap, so no canonical change for existing chords.
- New registry API:
  - `IsModifierVk` (Ctrl/Shift/Alt/Win/LeftAlt/RightAlt — matches TransformsPage `IsModifierToken` exactly, so config and studio agree on what counts as a modifier).
  - `ChordHasModifier` (any modifier among `keys`).
  - `BindingsConflict` (subset/superset via sorted-unique `std::includes` — mirrors TransformStore `TokenSetsCollide`; empty binding never conflicts).
  - `ChordsConflict` (parse both, unparseable never conflicts — loader ignores unparseable chords, so they cannot steal capture).
  - `FindConflict(candidate, registry, exclude_role)` returns the colliding entry label (or `Serialize` when label empty). Role-agnostic except the excluded self-role, so re-recording the same chord for the same target passes while any other role's chord is flagged.
  - `ValidateRecorderChord(candidate, registry, target_role)` error strings reuse the TransformsPage exact copy: empty/parse-fail → `A shortcut needs a key`; no modifier → `A shortcut needs a modifier key`; conflict → `That shortcut is already in use`; with the single exemption `{Escape}` for `BindingRole::Cancel` (schema default, must stay recordable). DECISION: no "needs a key" check for modifier-only chords — `Ctrl+Win` (ptt default) and `RightAlt` (legacy) are modifier-only and must stay recordable; the studio key-required rule stays studio-only (TransformsPage untouched, forbidden).
  - `RoleForConfigKey` / `ConfigKeyForRole` / `IsConfigHotkeyKey` / `ConfigHotkeyKeys` (5 keys in canonical order).
- `LoadBindings`: appended a ptt-first deduplication pass (`BindingsConflict` against already-kept entries; later duplicates dropped). DECISION: first-wins in existing push order (ptt > handsfree > cancel > command > paste_last) implements "conflict disables + adding a hotkey may never steal dictation capture" even for hand-edited configs that bypass the recorder. Distinct chords sharing a modifier (e.g. `Ctrl+Win` / `Ctrl+Shift+Space`) are not subset relations and survive unchanged.
 INVARIANT: `LoadBindings` stays permissive (loads `Escape`-only, `RightAlt`-only, `F1` hand edits) — recorder strictness never leaks into the loader, so existing configs cannot become unloadable after this change.

### Phase 2 — src/core/UiBus.{h,cpp} (additive, header-inline)
- Added `UiBus::StartLearnPayload(target)` and `UiBus::ParseStartLearnTarget(payload)`: empty string round-trips to `hotkey_ptt` (Home Learn empty-payload callers + pre-NEED retained); non-empty passes through unmodified (validation via `IsConfigHotkeyKey` at producer/consumer, never silently redirected to ptt). No enum change — no new silent-drop case for the main.cpp drain switch, no `-Wswitch` risk. `UiBus.cpp` untouched (header-inline statics, no new TU surface).

### Phase 3 — src/os/WindowsHotkeyListener.{h,cpp} (target-key, thread-safe learned state)
- `BeginLearnMode(target_key = "hotkey_ptt")` (default preserves no-arg `main.cpp:575` and Home callers bit-identically). Stores the pending target under `tracker_mutex`, normalizes empty → `hotkey_ptt`.
- New `TryTakeLearnedBinding(out_chord, out_target)` returns the completed chord plus the target captured at finalize; legacy single-arg version delegates to it (discards target) so unmodified main.cpp still compiles. New `PendingLearnTarget()` accessor for main-loop logging/validation.
- `HandleLearnMode` now holds `tracker_mutex` for every learned-state access (keys, binding, pending/completed targets) and derives the serialized `Binding` role from `learn_target` via `RoleForConfigKey` (unknown → PushToTalk fallback for opaque targets). Role does not change `Serialize` output (keys only) but keeps the `Binding` truthful for future consumers. Dispatch/tracker/suppress logic untouched.
 INVARIANT: learn mode still swallows every key (`DispatchKey` returns 1 before the tracker is fed), so dictation cannot fire mid-learn and no learned key leaks into `HotkeyTracker`.

### Phase 4 — src/ui/MainWindow.{h,cpp} (General tab recorder rows ONLY)
- Header: public `SetLearnedHotkey(target, chord)` (clears armed state for that target, mirrors `detected_hotkey_` for ptt so Welcome step 2 keeps working) + `SetRecorderError(target, message)` (stores per-target recorder error, clears armed row). Private `DrawGeneralHotkeyRow(config_key, label, helper, scale)` + `learn_target_ / learn_error_target_ / learn_error_` state. No `HotkeyBinding.h` include in the header.
- `SetDetectedHotkey` additionally clears `learn_target_` when the learned chord is ptt (legacy path: pre-NEED main.cpp always routes to ptt, so the armed row must clear even though `SetLearnedHotkey` has not been called yet).
- `DrawGeneralHotkeyRow`: label + helper + `DrawHotkeyChips(current)` + live conflict line (`FindConflict(current, LoadBindings(settings_), role)` → `That shortcut is already in use` in `err` color; current-chord display checks conflict only, never modifier, so hand-edited loader-permissive chords like `Escape` are not mislabelled) + `Record##<key>` / `Clear##<key>` buttons (unique ids per row) + armed `Press a chord...` + per-target `learn_error_` text. While any row is armed, other rows' buttons are `BeginDisabled` (prevents overlapping learns); the armed row shows no buttons, so Clear-while-armed is impossible. Clear = `SetString(key, "") + ApplyAndSave()` (fires the existing `ApplySettings` re-load; ptt clearing restores the `LoadBindings` fallback — see Risks).
- `DrawGeneralPage`: `##card_general_hotkey` (id preserved for scans) now holds 5 rows (`Push-to-talk`/`Hold to speak, release to type.`, `Hands-free`/`Double-tap to talk without holding.`, `Cancel`/`Stop dictation immediately.`, `Command mode`/`Hold to dictate a command.`, `Paste last`/`Paste your last dictation.`) separated by `Separator`. Other General cards (Sound/Motion/Window + A4 Other-hotkeys/Legacy cards) untouched.
- Studio 3 categories in General: deliberately NOT hosted (brief `may host`). Diff chord lives in `TransformStore` (MainWindow has no TransformStore ref; adding one needs a ctor change + main.cpp wiring — integration NEED, with page-owned recorder already covering slots). Scratchpad chord has no persisted store (`ScratchpadPage::shortcut_` in-memory) — hosting a recorder with nowhere to save would be fake UI. Both stay per-page (TransformsPage recorder consumes TransformStore; Scratchpad pill untouched, forbidden).

## NEED (integrator, main.cpp — FORBIDDEN to A3)

Replace `src/main.cpp:574-576` (StartLearnHotkey case) with:
```cpp
case UiCommandType::StartLearnHotkey: {
    const std::string target = UiBus::ParseStartLearnTarget(command.payload);
    if (HotkeyBinding::IsConfigHotkeyKey(target)) {
        hotkey.BeginLearnMode(target);
    } else {
        lwf::Log::Warning("learn: unknown target ignored");
    }
    break;
}
```
`HotkeyBinding.h` (line 17) and `UiBus.h` (line 41) are already included; no new include needed.

Replace `src/main.cpp:622-628` (learned poll) with:
```cpp
std::string learned;
std::string target;
if (hotkey.TryTakeLearnedBinding(learned, target)) {
    if (!HotkeyBinding::IsConfigHotkeyKey(target)) {
        lwf::Log::Warning("learn: unknown target ignored");
    } else {
        const auto role = HotkeyBinding::RoleForConfigKey(target);
        const std::vector<Binding> registry = LoadBindings(config.Store());
        const std::string error =
            HotkeyBinding::ValidateRecorderChord(learned, registry, *role);
        if (!error.empty()) {
            full.SetRecorderError(target, error);
        } else {
            config.Store().SetString(target, learned);
            config.Save();
            const std::vector<Binding> bindings = LoadBindings(config.Store());
            hotkey.UpdateBindings(bindings);
            transforms.SetHotkeyBindings(bindings);
            full.SetLearnedHotkey(target, learned);
        }
    }
}
```
Why this shape: registry is the pre-save `LoadBindings` (validation excludes the target role, so re-recording the same chord passes); on success the direct `UpdateBindings` refresh matches the old behavior (ApplySettings will re-load anyway, harmless duplicate); `SetLearnedHotkey` clears the armed row and keeps Welcome ptt in sync; invalid/unknown targets are dropped with a warning, never redirected to ptt. `ApplyAndSave` from Clear already flows through the existing `ApplySettings` case (lines 577-596) — no extra wiring needed for Clear. Studio global activation (Win+Alt+N / Win+Alt+O / Win+Alt+S hook Press) is NOT in this NEED: no `EngineEventType`/tracker role exists for studio chords today; if orchestrator wants global studio Press, it needs a separate engine/tracker design (new event + `DictationEngine` handling) — explicitly out of A3 scope.

## NEED (integrator, tests + CMake — FORBIDDEN to A3)

Tests are read-only in the A3 write-set; the equivalent coverage was verified by a standalone scratch harness (deleted after run, not committed — see Verification). Integrator: append the following to `tests/test_hotkey.cpp` (no new file needed, so no CMakeLists change for tests; existing `test_hotkey.cpp` is already in `lwf_tests` at CMakeLists.txt:160). Suggested cases cover T-024 acceptance (conflict / modifier / round-trip / uniqueness / UiBus payload):

```cpp
TEST_CASE("recorder vocabulary covers studio digits and F keys") {
    CHECK(HotkeyBinding::Parse("Win+Alt+1", Role::PushToTalk).has_value());
    CHECK(HotkeyBinding::Parse("Win+Alt+0", Role::PushToTalk).has_value());
    CHECK(HotkeyBinding::Parse("Win+Alt+O", Role::PushToTalk).has_value());
    CHECK(HotkeyBinding::Parse("Win+Alt+S", Role::PushToTalk).has_value());
    CHECK(HotkeyBinding::Parse("F1", Role::PushToTalk).has_value());
    CHECK_FALSE(HotkeyBinding::Parse("NotAKey", Role::PushToTalk).has_value());
    auto slot = HotkeyBinding::Parse("Win+Alt+1", Role::PushToTalk);
    REQUIRE(slot.has_value());
    auto again = HotkeyBinding::Parse(HotkeyBinding::Serialize(*slot), Role::PushToTalk);
    REQUIRE(again.has_value());
    CHECK(again->keys == slot->keys);
}

TEST_CASE("recorder conflict detection covers ptt versus studio chord") {
    CHECK_FALSE(HotkeyBinding::ChordsConflict("Ctrl+Win", "Win+Alt+1"));
    CHECK(HotkeyBinding::ChordsConflict("Win+Alt", "Win+Alt+1"));
    CHECK(HotkeyBinding::ChordsConflict("Ctrl+Win", "Ctrl+Win"));
    CHECK_FALSE(HotkeyBinding::ChordsConflict("Ctrl+Win", "Ctrl+Shift+Space"));
    std::vector<Binding> registry;
    registry.push_back(*HotkeyBinding::Parse("Ctrl+Win", Role::PushToTalk));
    registry.push_back(*HotkeyBinding::Parse("Escape", Role::Cancel));
    CHECK(HotkeyBinding::ValidateRecorderChord("Ctrl+Win", registry, Role::HandsFreeToggle) ==
          "That shortcut is already in use");
    CHECK(HotkeyBinding::ValidateRecorderChord("Ctrl+Win", registry, Role::PushToTalk).empty());
    CHECK(HotkeyBinding::FindConflict("Ctrl+Win", registry, Role::HandsFreeToggle).has_value());
    CHECK_FALSE(HotkeyBinding::FindConflict("Ctrl+Win", registry, Role::PushToTalk).has_value());
}

TEST_CASE("recorder requires a modifier except cancel escape") {
    std::vector<Binding> registry;
    registry.push_back(*HotkeyBinding::Parse("Ctrl+Win", Role::PushToTalk));
    CHECK(HotkeyBinding::ValidateRecorderChord("F13", registry, Role::PushToTalk) ==
          "A shortcut needs a modifier key");
    CHECK(HotkeyBinding::ValidateRecorderChord("Escape", registry, Role::PushToTalk) ==
          "A shortcut needs a modifier key");
    CHECK(HotkeyBinding::ValidateRecorderChord("Escape", registry, Role::Cancel).empty());
    CHECK(HotkeyBinding::ValidateRecorderChord("Ctrl+Win", registry, Role::PushToTalk).empty());
    CHECK(HotkeyBinding::ValidateRecorderChord("RightAlt", registry, Role::PushToTalk).empty());
    CHECK(HotkeyBinding::ValidateRecorderChord("", registry, Role::PushToTalk) ==
          "A shortcut needs a key");
}

TEST_CASE("serialize and load bindings round-trip all five keys") {
    SettingsStore store((std::filesystem::path("/tmp") / "lwf_a3_bind_test.json").string());
    store.SetString("hotkey_ptt", "Ctrl+Win");
    store.SetString("hotkey_handsfree", "Ctrl+Shift+H");
    store.SetString("hotkey_cancel", "Escape");
    store.SetString("hotkey_command", "Ctrl+Shift+Space");
    store.SetString("hotkey_paste_last", "Ctrl+Shift+P");
    const auto bindings = LoadBindings(store);
    REQUIRE(bindings.size() == 5);
    CHECK(bindings[0].role == Role::PushToTalk);
    CHECK(HotkeyBinding::Serialize(bindings[0]) == "Ctrl+Win");
    CHECK(bindings[1].role == Role::HandsFreeToggle);
    CHECK(bindings[2].role == Role::Cancel);
    CHECK(bindings[3].role == Role::CommandMode);
    CHECK(bindings[4].role == Role::PasteLast);
    CHECK(HotkeyBinding::ConfigHotkeyKeys().size() == 5);
    CHECK(HotkeyBinding::IsConfigHotkeyKey("hotkey_ptt"));
    CHECK_FALSE(HotkeyBinding::IsConfigHotkeyKey("bogus"));
}

TEST_CASE("loader disables later duplicates so ptt keeps capture") {
    SettingsStore store((std::filesystem::path("/tmp") / "lwf_a3_bind_test.json").string());
    store.SetString("hotkey_ptt", "Ctrl+Win");
    store.SetString("hotkey_handsfree", "Ctrl+Win");
    const auto exact = LoadBindings(store);
    CHECK(exact.size() == 4);
    bool ptt_kept = false;
    bool handsfree_kept = false;
    for (const auto& b : exact) {
        if (b.role == Role::PushToTalk) {
            ptt_kept = true;
        }
        if (b.role == Role::HandsFreeToggle) {
            handsfree_kept = true;
        }
    }
    CHECK(ptt_kept);
    CHECK_FALSE(handsfree_kept);
    store.SetString("hotkey_handsfree", "Win");
    CHECK(LoadBindings(store).size() == 4);
}

TEST_CASE("learn payload round-trips through UiBus") {
    CHECK(UiBus::StartLearnPayload("") == "hotkey_ptt");
    CHECK(UiBus::ParseStartLearnTarget("") == "hotkey_ptt");
    CHECK(UiBus::ParseStartLearnTarget(UiBus::StartLearnPayload("hotkey_cancel")) ==
          "hotkey_cancel");
    CHECK(HotkeyBinding::ConfigKeyForRole(Role::PasteLast) ==
          std::string("hotkey_paste_last"));
}
```
`tests/test_hotkey.cpp` already includes `HotkeyBinding`/`SettingsStore`/`filesystem` via `test_hotkey.cpp:1-5` plus `Core/HotkeyBinding.h` (verify include for `UiBus.h` at integrator time — add `#include "core/UiBus.h"` if absent). CMakeLists: no change if appending to the existing file.
Also suggested source-scan append to `tests/test_overlay_settings_ui.cpp` (follows its `count_occurrences` pattern, mirrors the W3 S1 structure scan): require `DrawGeneralHotkeyRow` present, `##card_general_hotkey` present, `StartLearnPayload` present, `SetLearnedHotkey` present, `Record##`/`Clear##` present, and 5 `DrawGeneralHotkeyRow("hotkey_` occurrences (`hotkey_ptt`, `hotkey_handsfree`, `hotkey_cancel`, `hotkey_command`, `hotkey_paste_last`).

## Review Audit (self-review per phase + final)

- Correctness / root cause: learn single-target root cause is removed at the listener (pending target stored, role derived) + bus payload (target string) + main-loop seam (`TryTake(chord,target)`). UI no longer hardcodes ptt. The fix is architectural, not symptom patching. PASS.
- Phase 1 review (HotkeyBinding):
  - CRITICAL fixed during scratch verification (see Verification): initial `ValidateRecorderChord` copied the TransformsPage "needs a key" rule and rejected `Ctrl+Win` (the ptt DEFAULT, modifier-only) as `A shortcut needs a key`. Debug harness printed `HF->[A shortcut needs a key]` / `PTT->[A shortcut needs a key]` for the same chord that `FindConflict` correctly flagged/cleared. Root cause: config and studio disagree on modifier-only validity. Resolution: removed the key-required check from the config recorder only (brief requires modifier only); studio strictness remains in TransformsPage. Post-fix: `HF->[That shortcut is already in use]`, `PTT->[]`. Documented above as DECISION.
  - MAJOR found by same harness: `F1..F9` were emitted by `Serialize` (`CanonicalName`) but rejected by `Parse` (only `F10+` parsed), breaking round-trip for those keys. Resolution: `f1-9` parse branch (additive, existing `F13` test still passes — verified green).
  - Checked: `NotAKey` still rejected (preserves `test_hotkey` pin); `Escape`-only cancel still loads; subset loses to superset both ways; empty chord never "conflicts" (avoids disabling on empty parser results); loader dedup order is ptt-first so a duplicate handsfree cannot steal ptt.
- Phase 2 review (UiBus): header-inline statics leave enum/drain/copy semantics untouched; empty→ptt is the only implicit default and it matches the pre-A3 Home/Welcome contract. Invalid non-empty targets pass through for the consumer to drop with a warning (no silent ptt redirect). PASS.
- Phase 3 review (WindowsHotkeyListener):
  - Race fixed as part of the change: `HandleLearnMode` previously mutated `learned_keys/learned_binding` without `tracker_mutex` while `BeginLearnMode` held it. All learned state is now under `tracker_mutex` (pending + completed targets separated so `PendingLearnTarget` is empty while not learning and `TryTake` returns the finalized target, then clears it).
  - Deadlock check: `HandleLearnMode` locks `tracker_mutex` then calls only pure helpers (`RoleForConfigKey`, `Serialize`); `DispatchKey` holds no lock across the call. `Begin/TryTake/Pending` lock once. Single lock ordering (tracker then pure), no nesting. `learn_mode/learned_ready` remain atomic for the early-out load. PASS.
  - Compat check: default-arg `BeginLearnMode()` + single-arg `TryTakeLearnedBinding` delegate preserves all existing call sites (main.cpp:575 today, Welcome/Home empty-payload posts) so the tree builds before the NEED lands (verified: full `local-wisper-flow` build compiles both TUs, link fails only on pre-existing A2 `FileDialogs` symbols).
- Phase 4 review (MainWindow, General ONLY):
  - Verified file diff: hunks are (a) `HotkeyBinding.h` include, (b) `SetDetectedHotkey/SetLearnedHotkey/SetRecorderError`, (c) `DrawGeneralHotkeyRow` + `##card_general_hotkey` rewrite. A4 helpers (`IntRow/DoubleRow/BoolRow/StringFieldRow`), Sound/Motion/Window bodies, A4 `##card_general_other_hotkeys` text card, and Speech/Cloud/Cleanup/Privacy/About/Home/Welcome bodies are untouched by A3.
  - Source-scan pins re-verified after edits (see Verification): `page_focus_pending_ = true` == 2, `SetWindowFontScale` absent from MainWindow, `validation_error_.clear()` still ≥ 2 (no new clears added — recorder uses its own `learn_error_`), `IsItemDeactivatedAfterEdit` unchanged by A3, no `if (ImGui::SliderInt(` added.
  - `BeginDisabled/EndDisabled` balanced in all three branches (armed / other-armed / idle); record/clear ids are unique per key (`Record##hotkey_*`); `DrawHotkeyChips` per-row call keeps the chips contract (empty → `No hotkey set yet.`).
  - Conflict display uses `FindConflict` (conflict only), never `ValidateRecorderChord`, so a hand-edited modifier-less loader-permissive chord is not mislabelled as a recorder error. Validation happens once, at learn time, in the NEED consumer.
- Final review: `git diff` stat shows A4/A3 interleaved hunks in `MainWindow.{h,cpp}` + `SettingsStore.cpp` (A4 schema, preserved) + my four file pairs. No CMake/scripts/pages/SettingsStore edits by A3. No debug artifacts in src (scratch harness lives in the opencode temp dir, deleted/not committed). Backwards compat: config schema untouched by A3; old configs (including `hotkey_code` legacy, `Escape` cancel, `F13`, hand hex chords, 4-key duplicates) load exactly as before except duplicates now deduplicated ptt-first (behavior fix, test-pinned by suggested cases). New chord vocabulary only accepts previously-rejected chords.
- Independent review: change is Medium risk (internal recorder + additive core helpers; no public API/serial/auth/billing breaking change; Windows-only TU type-checked via MinGW, never claimed as "works" on hardware). Per brief, self-review suffices; no separate reviewer dispatched. Status COMPLETE modulo integrator NEEDs. If orchestrator classifies as High Risk (hotkey capture is dictation-critical), treat as REVIEW_REQUIRED — otherwise COMPLETE with NEEDs.

### Long-run test (Standing Orders 8)
- Source: root cause is missing target-key state at three layers (listener global, bus payload unused, UI single-chip). Fix adds pending-target state at the listener seam (single source) with role derived from the target, payload helpers, and per-row consumers — not a per-row hack. Validation + conflict logic lives once in `HotkeyBinding` (portable, testable) rather than copy-pasted into UI/hook/TransformStore. Good long-run shape.
- Load: `LoadBindings` per General frame is cheap (5 parses; called once per row for conflict display, bounded). No loop over dynamic datasets; no unbounded collections; learn mode is single-slot. Good.
- Hostile: malformed config strings (empty, `NotAKey`, `Ctrl+`, duplicates, subsets, hand hex) cannot crash or steal capture — parser remains total, dedup drops the later duplicate, unknown targets are dropped by the NEED consumer, `PendingLearnTarget`/`TryTake` are mutex-guarded. Clearing any row is a normal `SetString("")` + `ApplyAndSave` (listener/swallow behavior unchanged). Good.
- Operability: failures surface as exact-copy recorder text in the row (`A shortcut needs a modifier key` / `A shortcut needs a key` / `That shortcut is already in use`) rather than silent drops; armed state is visible (`Press a chord...` + other rows disabled); the `PendingLearnTarget` seam + `Log::Warning` on unknown targets give the main loop something to grep. Manual Windows checklist below is the hardware gate (hook capture cannot be proven here). Good.
- Reversal: revert is per-file (`HotkeyBinding` additive helpers + `LoadBindings` dedup + `UiBus` statics + listener target + General rows). No schema/format migration; new vocabulary only accepts chords that previously failed to load, so downgrade keeps the same keys (new single-letter chords would be unknown keys on an old build — they round-trip via `raw_unknown_` if the chord were a key, but chord VALUES are free strings, so old builds simply ignore the unparseable chord). Good.

## Verification Evidence (verbatim)

- [RAN git branch --show-current -> feature/studio-pages; git status before editing showed no uncommitted src except COMMS + sibling in-flight (see Git State for A4 interleave found during Phase 4)]
- [RAN cmake --build build --config Release --parallel 4 --target lwf_core -> [100%] Built target lwf_core (after HotkeyBinding fixes; first build also green)]
- [RAN cmake --build build --config Release --parallel 4 --target lwf_tests -> [100%] Built target lwf_tests, EXIT:0]
- [RAN ./build/lwf_tests.exe --test-case="parser accepts*,chord fires*,modifier released*,first chord*,win events*,key repeats*,two bindings*,double tap*,stuck chord*,injected events*,altgr*,escape is*,load bindings*" -> [doctest] test cases: 14 | 14 passed | 0 failed | assertions: 93 | 93 passed | Status: SUCCESS!] (existing vocabulary/tracker/loader pins hold after Parse extension + dedup)
- [RAN ./build/lwf_tests.exe --test-case="*uibus*,*ui bus*,*transform*,*overlay*,*main window keyboard*,*src ui font*,*studio navigation*,*last_page*" -> test cases: 31 | 31 passed | 0 failed | assertions: 356 | 356 passed | Status: SUCCESS!] (UiBus/TransformStore/overlay source-scans, keyboard-norm, font mechanism, navigation all green after General rewrite)
- [RAN ./build/lwf_tests.exe --test-case combined hotkey+overlay set (45 cases) -> [doctest] test cases: 45 | 45 passed | 0 failed | assertions: 449 | 449 passed | Status: SUCCESS!] (post-fix combined regression)
- [RAN ./build/lwf_tests.exe --test-case="*engine*" -> [doctest] test cases: 7 | 7 passed | 0 failed | assertions: 35 | 35 passed | Status: SUCCESS!] (dictation engine regression suite)
- [RAN cmake --build build --config Release --parallel 4 --target local-wisper-flow -> MainWindow.cpp.obj + WindowsHotkeyListener.cpp.obj built CLEAN, link FAILED only on: undefined reference to FileDialogs::PickModelFile(...)+NormalizeModelPath(...) (pre-existing A2 CMake NEED, same failure as W4-IMPL-A2/A4 reports; gRPC empty_bases warnings only, no new warnings)] — A3 TUs type-check; full-binary link pending integrator CMake wiring for A2 + A3 main.cpp NEED.
- [RAN standalone scratch harness (opencode temp dir, g++ linked against build/liblwf_core.a; NOT committed, no doctest — committed tests are integrator NEED above): initial run 3 FAILURES (F1 expectation + Ctrl+Win needs-key) -> debug harness isolated root causes (Validate key-required rejected the default chord; F1..F9 Serialize/Parse gap) -> fixed both in HotkeyBinding.cpp -> reran -> 44 checks ALL OK (studio vocabulary Win+Alt+1/0/O/S + ctrl+o, F1/F13 modifier, conflict exact/subset/non-conflict, modifier F1/F13 + Cancel-Escape exemption + Ctrl+Win/RightAlt valid, 5-key LoadBindings round-trip + serialize, exact-duplicate + subset dedup ptt-kept/handsfree-dropped, UiBus payload empty→ptt + round-trip, Role/ConfigKey maps)]
- [RAN bash scripts/check_hygiene.sh -> hygiene: OK] + [RAN bash scripts/check_no_network.sh -> no-network: OK] (after final edits re-ran -> both OK)
- [VERIFIED grep] my diff adds zero `//` comments, zero TODO/FIXME/XXX, zero `windows.h` outside src/os (python diff scan: 291 added lines incl. sibling A4 hunks in the working tree, 0 comment/TODO/windows matches on added lines).
- NOT RUN: full `ctest --test-dir build` (scoped mandate — never run per brief); zig G3 type-check (no zig on box, pending per AGENTS.md — Windows sources are MinGW type-checked only, never claimed as "works"); hardware hook/manual checklist below (requires Devansh Windows run + main.cpp NEED applied — recorder cannot function end-to-end until the NEED lands, which is the declared deferred seam).

## Manual checklist (Devansh, Windows hardware) — PASS/FAIL placeholders
Record + persist + survive restart (per key, after main.cpp NEED applied):
1. Settings > General > Push-to-talk row: click Record, press Win+Ctrl+T, release. Row shows Win+Ctrl+T chips, "Saved" flashes. Restart the app: chord still Win+Ctrl+T, hold it to dictate. [ ] PASS / [ ] FAIL — notes: ____
2. Repeat for Hands-free (e.g. Ctrl+Shift+H): record, dictate hands-free with it, restart, still works. [ ] PASS / [ ] FAIL — notes: ____
3. Repeat for Cancel (set Escape, the default): record Escape, start dictation, press Escape mid-capture → dictation aborts. [ ] PASS / [ ] FAIL — notes: ____
4. Repeat for Command mode and Paste last (distinct chords): each records, persists across restart, fires its action. [ ] PASS / [ ] FAIL — notes: ____
5. Conflict disables: with ptt = Win+Ctrl+T, try to record Hands-free as Win+Ctrl+T (same chord) → row shows "That shortcut is already in use", Hands-free keeps its old chord, ptt still dictates (capture never stolen). Try subset (Hands-free = Ctrl alone vs ptt Win+Ctrl+T is NOT a subset — try Hands-free = Win vs ptt containing Win, e.g. set ptt Win+Alt+1 then try Hands-free Win+Alt → rejected with the same warning). [ ] PASS / [ ] FAIL — notes: ____
6. Modifier-required: press F13 (or Escape for a non-cancel row) → row shows "A shortcut needs a modifier key", nothing saved. Press Win alone → hmm, single-modifier Win IS accepted by design (legacy RightAlt precedent) — verify it neither crashes nor steals ptt (and Clear it afterwards). [ ] PASS / [ ] FAIL — notes: ____
7. Clear: Clear any row → chips show "No hotkey set yet." Restart: for handsfree/cancel/command/paste_last the binding is gone (Clear had fallen through to ApplySettings re-load — verify no stale fire); for ptt the row is empty but dictation still works on Ctrl+Win fallback (documented ptt-cannot-be-disabled). Re-record the cleared key. [ ] PASS / [ ] FAIL — notes: ____
8. Welcome regression: with first_run forced, Welcome step 2 "Detected: ..." + "That is my hotkey" still sets ptt (Home Learn empty-payload path still routes to ptt superseded by NEED — Welcome leans on `SetDetectedHotkey`, unchanged). [ ] PASS / [ ] FAIL — notes: ____
9. Armed navigation: click Record on Hands-free, click Home before pressing (learn stays armed on the hook thread), press a chord → it saves to Hands-free (armed survives page switch), no dictation fires during learn. [ ] PASS / [ ] FAIL — notes: ____

## Unverified Assumptions & Remaining Risks
- [ASSUMED] Hook-thread learned-state locking is correct as implemented (single `tracker_mutex`, no ordering inversion) — code-verified + MinGW-built, but the data-race fix itself needs the hardware checklist (item 9) to prove no torn chord under real key timing.
- [ASSUMED] ptt Clear fallback ("empty row but Ctrl+Win still active") is acceptable to users because it is the pre-existing `LoadBindings` contract; alternative (hide Clear for ptt) was rejected to satisfy the brief's literal row shape. Orchestrator may ask for the hide-Clear variant; single-site change in `DrawGeneralHotkeyRow`.
- A4 `##card_general_other_hotkeys` text fields now coexist with recorder rows for the same 4 keys (A3 preserved sibling uncommitted hunks; A4 explicitly planned "A3 recorder rows replace this card organically"). Until orchestrator removes the text card, typed chords bypass `ValidateRecorderChord` (no conflict/modifier check) and stale-seed behavior can show the pre-recorder string. Integration must delete the 4 `StringFieldRow` hotkey lines (keep helpers + other keys) or wire the same validation into them.
- Studio global Press (Win+Alt+N/O/S actually firing outside the Transforms page) does not exist after A3 — slot/diff/scratchpad chords are recorded/validated but activation remains via the Transforms page `TryItOut`/diff button and future engine work. Brief's `ProcessTransformShortcut` NEED is recorded as deferred design, not half-wired.
- G3 zig pending (no zig), full ctest NOT-RUN (scoped), committed doctest cases for A3 are integrator NEED (this report's scratch equivalent is the evidence).

## Git State
- Branch: feature/studio-pages (verified [RAN git branch --show-current]; never switched).
- CORRECTION (post-verification, evidenced): while A3 was editing, the orchestrator landed A4 code commit `5a2a618` (2026-11-07 18:57) with `src/core/SettingsStore.cpp + src/ui/MainWindow.{h,cpp}`. At that moment the working tree already held A3's `MainWindow.h` hunks (`SetLearnedHotkey/SetRecorderError/DrawGeneralHotkeyRow` decls + `learn_target_/learn_error_*` state) and `MainWindow.cpp` setter defs (`SetDetectedHotkey` clear + `SetLearnedHotkey` + `SetRecorderError`) — so the A4 explicit-path commit swept those A3 hunks under `Task: T-025 / Agent: W4-IMPL-A4` trailers. Verified: [RAN git show 5a2a618:src/ui/MainWindow.h -> SetLearnedHotkey True, DrawGeneralHotkeyRow True, learn_target_ True] and [RAN git show 5a2a618:src/ui/MainWindow.cpp -> SetLearnedHotkey def True, DrawGeneralHotkeyRow def False (not yet written)]. HEAD therefore declared `DrawGeneralHotkeyRow` without a definition until this A3 pass added it (HEAD `lwf_tests` unaffected — it does not link `MainWindow.cpp`; `local-wisper-flow` HEAD link was additionally blocked on A2 FileDialogs anyway). Orchestrator: accept the attribution or amend/split `5a2a618` (A3 hunks are exactly the setter/state/decl lines listed above; `git show 5a2a618` + `git diff` isolate them). A3 never committed and never staged.
- Remaining A3 working-tree diff vs HEAD (my files only, explicit paths for the A3 code commit): `src/os/WindowsHotkeyListener.h`, `src/os/WindowsHotkeyListener.cpp`, `src/core/HotkeyBinding.h`, `src/core/HotkeyBinding.cpp`, `src/core/UiBus.h` (no `UiBus.cpp` change — header-inline statics), `src/ui/MainWindow.cpp` (only `DrawGeneralHotkeyRow` def + `##card_general_hotkey` rewrite; `MainWindow.h` has no remaining diff vs HEAD because it is already committed, see above). Trailers: `Task: T-024 / Agent: W4-IMPL-A3 / Wave: 4 / Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-A3-T-024.md`. Did NOT commit (orchestrator sequencing per brief); code left in working tree.
- Docs commit (explicit path only): `docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-A3-T-024.md` (this report; not yet committed either — orchestrator commits docs). Left uncommitted (never staged by A3): `docs/agent-ops/20261107-studio/COMMS.md` (A3 START/CLAIM/CONFLICT/NEED/CORRECTION appended per protocol; orchestrator owns this file), sibling untracked (briefs `W4-IMPL-A*.md`/`D/N/S/T.md`, `W4-IMPL-S-T-017.md`, `insights_page_test.exe`, `src/ui/pages/InsightsPage.h`, `tests/test_insights_page.cpp`). Scratch harness (`C:/Users/devan/AppData/Local/Temp/opencode/lwf_a3_verify.*`, `lwf_a3_dbg.*`) is outside the repo and will not be committed. No `git add -A`, stash, reset, checkout, rebase, or clean used.




