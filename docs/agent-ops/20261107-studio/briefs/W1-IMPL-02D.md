# Brief — W1-IMPL-02D (implementer) · wave 1 · run 20261107-studio

## 1. Role and mission
Mission: SnippetStore v2 (validation + longest-match + Save) and TransformStore (10 slots, opt-in, shortcuts, v1→v2 migration). Root cause: snippets load-only, transforms have no store/slots/shortcuts.
Role: implementer.

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; Prompt {} contract — TransformManager requires {text}; D7 auto-append; D5 defaults embed <selection>{text}</selection> + data-not-instructions sentence)
3. docs/agent-ops/20261107-studio/00-mission.md
4. Approved plan: §§C,D,E (P07, P08+P10), §H.
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-datastore.md (SnippetStore load-only; TransformManager throws on missing file, requires {text}, installer seeds v1 at installer/lwf.iss:43).
6. Read src/core/SnippetStore.{h,cpp}, src/core/TransformManager.{h,cpp}, src/core/SettingsUiModel.{h,cpp} (SnippetEditorModel) before coding.

## 3. Task / acceptance
- SnippetStore v2: Entry{id, trigger, expansion, created_at, updated_at}; keep Load/Expand/Normalize signatures; add Save (snippets.json v2, atomic tmp+rename — do not depend on 02A AtomicFile header); matching: case-insensitive whole-phrase, longest trigger wins (char count), punctuation/extra spaces ignored ONLY when trigger spoken alone; casing of expansion preserved; empty expansion removes entry; empty trigger ignored. Validation messages EXACT: "Please enter a snippet" / "Please enter a word" / "Snippet too long" (4000 chars, checked before empty-expansion) / "Snippet already exists" (cross-field: trigger-or-expansion matches either field of another snippet, case-insensitive).
- src/core/TransformStore.{h,cpp}: 10 slots (1 Polish + 2 Prompt Engineer built-ins, 3-10 custom ≤8), shortcuts Win+Alt+1..9,0 + diff Win+Alt+O default, fields {slot,name,prompt,shortcut,built_in,created_at,updated_at}; defaults: Polish prompt embeds <selection>{text}</selection> (D5) starting "Improve clarity, conciseness, readability and tone..." + data-not-instructions sentence; Prompt Engineer similarly "Rewrite the text as a clear..." (D5); custom prompt ≤2000 chars with {text} auto-append on save (D7); opt-in single bool default OFF; shortcut conflict check vs HotkeyBinding registry (conflict → disabled + warning); transforms.json v2 atomic; v1 array {name,prompt} → v2 conversion on first load (.bak); missing file → seed built-ins (never construct TransformManager's throwing ctor on the raw path — wrap it).
- Gate: both .cpps write files → check_no_network.sh allowlist owned by 02A — post NEED with exact lines, report DEFERRED-DEPENDENCY.
- Tests (tests/test_snippetstore_v2.cpp, tests/test_transformstore.cpp new): 4 validation messages exact; longest-match; casing preserved; punctuation-alone rule; empty expansion removes; whole-word/case-insensitive matching; TransformStore: slot seeding, {text} auto-append, 2000-char cap, opt-in default OFF, v1→v2 migration, missing-file seed, shortcut conflict flag. Hermetic temp fixtures.
Acceptance: both stores exist, tests added, hygiene OK, NEED(s) posted, NOT-RUN if build contention.

## 4. Ownership
WRITE-SET: src/core/SnippetStore.{h,cpp}, src/core/TransformStore.{h,cpp}, src/core/TransformManager.{h,cpp} (missing-file tolerance ONLY if needed — else wrap at TransformStore), tests/test_snippetstore_v2.cpp, tests/test_transformstore.cpp
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt, scripts/check_no_network.sh, src/main.cpp, src/ui/**, src/core/AtomicFile.*, TextDiff.*, InsightsModel.*, StatsStore.*, DictionaryStore.*, NotesStore.*, AppContextResolver.*.

## 5. Siblings
02A helpers; 02B StatsStore; 02C DictionaryStore; 02E NotesStore. No shared files.

## 6. Traps
R5: never break existing snippets.json/transforms.json (v1 fixtures in tests must still load). Spec §4.4: transforms never counted in stats (caller concern, Wave 2 — just don't record). No code comments.

## 7. Environment
Scoped: bash scripts/check_hygiene.sh only. No cmake --build. No full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w1-impl/W1-IMPL-02D-T-00708.md (ORCH-DIGEST first). Commits trailers (Task: T-007,T-008 / Agent: W1-IMPL-02D / Wave: 1 / Report: <path>). Final <=8 lines.
