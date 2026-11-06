# Brief — W2-IMPL-A5A6 · wave 2 · run 20261106-gui

Mission: GUI-A5 (idle cost) + GUI-A6 (mini contrast) + regression tests. Read STANDING_ORDERS, AGENTS.md, mockup §2/§5.

AC (measurable):
- A5: RenderFrame early-out for non-animated idle states (Ready/Paused) after dwell/fade bookkeeping + cheap hover poll; dirty_ flag set on SetState/PushLevel/OnContentScale/UpdateDrag; status-word CalcTextSize cached (1-entry); SnapshotInto/At accessor (keep Snapshot for tests); no per-frame heap alloc in Listening/Attention path.
- A6: mini_idle_alpha 0.35->0.55, mini_active_alpha 0.65->1.0; status-word 0.75 discount removed (draw at badge alpha); mini_message_w 140->250; second 13px bitmap font + PushFont around DrawAttentionMessage. Do NOT change palette hues.
- Tests in tests/test_overlay_settings_ui.cpp: idle/active alpha floor assertions; source-scan that status word no longer multiplied by sub-1.0 constant; keep contrast tests green.
- Build lwf_tests + local-wisper-flow green; ctest lwf-tests green; hygiene green.

WRITE-SET: src/ui/Overlay.{h,cpp}, src/ui/theme/ThemePalette.h, src/core/OverlayModel.{h,cpp}, tests/test_overlay_settings_ui.cpp. FORBIDDEN: src/ui/MainWindow.* (sibling owns). Explicit git paths, trailers Task: T-005/T-006, Agent: W2-IMPL-A5A6, Wave: 2.

Report: docs/agent-ops/20261106-gui/reports/w2-impl/W2-IMPL-A5A6-T-005.md (code commit then docs commit).
