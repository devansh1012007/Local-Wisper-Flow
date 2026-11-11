# T10e capture notes

- Script: `t10e-capture.ps1` (isolated `LWF_HOME` `build/t10e-lwf-home` seeded
  from `build/t10d-lwf-home` with `first_run=false`,
  `whisper_model_path=ggml-base.en.bin`, `theme=graphite`,
  `last_page=Scratchpad`, plus seeded `notes/` (`index.json` with 2 entries:
  `note-alpha` "Project Alpha" modified 2023-11-14, `note-beta` "Beta" pinned
  modified 2026-11-07, with matching `note-alpha.md`/`note-beta.md` bodies),
  tray `Request-OpenSettings`, `scripts/capture_baseline.ps1` `-Method Screen`).
- Captures: `t10e_scratchpad_960x640.png` (56674 B) and
  `t10e_scratchpad_760x520.png` (46199 B) @100% DPI.
- The Scratchpad page renders on the Graphite live palette: `Scratchpad` title
  + `B E T A` badge, `Add to Flow Bar` off + `Win Alt S` shortcut pill
  (`Click to enable shortcut`), the `Capture ideas without leaving the flow`
  banner (`Start new note` ghost button + mock note card), `Recents` with the
  search field + `?`/`+`/`R` icon buttons, then the pinned `Beta` row (`P`,
  2026-11-07) above `Project Alpha` (2023-11-14), previews truncated to 80
  chars. At 760x520 `Project Alpha` sits below the fold (scrollable, scrollbar
  visible — same min-size bottom clipping as T0.5/T8). The undo toast
  (`HeatColor(3)` Undo label, `palette.text` fill / `palette.surface` text) is
  not visible in these captures (no delete pressed).
- ScratchpadWindow (third OS window `Scratchpad`, 760x620) frame capture is
  owed: opening it requires an `OpenScratchpad` command (clicking `Start new
  note` or a recents row), and synthetic clicks do not reach the GLFW window
  in this subagent session (T5-T10d pattern; `scripts/capture_baseline.ps1`
  has no `OpenScratchpad` helper, only tray `WM_APP+8` open-settings). The
  window's palette conversion is proven by source pins (tab chrome on
  `current_palette_->surface/text/muted/accent`, clear on `current_palette_->bg`,
  queue-only `ApplyTheme` intact) rather than a frame capture.
- Home/Settings regression proof: `docs/agent-ops/20261109-redesign-v2-t13a/t13a_home_960x640.png`
  + `t13a_general_960x640.png` (pre-existing, unchanged by this task); the
  `MainWindow.cpp` diff is the single Scratchpad-branch line
  (`scratchpad_page_->Draw(scale, *current_palette_)`), Home/Settings paths
  untouched, and the full build + test suite is green.
- Owed (same subagent-input limit as T5-T10d): click-through (Start new note,
  row open, pin/delete + undo 5s, flow-bar toggle, shortcut pill arm, search/
  retakes), Scratchpad editor window frame, dictation into the focused editor,
  autosave `Saved` dwell, Tab walk, 150%/200% DPI matrix, and the Paper-theme
  eyeball.
