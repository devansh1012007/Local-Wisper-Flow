# Full UI Redesign Plan — 2026-11-08

> SUPERSEDED 2026-11-09 by ui-redesign-v2 plan (owner decision). The redesign direction was replaced; this document is retained for history only. New plan: C:\Users\devan\.opencode\plan\ui-redesign-v2-draft.md

Status: DRAFT. Branch for implementation: `ops/ui-full-redesign` (new branch from hierarchy tip 3963bab).
Spec/source: user request "change the color palate and fully redesigne the ui" + skill @lwf-desktop-gui + AGENTS.md rules.

## 1. Problem statement and goal
Current UI has three visual tensions:
1. Two palettes fighting: native dark Hunter/Brass (`kDarkPalette`) used by Full/Mini vs light teal (`kStudioPalette`) used by Studio pages/widgets. Users see dark rail with light teal cards embedded — split identity.
2. Frozen-token trap: `ThemePalette.h` must match `docs/design/mockup.html` contrast tables exactly (tests pin hex values, contrast 4.5:1 text / 3:1 meaningful pairs). Prior lightening (5ef2c98) was a misdiagnosis — real bug was ImGui context/viewport (1a8304f). Any palette change without mockup regen + test update fails CI.
3. Dead token + radius collision: `metrics::rail_width=208` unused (local `kRailWidth=220`), `radius_card` 10 native vs 12 studio with same name.

Goal: single coherent visual identity with hierarchy/density clarity, keyboard/DPI correct, screenshots before/after on Windows.

Non-goals:
- No engine/audio/STT/LLM/injection/FSM/store changes.
- No new dependencies, no FreeType/real fonts/ no icon font (font approval pending), no multi-viewport/docking, no demo.
- No feature additions (catalog, sync). Visual/UX only in src/ui + mockup + tests + docs.
- Do not touch Overlay minis' Win32 styles except via src/os.

Acceptance criteria:
- AC1 Contrast (corrected per critique): every text pair incl. accent/ok/warn/err/brass/muted/on_accent ≥4.5:1 (test:208-212); meaningful non-text (focus, toggle track, control border) ≥3:1; only declared hairlines (border/bg, border/surface) stay <3.0 exempt with same assert (test:217).
- AC2 Single accent per view (R9), state never color-alone (R8).
- AC3 No hex literal or `kDarkPalette` direct in draw code (R5); all sizes from metrics/design-px tokens (R6); controls ≥32px (R10).
- AC4 Keyboard: Tab traversal + 2px focus ring visible; rail/invisible buttons pass EnableNav; screenshots at 100/150/200% DPI, 960x640 default and 760x520 min.
- AC5 `ctest -R lwf-tests` green, `check_hygiene.sh` OK, `check_no_network.sh` OK. G3 zig pending (no zig on dev box — record in docs/QUESTIONS.md).
- AC6 Screenshots: Full Home/Settings/Welcome + 5 studio pages + Mini states (Ready/Listening/Transcribing/Needs attention) at 960x640 and 760x520, DPI 1.0/1.5. Compared vs baseline run docs/agent-ops/20261108-ui-hierarchy.
- AC7 Mockup regenerated first (new tokens + contrast tables), then header, then tests. `docs/UI_AUDIT.md` guardrails updated if policy changes. `docs/PROGRESS.md` one row per task.

## 2. Facts vs assumptions
Facts (investigator-verified file:line):
- Dark: bg #0E1714, surface #14221D, border #29392F, text #EDE6D6, muted #A8B3AA, accent/brass/focus #C9A96E, ok #8FB39A, warn #D0A65A, err #D98A8F/#B5565F (ThemePalette.h:46-61). Light Ivory/Navy (#F6F1E4/#FFFDF7/#D9CFBA/#1C2B26/#5B665F/#1F3A5F) has zero runtime uses (test-only).
- Studio: page #FAFAF8, card #F4F2EE/#FFFFFF, teal #1E5E5A/#2F8077/#7DBFB7/#CDEBE7, ink #1C1C1C (StudioPalette.h:24-38). Radius collision documented (StudioPalette.h:43-45).
- Mockup contrast tables (mockup.html:175): dark text/bg 14.67, text/surface 13.23, muted/surface 7.59, accent/surface 7.35, err_fill/surface 3.50, on_accent/accent 8.15, borders 1.49/1.35 FAIL-below (exempt).
- Runtime: MainWindow+Overlay apply only kDarkPalette; ScratchpadWindow applies only ApplyStudioStyle; pages draw with kStudioPalette constants destructively.
- One context per window; hide-on-close; live-apply with Saved dwell (1.5s); 32px min control; DPI scale-once via FontScaleMain.
- D-rail locked: kRailWidth=220 local "Rail is 220 on purpose", token 208 dead (MainWindow.cpp:56-58).
- History: lightened palette reverted (5ef2c98 → 1a8304f). Fonts EB Garamond+Inter approved but not bundled (code still default font scaled). Teal pixel-measured provisional (R-01 open).

