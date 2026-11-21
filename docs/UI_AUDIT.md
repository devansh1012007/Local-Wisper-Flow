# UI AUDIT — D0

> Addendum (2026-11-06): the "one context per window" fix in the R1 re-land was
> incomplete — `ImGui::CreateContext()` restores the previous context after
> initializing the new one, and both Init() paths read `GetCurrentContext()`
> instead of the returned context, so the windows still shared one context and
> the overlay's backend overwrote it in Release. The Full window rendered with
> `DisplaySize` = the 140x70 Mini badge and a (0,0,140,70) GL viewport, i.e. a
> black screen with a tiny pill bottom-left. Fixed in 1a8304f; see AGENTS.md
> "UI (GLFW + Dear ImGui, two windows)". The audit's verdict stands; this
> addendum records the post-R1 state.

Audit of everything under `src/ui/`, plus the UI-adjacent wiring in `src/main.cpp`,
as the baseline for the GUI redesign (Mini resident dot/pill + Full window).

Method: every file was read in full. Backend contracts were verified against the
exact pinned dependency versions (`CMakeLists.txt`: GLFW 3.4, Dear ImGui
`v1.92.9b`, both via FetchContent) by reading the tag's sources, not from memory.

Hard constraint carried into every verdict below: the redesign is visual/UX only.
Audio capture, STT, LLM cleanup, and injection logic are not touched. Every
proposed change lives in `src/ui/` (plus a new shared theme/font module) and in
the UI wiring sections of `src/main.cpp`.

## 0. Verdict summary

| Component | Files | Lines | Verdict for the redesign |
|---|---|---|---|
| `MainWindow.{h,cpp}` | 2 | 153 | **Keep** as Full-window host and app heartbeat; replace all content (state/FPS debug panel becomes the Home page). Gets the per-window ImGui architecture fix. |
| `Overlay.{h,cpp}` | 2 | 231 | **Keep** the window, flags and core-model split; replace visuals (this becomes Mini: dot + expanding pill). Gets its own ImGui context. |
| `SettingsWindow.{h,cpp}` | 2 | 224 | **Absorb, then delete.** Tab content becomes pages inside Full (General/Speech/Cleanup/History/Privacy/About). The standalone OS window, its backend init and its Save/Cancel footer go away. |
| `OnboardingWizard.{h,cpp}` | 2 | 110 | **Keep the flow, rehome it.** Mic test / hotkey capture / model catalog are the right first-run steps, but they become a route inside Full. The standalone `ImGui::Begin` rendering is deleted (it is broken as wired, see D3). |

Nothing in `src/ui/` is deleted outright in D1: `MainWindow` and `Overlay` evolve
in place; the other two stop being separate OS windows once Full hosts their
content.

Provenance note: the entire current `src/ui/` tree was created or rewritten in a
single batch commit (`8e4945a`, "Add comprehensive unit tests...", 617 insertions
in `src/ui/`). The manual-check PASS entries in `docs/PROGRESS.md` (D2/D4/D8 at
commit `94872bd`) refer to an earlier UI that no longer exists. **The current UI
has never been verified on Windows.** Four defects below are verifiable from the
sources alone.

## 1. Cross-cutting architecture finding — one ImGui context, three backends

Severity: **BLOCKER** for any D1 work. This must be fixed before any restyling,
because the fix changes where every `NewFrame`/`Render` call lives.

`main.cpp` initializes three GLFW windows, and each of `MainWindow::Init`
(`MainWindow.cpp:43-50`), `Overlay::Init` (`Overlay.cpp:69-73`) and
`SettingsWindow::Init` (`SettingsWindow.cpp:27-31`) calls
`ImGui_ImplGlfw_InitForOpenGL` + `ImGui_ImplOpenGL3_Init` — **all on the single
ImGui context created by `MainWindow::Init`** (init order in `main.cpp:411-421`:
dashboard, then overlay, then settings).

Dear ImGui's backends allow exactly one platform backend and one renderer
backend per ImGui context. Verified in the pinned tag:

- `imgui_impl_glfw.cpp:674`: `IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");`
- `imgui_impl_opengl3.cpp:1029`: `IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");`

