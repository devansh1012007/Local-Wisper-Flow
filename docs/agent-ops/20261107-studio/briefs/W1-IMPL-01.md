# Brief — W1-IMPL-01 (implementer) · wave 1 · run 20261107-studio

## 1. Role and mission
Mission: write the F0 deliverable docs/STUDIO_PLAN.md (repo copy of the approved plan) + AGENTS.md key-rule amendment text, and record F0 answers for Devansh. This unblocks all later UI work; UI code is forbidden until Devansh signs F0.
Role: implementer (docs only).

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (esp. gates + keys paragraph)
3. docs/agent-ops/20261107-studio/00-mission.md
4. Approved plan: C:\Users\devan\.opencode\plan\2026-11-07-studio-pages-and-settings-gui.md (full)
5. Your INV reports: docs/agent-ops/20261107-studio/reports/w0-investigation/ (corrections noted)
6. Screenshots: 4/5 received in conversation (Insights/Dictionary/Snippets/Transforms). Scratchpad screenshot still outstanding — record as pending.

## 3. Task and acceptance criteria (T-001, plan P01 + D3 amendment)
- Write docs/STUDIO_PLAN.md: file list, per-JSON migration plan (dictionary.json 4 shapes, snippets.json v2, transforms.json v1→v2, stats.json new, notes/ new, config.json additive incl. last_page/llm_provider/quiet_speech_boost), palette table (measured values PROVISIONAL — mark "visual estimate pending pixel-picker; source images to be saved to docs/screenshots/source/"), banner copy policy, fonts (EB Garamond + Inter OFL — list intended file names + license + estimated size), answers to Devansh (a)-(f) with D12-D15.
- Edit AGENTS.md: amend "Keys come from the environment only" to cover D3 api_keys.json persistence (separate file, redacted, never config.json) + note OpenRouter gate exception D2 (text only; no behavior change).
- Docs only — no src/ edits.
Acceptance: both files exist, compile-n/A, hygiene OK (bash scripts/check_hygiene.sh), git diff touches only the two files.

## 4. Ownership
WRITE-SET: docs/STUDIO_PLAN.md (new), AGENTS.md
READ-ONLY: everything else. FORBIDDEN: src/**, CMakeLists.txt, tests/**, scripts/**.

## 5. Siblings
W1-IMPL-02..06 work on src/core/*.{h,cpp} (new files, details in their briefs). No shared files with you.

## 6. Skip list / traps
No UI code (F0 stop). Palette values are estimates — do NOT claim "measured". Screenshots: don't embed/claim exact hexes without the missing Scratchpad image.

## 7. Environment
Scoped: bash scripts/check_hygiene.sh only. Do NOT run cmake build or full ctest (shared build dir).

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w1-impl/W1-IMPL-01-T-001.md (ORCH-DIGEST first, implementation template). Commits: docs only, trailers Task: T-001 / Agent: W1-IMPL-01 / Wave: 1 / Report: <path>.
Final message <=8 lines: STATUS | agent | task | report path | commit SHAs | blockers.
