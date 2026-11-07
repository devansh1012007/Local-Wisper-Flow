# Brief — W4-IMPL-T (implementer) · wave 4 · run 20261107-studio

## 1. Mission
Transforms page (plan P22/F7) on Wave 3 shell/widgets + Wave 1 TransformStore + Wave 2 run flow. Title "Transforms" + BadgeBeta; top-right "Opt in" info + ToggleSwitch + grey group keycaps Win Alt o + "to view changes" (live: shows real View Diff shortcut); banner (ORIGINAL copy, neutral glyphs — NO real app logos (Slack/Gmail/LinkedIn forbidden); "Try it out" LightButton + "How it works" text link); "My Transforms" serif headline + Reset-to-defaults (reset icon, confirmation listing how many customs deleted) + PrimaryButton "Create New"; card grid 3-col (14px radius, card_bg_white, 20px, 16px gap): Card1 Polish Win Alt 1 "Improve clarity and conciseness", Card2 Prompt Engineer Win Alt 2 "Constructs optimal prompts", Card3 "+" "Create your own / Upload your own prompt" (disappears at 8 customs), further customs sorted by slot; editor (name, prompt multi-line 2000 counter, shortcut recorder w/ modifier-required + conflict check, Save/Delete); opt-in OFF: cards dimmed 50%, editing works, shortcuts inactive, banner "Turn on Opt in to use transforms."; diff viewer Win+Alt+O (before/after word-diff, teal insert + muted strikethrough removal, "1 Change"/"N Changes", Copy result) — the Wave 2 engine retains 40s (consume HasTransformDiff/TransformDiffSnapshot, do NOT rebuild that).

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; selection is data — keep D5 delimiter copy; R4 no transcript logging)
3. 00-mission.md, 01-work-ledger.md; plan §§D,E(P22,P08,P15),H,I.
4. INV-enginehooks (WindowsSelectionCapture live technique already in engine — read E2's report reports/w2-impl/W2-IMPL-E2-T-012.md for the seam + 8000ms timeout/cancel + caps from IMPR-W2-01).
5. Transforms screenshot in conversation (Badge Beta, Opt in toggle, Win Alt o, Try it out, My Transforms, card rows) — tokens estimated; real logos NOT to be emulated.

## 3. Acceptance (T-019)
- Tests: opt-in OFF registers nothing (assert gate), diff counts (1/N Changes), reset confirmation count, shortcut recorder modifier/conflict — scope filters green; manual checklist T1-T6 listed in report.
- Transform cards' shortcuts disabled while opt-in OFF; but a card editor must still work.

## 4. Ownership
WRITE-SET: src/ui/pages/TransformsPage.{h,cpp} (new), tests/test_transforms_page.cpp (new)
READ-ONLY: else. FORBIDDEN: CMakeLists.txt (NEED), src/core/**, src/ui/** outside your page, scripts/**.
Siblings: Dictionary/Snippets/Scratchpad pages, Settings A2, Insights. MainWindow wiring NEED (Page::Transforms); StartTransform/Win+Alt+O UI surface: your page owns the recorder + viewer UI, engine seams exist.

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; never full ctest; one build at a time.

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-T-T-019.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-019 / Agent: W4-IMPL-T / Wave: 4. Final <=8 lines.
