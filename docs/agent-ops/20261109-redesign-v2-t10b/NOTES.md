# T10b capture notes

- Script: `t10b-capture.ps1` (mirrors the T10a dictionary capture: isolated
  `LWF_HOME` `build/t10b-lwf-home` seeded from `build/t9-lwf-home2` with
  `first_run=false`, `whisper_model_path=ggml-base.en.bin`,
  `theme=graphite`, `last_page` set to `Snippets`, plus a seeded
  `snippets.json` (3 entries: `my email`, `my booking link`,
  `home address`) so the ruled rows render, tray `Request-OpenSettings`,
  `scripts/capture_baseline.ps1` `-Method Screen`).
- Captures: `t10b_snippets_960x640.png` (27591 B) and
  `t10b_snippets_760x520.png` (25026 B) @100% DPI.
- The Snippets page renders on the Graphite window background: banner
  `Reusable phrases.` with the example pair chips, then three ruled rows
  (`trigger → expansion`) separated by 1 px hairlines on `palette.bg` —
  confirm from the capture before the Paper-theme eyeball.
- Owed (same subagent-input limit as T5-T10a): click-through (Add new,
  row edit, hover edit/delete, sheet Cancel/Delete/Save, trigger-Enter
  save, delete + Undo toast, banner close), Tab walk, 150%/200% DPI
  matrix, and the Paper-theme eyeball.
