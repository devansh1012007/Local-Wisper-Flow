<!-- ORCH-DIGEST:START -->
agent: FINAL-REV | scope: run 20261106-gui whole branch 42a7ea0..f803729 (src+tests) | verdict: APPROVE-WITH-CHANGES
findings: BLOCKER 0 | MAJOR 0 | MINOR 3 | NIT 4
claims_audit: 9 claims checked / 0 unsupported or false
band_aids_found: none
top_3_risks: single-pair ApplyImGuiStyle gate across two ImGui contexts (redundant same-scale callback can re-run cumulative ScaleAllSizes); residual Tick/TakeTail race beyond serialized commit (documented, production single-caller); no runtime ImGui verification (Windows-only, source-scan tests only)
optimizations: DO-NOW 0 | DO-LATER 2 | NOT-WORTH-IT 2
reversal_check: reasoned-only
confidence: med (portable tests RAN green; Windows GUI behavior NOT verified on this box)
<!-- ORCH-DIGEST:END -->

# Code review — FINAL-REV

## 0. Method
- Branch `fix/gui-typography-dpi` verified; diff range `42a7ea0f7bbc5ae71b84ec9a838d0558dcb303f0..f803729` (HEAD).
- `git diff --stat` then per-file detailed diffs for all 12 changed files under `src`+`tests` (732 insertions, 94 deletions); read current file context around every changed site (ThemePalette.cpp full, DictationEngine.cpp 205-414+569-599, Overlay.cpp 230-529, MainWindow.cpp 120-329+diffs).
- Read-only grep sweeps for obsolete patterns (`SetWindowFontScale`, `ApplyImGuiStyle`, `PushFont/PopFont`, `dirty_`, `commit_mutex_`, TODO/debug leftovers).
- RAN (prebuilt `build/lwf_tests.exe`, 22:31 build vs HEAD 22:34, last code commit predates binary): `-tc="*design px*"` 2 passed/33 assertions; `-tc="*exactly once*"` 1 passed/5; `-tc="*waveform ring SnapshotInto*"` 1 passed/10; combined 8 cases 91 assertions passed (font mechanism, mini badge alphas, mini status, idle early-out, keyboard nav, waveform covers/keeps). Binary matches HEAD code (docs-only commits after 8223834). Full suite NOT run (scoped-check rule); Windows GUI not run.

## 1. Blind-pass findings (before reading implementer reports)
1. `g_last_applied_context/g_last_applied_scale` is a single global pair shared by two ImGui contexts (MainWindow + Overlay). Alternating contexts defeat the gate: a redundant same-scale re-apply for context A after context B applied re-runs cumulative `ScaleAllSizes`. See R-FINAL-001.
2. `commit_mutex_` covers only `MaybeCommitStreamingSegment`; `TakeTail()`/`StopAndTranscribe` still race a concurrent commit — header documents this, but the test's 8 concurrent `Tick()` calls exercise more than the serialized path (timeout/DeviceLost branches race benignly only under test settings). See R-FINAL-002.
3. "Briefly held / non-blocking" comment overstates: the lock spans `GetRecordedAudio()` copy + `NormalizeLoudness`. See R-FINAL-003.
4. No blockers: Push/Pop balanced, no `SetWindowFontScale` remains, no debug/TODO leftovers, idle wakeups cover all setters, font cache invalidation wired at every style re-apply site.

