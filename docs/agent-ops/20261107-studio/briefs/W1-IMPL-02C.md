# Brief — W1-IMPL-02C (implementer) · wave 1 · run 20261107-studio

## 1. Role and mission
Mission: implement DictionaryStore (v2 file handling replacing PersonalDictionary's file handling, keeping its public functions working) — the highest-risk data-layer task (migration + first-ever Save path). Root cause: dictionary.json is load-only, no UI, no fix counts.
Role: implementer.

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; additive keys; never break existing JSON)
3. docs/agent-ops/20261107-studio/00-mission.md
4. Approved plan: §§C,D,E (P06, P13), §H (T-mig x4, T-dup, T-count, T-reload), §I. D-OP2: single shared dictionary instance (pipeline + page + cloud keyterms) — design for reload-on-save, not restart.
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-datastore.md (PersonalDictionary legacy/modern shapes, Enforce string-only today, DictionaryEditorModel exists unwired — reuse it if it helps).
6. Read src/core/PersonalDictionary.{h,cpp} fully before coding; read src/core/SettingsUiModel.{h,cpp}.

## 3. Task / acceptance
- src/core/DictionaryStore.{h,cpp}: Entry{id, phrase, replacement (optional), starred, source manual|imported, usage_count, created_at, updated_at}; migration reads all 4 shapes ever written (bare string array, {words:[]}, {words:[],replacements:[{from,to}]}, v2 with schema_version 2) — write v2, keep dictionary.json.bak of the pre-migration file (once); first-run seed {phrase:"btw", replacement:"by the way", source:"manual"} (do NOT seed user name); rules: one replacement per entry, case-insensitive trimmed duplicate → "Word already exists.", max 2000 entries, search phrase+replacement case-insensitive substring, starred-first then newer-first; Save atomic (tmp+rename pattern, same as StatsStore sister — do NOT depend on 02A's AtomicFile header); import/export JSON v2.
- PersonalDictionary: add fix-count return from Enforce without breaking old callers (overload or out-param with default). Keep Load() working.
- Shared-instance prep: if PersonalDictionary holds loaded state, add `Reload()` semantics so page edits can refresh pipeline + cloud keyterms (D-OP2) — implement the reload hook here; wiring into main.cpp is Wave 2 (do NOT touch main.cpp).
- Gate: DictionaryStore.cpp writes files → needs check_no_network.sh allowlist; that script is owned by 02A — post NEED with exact line, finish safe work, report DEFERRED-DEPENDENCY.
- Tests (tests/test_dictionarystore.cpp new): 4-shape migration (fixture files in-test), dup rejection, seed on first run, search, starred priority sort, max 2000 (boundary: 2000 ok, 2001 rejected), case-insensitive trim dedup, fix-count correctness (fake replacements), .bak created once. Hermetic temp fixtures; never touch the repo's real dictionary.json (V0ConfigFixture). Use temp_directory_path.
Acceptance: store + migration + count overload exist, tests added, hygiene OK, NOT-RUN noted if build contention, NEED posted.

## 4. Ownership
WRITE-SET: src/core/DictionaryStore.{h,cpp}, src/core/PersonalDictionary.{h,cpp} (count overload only), tests/test_dictionarystore.cpp
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt, scripts/check_no_network.sh, src/main.cpp, src/ui/**, src/core/AtomicFile.*, TextDiff.*, StatsStore.*, SnippetStore.*, TransformStore.*, NotesStore.*, AppContextResolver.*.

## 5. Siblings
02A helpers; 02B StatsStore; 02D Snippet+Transform; 02E Notes. No shared files.

## 6. Traps
Do NOT change Enforce matching order (whole-word case-insensitive, longer phrases first) — P06 keeps pipeline order. Do NOT add cloud/sync features. R5: never break existing files. Cap 2000 documented in header/schema comment-free form (header doc block must be free of code comments — describe via no comments; use commit msg + report).

## 7. Environment
Scoped: bash scripts/check_hygiene.sh only. No cmake --build (shared build dir). No full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w1-impl/W1-IMPL-02C-T-006.md (ORCH-DIGEST first). Commits trailers (Task: T-006 / Agent: W1-IMPL-02C / Wave: 1 / Report: <path>). Final <=8 lines.
