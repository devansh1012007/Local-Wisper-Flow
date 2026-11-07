# Brief — W4-IMPL-I (implementer) · wave 4 · run 20261107-studio

## 1. Mission
Insights page (plan P21/F6) on Wave 3 shell/widgets + Wave 1 StatsStore/InsightsModel/TextDiff + Wave 2 stats hook. Spec §5.1 exactly: title "Insights" 28px; top-right 56px circular share badge (teal_300 disk, repeating "SHARE •" ring, upload icon; click → share dialog with two cards "Flow Highlights" (streak, wpm, total words, apps) + "Flow Streak" (current streak), buttons "Copy image"/"Download image" disabled labelled "Coming soon" (image rendering = v2, no PNG encoder invented)); TabRow "Your usage" (active) / "Your voice" (D15: empty state "Not available yet."); Row 1 three StatCards (~185px): A wpm big number + "WORDS PER MINUTE" + info tooltip ("Your average speaking speed while dictating.") + RadialGauge (center "Average" above number 16px; footnote "Ranking against other users needs an account, which this app does not have." — screenshot's "Top 0.1%" MUST NOT be emulated); B fixes made by Flow + divider + "{n} words corrected" / "{n} dictionary fixes" rows with info icons; C total words + "TOTAL WORDS DICTATED" + divider + monitor "Desktop / {n} words" + DISABLED OutlinePill "Download on mobile" (tooltip "Mobile is not available in this app."); Row 2: D "Desktop usage" 24px + "TOTAL APPS USED | {n}" + 6 ProgressPill rows (AI PROMPTS / EMAILS / WORK MESSAGES / PERSONAL MESSAGES / DOCUMENTS / OTHER TASKS, caps + count e.g. "17 AI PROMPTS", neutral icons); E "{n} day streak" 24px + caps "LONGEST STREAK | {n} DAYS" + month navigator (chevrons, months with data + current) + ActivityGrid + legend "More [4 squares] Less [empty]". Empty state (no data): cards show "0" and "—", D shows "No dictations yet. Hold Ctrl + Win to start.", E shows empty calendar. Refresh: StatsStore generation counter re-read on render (no polling — Wave 1 StatsStore exposes it; verify its name in the header before using).

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; R3 never invent stats — empty states, never placeholder numbers; R4 transcripts never logged)
3. 00-mission.md, 01-work-ledger.md; plan §§D,E(P05,P04,P12,P21),H,I.
4. INV-enginehooks (Data source: DictationRecord fields; StatsStore API from W1-IMPL-02B report), Wave 2 E1 report for the Record contract.
5. Insights screenshot in conversation (154 WPM / 43 fixes / 974 words / 17 AI PROMPTS 100% / 0 STAKES, activity legend) — emulate layout, NOT the "Top 0.1%" / enabled mobile pill.

## 3. Acceptance (T-018)
- Tests: WPM with 0/59/60 seconds of speech; streak across midnight local; longest streak across a gap; calendar levels; category shares sum 100 or 0; distinct app count — scope filters green; manual checklist I1+I2+I3 listed in report.
- StatsStore generation counter must be the refresh trigger (no timer/polling).

## 4. Ownership
WRITE-SET: src/ui/pages/InsightsPage.{h,cpp} (new), tests/test_insights_page.cpp (new)
READ-ONLY: else. FORBIDDEN: CMakeLists.txt (NEED), src/core/** (besides the D-OP index header tweak? — no; NEED if blocked), src/ui/** outside your page.
Siblings: batch 1 pages (landed), A4 all-keys + A3 recorders (batch 2 parallel). MainWindow wiring NEED (Page::Insights).

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; never full ctest; one build at a time.

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-I-T-018.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-018 / Agent: W4-IMPL-I / Wave: 4. Final <=8 lines.