Consequences:

- **Debug builds** abort at `overlay.Init()` — the second platform-backend init.
- **Release builds** (`NDEBUG`, IM_ASSERT compiled out): each init overwrites
  `io.BackendPlatformUserData`/`BackendRendererUserData`, so the *last* window
  (the hidden 720×560 settings window) owns the backend data. Every
  `ImGui_ImplGlfw_NewFrame()` — including the dashboard's and the overlay's —
  then reports the settings window's size and input state. The dashboard and
  the pill render with `DisplaySize` 720×560 from a hidden window and with
  mouse state that never updates. (Per-window drawing still lands in the right
  framebuffer only because each `RenderFrame` makes its own GL context current.)
- **Shutdown** is worse: `overlay.Shutdown()` (`main.cpp:503`) frees the one
  backend-data block and nulls the user-data pointers;
  `settings_window.Shutdown()` (`main.cpp:504`) then calls
  `ImGui_ImplGlfw_Shutdown()` again, which dereferences the null backend data
  (`if (bd->InstalledCallbacks)` in `imgui_impl_glfw.cpp:803` — there is no
  null guard after the assert). Release build: null dereference on exit.
  Debug build: assert. `MainWindow`'s destructor then does it a third time.

Required fix (the pattern D1 adopts):

- **One ImGui context per OS window.** Mini (overlay GLFW window) and Full
  (main GLFW window) each get `ImGui::CreateContext()`, their own backend
  instances, their own font atlas, and the render loop switches with
  `ImGui::SetCurrentContext()` before each window's
  `NewFrame`/`Render`/`RenderDrawData` bracket. This is the documented approach
  for multiple windows without dragging in multi-viewport (which we do not want
  on a click-through overlay).
- Exactly one component in the app owns context creation/teardown order; the
  others become pure "render into my context" classes. The settings window as
  an OS window disappears entirely (absorbed into Full), which reduces the
  count from three windows to two.

## 2. File-by-file

### 2.1 `MainWindow.{h,cpp}` — debug dashboard that is accidentally the app's heartbeat

What it is: a decorated 480×200 GLFW window ("Local Wisper Flow",
`MainWindow.cpp:33`) rendering three lines: the FSM state string, an audio-level
progress bar, and an FPS counter (`MainWindow.cpp:79-87`). It creates the app's
only ImGui context and owns the `RunOnce`/`ShouldClose` cadence the main loop
polls (`main.cpp:428`, `492`).

What is right (keep):

- The GLFW+ImGui bootstrap sequence and the careful `Shutdown()` ordering
  (`MainWindow.cpp:101-123`) are sound in isolation.
- `RunOnce()` as a tick the shared loop can call is the right shape for a
  background utility.
- Constructed with const refs to `DictationFSM`/`MiniaudioCapture` and reads
  only — no engine logic leaked in.

What is wrong:

- Content is a developer debug panel (raw `ToString(state)`, FPS), not a Home
  page. It must become the Full window's Home (status, last transcript, quick
  actions) per `docs/design/mockup.html` §3.
- `ImGui::SetWindowFontScale(1.8f)` (`MainWindow.cpp:81`) is a size hack, not a
  font strategy. No font is loaded anywhere in the app (default embedded 13 px
  bitmap font, DPI-blind). D1 loads real TTFs at per-DPI sizes into each
  context's atlas.
- Closing this window quits the whole app (`main.cpp:428` gates the loop on
  `dashboard.ShouldClose()`). For a background dictation utility, closing Full
  must hide it; the tray owns Quit. (Wiring change in `main.cpp`, not engine.)
- Hardcoded clear color `0.08/0.09/0.11` (`MainWindow.cpp:94`) — moves to the
  shared theme tokens.
- Window title says "Wisper" (project spelling) — keep the product name
  consistent with the tray and About strings when restyling.

Verdict: **keep the file and its role; replace the body.** It becomes
`FullWindow` in spirit (rename optional, do it in D1 to avoid a `git mv` +
rewrite mixing).

### 2.2 `Overlay.{h,cpp}` — the pill, ancestor of Mini

