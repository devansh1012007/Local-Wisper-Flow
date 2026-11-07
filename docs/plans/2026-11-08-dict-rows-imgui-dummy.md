# Plan — dict_rows ImGui Dummy fix (2026-11-08)

## Problem
ImGui 1.92 errors `Please submit Dummy() afterwards...` in `##dict_rows` when the last row is hovered/focused. `DictionaryPage::DrawRows` restores the cursor with a trailing `SetCursorScreenPos(saved_cursor)` at `DictionaryPage.cpp:331` and ends the child at `:334` with no item. The restore sits `ItemSpacing` (8px) beyond item bounds, so ImGui asserts on `EndChild`. Same idiom in `SnippetsPage.cpp:218` and `InsightsPage.cpp:300`.

## Goal / non-goals
- Goal: zero ImGui error on last-row hover/focus in all three pages; row pitch and button geometry unchanged.
- Non-goals: no rewrite of overlay buttons to draw-list (loses keyboard nav/hit-testing); no change to `Toast::Draw`/`ScratchpadPage` parent-window restores (unconfirmed, out of scope, see risks).

## Acceptance criteria
1. `DictionaryPage.cpp:331`, `SnippetsPage.cpp:218`, `InsightsPage.cpp:300` each followed by `ImGui::Dummy(ImVec2(0.0f, 0.0f));`.
2. `ctest --test-dir build` green (23/23-equivalent suite), `scripts/check_hygiene.sh` OK, `scripts/check_no_network.sh` OK.
3. Manual repro (Devansh, Windows): Dictionary/Snippets/Insights → hover/Tab-focus last row → no error, rows unchanged.

## Facts (from W0 investigator) vs assumptions
- Facts: file:line evidence above; `TransformsPage.cpp:357-358` already does the `Dummy` remedy; empty list cannot trigger (has its own `Dummy`).
- Assumptions: user build == HEAD; trigger is hover or keyboard focus on last row.

## Candidate approaches
- **A. `Dummy(0,0)` after each restore (chosen).** Smallest, layout-preserving, matches precedent. Cost: 1 line × 3 files.
- **B. Delete the restore.** Rejected — next-row Y drifts by row(52px)−button(40px) delta, breaks separator geometry.
- **C. Skip restore only on last row.** Rejected — special-case pitch drift on last row; same bounds-check fragility.

## Atomic tasks + dependency graph
- T-001 (single task, no parallelization possible — 3 files, one idiom): add the 3 `Dummy(0,0)` lines; add source-scan regression in `tests/test_dictionary_page.cpp` asserting a `Dummy` follows the restore (established pattern at L314-341). No dependencies. Done criteria: AC 1-2.

## Blast radius
- Public APIs / schema / migrations: none. Config: none. Runtime: child auto-resize height grows by intended spacing only (restore already encoded it).
- Rollback: revert 3 lines + test.
- Risk: `Toast::Draw`/`ScratchpadPage:319-324` parent-window restore may show the same assert in `##content` — out of scope here; logged as follow-up. Manual verification still required on Windows hardware (G3 zig pending, as elsewhere).

## Status: done (8773d94 + compensated follow-up d78fc42, delta-review APPROVE; manual Windows repro pending)

## Review log
- R1 (8773d94): REJECT — plain `Dummy(0,0)` added ItemSpacing.y drift (+8px hover shift / AutoResize growth / per-row compounding in Insights). Verified vs ItemSize math.
- R1-fix (d78fc42): restore compensated by `- ImGui::GetStyle().ItemSpacing.y` before `Dummy` → CursorPos lands at saved cursor, CursorMaxPos at row bottom (layout-invariant). Delta-review APPROVE, gates green. Test fails on plain-Dummy sources, passes on compensated.
- Follow-up (untouched, pre-existing): same trailing-restore pattern in ScratchpadPage.cpp:297/324 (parent-window, unconfirmed) — not in this fix.
- G3 zig + Windows runtime + live-API checks: NOT RUN (honest carry).