## 2. Claims audit
| # | Implementer claim | Verified? | Evidence / finding |
|---|---|---|---|
| 1 | Wave1: obsolete SetWindowFontScale removed; scoped PushFont design-px tokens | Yes | grep `SetWindowFontScale` in src: 0 hits; PushFont/PopFont balanced (RunOnce 277/318, DrawAttentionMessage 612/622, rail/cards adjacent) |
| 2 | ApplyImGuiStyle applies scale once per change (incl. 1.0) | Mostly | gate exists [ThemePalette.cpp:42-73] but single-pair across 2 contexts (R-FINAL-001) |
| 3 | Rail keyboard activation + focus restore + commit-on-deactivate | Yes (lexical) | MainWindow.cpp rail `pressed` return, NavActive-gated `SetKeyboardFocusHere`, 3x `IsItemDeactivatedAfterEdit`; no runtime test (Windows-only) |
| 4 | Idle early-out: 3-verdict poll (inflated/exact/button) | Yes | Overlay.cpp:281-308; all setters set `dirty_` (7 sites); dwell/fade run before early-out |
| 5 | Contrast: idle 0.55/active 1.0, status word at badge_alpha, 13px message font | Yes | ThemePalette.h tokens; Overlay.cpp:519; PushFont wraps CalcTextSize+AddText in order (push<calc<add<pop) |
| 6 | Zero-alloc Listening path via SnapshotInto + At | Yes | Overlay.cpp:422-440; Snapshot retained for compat; tests pass |
| 7 | Wave3: concurrent Tick commits exactly once | Yes | commit_mutex_ [DictationEngine.cpp:233]; test RAN green (5 assertions) |
| 8 | Tick contract narrowed to serialized commit only | Yes | DictationEngine.h:44-53 explicitly lists non-thread-safe remainder + TakeTail residual |
| 9 | Waveform At throws past newest; SnapshotInto reuses scratch | Yes | OverlayModel.cpp; tests RAN (10+ assertions) |

## 3. Long-run test (independent answers)
- **Source:** commits serialized under `commit_mutex_` with lock order `commit_mutex_ -> StreamingDictator::mutex_`; style gate is the one place where shared global state (single context/scale pair) mismatches the two-context reality — fix at source (per-context map), not call sites.
- **Load:** per-frame allocations removed on Listening path; idle frames now skip GL/ImGui entirely. Commit lock spans a full-buffer copy — bounded by recording length, only contended among Tick callers; fine at production single-caller, DO-LATER if Tick ever goes multi-threaded.
- **Hostile:** `WaveformRing::At` throws (not UB) on OOB; `TakeTail` bounds-checked; empty/oversized ring covered by test. No injection surface touched. Menu/Trust card `provider.c_str()` fallback uses a live `std::string` — safe.
- **Operability:** idle poll self-corrects ±1px fractional-DPI geometry mismatch (re-polled every frame); DPI pending excluded from early-out. Stale-context-pointer risk after DestroyContext is unreachable today (no re-Init path).
- **Reversal:** all changes are additive/localized: gate map can replace single-pair without API change; SnapshotInto/At additive (Snapshot preserved); commit mutex is a pure serialization addition; token value changes revert cleanly. Docs/agent-ops excluded from verdict per brief.

## 4. Findings
### R-FINAL-001 [MINOR] ApplyImGuiStyle gate is single-pair for two contexts
- Location: `src/ui/theme/ThemePalette.cpp:30-46` · Evidence: `g_last_applied_context`/`g_last_applied_scale` hold one pair, but `MainWindow::Init` (MainWindow.cpp:159) and `Overlay::Init` (Overlay.cpp:125) each apply through their own context, and both re-apply on pending DPI (MainWindow.cpp:254, Overlay.cpp:319). Sequence Main(1.5) → Overlay(1.5) → redundant Main callback at 1.5 (GLFW OnContentScale sets pending unconditionally when `scale_x > 0`, Overlay.cpp:196-200) makes gate miss and re-run cumulative `ScaleAllSizes(1.5)` on Main's style → drift. Normal path (genuine scale change) is correct. · Why it matters: latent cumulative style inflation; comment "re-apply cannot drift" overclaims. · Recommended fix (root cause): per-context last-applied map (e.g. `std::array`/small vector of {context, scale}, or store the pair in a per-window struct passed in) — do not just add a second global.

### R-FINAL-002 [MINOR] Residual Tick race beyond the commit is documented but untested-by-construction
- Location: `src/core/DictationEngine.h:44-53`, `src/core/DictationEngine.cpp:269-279` · Evidence: `TakeTail()` reads `committed_samples_` without `commit_mutex_`; header honestly discloses it. The concurrent-Tick test only proves the commit path; a future production caller taking the header's "Tick is safe" at face value could interleave `StopAndTranscribe` timeout with a commit and lose/duplicate tail audio. · Why it matters: production is single-`Run()`-worker (DictationEngine.cpp:88-99) so unreachable today; risk is future misuse. · Recommended fix: when Tick ever gains a second production caller, extend the mutex (or snapshot committed under it) in the same change — no action needed otherwise; keep the header warning.

