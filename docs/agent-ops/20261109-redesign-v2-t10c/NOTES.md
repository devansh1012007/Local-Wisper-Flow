# T10c capture notes

- Script: `t10c-capture.ps1` (mirrors the T10b snippets capture: isolated
  `LWF_HOME` `build/t10c-lwf-home` seeded from `build/t10b-lwf-home` with
  `first_run=false`, `whisper_model_path=ggml-base.en.bin`,
  `theme=graphite`, `last_page` set to `Transforms`, and
  `transforms.json` `enabled=true` so the full banner renders, tray
  `Request-OpenSettings`, `scripts/capture_baseline.ps1` `-Method Screen`).
- Captures: `t10c_transforms_960x640.png` (57497 B) and
  `t10c_transforms_760x520.png` (51193 B) @100% DPI.
- The Transforms page renders on the Graphite (live palette) background:
  header `Transforms BETA` with the opt-in cluster (info `i`, toggle on,
  `Win Alt O` keycaps, `to view changes`), the full banner
  (`Select a few words. Press a shortcut. Done.` + `Try it out` +
  `How it works` + close `x`), then `My Transforms` with `Reset to
  defaults` / `Create New` and the grid: `Polish` card, `Prompt
  Engineer` card, `Create your own` card — each built-in card shows the
  new per-slot `Run` button anchored to its bottom edge.
- Slot honesty (plan AC): each card's `Run` posts its real slot
  (`RunSlot(entry.slot)` -> `StartTransform` `to_string(slot)` ->
  `engine.RequestTransform(N)`); the banner `Try it out` keeps its fixed
  polish payload `"1"` as a documented shortcut (existing polish pin
  unchanged and still green). `edit_error` now renders in
  `palette.err` (FLAG comment removed); the prompt counter and the
  `Shortcut conflict` line stay `palette.muted` (correct).
- Owed (same subagent-input limit as T5-T10b): click-through (card Run
  on slots 1 and 2, card click opens editor, opt-in toggle, banner
  Try it out / How it works / close, editor Save/Delete/Restore, record
  flow, reset confirm, diff open/copy, toast), Tab walk, 150%/200% DPI
  matrix, and the Paper-theme eyeball.
