# W4-IMPL-S — Snippets page (T-017)

## ORCH-DIGEST
- Status: IN PROGRESS (report created early per Standing Order 21; evidence appended below).
- Scope: src/ui/pages/SnippetsPage.{h,cpp} (new) + tests/test_snippets_page.cpp (new). Nothing else touched; MainWindow/Cmake wiring are NEEDs (see below).
- Stacks on Wave 3 shell/widgets (StudioPalette, LightButton, Chip, Sheet, Toast, IconButton) + Wave 1 SnippetStore v2 (Validate/Upsert/RemoveById/Expand/Normalize). No matching re-implemented in the page.
- Pending sections: Context Report, Phased Changes, Review Audit, Verification Evidence, NEEDs.

## Context Report (pre-change investigation)

### 1. Root cause / entry points
- [VERIFIED src/ui/MainWindow.cpp:951-954] `MainWindow::DrawSnippetsPage` is a Wave-3 stub: DrawPageChrome("Snippets", "Reusable phrases.") + "Not available yet.". Page enum Page::Snippets exists [VERIFIED src/ui/MainWindow.h:24] and rail routes to it [VERIFIED MainWindow.cpp:344].
- [VERIFIED src/core/SnippetStore.h] SnippetStore v2 API: Entry{trigger,expansion,id,created_at,updated_at}; Validate(trigger,expansion,exclude_id) -> exact spec messages; Upsert(trigger,expansion,exclude_id,error); RemoveById/RemoveByTrigger; Save()/Save(path); Entries()/Warnings()/Loaded(); Expand(text) honors mode (whole default, contains). Single mutex; accessors return copies (IMPR-W1-01).
- [VERIFIED src/core/SnippetStore.cpp:397-438 ValidateLocked] Order IS the spec order: trim-empty trigger -> "Please enter a word"; code-point cap 4000 -> "Snippet too long"; trim-empty expansion -> "Please enter a snippet"; cross-field case-insensitive duplicate -> "Snippet already exists". Upsert [VERIFIED :440-504]: too-long checked BEFORE empty-expansion removal; empty expansion on existing trigger deletes and succeeds; id-alone predicate when exclude_id set (IMPR-W1-01 MAJOR-6); expansion stored verbatim, trigger trimmed.
- [VERIFIED src/core/SettingsUiModel.h:43-56 / .cpp:107-141] SnippetEditorModel (rows Add/Remove/Dedupe/Valid) exists unwired; Valid() messages ("snippet trigger cannot be empty", "'x' expansion exceeds 4000 characters") are NOT the spec-exact messages, so the page must validate through SnippetStore::Validate and use the editor model only as row buffer (Add/Remove/Dedupe), bypassing Valid().
- [VERIFIED src/main.cpp:378-397] Production wiring: SnippetStore snippets; Load(data_dir\snippets.json) else SetEntries({}); SetMode(snippet_mode); pipeline.SetSnippets(&snippets). Page must hold a SnippetStore& (shared instance) and call Save() after mutations (path_ set by Load); pipeline then sees edits by construction.
- [VERIFIED src/ui/widgets/Widgets.h + Overlays.h] Reusable widgets: LightButton(id,label,scale)->pressed; Chip(id,label,selected,scale); IconButton(id,glyph,scale); RowList(id,rows,scale) with empty "Not available yet."; Banner(id,headline,body,*open,scale); Sheet Open/Close/Begin(title,width,scale)/End (Esc closes); Toast Show(message,action_label)/Draw/TakeAction (dwell = studio_metrics::toast_dwell_s = 5.0s).
- [VERIFIED src/ui/theme/StudioPalette.h] kStudioPalette teal tokens + studio_metrics (radius_banner 16, row 10, button 8, chip 6, row_height 52, hit_size 32, toast_dwell 5.0s). Unselected Chip = card_bg_white at 18% glass (Buttons.cpp:189) -> the "glass chips" in the brief.
- [VERIFIED src/ui/widgets/Display.cpp:270] Banner close X is IconButton glyph "x" (ASCII). Default-font glyph support for pencil U+270E is not verifiable in this env; row actions use IconButton with text glyphs (ASCII "i"/"x"? -> decided in code: pencil glyph retained per brief wording, flagged as visual-check item).

### 2. Affected files / consumers
- New: src/ui/pages/SnippetsPage.h (portable SnippetsPageModel header-inline + SnippetsPage renderer decl; NO ImGui in header so lwf_tests can link-free include it), src/ui/pages/SnippetsPage.cpp (ImGui rendering only), tests/test_snippets_page.cpp (doctest; instantiates model + SnippetStore only).
- Consumers: MainWindow::DrawSnippetsPage (NEED), main.cpp SnippetStore wiring (NEED). TextPipeline reads the same SnippetStore instance (no pipeline change).

### 3. Invariants preserved
- SnippetStore v2 JSON shape untouched (page never writes snippets.json directly; all writes via store Save() = tmp+rename+.bak). snippets.json unbroken.
- Matching semantics untouched: page calls store Expand()/Normalize(); no re-implementation.
- No code comments (AGENTS.md); no windows.h (src/ui not src/os); PushFont(nullptr, design_px) only, never SetWindowFontScale; keyboard nav via widget return values + EnableNav (no IsItemClicked); no secrets logged.
- 4000 cap counts code points in store; page never hardcodes a byte cap.

### 4. Risks / edge cases
- lwf_tests does not link ImGui (W3 S1 precedent) -> model must be header-inline in SnippetsPage.h; .cpp renders only. CMakeLists is FORBIDDEN (NEED: add SnippetsPage.cpp to WIN32 sources + test file to lwf_tests + font_sources entry).
- Save() with empty path returns false (no Load first). Page mutations ignore Save() result? No — deliberate: if Save fails, in-memory state is ahead of disk; page keeps working and surfaces nothing (store is Loaded-path in prod). Documented; test pins Save(Path) round-trip.
- Delete -> undo: single-slot stash suffices (one deletion at a time); a second delete replaces the stash (first entry stays deleted). Undo restores by Upsert(trigger,expansion,error) which regenerates id + fresh timestamps (id stability across undo is not spec-required; store Update keeps created_at only on same-id edit).
- Banner copy beyond the two example chip strings is unspecified (screenshots not in repo) -> chips carry the exact example strings; no invented headline copy.

### 5. Expected behavior / tests
- Banner example: trigger chip "my booking link" -> arrow -> expansion chip "https://example.com/book", glass (unselected Chip style).
- LightButton "Add new snippet"; rows "trigger -> expansion" (U+2192) truncated at width; hover pencil+X IconButtons; sheet edits single/multi-line (6 visible rows); delete -> Toast 5s with Undo action.
- Tests: 4 exact validation messages + order (4001-space = too long, not snippet); longest match (contains mode); casing preserved + case-insensitive trigger; punctuation rule (whole "My Email!!" expands, "my, email" contains does not); undo delete round-trip incl. persistence.

## Sessions Log
- [PENDING] Implementation, build, scoped test run, hygiene, commits.