### R-FINAL-003 [MINOR] Commit lock comment overstates brevity
- Location: `src/core/DictationEngine.cpp:224-228` · Evidence: lock spans `GetRecordedAudio()` (253), segment slice (257-258) and `NormalizeLoudness` (261-265), not just the `CommitSegment` enqueue. · Why it matters: future maintainers may reason "held only briefly" while sizing contention. · Recommended fix: reword comment to "held for one copy+slice+enqueue; bounded by buffer size; contention only among Tick callers".

### R-FINAL-004 [NIT] Stale global context pointer after DestroyContext
- Location: `src/ui/theme/ThemePalette.cpp:30` · Evidence: `Shutdown()` destroys the ImGui context (MainWindow.cpp:177, Overlay equivalent) but the gate still points at it; a re-Init allocating a context at the same address with the same scale would skip the initial apply. No re-Init path exists in `src/main.cpp` (single Init). · Fix: clear gate on Shutdown, or fold into R-FINAL-001's per-context map keyed weakly.

### R-FINAL-005 [NIT] Rail focus restore: mouse-click clear depends on DrawRail running
- Location: `src/ui/MainWindow.cpp:385-396` · Evidence: `IsMouseClicked` clear is inside DrawRail, which runs every visible `RunOnce` (rail always drawn), so unreachable stuck-arm in practice. If the window is hidden, `RunOnce` returns early (240-242) and the arm survives until re-show — which is the intended behavior. No change needed; noted for the record.

### R-FINAL-006 [NIT] Idle-poll geometry duplicates DrawBadge with int truncation
- Location: `src/ui/Overlay.cpp:289-299` vs `366-379` · Evidence: poll recomputes dot rect from `glfwGetWindowSize` with `static_cast<int>` truncation while DrawBadge uses ImGui `DisplaySize` float math. Comment at 274-279 acknowledges ≤1 extra frame per crossing. Acceptable; a shared helper would remove the duplication if the badge geometry ever changes again.

