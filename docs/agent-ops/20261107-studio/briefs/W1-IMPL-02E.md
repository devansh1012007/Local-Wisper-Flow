# Brief — W1-IMPL-02E (implementer) · wave 1 · run 20261107-studio

## 1. Role and mission
Mission: implement NotesStore (Scratchpad backend: notes/index.json + notes/<id>.md, CRUD/pin/search, crash reconciliation). Root cause: Scratchpad has no backend at all.
Role: implementer.

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; R4: never log transcript/audio; atomic writes)
3. docs/agent-ops/20261107-studio/00-mission.md
4. Approved plan: §§C,D,E (P09), §H (T-notes incl. orphan reconcile). Two-file atomicity: md-first then index; load reconciles (drop dangling index entries / adopt or prune orphan .md).
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-datastore.md (bat-of-patterns for tmp+rename; HistoryStore for mutex precedent).

## 3. Task / acceptance
- src/core/NotesStore.{h,cpp}: Create/Update/Delete/Pin/Unpin/List (pinned first, then modified_at desc)/Search (title+body, case-insensitive); storage notes/index.json {id,title,created_at,modified_at,pinned} + notes/<id>.md (Markdown body); titles default "Untitled", first body line shown as title when unset; atomic writes (md rename first, then index rename); load reconciliation: missing .md → drop index entry; orphan .md → adopt (assign id, default timestamps?) or prune — pick one, document it, test it; thread-safe mutex; Delete keeps 5s-undo window support (structural: caller holds the deleted copy — expose the entry in the delete result so the UI toast can restore it); privacy: never leave PC (no network, no logging).
- Tests (tests/test_notesstore.cpp new): CRUD, pin ordering, search, title-from-first-line, orphan reconcile (both directions), dangling entry drop, delete-returns-entry-for-undo, concurrent Create smoke. Hermetic temp fixtures (temp_directory_path / unique subdir per case).
- Gate: NotesStore.cpp writes files → check_no_network.sh allowlist owned by 02A — post NEED with exact line, report DEFERRED-DEPENDENCY.
Acceptance: store exists, tests added, hygiene OK, NEED posted, NOT-RUN if build contention.

## 4. Ownership
WRITE-SET: src/core/NotesStore.{h,cpp}, tests/test_notesstore.cpp
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt, scripts/check_no_network.sh, src/main.cpp, src/ui/**, src/core/AtomicFile.*, TextDiff.*, InsightsModel.*, StatsStore.*, DictionaryStore.*, SnippetStore.*, TransformStore.*, AppContextResolver.*.

## 5. Siblings
02A helpers; 02B StatsStore; 02C DictionaryStore; 02D Snippet+Transform. No shared files.

## 6. Traps
No rich-text rendering here (Wave 4 editor). No images. No code comments. Don't log note bodies.

## 7. Environment
Scoped: bash scripts/check_hygiene.sh only. No cmake --build. No full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w1-impl/W1-IMPL-02E-T-009.md (ORCH-DIGEST first). Commits trailers (Task: T-009 / Agent: W1-IMPL-02E / Wave: 1 / Report: <path>). Final <=8 lines.