What it is: a borderless, always-on-top, transparent, click-through 260×44
GLFW window pinned above the taskbar (`Overlay.cpp:15-17`, `47-55`), rendering
per-state visuals driven by `core/OverlayModel` (states Hidden/Listening/
Processing/Error/Cancelled, waveform ring, auto-hide timers, fade-in over
120 ms, reduce-motion switch).

What is right (keep — this is the good part of the current UI):

- The entire GLFW hint set (`Overlay.cpp:47-55`): undecorated, floating,
  transparent framebuffer, mouse-passthrough, no focus, no focus-on-show —
  exactly right for a resident overlay — plus `WindowStyles::ApplyOverlayStyles`
  on the Win32 handle.
- The model/view split: all logic (state table, auto-hide ms, `WaveformRing`,
  `PillPosition` with DPI-scaled geometry) lives in `core/OverlayModel`,
  unit-tested in `lwf_core` with no UI dependency. The redesign keeps this and
  extends the state vocabulary for Mini (idle dot vs listening pill vs error).
- Early-out render loop when hidden (`Overlay.cpp:119-124`), auto-hide
  countdown, and the reduce-motion branch are all behaviors worth carrying.

What is wrong:

- Participant in the shared-context defect (§1) — its `Init` is the second
  backend init; in release its frames report the settings window's size.
- Fixed pixel metrics: 260×44 window, 22 px rounding, 24 px plot height
  (`Overlay.cpp:15-16`, `159`, `170`) with no DPI scaling of content (only the
  window geometry is scaled inside `OverlayModel::PillPosition`).
- `SetWorkArea` is never called from `main.cpp`; the work area is read once at
  init from `SystemParametersInfoW` (`Overlay.cpp:63`, `19-35`) and never
  updates on display changes or DPI changes.
- Mouse-passthrough is permanent: nothing about the Mini can ever be hovered,
  clicked, or dragged. The redesign needs a hover/drag policy for the dot
  (e.g. a hit-test toggle via `WS_EX_TRANSPARENT` flip, or drag by hotkey).
  This is the only place a Win32-level interaction change is allowed.
- Hardcoded colors (`0.08,0.08,0.10,0.92` bg at `Overlay.cpp:160`, error red
  at `:178`) and default 13 px font — everything moves to theme tokens +
  loaded fonts per the mockup.
- Text-only states ("transcribing...", "cancelled", `Overlay.cpp:175`, `181`)
  do not match the Mini spec (dot + expanding pill with waveform); this is the
  visual core of D1.

Verdict: **keep; evolves into Mini.** Own ImGui context, theme tokens, new
`DrawPill`, DPI-aware metrics, display-change handling.

### 2.3 `SettingsWindow.{h,cpp}` — a second app pretending to be a dialog

What it is: a hidden 720×560 GLFW window shown on demand (tray, second
instance, first run), with tabs General / Speech / Cleanup / History / Privacy
/ About, Save/Cancel footer, validation via `core/SettingsUiModel`, and UiBus
commands on save (`ApplySettings`, `StartLearnHotkey`).

What is right (keep — the content model):

- The settings surface itself maps cleanly onto the redesign: General+hotkey →
  Full General page; Speech/Cleanup → Full Speech page; History, Privacy,
  About → their own pages. All widgets survive as content.
- Validation-before-write via `SettingsUiModel::ValidateInt/ValidateModelPath`
  and the `restart_needed_` concept are worth keeping (as a per-page banner).
- Posting `UiCommand`s through `UiBus` instead of reaching into the engine is
  exactly the right pattern; the redesigned Full window keeps speaking only
  UiBus.

What is wrong:

- **D2 (crash):** `ImGui::Checkbox("hands-free double tap", nullptr)`
  (`SettingsWindow.cpp:89`). Dear ImGui `Checkbox` has no null guard — it
  dereferences unconditionally: `imgui_widgets.cpp:1268`: `bool checked = *v;`.
  General is the first tab, so **opening Settings crashes the app** (null read
  in release, assert in debug). The toggle also writes nowhere even if fixed.
