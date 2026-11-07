# Mission — wave run 20261106-gui

- **Run ID:** 20261106-gui
- **Date:** 2026-11-06 (session); gates re-run when scheduled
- **Mission statement:** Finish the Local-Wisper-Flow GUI fix/improvement stream on branch `fix/gui-typography-dpi` — replace the obsolete ImGui `SetWindowFontScale` font mechanism with scoped `PushFont` / design-px typography tokens (task A2), complete the remaining approved GUI tasks (A1 keyboard nav, A3 copy/state, A5 idle cost, A6 mini contrast), clear the engine/CI follow-ups (B1 streaming commit race, B2 zig type-check G3, C1 CI watch), and add further UX features as directed.

- **Branch / base:** `fix/gui-typography-dpi` @ `42a7ea0f7bbc5ae71b84ec9a838d0558dcb303f0` (contains base `63f543a` + BoringSSL `OPENSSL_NO_ASM` fix commit). Note: operation branch kept as the user's feature branch rather than `ops/20261106-gui` per user direction.

- **Pre-existing state at start of run:** clean working tree, no uncommitted changes. HEAD `42a7ea0`. Gates: G1 build OK; G2 `ctest -R lwf-tests` → 1/1 Passed (17.27 s); full `ctest` not run to completion this session (re2 exhaustive suites timed out; infrastructure-only, unrelated); G3 zig pending (no zig on box); G4 `scripts/check_hygiene.sh` OK (after Overlay windows.h fix); G5 not run; `scripts/check_no_network.sh` OK.

- **Out of scope (this run):** real TTF font loading (font approval pending — flagged by audit D6/R1 as awaiting approval; must NOT be bundled here); engine logic beyond task B1; Windows-hardware claims.

- **Assumptions (A-1 …):** A-1 scoped font mechanism uses design-px tokens + `ImGui::PushFont(NULL, design_px)` + `PopFont`, mirroring ImGui 1.92.9b's sanctioned pattern; `metrics::mini_text_px` (8.5) is the only existing font token.

- **Open questions (Q-1 …):** Q-1 effective `FontSizeBase` for Full-window context (default font `LegacySize`); Q-2 font-approval record location.
