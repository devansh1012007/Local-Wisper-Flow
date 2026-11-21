# Mic diagnosis + device selection (2026-11-08)

Branch: `ops/ui-full-redesign` (sequential after `1bc8653` API-key rows).

## Problem
User reports no words after speaking. Investigation (two read-only passes, evidence below):
- PCM is captured but silent: engine reached the peak gate (`DictationEngine.cpp:561-568`
  `audio: peak level X < 0.005`), which only runs when `full.size() >= min_tap_samples`.
- Init failure ruled out: ctor throws -> `fatal:` + exit 1 (`main.cpp:803-806`).
- Callback alive: would otherwise log `tap ignored: recording too short`, never the peak line.
- Root cause: system default capture endpoint delivers digital silence (wrong/muted/OS
  privacy-blocked device). `MiniaudioCapture` always opens `NULL` = default
  (`MiniaudioCapture.cpp:22-30`; miniaudio.h:8766-8768 "NULL uses system default").
- Dead plumbing found: `mic_device` schema key exists (`SettingsStore.cpp:52`,
  hot_apply=false) + free-text row (`MainWindow.cpp:1732-1734`) but zero readers in src/.

## Goal
1. Diagnosable at runtime: log the active capture device name (no transcripts/secrets).
2. User can select the microphone in Settings -> Speech; persisted via existing
   `mic_device` key; restart-banner model (schema hot_apply=false matches ctor-time init).

## Non-goals
- Hot-swap while running (device init is ctor-time; restart banner is the contract).
- VAD/STT/filter/injector changes; config schema changes (key already exists).
- Changing `IAudioCapture` interface (selection lives on concrete MiniaudioCapture).

## Facts (verified file:line)
- miniaudio 0.11.25 (CMakeLists.txt:52); `ma_context_get_devices` miniaudio.h:8490;
  `capture.pDeviceID` :7131; NULL=default :8766-8768; both guarded by
  `#ifndef MA_NO_DEVICE_IO` (undefined repo-wide).
- `MiniaudioCapture` compiled only in full target (CMakeLists.txt:347-349); lwf_tests
  never compiles it -> adapter code verified by manual checklist (repo rule).
- `ma_device_init(nullptr,...)` self-manages context today; enumeration + id use needs
  an owned `ma_context` member for id validity across contexts.
- Only construction site: `main.cpp:334` `MiniaudioCapture audio(kSampleRate, 1);`.
- MainWindow holds `const MiniaudioCapture&` (MainWindow.h:95,205) — listing is a
  static method, no const problem.
- Combo codebase pattern: `BeginCombo` + `SetNextItemWidth(-1.0f)` (MainWindow.cpp:1530).
- Copy-lint bans whole words {oops,fsm,vad,llm,whisper,provider,providers,pipeline,
  pipelines,stt} + `!` in src/ui literals (test_overlay_settings_ui.cpp:1113-1114).

## Approach (chosen)
A (core/audio, sequential) then B (UI), disjoint files, interface fixed up front:
- A: `MiniaudioCapture` gains owned `ma_context`, ctor param
  `std::string device_name = ""` (additive, default = today's behavior), static
  `ListCaptureDevices()` returning display names; name->id match with fallback to
  default on no-match or init-fail (app must always start); log active device name.
  `main.cpp` reads `mic_device` before construction (verify settings load order).
- B: replace dead free-text General row with a picker card `##card_mic_device` in
  DrawSpeechPage (between model and level cards); `BeginCombo ##mic_pick`, entries
  "System default" + `ListCaptureDevices()` (lazy-cached, not per-frame); select ->
  `SetString("mic_device", ...)` + `restart_needed_ = true` + `ApplyAndSave()`;
  pure `SettingsUiModel::MatchMicDeviceIndex` for selection highlight + unit tests;
  source-scan pins (card/id presence, SetString+restart ordering, schema-key->control
  scan still green).

## Rejected alternatives
- Free-text field (status quo): unusable, and dead (no reader).
- Hot-swap on selection: needs device mutex + engine-queue command; restart banner is
  the existing contract for hot_apply=false keys — smaller blast radius.
- Persisting raw `ma_device_id` union bytes: backend-specific, not human-readable;
  name match with default fallback is sufficient (duplicate names -> first match).

## Acceptance criteria
- AC1: app starts even if stored `mic_device` is stale/absent (falls back to default,
  log says which device is active).
- AC2: Settings -> Speech shows a mic picker; selecting persists + shows restart
  banner; "System default" is the empty-value entry.
- AC3: gates green: lwf_tests 1/1, hygiene, no-network; full exe links; no new pins
  weakened; copy-lint clean.
- AC4: docs — PROGRESS row + MANUAL_TEST_WINDOWS mic-selection steps (level meter
  moves for chosen device).

## Risks
- Device list empty (no capture devices): picker shows only System default — must not
  crash or assert (never assert non-empty in tests; null backend on Linux CI).
- Name ambiguity (two identical names): first match wins; documented.
- Settings load order vs audio construction in main.cpp: A verifies and reorders if
  needed (settings is independent of audio).
