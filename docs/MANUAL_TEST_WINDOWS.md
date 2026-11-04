# MANUAL TEST (WINDOWS)

Devansh runs these on a real Windows machine. Nothing here counts as verified
until a PASS box is filled. Add one section per task id with exact steps.

## Checklist index

| Stage | Section | Area |
|-------|---------|------|
| A | A0-A5 foundation | baseline dictation regression after the engine rewrite |
| B | (B2/B3 items folded into foundation + D sections) | hook, AltGr, cancel |
| C | C1 injection hardening | clipboard history, restore, per-app chords, elevated |
| C | C5 context-aware tone | profile switch per foreground app |
| C | C7 command mode | select text, voice edit, clipboard restored |
| C | C9 quiet speech boost | quiet mic gain without clipping |
| D | D2 overlay pill | states, focus pass-through, DPI |
| D | D3 tray / single instance / autostart | menu, explorer restart, Run key |
| D | D4 settings window | persistence, validation, restart badges |
| D | D5 history | search, retention, export |
| D | D6 audio feedback | tones, volume switch |
| D | D7 error surfacing | one error per 10 s window per code |
| D | D8 first-run onboarding | wizard, downloads, unverified hash warning |
| E | E1 streaming | long dictation, pause commits, cancel, streaming off |
| E | E2 resilience | mic unplug, missing models, corrupt config, disk full |
| E | E3 crash handling | crash file, one-time notice, About version |
| F | F4 installer | per-user install, tasks, uninstall data choice |
| S | S4/S5 cloud STT | consent UI, gating, fallback, secrets, cancellation |

Portable logic (FSM tables, tracker, pipeline, stores, streaming fallback,
resilience/chaos, privacy) is unit-tested on Linux and covered by ctest; the
sections below only list what needs real Windows.

## A0-A5 foundation

### A: app still dictates exactly as before
1. Build on Windows: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then
   `cmake --build build --config Release` (two-phase configure: re-run cmake after
   the first build finishes so llama links).
2. Put `ggml-base.en.bin` next to the exe or set `whisper_model_path`.
3. Launch `local-wisper-flow.exe`. Hold the hotkey, speak one sentence, release.

Expected: transcript cleaned by LLM is pasted into the focused app, FSM lines appear
in the log, dashboard still renders.

```
[ ] PASS   [ ] FAIL   notes:
```

### A4: config migration
1. Start the app once with a pre-change `config.json` (only the six v0 keys).
2. Let it save, inspect `%APPDATA%\LocalWisperFlow\config.json`.

Expected: v0 keys unchanged, new keys added with defaults, `config.json.bak` created.

```
[ ] PASS   [ ] FAIL   notes:
```

## C1: injection hardening
1. Dictate into Notepad - text pastes as before, clipboard still holds its previous
   content afterwards.
2. Copy an image (Win+Shift+S or Paint copy), dictate, then paste in Paint - the
   image must still be on the clipboard after dictation.
3. Win+V (clipboard history) after dictation - the dictation text must NOT appear.
4. Dictate into Windows Terminal - paste chord switches to ctrl+shift+v
   (app_profiles.json ships a rule) and text lands without bracketed-paste junk.
5. Open Notepad as administrator, dictate while it is focused - expect a clear
   "target app runs elevated" notification, no crash, no partial paste.
6. Dictate a >4000 character transcript (read a page aloud twice) - injection
   splits at word boundaries, no text lost.
7. Set inject_delay_ms to 200, dictate into a slow app - text still lands.

```
[ ] PASS   [ ] FAIL   notes:
```

## C5: context-aware tone
1. Dictate a sentence in Slack/Discord - tone stays casual, no trailing period
   added, no formal wording.
2. Dictate in VS Code - identifiers keep their case, no trailing period, code is
   not rewritten.
3. Dictate in Outlook - professional tone with proper punctuation.
4. Dictate in Notepad - default tone.
5. While dictating, verify no window titles appear in `logs/lwf.log`.

```
[ ] PASS   [ ] FAIL   notes:
```

## C7: command mode
1. Select a sentence in Notepad, hold the command hotkey, say "make this shorter" -
   the selection is replaced with a shorter version, clipboard keeps its previous
   content.
2. Select text, hold command, say "translate to Spanish" - selection is replaced
   with the translation.
3. Hold command with NOTHING selected - expect a clear "Select some text first"
   notification, no capture starts.
4. Hold command with the LLM model file removed (rename it, restart) - expect the
   "Command mode needs the language model" error, selection untouched.
