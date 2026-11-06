<!-- ORCH-DIGEST:START -->
agent: W2-IMPL-A5A6 | task: T-005/T-006 | status: DONE
code_commits: d9e86dd | report_commit: pending
files_changed: 6 (src/ui/Overlay.h, src/ui/Overlay.cpp, src/ui/theme/ThemePalette.h, src/core/OverlayModel.h, src/core/OverlayModel.cpp, tests/test_overlay_settings_ui.cpp)
contracts_changed: none
tests: added 3, changed 0; scoped run: pass
ownership_violations: none
deferred_dependencies: none
out_of_scope_findings: 0
temporary_mitigation: no
deep_issue_flag: no
least_confident_about: idle hover poll geometry parity between OS pixels and ImGui DisplaySize under non-1.0 DPI (no live GUI to observe)
confidence: high (build/tests/hygiene green; GUI painting not verified live)
last_comms_line_read: 2026-11-06T20:00:00Z
<!-- ORCH-DIGEST:END -->

# Implementation report — W2-IMPL-A5A6 — T-005/T-006 GUI-A5 + GUI-A6

## 1. Task and acceptance criteria (restated)
- A5: RenderFrame early-out for non-animated idle states (Ready/Paused) after dwell/fade bookkeeping + cheap hover poll; dirty_ flag set on SetState/PushLevel/OnContentScale/UpdateDrag; status-word CalcTextSize cached (1-entry); SnapshotInto/At accessor (keep Snapshot for tests); no per-frame heap alloc in Listening path.
- A6: mini_idle_alpha 0.35->0.55, mini_active_alpha 0.65->1.0 in ThemePalette.h; status-word 0.75 discount removed (draw at badge alpha); mini_message_w 140->250; second 13px bitmap font + PushFont/PopFont around DrawAttentionMessage. Do NOT change palette hues, do NOT touch MainWindow.
- Tests in tests/test_overlay_settings_ui.cpp: idle/active alpha floor assertions; source-scan that status word no longer multiplied by sub-1.0 constant; keep contrast tests green.
- Verification: cmake --build build --config Release --parallel 4 --target lwf_tests; cmake --build build --config Release --parallel 4 --target local-wisper-flow; ctest --test-dir build -R lwf-tests; bash scripts/check_hygiene.sh.

## 2. Verification that the problem exists
[VERIFIED src/ui/Overlay.cpp:327-332 pre-change] Ready idle multiplied badge_alpha by mini_idle_alpha (0.35), others by mini_active_alpha (0.65).
[VERIFIED src/ui/Overlay.cpp:421-435 pre-change] status word drawn with Pack(muted, 0.75f * badge_alpha) — extra 0.75 discount compounding the sub-1.0 token alphas.
[VERIFIED src/ui/Overlay.cpp:347 pre-change] Listening path did waveform_.Snapshot() — a per-frame vector alloc.
[VERIFIED src/ui/Overlay.cpp:212-271 pre-change] RenderFrame did full GL/ImGui work every tick regardless of state; no dirty flag or early-out; CalcTextSize for the status word ran every frame.
[VERIFIED src/ui/theme/ThemePalette.h:119,123-124 pre-change] mini_message_w = 140.0f; mini_idle_alpha = 0.35f; mini_active_alpha = 0.65f.
[VERIFIED src/ui/Overlay.cpp:95-100 pre-change] Init added only the 8.5px bitmap font; DrawAttentionMessage used GetFont()/GetFontSize() with no PushFont scope.
[VERIFIED docs/design/mockup.html section 2 + section 5] mini Ready note: "whole badge at 55 percent opacity, 100 on hover"; motion table: "Saved note 1.5 s". This pins the A6 alpha/dwell values to the design source. [RAN Select-String over mockup -> matched those strings]
[VERIFIED imgui.h vendored D:/lwf/_deps-src/imgui] PushFont(ImFont*, float) two-arg form; AddFontDefaultBitmap SizePixels 13px recommended; Fonts[0] is default. Confirms the second-font approach compiles in v1.92.9b.