- **D4 (overflow):** `SettingsWindow.cpp:97-99` copies the stored
  `whisper_model_path` into `char model_path[512]` with an unchecked
  `std::copy(current.begin(), current.end(), model_path)` — a stored path of
  512+ chars overflows the stack buffer; 511 chars leaves no terminator.
  Both defects are fixed for free when the widgets are ported to the Full
  pages (use `std::string` + `ImGui::InputText` with `%s` semantics or the
  callback form).
- Participant in the shared-context defect (§1) — its `Init` is the third
  backend init, and the one that "wins" in release.
- Save/Cancel semantics are inconsistent: the title-bar X (`Begin("Settings",
  &open_)`, `SettingsWindow.cpp:79`) closes without `Reload()`, leaving
  half-applied edits in the store; "Save" doesn't close; "Cancel" reloads and
  closes; `validation_error_`/`restart_needed_` never clear. The redesign
  replaces this with live-apply per page + an explicit "restart required"
  banner (see mockup §4), no footer buttons.
- Path entry is a raw text field; model choice deserves the file-picker /
  catalog treatment already sketched by onboarding step 3.
- Hardcoded tab colors and clear color (same as §2.2).

Verdict: **absorb into Full, then delete.** No standalone OS window, no own GL
context, no footer. The class dissolves into `FullWindow` pages + the existing
`SettingsStore`/`SettingsUiModel` calls.

### 2.4 `OnboardingWizard.{h,cpp}` — the right flow, never once rendered

What it is: a 4-step first-run flow (mic test → PTT chord capture → model
catalog from `models.json` → finish), with `SetMicLevel`/`SetDetectedHotkey`
fed from the main loop (`main.cpp:468-473`, `490`).

What is right (keep):

- The step order and the feed-in pattern (level and detected chord pushed from
  the loop, wizard never polls audio or the hotkey listener directly) are
  correct and survive unchanged as a Full-window route.
- Gating on `first_run || whisper_model_path.empty()` (`OnboardingWizard.cpp:10-13`)
  matches the "app must be usable after a broken model load" requirement.

What is wrong:

- **D3 (broken rendering):** `RenderFrame()` calls `ImGui::Begin(...)`
  (`OnboardingWizard.cpp:35`) with no `ImGui::NewFrame()` before it and no
  `Render` after. In the main loop it runs after `settings_window.RenderFrame()`
  has already ended its frame (`main.cpp:492-495`), so `Begin` executes outside
  any frame: `imgui.cpp:7531`: `IM_ASSERT(g.WithinFrameScope); // Forgot to
  call ImGui::NewFrame()`. Debug: abort on first run. Release: undefined
  behavior. The wizard has never been displayable in this form.
- Step 1's "Open microphone settings" button only shows a tooltip saying
  `run: ms-settings:privacy-microphone` (`OnboardingWizard.cpp:44-46`) instead
  of launching it (`ShellExecuteA` is already used in `main.cpp:437`, so the
  capability exists and is one UiBus command away).
- Step 3 text promises "Downloads only start when you click Download"
  (`OnboardingWizard.cpp:73-74`) but no Download button exists — only "Skip".
  Either the button arrives in D1's Full onboarding route or the copy changes.
- `Onboarding::ParseModelsJson("models.json")` (`OnboardingWizard.cpp:56`) is
  cwd-relative; the redesign resolves it against the exe/data dir like every
  other path in the app.

Verdict: **keep the flow and state logic; rehome as a Full-window route.**
Delete the standalone `Begin/End` rendering entirely.

## 3. Adjacent UI surface (context — not in `src/ui/`, not redesigned)

- `os/WindowsTray.cpp:109-119` — tray menu: Enable dictation (check) |
  Settings... | History... | Open logs folder | Autostart (check) | Quit.
  Stays as the always-available control surface; Mini's right-click can reuse
  the same UiBus commands. **Bug:** `UiCommandType::OpenHistory` is routed to
  the logs folder (`main.cpp:435-439` falls through to `OpenLogsFolder`), and
  no History UI exists at all — the tray's "History..." misleads. The redesign
  either gives Full a History page (preferred; data model `HistoryStore`
  already exists) or drops the menu label. Not fixed in D0; registered as D5.
