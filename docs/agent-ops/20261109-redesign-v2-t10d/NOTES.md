# T10d capture notes

- Script: `t10d-capture.ps1` (mirrors the T10c capture: isolated `LWF_HOME`
  `build/t10d-lwf-home` seeded from `build/t10c-lwf-home` with
  `first_run=false`, `whisper_model_path=ggml-base.en.bin`,
  `theme=graphite`, `last_page=Insights`, plus a seeded `stats.json`
  (5 days, 2026-11-06..10, 1524 words total) so WPM / streak / grid render
  with data, tray `Request-OpenSettings`, `scripts/capture_baseline.ps1`
  `-Method Screen`).
- Captures: `t10d_insights_960x640.png` (54766 B) and
  `t10d_insights_760x520.png` (40022 B) @100% DPI.
- The Insights page renders on the Graphite (live palette) background:
  `Insights` title alone (share badge and tab row removed honestly, no
  stub remains), then the 3 top cards (138 WPM `Average` gauge; 16 fixes
  with `10 words corrected` / `6 dictionary fixes`; 1524 total words with
  `Desktop / 1524 words` + disabled `Download on mobile` pill) and the 2
  bottom cards (`Desktop usage` with `TOTAL APPS USED | 2` + category
  pills; `5 day streak` with `LONGEST STREAK | 5 DAYS`, `November 2026`
  month nav `<` / `>`, and the lime/grey activity grid + legend).
- Cards use the hairline chrome (`palette.bg` fill + Borders hairline);
  the HeatColor ramp fills are kept for the grid and legend swatches as
  data viz. Palette threading is explicit (`palette.bg/border/muted/text/
  accent`); no Paper read remains in the page.
- Owed (same subagent-input limit as T5-T10c): click-through (month
  `<` / `>` nav, info tooltips, mobile pill disabled tooltip), Tab walk,
  150%/200% DPI matrix, and the Paper-theme eyeball.