### R-FINAL-007 [NIT] Source-scan test for copied_until_ only checks presence
- Location: `tests/test_overlay_settings_ui.cpp` keyboard-nav case · Evidence: checks `copied_until_`/`"Copied"` strings exist; dwell expiry logic (GetTime vs saved_dwell) and SetLastTranscript reset are not behaviorally tested. Consistent with the established lexical-guard pattern (lwf_tests doesn't link ImGui); acceptable, flagged as coverage gap only.

## 5. Attempts to break it
- **Gate alternation (reasoned):** Main@1.5 → Overlay@1.5 → Main@1.5 redundant → gate misses (context differs) → cumulative drift confirmed by code reading; requires redundant GLFW callback; MINOR not MAJOR because genuine callbacks always pair with a real scale change and both Init applies are correct.
- **Push/Pop imbalance hunt (grepped):** every PushFont has a matching PopFont on all paths; no early returns between (RunOnce early returns precede the Push at 240-242; DrawAttentionMessage has no early return). No break found.
- **Idle freeze hunt:** enumerated every mutator (SetState, PushLevel, SetReduceMotion, SetWorkArea, SetSavedPosition, OnContentScale, UpdateDrag x3, dwell expiry via SetState, pending DPI, dragging) — all set `dirty_` or bypass early-out. No freeze path found.
- **Concurrent Tick (RAN):** `*exactly once*` test green (8 threads, 1 segment, then stream+tail intact). Residual TakeTail race reasoned, not reproduced (requires concurrent StopAndTranscribe, which the contract forbids).
- **Obsolete pattern sweep:** `SetWindowFontScale`, `if (scale != 1.0f)`, `0.75f * badge_alpha`, `if (ImGui::SliderInt(` (cleanup), `IsItemClicked` rail switch — all absent. No debug/TODO/homebrew leftovers in diff (remaining `std::cout` are pre-existing main.cpp console UI).

## 6. Dimension checklist
- Correctness: PASS (one latent gate drift, R-FINAL-001).
- Root cause vs symptom: wave1/wave2/wave3 all fix at source (mechanism replacement, poll verdicts, mutex); gate is near-source but single-pair (MINOR).
- Regression/compat: `Snapshot()` preserved; token/API changes additive; `ApplyImGuiStyle` signature unchanged.
- Concurrency/idempotency: commit exactly-once proven; documented residual elsewhere.
- Error handling: `At()` throws; commit guards (silence_start > total, committed > silence_start, size check) run under lock.
- Security/abuse: no new input/network surface; cloud Trust card text is static strings + provider key from settings.
- Performance: zero-alloc Listening frame; idle skip; commit lock bounded — all sound.
- Migration/rollback: no persistence/schema changes in this range; clean revert.
- Observability: no new logging; behavior knobs are existing tokens.
- Maintainability: DECISION/INVARIANT comments accurate except R-FINAL-003 wording; per-context gate would match comments better.
- Docs impact: headers document contracts (Tick, ApplyImGuiStyle, Overlay idle invariants); docs/agent-ops excluded from code verdict per brief.

## 7. Optimization and simplification opportunities
| # | Location | Evidence | Impact | Effort | Risk | Verdict |
|---|---|---|---|---|---|---|
| 1 | ThemePalette.cpp gate | single pair, two contexts | correctness (drift) | small | low | DO LATER (with next GUI touch; not blocking) |
| 2 | DictationEngine commit lock | copy+normalize under lock | negligible today (single caller) | medium (restructure) | medium | NOT WORTH IT now |
| 3 | Overlay poll geometry | duplicated rect math | maintenance only | small | low | NOT WORTH IT (comment already covers) |
| 4 | Snapshot() callers | retained snapshot allocates | no hot caller remains (only tests) | — | — | NOT WORTH IT |

## 8. Cross-agent / cross-task interactions observed
- **Wave1 fonts x Wave2 idle:** `cached_status_size_` is computed in DrawBadge (default font only) and invalidated at both DPI re-apply sites (Overlay.cpp:320) and SetWorkArea (174) — the wave1 `PushFont(message_font_)` scope in DrawAttentionMessage never contaminates it (push/pop encloses its own Calc/Add). No conflict.
- **Wave1 gate x Wave2 idle:** early-out explicitly excludes `pending_dpi_scale_ > 0` and `dragging_`, so a style re-apply can never be skipped by the idle path; gate-skip only occurs at same ctx+same scale, where the font cache is also still valid. The single-pair flaw (R-FINAL-001) is the only cross-effect and needs a redundant callback.
- **Wave1 fonts x Wave2 nav:** RunOnce's body `PushFont` wraps DrawRail, so `SetKeyboardFocusHere`/focus rings render at body size; rail/card PushPop scopes nest correctly inside it. No conflict.
- **Wave2 nav x Wave2 idle:** independent windows/contexts; rail focus machinery never touches Overlay state. No conflict.
- **Wave2 alloc-free x Wave3 race:** `WaveformRing` (core) vs `DictationEngine` commit — no shared state. `Snapshot()` retention keeps any pre-existing callers compiling.
- **Wave3 race x production caller:** `Run()` calls Tick from a single worker; the narrowed header contract matches production reality. The concurrent test is a contract stressor, not a production pattern.

## 9. What I could not verify
- Windows-only runtime behavior: DPI move live re-apply, rail keyboard navigation/focus restore, idle badge wake on real cursor, PushFont bitmap scaling at 8.5/13/15/18/28px, Copy dwell, slider commit-on-deactivate — no Windows GUI run on this box (MSYS2/MinGW, no display); covered by source scans + manual checklists only.
- Full `ctest` suite (scoped-check rule; ran 11 targeted cases/139 assertions green against the prebuilt binary).
- Whether GLFW ever fires a redundant same-scale content-scale callback (determines R-FINAL-001's real-world hit rate; code reading says the callback handler cannot itself distinguish).
- The "fail-before" claim for the e0a3882 gate markers was checked via the test's cited git-show comparison, not re-executed here.