## 3. Root cause
- Under-contrast idle badge: token alphas 0.35/0.65 below the mockup 0.55/1.0, and the status word additionally discounted by 0.75. Windows-only appearance, but tokens/DrawBadge are portable logic covered by tests.
- Idle cost: Mini rendered a full ImGui/GL frame every tick in static Ready/Paused states; Listening allocated a vector each frame to read the waveform.

## 4. Approach chosen and why
- Replace tokens with mockup values (0.55/1.0, width 250, saved dwell 1.5 s); drop the 0.75 factor; no palette hue change (hues verified unchanged in the diff).
- Add dirty_ + early-out in RenderFrame for Ready/Paused only (Listening/Thinking/Done/Attention always render). Dwell/fade bookkeeping still runs every frame; then a cheap OS-level IsCursorInsideRect poll wakes the badge when hover changes. Fade unsettled / pending DPI / dragging forces a full frame. Other states keep rendering every tick (no behavior change for animated paths).
- WaveformRing gets SnapshotInto(out) + At(i); Snapshot() now delegates to SnapshotInto (kept for existing tests). Listening reuses a member scratch vector so the per-frame render performs zero heap allocations after warm-up (reserve grows once).
- Status-word CalcTextSize cached in a 1-entry cache keyed by word content; invalidated when the DPI scale re-applies the style (the only thing that changes the measured size, since FontScaleMain scales the design size).
- Second 13px bitmap font added in Init (message_font_ pointer, falling back to Fonts[0] if AddFontDefaultBitmap ever returned null); DrawAttentionMessage wraps CalcTextSize + AddText in PushFont(message_font_, font_body_px)/PopFont.
- saved_dwell 1.6->1.5 per sibling NEED (W2-IMPL-A1A3 COMMS line + mockup motion table). Alternatives rejected: changing the early-out to time-slice rendering (would add latency/drop frames with no benefit); keying the font cache by (word, scale) pair instead of clearing on scale apply (equivalent, but clearing is simpler and the apply is rare).

## 5. What changed (per file)
| File | Change | Why |
|---|---|---|
| src/ui/theme/ThemePalette.h | mini_message_w 140->250.0f; mini_idle_alpha 0.35f->0.55f; mini_active_alpha 0.65f->1.0f; saved_dwell 1.6f->1.5f; refreshed mini-badge comment | A6 tokens + A1A3 sibling NEED, mockup-pinned; no hue changes |
| src/ui/Overlay.h | + dirty_, poll_near_dot_, levels_scratch_, status-word cache fields, message_font_; #include <vector>; ImFont fwd decl; comment refresh | A5 state for early-out/cache/scratch + A6 second font |
| src/ui/Overlay.cpp | Init: second 13px bitmap font. dirty_=true in SetState/PushLevel/SetReduceMotion/SetWorkArea/SetSavedPosition/OnContentScale/UpdateDrag (start/move/end). RenderFrame: dwell/fade bookkeeping, then Ready/Paused early-out with OS hover poll; dirty_=false after swap; cache invalidated on DPI apply. DrawBadge Listening: SnapshotInto into levels_scratch_. Status word: cached CalcTextSize, Pack(muted, badge_alpha). DrawAttentionMessage: PushFont(message_font_, font_body_px)/PopFont scope | A5 idle cost + A6 contrast/font |
| src/core/OverlayModel.h | WaveformRing::SnapshotInto(std::vector<float>&) + At(std::size_t) declared (Snapshot kept) | Alloc-free per-frame read + legacy test API |
| src/core/OverlayModel.cpp | SnapshotInto/At implemented (oldest-first, At throws std::out_of_range past newest); Snapshot delegates; + <stdexcept> | So the Listening path no longer allocates per frame |
| tests/test_overlay_settings_ui.cpp | +<stdexcept>; 3 new test cases: alpha/width/dwell floors, SnapshotInto/At vs Snapshot equivalence + bounds, source-scan for badge-alpha draw and PushFont scope | Regression guards for A5/A6 |