Assumptions (need validation):
- User wants light or dark as primary? Assumption: keep dark-primary (app starts dark, Full window hides to tray, Mini dot 22px). If user wants light primary, plan branches.
- Proposal palette: "Lake & Brass II" — deep pine/ink bg, parchment surface, brass accent for both dark/light; unify studio teal to brass (de-teal). Contrast tuned to pass 4.5/3 programmatically. Assumption flagged for owner confirm.

## 3. Candidates compared
### Option A — Palette-only swap (keep layout, change tokens + mockup + tests)
Pros: smallest diff, fast, low risk. Cons: doesn't fix dual-palette split, rail drift, hierarchy density; studio teal stays misfit. Verdict: insufficient for "fully redesign".
### Option B — Single-palette full redesign (recommended)
Unify all surfaces to one token system: dark primary in same Palette struct, light as themed variant, studio pages migrate to same Palette (de-teal), new mockup + new tokens, resize rail to token, reconcile radius_card to single value, recompute all contrast pairs, screenshots gate.
Cons: larger (10+ files), mockup + tests + header move together, needs Windows screenshots to prove. Pros: resolves identity split, meets R9 one accent, preserves UiBus/live-apply invariants.
Verdict: CHOSEN.
### Option C — Keep Hunter/Brass but re-layout only (no color change)
Pros: no mockup regen risk. Cons: ignores explicit user request to change palette. Verdict: rejected (user explicitly asked palette change).

Why B: only option honoring user request while respecting contrast/test gates and layered architecture.

## 4. Atomic phases (each leaves build green; dependencies)
Ph0 Baseline + branch (DONE: ops/ui-full-redesign exists, ctest 1/1 30s green, hygiene OK, no-network OK)
Ph1 Design + Mockup (docs/design/mockup.html — new palette, type scale, layout rules, contrast tables recomputed, token table for impl; single agent, owns mockup.html + PLAN appendix)
Ph2 Token layer (ThemePalette.h + StudioPalette.h deprecated or re-export single Palette + ThemePalette.cpp style mapping; tests/test_overlay_settings_ui.cpp updated to new pins while keeping 4.5/3.0 policies; separate agent, ONLY theme+tests; depends Ph1)
Ph3 Widgets + shared controls (Widgets.h, Buttons.cpp, Display.cpp, Overlays.cpp — kill dual-palette dependence, enforce R5 accessor Pack(kDarkPalette), 32px, focus ring; depends Ph2)
Ph4 Full window shell (MainWindow.{h,cpp} — rail 220→token, page chrome, pinned tests update font_sources/kRailWidth; depends Ph2+Ph3)
Ph5 Pages vertical slices (one page per agent: Home/Settings, Dictionary, Insights, Snippets, Transforms, Scratchpad; each: token migrate + hierarchy tightening + keyboard check; disjoint files, parallel-safe; depends Ph3)
Ph6 Mini/Overlay + ScratchpadWindow (third context, drag, states, attention; depends Ph2)
Ph7 Screenshots + docs (Windows captures, manual test R/E1 checklist, docs/PROGRESS rows, docs/UI_AUDIT update; orchestrator+manual)

Dependency graph:
```
Ph0 done
Ph1 -> Ph2 -> Ph3 -> Ph4 -> Ph5 (parallel pages), Ph6
Ph1 -> Ph2 -> Ph3 -> Ph4 -> Ph7
Ph5/Ph6 -> Ph7 (screenshots)
```
Parallel safe: Ph5 pages among themselves (disjoint files), Ph6 independent after Ph2 (own files). Ph2 is hotspot (single writer).

## 5. Blast radius, rollback, risks
Blast radius: API BREAK vs StudioPalette — Palette struct stays, but StudioPalette struct (page_bg/teal_*/heat_empty) must be deleted/replaced. Tasks: drop StudioPalette.{h,cpp} residents, migrate pages/widgets to Palette, rewrite HeatColor to accent ramp (4 levels) + CalendarLevel parity pins. DATA/SCHEMA — none. BACKWARD COMPAT — config/layout unchanged. CONFIG — none. PERFORMANCE — none. CONCURRENCY — none. SECURITY — no network/secrets; contrast is accessibility surface.
Rollback: revert Ph2 commit (header+mockup+tests together), Ph3+ revert each tranche; git revert per tranche, before Ph7 screenshots.
Risks: contrast computed wrong → test fail loud; test weaken temptation (forbidden — keep 4.5/3.0, document any exemption in header+mockup); screenshot mismatch (need Windows capture); font proposal not bundled (keep default font 13px scaled); Mini drift (focus/drag/passthrough regressions — preserve UpdateDrag + pass-through logic); rail shrink clipping (760x520 min — verify).
Security: no new network; hygiene `windows.h` ban must hold (G4).
Second-order: single palette makes future light-mode promotion trivial (reuse same struct); removing teal simplifies heatmap colors (map heat via accent ramp, recheck contrast).