5. Hold command and press Escape while speaking - selection and clipboard are
   untouched.

```
[ ] PASS   [ ] FAIL   notes:
```

## C9: quiet speech boost
1. Enable `quiet_speech_boost`, dictate very quietly in a noisy room - transcript
   should be usable; loud dictation is unchanged; check no clipping artifacts.

```
[ ] PASS   [ ] FAIL   notes:
```

## D2: overlay pill
1. Dictate - pill appears bottom-center 16px above the taskbar with a live
   waveform; typing in the target app continues uninterrupted (pill never takes
   focus).
2. Force an error (rename the whisper model) - red pill with a short message
   auto-hides after 3 s.
3. Cancel a dictation - brief "cancelled" pill.
4. Set `reduce_motion` - animation disabled.
5. Move the foreground window to a second monitor - pill follows that monitor's
   work area; check DPI scaling at 150%.

```
[ ] PASS   [ ] FAIL   notes:
```

## D3: tray, single instance, autostart
1. Launch the exe twice - the second exit leaves one tray icon and one hook
   (check logs).
2. Kill explorer.exe and restart it - tray icon returns.
3. Toggle "Start with Windows" in the tray menu - verify
   HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
4. Trigger an error twice within 10 s - only one balloon notification.

```
[ ] PASS   [ ] FAIL   notes:
```

## D4: settings window
1. Open Settings from the tray; change every setting; restart the app - values
   persisted.
2. Change whisper model path - "restart needed" badge appears.
3. Enter an invalid value (0 for min hold) - inline red error, nothing saved.
4. Cancel - values revert to the last save.

```
[ ] PASS   [ ] FAIL   notes:
```

## D5: history
1. Dictate a few sentences; open History (tray menu); search a word; Copy puts
   the text on the clipboard; "Add words to dictionary" adds to dictionary.json.
2. Set history_enabled=false - nothing new recorded.
3. keep_raw_in_history stays off - history.jsonl contains no raw field.

```
[ ] PASS   [ ] FAIL   notes:
```

## D6: audio feedback
1. Dictation start/stop make short tones; errors make a low tone; volume slider
   works; switching sound_enabled off silences everything; start tone does not
   delay capture (audio of the first word is not lost).

```
[ ] PASS   [ ] FAIL   notes:
```

## D7: error surfacing
1. Each failure path (mic removed, model missing, clipboard busy) shows the
   matching overlay message, tray notification and log entry exactly once per
   10 s window.

```
[ ] PASS   [ ] FAIL   notes:
```

## D8: first-run onboarding
1. Delete %APPDATA%\LocalWisperFlow and start fresh - the wizard walks through
   microphone test, hotkey learn, model list and a trial dictation.
2. models.json entries with empty sha256 show an amber "unverified file" warning.
3. Download only starts after an explicit click; killing the app mid-download and
   restarting resumes from the partial file.

```
[ ] PASS   [ ] FAIL   notes:
```

## E1: chunked streaming transcription
1. Dictate a sentence longer than ~15 s with clear pauses (> 0.7 s) between
   phrases; the final text is one continuous paragraph, in the spoken order,
   with no duplicated or dropped words at the pause boundaries.
2. Dictate a short (< 3 s) utterance; result is identical to pre-streaming
   behavior (single pass, no visible difference).
3. Set `streaming_stt` to false in config.json, restart, repeat step 1; result
   is still correct (one-shot path unchanged).
4. Press Esc during a long streaming dictation; nothing is injected, no error
   besides the cancel notice, app returns to idle.
5. With `debug_log_transcripts` off, logs contain no transcript text while
   streaming (only latency/status lines).

```
[ ] PASS   [ ] FAIL   notes:
```

## E2: resilience
1. Start a dictation and unplug the USB headset mid-utterance: the app shows a
   clear "microphone lost" error (overlay + tray), returns to idle, nothing is
   injected; plugging the mic back and dictating again works without restart.
2. Rename the whisper model file and start the app: it launches anyway, shows
   the ModelMissing notice and Settings > Speech is reachable.
3. Remove the LLM gguf and start the app: one-time "cleanup is disabled"
   notice; dictation still inserts raw text.
4. Put invalid JSON into config.json and start: notice about corrupt config;
   old file remains next to config.json.bad; app uses defaults.
5. Fill the disk (or write-protect %APPDATA%) and dictate: app never crashes;
   history silently misses entries; a warning appears in logs.

```
[ ] PASS   [ ] FAIL   notes:
```

