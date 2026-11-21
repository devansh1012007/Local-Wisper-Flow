# Studio checklist (spec §9) — F9 sweep result

> SUPERSEDED (visual system) 2026-11-09 by the Hairline identity on
> `ops/ui-redesign-v2` — see `docs/design/DESIGN.md`. The studio visual system
> this checklist pins (teal / StudioPalette / rail / studio cards) was
> re-skinned to Hairline in T3-T10; this document is retained for history only
> (T16 docs sweep).

Status of every §8 checkpoint item (I/D/S/T/Sn/P1). PASS-able automatable items are pinned by
doctest; Devansh-manual items require a Windows run with the real binary (this box has no zig
and no window runtime — G3 "pending on Windows" per AGENTS.md).

Verdict key: PASS (automatable, proven) | MANUAL (needs Devansh on Windows) | PROVISIONAL
(passes at token level; pixel-measured F0(a) still pending — source screenshots are not in the
repo, so no pixel claim is made anywhere in this sweep).

## Contrast (T-contrast, automated)

- PASS (token math, `tests/test_overlay_settings_ui.cpp` "studio palette contrast"): ink on all
  five light fills >= 4.5; muted on page/rail/card/white/cream >= 4.5 (muted #6E6A62 resolves
  the Wave-3 blocker: old #7A776F was 4.00 on card fills); white on teal_700 >= 4.5 (primary
  buttons) and on teal_500 >= 4.5 (hover); teal_700 on teal_100 >= 4.5 (badge/chip); teal_300
  on ink >= 4.5 (toast action); teal_500 underline vs page >= 3.0; toggle ink-vs-muted >= 3.0.
- PROVISIONAL (documented exception, do not relax dark tokens): `border` hairlines sit at
  1.13-1.27 vs fills (< 3.0) — planned decorative exception (STUDIO_PLAN §5), pinned `< 3.0`.
- PROVISIONAL (F0(a) pixel pending): heat ramp vs page — teal_300 2.01, teal_100 1.21,
  heat_empty 1.62 (< 3.0). Heat-cell encoding below 3:1 is a Wave-1 design estimate; F0(a)
  pixel re-measure must arbitrate before any ramp change.
- Devansh action (F0(a)): save the 5 source screenshots to `docs/screenshots/source/`, re-run
  the token table, update `tests/test_overlay_settings_ui.cpp` token pins in the same commit.

## Copy (T-copy, automated)

- PASS (source scan `F9 copy lint bans internal terms and exclamation in display strings`):
  no display string in `src/ui/**` contains Oops, `!`, FSM/VAD/LLM/STT/Whisper/Provider/Pipeline
  as whole words. "Cleanup", "Hotkey", "API key" intentionally kept (product feature names /
  standard user terms, not internal terms).
- MANUAL: re-read every page on Windows for tone/clarity; banner copy per D13 is original.

## Keyboard (automated where strippable, rest manual)

- PASS (source-pinned): rail focus is NavActive-gated (MainWindow.cpp); every interactive
  widget draws a focus ring (page buttons, icon buttons, toggle, chip, tabs, toast action,
  text links); Modal/Sheet EndPopup pairing (Esc + Close-then-End paths).
- MANUAL for Devansh: Tab through rail, tabs, search, sheet, dialog, editor, pickers on each
  page; verify initial focus on dialog open, Esc closes sheet/dialog/confirm, modal focus
  stays inside, Toast action is reachable and activates with Enter/Space, dropdown menus
  (sort) open/close by keyboard.

## Checkpoints

- I1 (Insights): MANUAL — tabs, gauge "Average" label, calendar colors, share dialog disabled
  buttons reference, generation-counter refresh on Windows.
- D1 (Dictionary): MANUAL — add/duplicate/search/star/undo; dialog sort.
- S1 (Snippets): MANUAL — 4 validation messages, sheet flow.
- T1-T6 (Transforms): MANUAL — editor, shortcut recorder, diff viewer (Win+Alt+O),
  opt-in dimming, reset confirm.
- (Scratchpad S1n-S5n: retired — the Scratchpad page and its functions were
  removed outright in remove/scratchpad-page.)
- P1 (Privacy): MANUAL — delete-all two-step confirm on Windows. A4 delete-scope findings
  R-W4-001/002/003/005/006/007 were FIXED by W4-IMPR-01 (98f7685); R-W4-004 (usage_count
  dead) remains open.

## Gates

- PASS: `cmake --build build --config Release --parallel 4 --target lwf_tests`, scoped ctest
  `test_overlay_settings_ui` (52 cases incl. new F9 pins), `bash scripts/check_hygiene.sh`,
  `bash scripts/check_no_network.sh`.
- PENDING on Windows: G3 zig type-check (no zig on dev box); full 23/23 ctest (scoped mandate);
  MSVC build (CI lane only).

## Integration (W5-IMPL-INT, T-026-integration)

- PASS: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build
  --config Release --parallel 4` (exit 0, exe links), full
  `ctest --test-dir build` 23/23 in 660.70s, `bash scripts/check_hygiene.sh`,
  `bash scripts/check_no_network.sh`. Evidence: gates/gate-w5-1.md.
- PASS: CMake registers OpenRouterProvider.cpp (lwf_core) +
  test_openrouter.cpp (lwf_tests); `*openrouter*` 19/19 cases, 118 asserts.
- PASS: MainWindow Privacy/About/Cloud/Trust copy — O egress + P lint + RESEED
  delete-all clause drop (dictionary tombstone f66943b). No `!`, no internal
  terms; local-first promise kept; OpenRouter opt-in stated on Privacy + About.
- MANUAL (Devansh on Windows): live OpenRouter cleanup (key + model), missing
  key/model degrade, cancel/timeout — O report steps 1-7; G3 zig still pending.
