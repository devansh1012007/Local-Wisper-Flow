# Brief — W4-IMPL-S (implementer) · wave 4 · run 20261107-studio

## 1. Mission
Snippets page (plan P20/F5) on Wave 3 shell/widgets + Wave 1 SnippetStore v2. Spec §5.3 verbatim behavior; spec §4.3 validation messages EXACT: "Please enter a snippet" / "Please enter a word" / "Snippet too long" (4000, before empty-expansion) / "Snippet already exists". Banner examples: trigger "my booking link" → expansion "https://example.com/book" (glass chips both); LightButton "Add new snippet"; rows "trigger → expansion" truncated at width; hover pencil+X; sheet edits single/multi-line (6 visible); delete = undo toast 5s; longest-match/casing/punctuation already Wave 1-tested — your page must not re-implement matching.

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; exact copy; keyboard nav)
3. 00-mission.md, 01-work-ledger.md; plan §§D,E(P20),H,I.
4. INV-ui-shell (SnippetEditorModel unwired — wire it); Wave 1 reports W1-IMPL-02D (SnippetStore v2 API + Upsert predicates after IMPR-W1-01).
5. Snippets screenshot in conversation (2 images) — rows my Flow referral → …, my email address → …, organize thoughts prompt → …; tokens estimated.

## 3. Acceptance (T-017)
- Tests: each validation message exact, longest match, casing, punctuation rule, undo delete — scope filters green; manual checklist S1+S2 listed in report.
- Snippets run after dictionary, before injection (existing order — do not touch pipeline).

## 4. Ownership
WRITE-SET: src/ui/pages/SnippetsPage.{h,cpp} (new), tests/test_snippets_page.cpp (new)
READ-ONLY: else. FORBIDDEN: CMakeLists.txt (NEED), src/core/** (besides test fixes? no — NEED), src/ui/** outside your page, scripts/**.
Siblings: Dictionary/Transforms/Scratchpad pages, Settings A2, Insights. MainWindow wiring NEED with exact lines (Page::Snippets).

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; never full ctest; one build at a time.

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-S-T-017.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-017 / Agent: W4-IMPL-S / Wave: 4. Final <=8 lines.
