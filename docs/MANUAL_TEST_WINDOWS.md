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
| S | S4-S8 cloud STT | consent UI, gating, fallback, secrets, cancellation, three providers |
| MIC | MIC-003 microphone picker | Settings > Speech device list, restart banner, level meter per device |
| OR | OR-UI-01 OpenRouter model picker | Settings > Cleanup live model list + manual id |

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

## S4-S8: cloud STT (Gnani, ElevenLabs, Muse)
1. Fresh install: Settings > Cloud shows provider local/gnani/elevenlabs/muse;
   consent box is unchecked; dictated text still comes from local Whisper.
2. Tick the consent box, set stt_provider=gnani, do NOT set GNANI_API_KEY,
   restart: log carries a warning that the key env variable is not set, and
   dictation still works through local Whisper (no cloud request). Repeat for
   ELEVENLABS_API_KEY and META_MODEL_API_KEY.
3. Set GNANI_API_KEY, run `python scripts/gnani_smoke.py --file
   tests/data/sample.wav --dry-run` (layout check), then without --dry-run:
   the live response shape must match src/stt/cloud/GnaniContract.h; if field
   names differ, update the contract constants before trusting cloud mode.
   For elevenlabs/muse no offline smoke script exists - verify the first live
   dictation against the contract sections of docs/STT_CLOUD.md.
4. With cloud enabled per provider, dictate: log lines show provider=<name>,
   request_id, status, bytes and latency - never the key, never the
   transcript.
5. Break the key (set the env var to garbage): the utterance still lands
   (cloud-to-local fallback for that utterance) and the log shows the auth
   error category.
6. Press Esc while a cloud transcription is in flight: nothing is injected,
   no fallback fires, app returns to idle.
7. Search the whole logs directory for all three API key values: zero hits.
   Also confirm config.json never contains any of them.
