# Brief — W2-DEEP-01 (DEEP DESIGN, no code) · wave 2 · run 20261108-ui-hierarchy
## 1. Mission
Design decision record for token gaps: 24/17/10 fonts, 12px gap, 10/6 gaps, MainWindow 14, SetNextItemWidth structural, then implementation plan for tranche 2. Why: Wave 1 allowlist 26 sites; mapping them wrongly shifts pixels. Design must preserve same-value today while fixing coupling.
## 2. Required reading
STANDING-ORDERS, AGENTS.md UI + frozen tokens rule, 00-mission D-rail/constraints, W0-INV-02 sections 5-10 (T-003), W0-INV-04 mockup mapping, W1-REV-01 section 7 DO LATER + section 3 long-run, W1-IMPL-02 tranche-1 report, tests/test_overlay_settings_ui.cpp BanAllowance 1136-1159 + new negative test, spec free-form design (templates? use report-implementation + deep addendum F).
## 3. Design deliverable (NO source edits in this task)
Write design record to reports/w2-deep/W2-DEEP-01-design.md (create dir reports/w2-deep). Include:
- Inventory table: every allowlisted (file,api,value, allowlist reason) from BanAllowance caps
- Options A (additive studio_metrics tokens, same-value) / B (extend metrics::* spacing + fonts) / C (map to nearest existing token with pixel shift) with tradeoff table (correctness, density, coupling, test impact, palette freeze compliance)
- Chosen tokens per gap with exact namespace, name, value, justification (e.g. studio_metrics::font_heading_px=24? or font_24? Follow existing naming: font_banner/caps/page_title/helper; propose where each literal lands)
- Palette split fix (metrics::* usage inside studio pages) — migrate to studio_metrics equivalents or keep metrics and document coupling
- kRailWidth doc + radius_card 10-vs-12 collision handling (document only, no value change)
- Where structural literals stay (SetNextItemWidth 240/360, pill_h+18, cursor_x, button-width +28/+32, 40.0f rail) and why
- Test plan: exact BanAllowance updates (caps 26→0?), token value pins (test_overlay_settings_ui.cpp:698-738 region), negative scanner test still passes, glob/file-list expansion decision
- Failure-mode table, rollout/rollback (revert order), observability (screenshots), open risks
- Write-set for implementation tranche (which files/symbols), contract-set (headers), sequencing after Wave 1 HEAD (ea04e1a..)
## 4. Ownership
WRITE-SET: docs/agent-ops/20261108-ui-hierarchy/reports/w2-deep/W2-DEEP-01-design.md only (plus COMMS lines)
READ-ONLY: source/tests. NO source edits, NO commits of source.
## 5. Siblings
None in Wave 2 design sub-wave. Implementation comes after ORCH approves design (single dedicated agent).
## 6. Traps
Theme values frozen — additive tokens must not change existing values or contrast. Mockup regen not required for additive spacing/font tokens (no contrast change) but must state why. Do NOT propose palette unification (ApplyStudioStyle) — out of scope.
## 7. Env
Read/grep only. No builds.
## 8. Deliverables
Design report with ORCH-DIGEST (status DONE, chosen option, token table count, risks). Docs-only commit trailers Task: T-003 Agent: W2-DEEP-01 Wave: 2 Report: <path>. Final ≤8 lines: STATUS|agent|task|report|SHAs|blockers.
