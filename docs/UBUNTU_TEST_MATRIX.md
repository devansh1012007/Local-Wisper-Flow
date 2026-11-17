# Ubuntu 24.04 port - test matrix (L0)

Branch `linux/ubuntu-24.04`. Windows behaviour must not change (R1) - every
gate below is additive to the existing Windows CI.

## Targets

| Priority | System | Session | Status |
| --- | --- | --- | --- |
| Primary (must work) | Ubuntu 24.04 LTS, GNOME | Wayland (default) | pending L2+ |
| Secondary (must work) | Ubuntu 24.04 LTS, GNOME | Xorg | pending L2+ |
| Tertiary (best effort) | Xubuntu 24.04, XFCE | Xorg | pending L2+ |
| Out of scope | Kubuntu/KDE, Ubuntu <= 22.04, ARM, Flatpak/Snap | - | - |

## Gates (run on every phase)

| Gate | What | How it is checked at L0 |
| --- | --- | --- |
| G1 | Linux build with zero new warnings | local build clean; CI build steps fail on warnings only when compilers are set to error - visual check on every phase until a -Werror policy is decided |
| G2 | ctest green | `ctest --test-dir build --output-on-failure` in the linux and linux-ubuntu-24 jobs |
| G3 | hygiene + licence scripts | `check_hygiene.sh`, `check_no_network.sh`, `check_licenses.sh` in the linux job (existing) |
| G4 | Windows CI job green, unchanged | the windows job is untouched by this branch; GitHub Actions verifies |
| G5 | portable core builds with NO desktop libraries | local bare-sandbox build (see UBUNTU_DEPS.md); dedicated container job planned for L1 |

## CI coverage

| Job | Runner | Purpose |
| --- | --- | --- |
| linux (existing) | ubuntu-latest | hygiene, licences, portable build, ctest |
| linux-ubuntu-24 (new, L0) | ubuntu-24.04 (pinned) | noble dep install (R5 name check), portable build, ctest, xvfb smoke |
| windows (existing) | windows-2022 | unchanged - must stay green (R1) |
| windows-release (existing) | windows-2022 | unchanged; a linux release job is added at L8 |

The L0 smoke step runs the current Linux app binary (a stub that loads
config, prints one line, exits 0) under `xvfb-run` and greps its banner. It
proves the binary links and runs with a display; it becomes the real GUI
smoke (window opens, first frame renders, exit) at L4.

## Manual tests (Devansh runs; PASS/FAIL + screenshot)

| # | When | Test | Decision/feature | Status |
| --- | --- | --- | --- | --- |
| M1 | after L2 | X11: hold the hotkey in gedit, text appears, cursor position correct | D2/X11 hotkey + L3 injection | PENDING |
| M2 | after L4 | X11: settings window opens, every page renders, no clipped controls | L4 GUI | PENDING |
| M3 | after L4 | X11: mini overlay stays on top, does not steal focus, drags | L4 overlay | PENDING |
| M4 | after L5 | GNOME: tray icon appears; Start / Pause / Quit work | D6 SNI tray | PENDING |
| M5 | after L7 | Wayland: hotkey (D2a) fires in gedit; if not supported, the settings page shows the reason | D2 (portal absence is the verified expectation) | PENDING |
| M6 | after L7 | Wayland: dictation puts text on the clipboard; notification says Ctrl+V | D1a | PENDING |
| M7 | after L7 | Wayland: models download with hash verification; update check reports the current version | L6 transport | PENDING |
| M8 | after L8 | Fresh Ubuntu 24.04 VM, AppImage only: install, run, dictate, uninstall; data folder untouched unless the user deletes it | D4 packaging | PENDING |
| M9 | after L1/L4 | Microphone: a silent or muted mic shows the "mic is silent" message, not a silent failure | audio UX parity | PENDING |
| M10 | after L1 | kill -SEGV the process; the crash log is written to the data folder | CrashHandler (POSIX) | PENDING |

Primary AND secondary targets must PASS, or the result is recorded as a
known limit with the reason (spec section 7).

## Known limits to state in the README (spec section 8, landed at L8)

- Wayland cannot paste into apps directly without a portal consent each
  session; the default is clipboard plus Ctrl+V (D1a).
- Wayland does not allow the always-on-top mini overlay in GNOME (D3).
- Active-window app detection is not available on Wayland; profiles fall
  back to "default".
- Global hotkey on Wayland depends on the portal version - absent on GNOME
  46 (verified, D2); evdev opt-in is labelled and off by default.
- No code signing on Linux; verify with the checksums and GPG key.