8. stt_language mapping: set stt_language="kn,hi" - gnani receives language
   kn,hi; elevenlabs receives language_code kn; muse receives languageBias
   ["Kannada","Hindi"] (check the first live request with a proxy or the
   vendor dashboards' request logs).

```
[ ] PASS   [ ] FAIL   notes:
```

## R: GUI redesign step 1 (theme, mini, full window)

> 2026-11-06 bring-up note: the long-standing "black screen" root cause is fixed
> (1a8304f - both windows had silently ended up sharing one ImGui context, so the
> Full window rendered clipped to the 140x70 Mini badge's size at the bottom-left).
> The model path also now resolves when the exe is in `build/` (exe-dir parent +
> cwd fallbacks, ba46cbf/db4e40a). Both windows render (verified by window capture).
> Every item below is still UNVERIFIED until ticked on this machine.

> 2026-11-10 sweep note (T16): the UI nouns below were updated to the Hairline
> reality (top strip instead of the left rail, Graphite/Paper instead of
> Hunter/Brass/Ivory); the step structure and the page content they describe
> are the R1-era shapes — for the CURRENT ui-redesign-v2 checks run section
> V2 at the bottom of this file.


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
2. Hold the PTT hotkey and speak: filled mic in the accent color, breathing
   ring, five level bars moving with your voice.
3. Release: three thinking dots, then a brief check, then back to Ready.
4. Tray: disable dictation. Badge shows the paused mic (muted, slash). Click
   it: dictation re-enables (verify the tray checkmark follows).
5. Stop the mic (unplug or privacy toggle) and trigger a dictation: the
   attention dot appears; hover it to read the message; it clears after 6 s.
6. Set `"reduce_motion": true` in `%APPDATA%` config.json and restart.
   Repeat 2-3: ring and dots static, bars settle. Turn it back off.

### R-c: full window
1. Tray > Settings...: the full window opens (960x640 default, native title
   bar, top-strip navigation, dark Graphite theme — Paper is selectable in
   Settings > General and swaps live).
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
2. The full window opens on Welcome: intro paragraph, the current
   push-to-talk chord (read-only), the model list with unverified warnings
   and the Download/Browse row, and Get started lands on Home. (The 4-step
   mic-test wizard was retired in followups-1.)
3. Relaunch: no Welcome; window stays hidden until opened.

### R-e: regression basics
1. One normal dictation end to end: text lands where the cursor is.
2. Cancel one dictation with the cancel hotkey: badge returns to Ready, no
   attention dot.
3. Second instance: launching the exe again just opens the full window.
   (OWED/BLOCKED, pre-PR review: `SingleInstance::SignalOpenSettings` only
   opens the `Local\LwfOpenSettingsEvent` event — nothing in src/ creates or
   waits on it, so the second-instance notification is dead end-to-end and
   this step cannot pass until the event watcher is added; see V2 step 6.)

## R2: 2026-11 wave-run UI changes (branch fix/gui-typography-dpi)
These landed in the 20261106-gui wave run and are NOT claimed as verified.
Do them on a Windows machine at 100%, 125% and 150% DPI.

### R2-a: keyboard navigation (GUI-A1)
1. Focus the Full window; verify Tab / Shift+Tab reaches top-strip items and
   toggles in a sane order, and Arrow keys move within the top strip.
2. Focused top-strip item / toggle shows a 2 px focus ring (kDarkPalette.focus).
3. Pressing Enter or Space on a focused top-strip item switches page.
4. Mouse click still works, and clicking does not steal focus from keyboard
   navigation mid-edit (NavActive gating).

### R2-b: copy / state feedback (GUI-A3)
1. Tap Copy in history: label switches to 'Copied' for ~1.5 s and reverts.
2. Save on the Welcome page flashes the 'Saved' state exactly as Settings-page saves do.
3. Dragging a slider commits once on release (deactivation), not continuously.

### R2-c: mini badge (GUI-A5 / A6)
1. Idle badge: after the attention dwell, the frame cost drops to nothing
   (no swap until hover/drag/state change); hovering wakes it.
2. Idle alphas readable on a dark desktop (0.55 idle / 1.0 active); attention
   message text legible at 13 px. Normal badge hugs the icons (38x48 at
   100% DPI); Attention expands to 250 px
   wide and shrinks back after the dwell (followups-1).

### R2-d: DPI scaling
1. Repeat R and R2 at 100%, 125%, 150% DPI. No double-scaled text, no
   layout drift after moving the window between monitors with different DPI.

## MIC-003: microphone picker (Settings > Speech)
Device selection UI (branch `ops/ui-full-redesign`, Task B). Not claimed as
verified until a PASS box is filled.

1. Tray > Settings... > Speech: a `Microphone` card sits between `Model` and
   `Microphone level`. Its combo lists `System default` first, then every
   capture device the machine has (a machine with no capture devices must show
   only `System default` - no crash, no empty popup).
2. Pick a named device: the `Saved` label flashes and the restart banner
   appears (this setting is not hot-applied).
3. Quit from the tray and relaunch. Hold the hotkey and speak: the level meter
   moves, the log line reads `MiniaudioCapture: active capture device: <name>`,
   and the transcript lands where the cursor is.
4. Repeat steps 2-3 for each listed device in turn: the log names each chosen
   device after its restart, and the level meter responds while speaking into
   it.
5. Pick `System default`, restart: the log reports the system default capture
   device and the meter responds on the OS default input.
6. Stale-name resilience: put a name that no longer exists in
   `%APPDATA%\LocalWisperFlow\config.json` -> `mic_device`, restart: the app
   still starts, the combo previews `System default`, and the log says the name
   was not found and the system default is used.

```
[ ] PASS   [ ] FAIL   notes:
```

## OR-UI-01: OpenRouter model picker (Settings > Cleanup)

Live model-list picker with a manual id fallback (branch
`ops/ui-full-redesign`, Task B). Not claimed as verified until a PASS box is
filled.

Setup: in Settings > Cleanup choose `openrouter` in the backend combo and save
an OpenRouter key (or set `OPENROUTER_API_KEY`). Note the network state you
use for each step.

a. Offline refresh + freeze check: go offline (airplane mode / unplugged). In
   Settings > Cleanup press `Refresh model list`. Expected: the status line
   shows `Loading models...` then an error (`Could not load the model list.` +
   detail), and the app stays responsive the whole time - both the Mini badge
   and the Full window render, hover, and animate while the button reads
   `Loading models...` (the fetch is off-thread; an 8 s timeout must never
   freeze either window). Pressing Refresh again during that time does nothing
   (no second network call).
b. List load: go online and press `Refresh model list`. Expected: status
   becomes `<n> models available.` and the `Cleanup model` combo lists the
   fetched ids in sorted order. When a model is already set and it is also in
   the fetched list, the combo previews that value.
c. Select, restart, cleanup: pick a model from the combo - the `Saved` label
   flashes and the restart banner appears. Quit from the tray, relaunch, then
   hold the hotkey and dictate a sentence: the Privacy tab mirrors the picked
   model, and the returned text is cleaned (not raw dictation). Pressing
   Refresh again after the restart now offers the same list, and the picked
   model still previews in the combo.
d. Manual id: in the `Type a model id` field (below the Refresh button) type a
   valid model id not in the fetched list, press `Use this model`: the `Saved`
   label flashes and the restart banner appears; after restart the combo
   previews that id and a dictation with cleanup on works. Clearing the field
   and pressing `Use this model` sets the model to empty (same as selecting
   the `No model set` placeholder); after restart the combo previews
   `No model set` and Settings > Privacy shows `OpenRouter model: not set`.

```
[ ] PASS   [ ] FAIL   notes:
```

## V2: ui-redesign-v2 manual checks (Hairline, 2026-11-10)

Checks for the Hairline redesign landed on `ops/ui-redesign-v2` (T2b-T15).
Steps 1-4 are the new-UI basics; steps 5-9 are the owed items recorded
during the tasks. Not claimed as verified until PASS boxes are filled on real
Windows hardware; the dev sandbox has no window runtime.

1. Top-strip navigation (T4a/T4b): the strip shows the current page; strip
   press and Ctrl+<digit> land on the same page with identical persistence,
   error-clear and focus semantics (shared ActivateStripPage path).
2. Theme switcher (Settings > General, T5 + T2b): Graphite <-> Paper swaps
   live on both windows (Full, Mini badge) without a
   restart; after an app restart the selected theme persists; a hand-edited
   bogus `theme` value in config.json falls back to Graphite.
3. Settings tabs click-through eyeball at 100%, 150% and 200% DPI: no clipped
   rows, banners or error copy; focus rings visible on strip items and
   controls; the ruled rows keep their slot heights.
4. Mini badge on the live palette (T15): Ready/Listening/Thinking/Done/Paused
   colors follow the selected theme; hover rises from the idle 0.55 alpha to
   full opacity; dragging works and never fights the click-through hit-test;
   the attention message clears after ~6 s.

Owed items recorded during T0-T15 (blocked on hardware/synthetic input):

5. Real Mini Attention state: unreachable in captures - main.cpp overrides it
   to Ready; needs a synthetic failure (mic loss) on hardware while capturing.
6. Dead second-instance OpenSettings event: launching a second instance posts
   SignalOpenSettings; verify the first instance raises the Full window.
   (BLOCKED until the event watcher is added: nothing in src/ currently
   creates or waits on `Local\LwfOpenSettingsEvent` — see the R-e-3 note.)
7. HotkeyCopy and Insights stubs: the Insights "Download on mobile" pill
   is a documented stub kept deliberately (T14 disposition); decide keep,
   implement or remove. (The TryItOut banner button was removed with the
   Transforms banner in followups-1; the TryItOut() method stays as a
   unit-tested API.)
8. T13a live model download: real catalog download with progress, cancel
   mid-transfer, and quit mid-download (worker join without crash, partial
   file handled on the next start).
9. Capture reruns owed from T0.5: 150% and 200% DPI captures, and the
   Listening and Thinking states.

Followups-1 additions (ops/ui-followups-1, 2026-11-12):

10. Live knobs: change Settings > General Volume, Speech max-recording and
    hands-free-silence sliders; the next dictation tone/timeout follows the
    new values without a restart (shared LoadEngineSettings in main.cpp).
11. Banners gone: Transforms and Snippets pages open without
    their page-top banners; Transforms shows no "Turn on Opt in" strip when opted out.
    (The Scratchpad page and its banner were removed outright afterwards —
    see item 20.)
12. Built-in transform edits visible: edit Polish's prompt; the card
    subtitle shows the real prompt ("Restore default" returns the stock
    copy). The "Create your own" card is centered.
13. Mini badge extent: normal badge hugs the dot; an attention error expands it to show the
    message and it shrinks back after the dwell; drag persistence still
    clamps inside the work area at 100/150/200% DPI.
14. Switch tracks: the toggle track reads as filled gray in Graphite AND
    Paper (border token); the square knob keeps on/off state.
15. Welcome: no 4-step wizard; model row still downloads/browses; Get
    started lands on Home and clears first_run.

Followups-2 additions (fix/ui-followups-2, 2026-11-12) — item 12 is
superseded by 18 (the built-ins are gone entirely):

16. Volume sample: dragging the Volume slider plays a short sample tone at
    the dragged amplitude while you drag (throttled) and once more on
    release; with Sound off nothing plays. The next dictation start/stop
    tone uses the new volume.
17. Echo flow name: the top-left wordmark, window title, tray tooltip,
    About row and crash/log headers read "Echo flow"; the data dir, registry
    run value and window class still read LocalWisperFlow (config and
    history survive the upgrade).
18. No default transforms: a fresh profile (or the migrated existing one)
    shows an empty Transforms grid — Polish and Prompt Engineer are gone
    (load drops legacy built_in entries, warns in the log); Create New can
    fill any slot 1-10; Delete works on every slot; the reset modal says
    every slot ends up empty.
19. Mini window scratchpad icon removal: with the Flow Bar toggle on,
    right-click the note icon in the mini window — the icon disappears and
    the badge re-hugs to the dot-only width; the Scratchpad page toggle
    adds it back (the header shows the right-click hint while the icon is
    on). (Superseded by item 20: the whole Scratchpad feature is gone.)

Scratchpad removal (remove/scratchpad-page, 2026-11-12) — items 11/13/19
above keep their historical wording; the Scratchpad surface no longer
exists:

20. Scratchpad fully removed: no Scratchpad destination in the top strip
    (eight destinations, Ctrl+1..8); no Scratchpad page, no floating
    Scratchpad window, no note icon on the mini badge (always 38x48);
    Settings > General has no "Scratchpad bar" row; a stale
    `last_page: "Scratchpad"` in config.json opens on Home; dictation
    always injects into the active window and always records stats and
    history.

```
[ ] PASS   [ ] FAIL   notes:
```
