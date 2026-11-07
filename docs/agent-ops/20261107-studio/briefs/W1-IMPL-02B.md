# Brief — W1-IMPL-02B (implementer) · wave 1 · run 20261107-studio

## 1. Role and mission
Mission: implement StatsStore (daily-rollup stats.json behind Insights) + app_profiles.json category defaults, with tests. Root cause: no stats collection exists; insights page has nothing to read.
Role: implementer.

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; additive keys; scripts/check_no_network.sh gate — your .cpp needs allowlist? See §4 NEED handling)
3. docs/agent-ops/20261107-studio/00-mission.md
4. Approved plan: §§C,D,E (P05, P11), §H (T-stats incl. 730-day cap), §I. Key correction: DictationEngine lacks app context today — Record() takes a full DictationRecord; plumbing app/duration happens in Wave 2 (P12). Your store only defines the record + persistence.
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-datastore.md + INV-enginehooks.md (HistoryStore record shape; text_mutex_ is DictationEngine's precedent for locking).

## 3. Task / acceptance
- src/core/StatsStore.{h,cpp}: `Record(DictationRecord{timestamp_local, duration_ms, final_text, raw_text, app_profile, process_name, words_corrected, dictionary_fixes})` (no-op when toggle off / cancelled-empty-failed — the latter filtered by caller P12, but Record() must ignore empty final_text defensively); storage stats.json `{YYYY-MM-DD:{words, dictations, speaking_ms, words_corrected, dictionary_fixes, by_category:{6 cats}, apps:{process:words}}}`; atomic write via temp-file+rename pattern (do NOT depend on W1-IMPL-02A's AtomicFile yet — copy the pattern or include its header if present? Safer: write your own tmp+rename and note the future dedup as a FINDING); thread-safe mutex (engine worker thread calls it); 730-day eviction; toggle `keep_usage_stats` read via SettingsStore? No — pass a bool SetUsageStatsEnabled(bool) to keep the store portable; load tolerates missing/corrupt file → empty state.
- App profiles: add additive `category` to AppContextResolver defaults only for the 6 spec categories (AI prompts: ChatGPT/Claude/Cursor/Gemini/Perplexity; Emails: Outlook/Thunderbird/Gmail-in-browser-title; Work messages: Slack/Teams; Personal: WhatsApp/Telegram/Messages; Documents: Word/Notion/Google Docs/Obsidian; Other tasks: everything else). Never send window titles anywhere. If app_profiles.json has a schema, handle unknown `category` key non-destructively.
- Word count: split final_text on Unicode whitespace (share with or duplicate the small helper — do NOT depend on TextDiff's header; note dedup FINDING).
- Tests (tests/test_statsstore.cpp new): Record+load round-trip, 730-day eviction (plant 731 days of entries), toggle off = no-op, empty final_text ignored, thread-safety smoke (8 threads x concurrent Record, no crash), category mapping per spec (process first, title second). Hermetic temp fixtures.
- Gate: StatsStore.cpp writes files → needs allowlist entry. scripts/check_no_network.sh is owned by W1-IMPL-02A this wave — do NOT edit it; post NEED in COMMS with the exact line to add, finish safe work, report DEFERRED-DEPENDENCY for that line.
Acceptance: StatsStore + profiles exist, tests added, hygiene OK, build attempted or NOT-RUN noted, NEED posted for gate line.

## 4. Ownership
WRITE-SET: src/core/StatsStore.{h,cpp}, src/core/AppContextResolver.{h,cpp} (category only), tests/test_statsstore.cpp
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt, scripts/check_no_network.sh, src/core/AtomicFile.*, TextDiff.*, InsightsModel.*, DictionaryStore.*, SnippetStore.*, TransformStore.*, NotesStore.*, src/ui/**, src/main.cpp.

## 5. Siblings
02A AtomicFile/TextDiff/InsightsModel; 02C DictionaryStore; 02D Snippet+Transform; 02E NotesStore. No shared files.

## 6. Traps
Do not invent app-context capture (Wave 2 P12). Do not log transcript text at default level (R4). No code comments.

## 7. Environment
Scoped: bash scripts/check_hygiene.sh only. No cmake --build (shared build dir) — mark NOT-RUN. No full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w1-impl/W1-IMPL-02B-T-00510.md (ORCH-DIGEST first). Commits with trailers (Task: T-005,T-010 / Agent: W1-IMPL-02B / Wave: 1 / Report: <path>). Final message <=8 lines.