## 6. Blast radius and contracts
- [RAN git diff --stat] Only the 6 write-set files changed (plus COMMS.md line and this report, committed separately). MainWindow.* untouched ([VERIFIED git diff --stat shows no MainWindow]).
- WaveformRing::Snapshot() behavior unchanged (delegates to SnapshotInto) — all pre-existing callers/tests are unaffected. New SnapshotInto/At are purely additive.
- ThemePalette metrics are read-only tokens; changing mini_message_w widens the overlay window geometry (Init + ApplyWindowGeometry + UpdateDrag all read the token, so no call-site update needed).
- No palette hues changed ([VERIFIED theme palettes test still green]); no public API or serialized-data change.

## 7. Tests
Added (all in tests/test_overlay_settings_ui.cpp):
1. "mini badge alphas meet the mockup contrast floors" — asserts mini_idle_alpha >= 0.55, mini_active_alpha == 1.0 (bounded <= 1.0), mini_message_w >= 250, saved_dwell == 1.5. Fails pre-fix (0.35/0.65/140/1.6).
2. "waveform ring SnapshotInto and At match Snapshot ordering" — wraparound order, At() endpoints, std::out_of_range past newest, buffer reuse. Fails pre-fix (no such members — compile error, which is the correct fail-before signal for a new API).
3. "mini status word is drawn at badge alpha without a discount" — source-scans Overlay.cpp for Pack(muted, badge_alpha) presence, absence of "0.75f * badge_alpha", and the PushFont(font_body_px) + PopFont scope. Fails pre-fix (old string present, new strings absent).
Existing contrast tests kept green ([RAN ./build/lwf_tests.exe --test-case="*contrast*" -> pass]).

## 8. Long-run test
- Source: root causes removed (token values + discount + per-frame alloc + unconditional full-frame render), not masked.
- Load: per-frame idle cost reduced (GL/ImGui pass skipped when settled); Listening/waveform read is allocation-free after first reserve; cache is bounded 1-entry. No unbounded growth.
- Hostile: At() is bounds-checked (throws rather than reading garbage); SnapshotInto cannot overflow (writes only filled_ elements); no new input surface, no secrets, no network.
- Operability: fade/dwell/clock bookkeeping still runs every frame so transitions complete; hover entry, drags, and DPI moves all force repaints.
- Reversal: revert commit d9e86dd to restore prior behavior; tests 1 and 3 will fail on the reverted tree.

## 9. Edge cases and abuse scenarios
- Hover in/out during idle: cheap poll detects the change and forces a full frame. Covered by code path inspection (no live GUI run — see risks).
- Click during idle on Paused (resume): click requires cursor near the dot, which itself flips the poll and forces a render+UpdateDrag this tick; click cannot be swallowed while the mouse is elsewhere.
- Drag: early-out is disabled while dragging_ (condition includes !dragging_); UpdateDrag also sets dirty_ on start/move/end.
- DPI move mid-idle: OnContentScale sets dirty_ and pending_dpi_scale_; the early-out requires pending_dpi_scale_ <= 0, and the apply invalidates the font-size cache before measuring again.
- SetState from within RenderFrame (dwell expiry to Ready): sets dirty_, then the same frame renders fully and clears dirty_ — settled idle resumes next tick.
- Reduce-motion: fade_settled is immediately true, so reduce-motion idle also early-outs (with hover/dirty wakeups); animated states still render per-frame as before.

## 10. Performance and security considerations
- Idle Ready/Paused with settled fade: one window-pos + window-size + a single OS cursor-rect query per frame instead of a full GL clear + ImGui NewFrame + draw + swap. No new allocations.
- Listening: SnapshotInto into a reused member buffer (no per-frame heap alloc after warm-up); status-word size is O(1) cache hit per frame instead of CalcTextSize each frame.
- No secrets, no network, no logging changes. PushFont/PopFont are strictly balanced (no early return between them).