## E3: crash handling
1. Kill the process forcefully (e.g. via Task Manager -> End Process on a debug
   build with an injected crash, or Task Scheduler): next launch shows "The app
   closed unexpectedly - see logs" exactly once; logs/ contains crash-*.txt
   with version, exception code, FSM state and the last log lines; the file is
   renamed to .seen.txt so the notice does not repeat.
2. About tab in Settings shows the app version matching the build.
3. No network traffic is generated by a crash (verify with a firewall or
   packet capture if desired).

```
[ ] PASS   [ ] FAIL   notes:
```

## F4: installer
1. Build the exe, drop onnxruntime DLLs and silero_vad.onnx into third_party/,
   compile installer/lwf.iss with Inno Setup 6, run the produced setup:
   installs per-user without any admin prompt into
   %LOCALAPPDATA%\Programs\LocalWisperFlow.
2. Start Menu shortcut works; desktop icon only when the box is ticked; the
   "Start with Windows" task writes HKCU\...\Run\LocalWisperFlow and unchecking
   is impossible by design (uninstall removes it; the in-app tray toggle
   manages it afterwards).
3. The app runs from the install location, imports the shipped config.json on
   first run into %APPDATA%\LocalWisperFlow, and no gguf/bin model weights are
   present in the install dir.
4. Uninstall: app is killed, files removed; when asked "remove your data?" ->
   No keeps %APPDATA%\LocalWisperFlow, Yes deletes it.
5. An unsigned build triggers SmartScreen on first launch ("More info" ->
   "Run anyway"); signing removes it.

```
[ ] PASS   [ ] FAIL   notes:
```

## R: GUI redesign step 1 (theme, mini, full window)

Build first, then run through these in order. Mark PASS only on real Windows
hardware. All of R replaces the old dashboard/settings/wizard windows.

### R-a: window architecture (audit D1 fix)
1. Build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then
   `cmake --build build --config Release` (two-phase as before).
2. Launch. Expect: no debug assert at startup; a small dot (the mini badge)
   bottom-center just above the taskbar; no main window unless first run.
3. Quit from the tray. Expect: clean exit, no crash, nothing in the crash
   folder (old code null-derefed on double shutdown).

### R-b: mini badge states
1. Ready: outline mic at reduced opacity. Hover it: it should come to full
   opacity and the desktop under it should not be blocked once you move away.
2. Hold the PTT hotkey and speak: filled brass mic, breathing ring, five
   level bars moving with your voice.
3. Release: three thinking dots, then a brief check, then back to Ready.
4. Tray: disable dictation. Badge shows the paused mic (muted, slash). Click
   it: dictation re-enables (verify the tray checkmark follows).
5. Stop the mic (unplug or privacy toggle) and trigger a dictation: the
   attention dot appears; hover it to read the message; it clears after 6 s.
6. Set `"reduce_motion": true` in `%APPDATA%` config.json and restart.
   Repeat 2-3: ring and dots static, bars settle. Turn it back off.

### R-c: full window
1. Tray > Settings...: the full window opens (960x640 default, native title
   bar, rail with Home/Speech/Cleanup/Privacy/About, dark Hunter-and-Brass
   theme).
2. Home: Right now line, hotkey chips, Trust card, Quick settings toggles,
   Last dictation (dictate something first, then Copy it back from the
   clipboard).
3. Quick settings: toggle Dictation off/on - the badge and the tray checkmark
   must follow immediately. Toggle Clean up text and Hands-free: each shows
   "Saved" top-right once.
4. Speech: model path edit shows "Applies after restart." after commit;
   language combo works; Boost quiet speech toggle applies live.
5. Cleanup: enable, skip-short slider, timeout slider, aggressive toggle.
6. Privacy: the transcript-logging toggle shows the red warning when on.
7. Close the window with the X: it hides; the app keeps dictating. Reopen
   from the tray. Quit must still come from the tray only.

### R-d: first run
1. Delete `%APPDATA%\LocalWisperFlow\config.json` (or set first_run=true and
   clear whisper_model_path), relaunch.
2. The full window opens on Welcome: mic test (bar moves), microphone
   settings button opens ms-settings, hotkey chord capture, model list with
   unverified warnings, finish lands on Home.
3. Relaunch: no Welcome; window stays hidden until opened.

### R-e: regression basics
1. One normal dictation end to end: text lands where the cursor is.
2. Cancel one dictation with the cancel hotkey: badge returns to Ready, no
   attention dot.
3. Second instance: launching the exe again just opens the full window.