- `core/OverlayModel.{h,cpp}` — keep as-is; D1 extends the state set for Mini
  (idle dot state, error pill copy) but the model stays pure and tested.
- `os/WindowStyles` — `ApplyOverlayStyles(HWND)` for the overlay; keep.
- `core/UiBus` — command vocabulary (8 commands, `UiBus.h:10-19`) is the only
  channel the redesigned windows may use; it already covers everything the
  current UI needs. A History command is the one likely addition.

## 4. Current surface → redesign mapping

| Current | Target (see `docs/design/mockup.html`) | What moves |
|---|---|---|
| `MainWindow` debug panel | Full / Home (§3) | File kept, content replaced; hide-on-close instead of quit |
| `Overlay` pill | Mini: idle dot + listening pill (§2) | File kept, new `DrawPill`, own context, theme |
| `SettingsWindow` tabs | Full pages: General, Speech (§4), Cleanup, History, Privacy, About | Content ported; window + footer deleted |
| `OnboardingWizard` steps | Full onboarding route (first run opens Full on Home) | State logic kept; rendering deleted |
| `WindowsTray` menu | Unchanged (plus Mini right-click parity) | None in D0; OpenHistory fix = D5 |
| `OverlayModel` states | Extended for Mini (idle/dot, richer error copy) | Additive, stays pure |

## 5. Defect register (burn-down for D1)

| # | Severity | Where | Defect |
|---|---|---|---|
| D1 | BLOCKER | `main.cpp:411-421` + all three `Init`s | Three platform/renderer backend inits on one ImGui context: debug abort, release last-wins + wrong DisplaySize, double-Shutdown null deref on exit (§1) |
| D2 | HIGH (crash) | `SettingsWindow.cpp:89` | `Checkbox(..., nullptr)` → null deref when General tab renders (`imgui_widgets.cpp:1268` has no null guard) |
| D3 | HIGH (UB) | `OnboardingWizard.cpp:35` + `main.cpp:492-495` | `Begin` outside any frame (`imgui.cpp:7531` assert); wizard never rendered correctly |
| D4 | HIGH (overflow) | `SettingsWindow.cpp:97-99` | Unchecked `std::copy` into `char[512]` on a user-controlled path |
| D5 | MED (misleading) | `main.cpp:435-439`, tray `History...` | `OpenHistory` opens the logs folder; no History UI exists |
| D6 | MED | all `src/ui/` | No font loading; `SetWindowFontScale(1.8)` hack; default 13 px font, DPI-blind |
| D7 | MED | `Overlay.cpp:15-17,159,170` | Fixed pixel metrics; no DPI scaling of content; work area never refreshed after init |
| D8 | MED | `Overlay.cpp:50` | Permanent mouse-passthrough: no hover/drag policy for the dot |
| D9 | LOW | `SettingsWindow.cpp:79,184-192` | Save/Cancel/X semantics inconsistent; validation/restart flags never clear |
| D10 | LOW | `OnboardingWizard.cpp:44-46,56,73-74` | ms-settings tooltip instead of launch; cwd-relative `models.json`; copy promises a nonexistent Download button |
| D11 | LOW | `MainWindow.cpp:86` | FPS counter in a user-facing window |
| D12 | LOW | includes across `src/ui/` | Mixed imgui include styles (`<imgui_impl_glfw.h>` vs `"backends/imgui_impl_glfw.h"`) — standardize when adding the shared UI module |
| D13 | HIGH (silent no-op) | `SettingsStore.cpp` schema | `dictation_enabled` was missing from the schema; `SetNumber` rejects unschemated keys, so the tray's "Enable dictation" wrote nothing and `GetBool` always returned false. Found during R1; the key is UI-only state |
| D14 | HIGH (data race) | `main.cpp` `EngineBridge` | The engine thread called `Overlay::SetState` directly, writing `state_`/`message_` while the render thread read them. Found during R1; fixed with a mutex-guarded attention box drained by the main loop |

D1-D4 are fixed structurally by the context-per-window rewrite and by porting
the widgets into Full pages; none of them require touching engine code.

## 5.1 Fix status after R1 (theme tokens, Mini, Full shell)

