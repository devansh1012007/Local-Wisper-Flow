# Brief — W4-IMPL-N (implementer) · wave 4 · run 20261107-studio

## 1. Mission
Scratchpad page + editor (plan P23/F8) on Wave 3 shell/widgets + NotesStore + Wave 2 scratchpad target + Overlay button. Title "Scratchpad" + BadgeBeta; top-right "Add to Flow Bar" info + ToggleSwitch (default OFF, key scratchpad_flow_bar from Wave 2) + grey pill recording shortcut (default Win+Alt+S shown "Win Alt S"; "Click to enable shortcut" until set); banner (ORIGINAL copy; LightButton "Start new note"; bottom-right mock note window 10px radius with toolbar close/sidebar/"Untitled"/+ and 2 placeholder lines); "Recents" 17px semibold + divider; search/new/refresh icons; list rows (title 15px, muted first-80-chars, modified date, pin icon; hover pin/delete); empty "No notes found" (also no-search-match); editor window (separate OS window 760x620, min 480x360): tab bar (close X per tab + plus), toolbar bold/italic/bullet/numbered/heading/mic (dictate-in-note), rich-text from Markdown (bold, italic, bullet, numbered, heading — no images v1), bottom bar word count + "Saved" muted. Dictation: push-to-talk inserts at caret when Scratchpad focused (Wave 2 target — consume it; never inject into another window).

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; R4 notes never leave PC; markdown round-trip; undo 5s; keyboard nav; NEVER share an ImGui context between windows — one window, one context, one GL context)
3. 00-mission.md, 01-work-ledger.md; plan §§D,E(P23,P09,P16),H,I.
4. INV-ui-shell (two-window architecture: MainWindow + Overlay each own ImGui+GL context; main loop RunOnce + RenderFrame pattern in src/main.cpp); Wave 1 reports W1-IMPL-02E (NotesStore API), Wave 2 E3 report (scratchpad target + Overlay dirty_ + engine seams).
5. Screenshot: Scratchpad image still outstanding (R-01) — tokens estimated; §5.5 text is the authority.

## 3. Acceptance (T-020)
- Tests: CRUD/pin/delete+undo timing/search/sort/Markdown round-trip/dictation-only-here — scope filters green; manual checklist S1n-S5n listed in report.
- Editor window: second Window — NEVER share an ImGui context with the Full window (the D1 black-screen lesson stands).

## 4. Ownership
WRITE-SET: src/ui/pages/ScratchpadPage.{h,cpp} (new), src/ui/ScratchpadWindow.{h,cpp} (new, editor window), tests/test_scratchpad_page.cpp (new)
READ-ONLY: else. FORBIDDEN: CMakeLists.txt (NEED), src/core/**, src/os/**, src/ui/Overlay.* (Wave 2 wired — read), scripts/**.
Siblings: Dictionary/Snippets/Transforms pages, Settings A2, Insights. MainWindow wiring NEED (Page::Scratchpad); editor-window creation in main loop = NEED for integration (exact lines, MainWindow/ScratchpadWindow own what they can).

## 5. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; never full ctest; one build at a time.

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w4-impl/W4-IMPL-N-T-020.md (ORCH-DIGEST first). Commits explicit paths; trailers Task: T-020 / Agent: W4-IMPL-N / Wave: 4. Final <=8 lines.
