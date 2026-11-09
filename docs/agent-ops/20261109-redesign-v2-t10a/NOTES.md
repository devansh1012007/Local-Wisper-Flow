# T10a capture notes

- Script: `t10a-capture.ps1` (mirrors the T9 home capture: isolated
  `LWF_HOME` `build/t10a-lwf-home` seeded from `build/t9-lwf-home2` with
  `last_page` set to `Dictionary`, tray `Request-OpenSettings`,
  `scripts/capture_baseline.ps1` `-Method Screen`).
- Captures: `t10a_dictionary_960x640.png` (24322 B) and
  `t10a_dictionary_760x520.png` (23708 B) @100% DPI.
- The Dictionary page renders with the `All`/`Personal` tabs only; the
  disabled `Shared with team` pill and the dialog `Share with team`
  checkbox are gone.
- Owed (same subagent-input limit as T5-T9): click-through (add/rename/
  star/sort/search/refresh/delete + undo, dialog Enter/Cancel, tab walk),
  150%/200% DPI matrix, and the Paper-theme eyeball. The one-line
  `*current_palette_` MainWindow wiring (`DrawDictionaryPage` passes
  `*current_palette_`) is also owed; the page already accepts an explicit
  palette with a Graphite default.
