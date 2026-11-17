# Ubuntu 24.04 port - decisions (L0)

Branch `linux/ubuntu-24.04`, cut from `origin/main` (0f8fb2e) + the review-fix
commits. Spec: the Ubuntu port brief. **Status: L0 complete - decisions D1-D6
below are PROPOSED and await approval before L1 starts (checkpoint L0).**

Everything in this document was verified against Ubuntu 24.04 "noble" sources
on 2026-11-17; where a claim could not be verified from here it is marked
"verify on VM" and appears in the manual test matrix
(`docs/UBUNTU_TEST_MATRIX.md`).

## Summary table

| Decision | Question | Recommendation | Status |
| --- | --- | --- | --- |
| D1 | Wayland text injection | Clipboard + notification (a); RemoteDesktop portal (b) later as an opt-in setting; ydotool (c) rejected | PROPOSED |
| D2 | Wayland global hotkey | Portal not available on GNOME 46 (verified) -> X11-first; evdev only as labelled opt-in | PROPOSED |
| D3 | Mini overlay on Wayland | Normal borderless window; no always-on-top on GNOME; tray-only fallback | PROPOSED |
| D4 | Packaging | AppImage + .deb (CPack), SHA256SUMS + GPG, no Store | PROPOSED |
| D5 | Speech/LLM defaults | Same as Windows: local whisper + local Qwen cleanup, cloud opt-in | PROPOSED |
| D6 | Tray | StatusNotifierItem over D-Bus, direct libdbus (no GTK); extension preinstalled on 24.04 (verified) | PROPOSED |

## D1 - Wayland text injection

- **(a) Clipboard + notification** [RECOMMENDED default]: put the text on the
  Wayland clipboard (wl-copy subprocess, v1), raise a libnotify notification
  "Dictated - press Ctrl+V". Works in every Wayland compositor, no
  permissions, no consent dialogs. Cost: one manual paste per dictation.
- **(b) RemoteDesktop portal**: real keystroke injection via
  `org.freedesktop.portal.RemoteDesktop`; GNOME shows a consent dialog once
  per session. Viable later as an opt-in setting ("type directly - approve
  the dialog each session"). Verify the session picker flow on GNOME 46 in a
  spike before committing to it.
- **(c) ydotool**: rejected - it wants system-wide uinput rights (a daemon
  running as root or input-group membership) for something the portal does
  with user consent.

X11 sessions do not need any of this: XTest types directly (L3), same
behaviour as Windows.

## D2 - Wayland global hotkey (push-to-talk)

Spec asked to VERIFY portal support on GNOME in Ubuntu 24.04 before choosing.
Verified 2026-11-17:

- `xdg-desktop-portal-gnome` on Ubuntu 24.04 is GNOME 46.2 and **does not
  implement `org.freedesktop.portal.GlobalShortcuts`**. Third-party reports
  hit the same wall on this exact combination (hex issue #781, "GNOME 46
  Wayland (Ubuntu 24.04): GlobalShortcuts", Nov 2026: "xdg-desktop-portal-gnome
  doesn't implement org.freedesktop.portal.GlobalShortcuts"). The portal
  interface exists in the xdg-desktop-portal spec since 1.15 but the GNOME
  implementation has not landed.

Recommendation, in order:

1. **X11 sessions**: `XGrabKey` on the root window (or XInput2 raw events) -
   primary path, parity with Windows behaviour (L2).
2. **Wayland sessions**: detect the portal's absence at runtime and say so -
   the Settings page shows "global hotkey needs an X11 session (GNOME 46 has
   no GlobalShortcuts portal yet)", and the tray menu keeps working because
   it is in-process.
3. **evdev reader** (spec D2b): explicit, labelled opt-in only - requires the
   `input` group, reads every key press on the machine, and the UI must state
   that before enabling it (privacy rule R7). Ship it off by default.

## D3 - Mini overlay on Wayland

GNOME does not let normal apps force always-on-top or place windows. Default:
a normal small borderless GLFW window (decorations off, no focus grab);
fallback: tray-only mode. Recorded as a known limit in the README (L8). On
X11 the overlay keeps its Windows-class behaviour (override-redirect /
`_NET_WM_STATE_ABOVE`, XShape for click-through) - L4 scope.

## D4 - Packaging

- **AppImage** (single file, no root) + **.deb** built with CPack. Both get
  SHA256SUMS, checksums GPG-signed. No Snap, no Flatpak, no Store.
- AppImage runtime on noble needs `libfuse2t64` (2.9.9-8.1build1, verified)
  - document it in the install instructions; the .deb has no such quirk.
- Verified at L0: noble's `libglfw3-dev` is 3.3.10 (< 3.4), confirming GLFW
  3.4 must keep being built from source (FetchContent, as on Windows).

## D5 - Speech / LLM defaults on Linux

Same as Windows (no divergence): local whisper (base.en or small multilingual
per the models.json catalog), local Qwen cleanup model, cloud providers stay
opt-in behind the consent gate. The Models page and update check work once
L6 lands the libcurl transport; the catalog URLs and hashes are already
pinned and download-verified (see `scripts/verify_model_hashes.sh`).

## D6 - Tray implementation

**StatusNotifierItem over D-Bus, implemented directly on `libdbus-1-dev`
(1.14.10-4ubuntu4.1, verified)** - no libayatana-appindicator dependency,
which would drag GTK into the link closure for one feature. Ubuntu's shell
already displays SNI items through the AppIndicator extension:
`gnome-shell-extension-appindicator` (57-2) ships with the desktop and is
enabled by default on fresh 24.04 installs (reported by fresh-install users,
July 2024). One 24.04-era bug report describes tray icons disappearing after
relogin for some apps - M4 on the test VM confirms whether it affects us;
if it does, the fallback is the tray-less window mode, not a new backend.

## What L0 delivered

1. This document + `docs/UBUNTU_DEPS.md` + `docs/UBUNTU_TEST_MATRIX.md`.
2. CI job `linux-ubuntu-24` (ubuntu-24.04): installs the canonical dep list
   (`scripts/install_ubuntu_deps.sh` - unknown names fail CI, which is the
   R5 check), builds the portable core, runs ctest, and smoke-runs the app
   binary under xvfb.
3. No product code changed in L0; the Windows job is untouched (R1).