## 6. Tests, manual verification, docs
Unit (doctest): update pins in test_overlay_settings_ui.cpp (hex + contrast), keep policy checks (>=4.5, >=3.0, hairline exemption asserted), add pins for new tokens (radius_admin, spacing, fonts), update source-scan font_sources if MainWindow changes (kRailWidth pin string, 40.0f pin). Add fail-before test: new token value asserted, fails without? Actually old hex tests fail before header swap — expected red until Ph2 lands (atomic Ph2 commit: mockup+header+tests together).
Integration: nothing beyond ctest (no ImGui link in lwf_tests).
Manual Windows (cannot be claimed verified from Linux): 100/150/200% DPI, 960x640 + 760x520, Mini states, tray Quit-only, hide-on-close, live-apply Saved dwell (MANUAL_TEST_WINDOWS.md R/E1). Gates: G1 build -j4 zero new warnings, G2 ctest, G3 zig pending, G4 hygiene, G5 config_v0 in-suite.
Docs: /docs/PROGRESS.md rows BAG per task, update UI_AUDIT guardrails if policy changes, update mockup comment in ThemePalette.h (new D? source), do NOT edit mockup values without regenerating tables.

## 7. Open questions / decisions
- Palette core values: choose Lake&Brass II (proposed) vs alternative light-primary? Need owner confirm before Ph2.
- Light theme runtime: wire kLightPalette now or leave test-only? Proposal: keep dark-primary runtime; light stays test-covered palette variant to avoid second full pass.
- Fonts: stay default font scaled (approved EB Garamond+Inter pending bundling)? Proposal: stay default.

## 8. Critique log (planner-reviewer 2026-11-08)
- [critical] §5 "no API break" FALSE — StudioPalette (page_bg/teal_*/heat_empty) ≠ Palette (bg/accent/focus). De-teal breaks HeatColor + tests 809-814. Fix: add StudioPalette deletion + heat re-ramp task in Ph2.
- [critical] AC1 weakens policy — tests require accent/ok/warn/err/brass >=4.5 + hairline <3.0 (208-217). Fix: restore >=4.5 for those; AC1 corrected below.
- [critical] Ph2 underestimated — source-scan covers ~15 files + Theme markers + Pack() + rail/nav. Fix: Ph2 worker expands test list; mockup must cover studio by regen OR we keep screenshot-derived pins at StudioPalette.h:58-60.
- [major] AC3 undefined — MainWindow/Overlay use kDarkPalette directly everywhere. Fix: define R5 migration as Pack(kDarkPalette) stays allowed in theme-adjacent/window code, page/ widget code must use palette accessor; Phase 3 enumerates all sites.
- [major] Parallelism false — Ph5 pages collide on MainWindow.cpp (DrawSettingsPage) + widgets + test hotspot. Fix: serialize MainWindow.cpp until Ph3 done; Ph5 pages become sequential sub-waves.
- [major] Screenshots gate on Linux box impossible — Windows manual checklist owns verification. Fix: capture baseline screenshots at Ph0b before Ph2, gate merge on MANUAL_TEST_WINDOWS R/E1 (not ctest alone).
- New alternative B-mini considered: recolor ApplyStudioStyle/heat ramp to brass (2 files) preserves structs while unifying look — smaller than full struct unification. Decision needed from owner: B full struct unification vs B-mini recolor-only.

AC1 corrected: all text incl accent/ok/warn/err/brass/muted/on_accent >=4.5; meaningful non-text (focus, toggle track, controls border) >=3.0 as before; hairline border/bg + border/surface stay <3.0 exempt with same assert.

## 9. Resolutions committed
- Palette shape: Phase 2 includes StudioPalette deletion + HeatColor re-ramp + CalendarLevel parity tests.
- Test list expanded: all source-scan cases enumerated in Ph2 brief (font_sources ~15 files, Theme markers 307-322, Pack 380, rail/nav 644-695).
- Owner sign-off gate before Ph2 code edits (palette values frozen in Ph1 mockup).
- Light palette: proposals in Ph1 must keep kLightPalette passing 4.5/3 (test:199) — either new light values or keep Ivory/Navy tested values but re-verify.
- Reduce_motion/dwell/copy-lint/keyboard/last_page pins added to Ph2/Ph5 acceptance checklists.
- Screenshot baseline (Ph0b) will be taken on Windows before any token edits; Ph7 becomes delta-check.
