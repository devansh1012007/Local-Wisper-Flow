# Brief — W4-IMPL-D (implementer) · wave 4 · run 20261107-studio

## 1. Mission
Dictionary page (plan P19/F4) on the Wave 3 shell + widgets + Wave 1 DictionaryStore. Title "Dictionary" + PrimaryButton "Add new"; TabRow All/Personal/Shared-with-team(disabled+tooltip); search/sort/refresh icons; banner (ORIGINAL copy only — Wispr copy forbidden per §6; close X hides until list empty again; chips = user's most-used phrases max 5); RowList rows "phrase → replacement" (→ = U+2192); hover star/pencil/delete; starred-first sort; add dialog (Word or phrase + Replace with, inline "Word already exists.", Share-with-team disabled "Not available locally"); Ctrl+F search, Esc closes; sort menu Name(A-Z)/Newest/Most-used; Refresh re-reads disk; delete = undo toast 5s; search matches phrase+replacement case-insensitive.

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; never break dictionary.json; keyboard nav; contrast)
3. 00-mission.md, 01-work-ledger.md; plan §§D,E(P19),H,I.
4. INV-ui-shell + INV-datastore (DictionaryEditorModel exists unwired — wire it); Wave 1 reports W1-IMPL-02C (DictionaryStore API + Reload hook for D-OP2 shared instance), W1-IMPR-01 (Enforce single-pass).
5. Screenshots: Dictionary screenshot in conversation (2 image files) — chips "Add new word / Wispr Flow / Samir / Sara / Karol / Spyder", rows Devansh / Wispr Flow / btw → by the way / gadev2007@gmail.com / Devansh Gaur — PIXEL-ESTIMATED tokens only (muted provisional); copy per §6 original.

## 3. Acceptance (T-016)
- Tests: add / duplicate-exact / search over phrase+replacement / starred priority / undo-delete timing (toast 5s simulated) — failing-first where feasible; scope filters green.
- Manual checklist D1+D2 ready (list in report: add by hand, dictate check, duplicate message).
- No code changes to DictionaryStore semantics (they're Wave 1-tested); your page consumes its API + Reload.

## 4. Ownership
WRITE-SET: src/ui/pages/DictionaryPage.{h,cpp} (new), tests/test_dictionary_page.cpp (new)
READ-ONLY: everything else. FORBIDDEN: CMakeLists.txt (post NEED), src/core/**, src/ui/** (outside your page), scripts/**.
Siblings: Snippets/Transforms/Scratchpad pages (own files), Settings A2 pickers, Insights. MainWindow wiring for your page = Wave 4 integration (post NEED: Page::Dictionary → your Draw function, exact lines).

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests. No full ctest. Never build in parallel with a sibling (one build at a time — retry once on lock contention).

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-D-T-016.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-016 / Agent: W4-IMPL-D / Wave: 4. Final <=8 lines.