## 11. Commands run and results (verbatim summaries)
- cmake --build build --config Release --parallel 4 --target lwf_tests -> Built target lwf_tests (OverlayModel.cpp + test_overlay_settings_ui.cpp recompiled, linked OK).
- cmake --build build --config Release --parallel 4 --target local-wisper-flow -> Built target local-wisper-flow. (First attempt failed on the Windows `near` macro collision at Overlay.cpp:279; renamed the poll variable to poll_near and rebuilt green.)
- ./build/lwf_tests.exe --test-case="*mini badge alphas*,*waveform ring SnapshotInto*,*mini status word*" (from repo root) -> 3 passed. From build/ the source-scan case fails (REQUIRE on open) because the test resolves "src/ui/Overlay.cpp" relative to the working directory — same as the pre-existing T-001 source-scan tests; ctest runs it from the source root and passes.
- ./build/lwf_tests.exe --test-case="*mini badge alphas*,*waveform ring SnapshotInto*,*mini status word*,*contrast*" -> 5 passed, 48 assertions.
- ctest --test-dir build -R lwf-tests --output-on-failure -> 1/1 Passed (16.4 s).
- bash scripts/check_hygiene.sh -> hygiene: OK (exit 0).
- dart analyze -> NOT applicable (C++ project; dart not present).

## 12. Deviations from the plan/brief; ownership issues; deferred dependencies
- Also applied saved_dwell 1.6->1.5 in ThemePalette.h: sibling W2-IMPL-A1A3's NEED (COMMS T-002) explicitly requested it and ThemePalette.h is in my write-set; MainWindow reads the token so the fix propagates. Not in my own brief text, flagged here.
- COMMUNICATION env: `printf`/`tail`/`sh` are unavailable in the PowerShell host; used Add-Content for the COMMS append and `bash scripts/check_hygiene.sh` for the hygiene gate. No rule impact (append-only, one line, correct format).
- No ownership violations; no deferred dependencies remaining (sibling NEED resolved by this change).

## 13. Out-of-scope findings
- None new. Observation during verification (NOT changed): pre-existing source-scan tests resolve paths relative to the working directory, so running lwf_tests.exe from build/ fails those cases; ctest (source-root cwd) passes. No action taken per ownership/scope rules — recording as FINDING for ORCH to note.

## 14. Known limitations, risks, rollback notes
- Widget painting was not exercised live (no display/GUI run in this environment; same limitation as all Windows adapter code per AGENTS.md). The idle hover poll derives dot geometry from GLFW window size rather than ImGui DisplaySize; they agree at window pixel level, but under fractional DPI the rounding of the centered dot could theoretically differ by a pixel from DrawBadge's ImGui-space math. Worst case is a one-frame hover highlight discrepancy, not a missed click (pass-through sync still happens in DrawBadge on the next rendered frame; any state change forces dirty).
- Frame skipping does not change queueing: all engine->Overlay calls happen on the main-loop thread per the class contract; dirty_ is not atomic but the class is single-threaded.
- Rollback: revert d9e86dd.

## 15. Where I am least confident / what the reviewer should scrutinize
- The OS-space vs ImGui-space hover-polygon parity under non-1.0 DPI (above) — needs a real-hardware glance (docs/MANUAL_TEST_WINDOWS.md checklist) on a 150% scale display.
- That Adding a second bitmap font at 13px and pushing it via PushFont(message_font_, font_body_px) renders the attention message at the body size rather than double-scaling (FontScaleMain still applies once, same as MainWindow's scoped pattern). The build proves the API use compiles; only a live badge can show the pixels.

## 16. Self-review checklist
- [x] Root cause, not symptom
- [x] No band-aid patterns
- [x] Tests fail without the fix (alpha/source-scan fail pre-fix; new-API test is a compile-fail pre-fix)
- [x] Only write-set files changed (+ COMMS line + this report, per standing orders)
- [x] No secrets/PII
- [x] Conventions from AGENTS.md followed (no TODO/headers; tests-first kept; doc comments with DECISION/INVARIANT)

## 17. Commits
code: d9e86dd | report: pending