Landed in the R1 change (see docs/PROGRESS.md):

- D1 (blocker): fixed - one Dear ImGui context per OS window; the loop
  switches contexts; SettingsWindow and OnboardingWizard no longer exist as
  OS windows (absorbed into the Full window), so the count is two.
- D2, D3, D4: fixed by the absorption (widgets ported; the broken Checkbox
  and the unbounded path copy went away with the port; the wizard renders
  inside the Full window's frame).
- D5: unchanged in R1 (History page is R2; the tray label still opens logs).
- D6: partially - theme tokens and DPI scaling are in; real fonts still wait
  for approval, so the built-in font is scaled (`FontScaleMain`), with
  `SetWindowFontScale` for the two display sizes.
- D7: partially - all mini metrics scale with DPI; work-area refresh after
  display changes is still R3.
- D8: partially - hover (Ready opacity, attention message) and click-resume
  work via a runtime pass-through toggle; dragging the dot is R3.
- D9: fixed - pages apply live, footer buttons gone, restart banner replaces
  the stale flags.
- D10: fixed - ms-settings launches for real, models.json resolves from the
  exe directory, the copy no longer promises a download button.
- D11: fixed - the debug panel is gone.
- D12: fixed - one include style (`imgui.h`, `backends/...`).
- D13, D14: found and fixed during R1 (see register above).

## 6. Guardrails for D1+ (unchanged from the brief)

- Only `src/ui/`, a new `src/ui/theme/` (or similar) module, and the UI wiring
  in `main.cpp` change. `audio/`, `stt/`, `llm/`, injection, FSM, and stores
  are not modified.
- All engine interaction stays through `UiBus` commands and read-only
  snapshots, exactly as the current UI already does.
- GLFW 3.4 / ImGui v1.92.9b stay pinned; no new dependencies. Fonts and theme
  data are assets, not dependencies.
- No multi-viewport; two contexts, two windows.

## 7. Dual-palette retired — single Palette (Ph2-ShimDel, 2026-11-08)

The redesigned branch `ops/ui-full-redesign` retires the dual-palette split
(Hunter/Brass dark `kDarkPalette` + teal `kStudioPalette`): Ivory is the one
primary identity via the single `Palette` in `src/ui/theme/ThemePalette.h`,
with `studio_metrics` and `HeatColor` (brass ramp) moved verbatim into the
same header (`lwf::ui` namespace, ImGui-free). The deprecated `StudioPalette`
shim was deleted in `1e5651a`. Redesign guardrails R5-R9 are unchanged by
this retirement.

## 8. Status update — Hairline supersedes Ivory (ui-redesign-v2, 2026-11-10)

Recorded by the T16 docs sweep; §5.1 and §7 above stay as the point-in-time
records they were written as.

- §7's "Ivory is the one primary identity" is historical now. The Hairline
  identity (docs/design/DESIGN.md, signed off 2026-11-09) replaced Ivory on
  `ops/ui-redesign-v2`: two themes — Graphite (dark, runtime default) and
  Paper (light, config `theme` switch, live swap) — single accent A (lime
  0xB8E356 dark / 0x3F6212 light), heat ramp re-derived lime-over-Paper. Ivory
  and brass hex values survive only as history in the superseded mockup and
  plan documents.
- `docs/design/mockup.html` is superseded by DESIGN.md on this branch (legacy
  banner at the top of that file). The mockup.html section references in the
  audit text above describe the pre-Hairline direction and are kept verbatim
  as history.
- D5 (tray "History..." misroute, no History UI): CLOSED by T11 — the History
  page landed and the tray's OpenHistory routing was fixed.
- D8 (no hover/drag policy): hover, click-resume and dot drag are present on
  this branch (Overlay drag state + hit-test work); real-hardware verification
  is still owed.
- D6 (fonts) and D7 (DPI / work-area refresh) remain partial: content scales
  via FontScaleMain + design-px tokens, real fonts are still owed, and
  nothing calls `Overlay::SetWorkArea` after init (no display-change
  re-enumeration). Owed manual checks are consolidated in
  docs/MANUAL_TEST_WINDOWS.md section V2.
