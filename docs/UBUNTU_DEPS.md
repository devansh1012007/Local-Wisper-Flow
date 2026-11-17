# Ubuntu 24.04 port - dependencies (L0)

Target: Ubuntu 24.04 LTS ("noble"), amd64. This file is the canonical,
verified package list for the Linux port; `scripts/install_ubuntu_deps.sh`
encodes the install set and the `linux-ubuntu-24` CI job runs it - a wrong
package name fails CI, which is the R5 check ("never invent package names or
versions").

## How these entries were verified (2026-11-17)

The dev sandbox for this work is Debian, not Ubuntu, so versions were checked
against the noble archive (Launchpad / packages.ubuntu.com /
ubuntu.pkgs.org) instead of a local `apt-cache policy`. Rows whose version
column says "name-verified by CI" had no reachable noble version page at
verification time; their NAME is exercised by the CI job on the real
ubuntu-24.04 runner (apt fails the job on any unknown name). On a real
Ubuntu 24.04 machine, `apt-cache policy <name>` output supersedes this file -
refresh it if versions drift.

## Install set (build + CI)

Installed by `scripts/install_ubuntu_deps.sh` / the `linux-ubuntu-24` job:

| Package | Noble version (amd64) | Kind | Needed for | Verification |
| --- | --- | --- | --- | --- |
| build-essential | (runner image) | build | all phases | runner |
| cmake | via pip, repo CI pattern (`python3 -m pip install --user cmake`) | build | all phases | existing linux job |
| pkg-config | name-verified by CI | build | find_package from L2 | CI job |
| ninja-build | name-verified by CI | build | optional generator | CI job |
| libx11-dev | name-verified by CI | build | GLFW 3.4 source build (L4), X11 hotkey (L2) | CI job |
| libxrandr-dev | name-verified by CI | build | GLFW 3.4 source build | CI job |
| libxinerama-dev | name-verified by CI | build | GLFW 3.4 source build | CI job |
| libxcursor-dev | name-verified by CI | build | GLFW 3.4 source build | CI job |
| libxi-dev | name-verified by CI | build | GLFW 3.4 source build, XInput2 (L2) | CI job |
| libgl-dev | name-verified by CI | build | OpenGL headers (GLVND) | CI job |
| libegl-dev | name-verified by CI | build | EGL for the GLFW Wayland backend (L4/L7) | CI job |
| libxtst-dev | name-verified by CI | build | XTest injection (L3) | CI job |
| libxkbcommon-dev | 1.6.0-1build1 | build | keysym handling (L3/L7) | Launchpad, 2026-11-17 |
| libwayland-dev | name-verified by CI | build | only if direct Wayland is ever needed (L7 uses portals over D-Bus) | CI job |
| libdbus-1-dev | 1.14.10-4ubuntu4.1 | build | SNI tray (L5), notifications, portals (L7) | ubuntuupdates.org, 2026-11-17 |
| libnotify-dev | name-verified by CI | build | notifications (L5) | CI job |
| libcurl4-openssl-dev | 8.5.0-2ubuntu10.16 | build | IHttpTransport on libcurl (L6): model download + update check | Launchpad, 2026-11-17 |
| xvfb | 2:21.1.12-1ubuntu1 | ci | GUI smoke test | ubuntuupdates.org, 2026-11-17 |
| xauth | name-verified by CI | ci | xvfb-run helper | CI job |
| xclip | name-verified by CI | runtime | X11 clipboard subprocess (L3) | CI job |
| wl-clipboard | 2.2.1-1build1 | runtime | Wayland clipboard subprocess (L3, D1a) | Launchpad, 2026-11-17 |
| xdg-desktop-portal | name-verified by CI | runtime | FileChooser / RemoteDesktop portals | CI job |

## Runtime-only (user machines, not installed by CI)

| Package | Noble version (amd64) | Why | Verification |
| --- | --- | --- | --- |
| xdg-desktop-portal-gnome | 46.2-0ubuntu1 | GNOME portal backend; PREINSTALLED on the desktop session; no GlobalShortcuts (see UBUNTU_DECISIONS.md D2) | ubuntu.pkgs.org, 2026-11-17 |
| gnome-shell-extension-appindicator | 57-2 | displays StatusNotifierItem tray icons; PREINSTALLED and enabled on fresh desktop installs (D6; M4 confirms on the VM) | ubuntu.pkgs.org, 2026-11-17 |
| libfuse2t64 | 2.9.9-8.1build1 | required to EXECUTE AppImages on noble (D4); document in install instructions | packages.ubuntu.com, 2026-11-17 |
| libasound2-dev | 1.2.11-1ubuntu0.3 | NOT required: miniaudio dlopens ALSA/PulseAudio at runtime, no link-time dependency (portable core builds without it). Recorded because it is the classic wrong-suspect when audio breaks | packages.ubuntu.com, 2026-11-17 |

## Explicitly NOT used

| Package | Noble version | Reason |
| --- | --- | --- |
| libglfw3-dev | 3.3.10-1build1 (universe) | too old - the app needs GLFW 3.4 (3.3.10 < 3.4); GLFW is built from source via FetchContent, same tag 3.4 as Windows |
| libgl1-mesa-dev | 24.0.5-1ubuntu1 | transitional dummy package in noble (GLVND restructure); use libgl-dev |
| libayatana-appindicator3-dev | 0.5.92-1 (universe) | would pull GTK into the link closure for one feature; tray is implemented directly over D-Bus (D6) |
| ydotool | - | rejected in D1c: wants system-wide uinput rights |

## Portable-core rule (G5)

The portable core (lwf_core, lwf_tests, lwf-bench, lwf-cloud-bench, the app
binary) builds and tests with NO desktop libraries installed. Evidence at
L0: this branch's full build + ctest ran in a bare Debian sandbox with none
of the packages above present (only g++/make/cmake). The dedicated CI proof
(a container with only build-essential + cmake) lands with L1, when the
headless app branch makes the container job meaningful.
