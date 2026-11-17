#!/bin/sh
# Canonical Ubuntu 24.04 dependency install for Echo flow's Linux port.
# docs/UBUNTU_DEPS.md documents every entry (noble version, purpose,
# verification method); the linux-ubuntu-24 CI job runs this script, so an
# unknown package name fails CI - that is the R5 name check.
#
# Requires root (run via sudo). Runtime-only packages (AppImage libfuse2t64,
# GNOME AppIndicator extension) are deliberately NOT here - see
# docs/UBUNTU_DEPS.md "Runtime-only".
set -eu
export DEBIAN_FRONTEND=noninteractive

apt-get update

apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    ninja-build \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libgl-dev \
    libegl-dev \
    libxtst-dev \
    libxkbcommon-dev \
    libwayland-dev \
    libdbus-1-dev \
    libnotify-dev \
    libcurl4-openssl-dev \
    xvfb \
    xauth \
    xclip \
    wl-clipboard \
    xdg-desktop-portal

echo "ubuntu deps: OK (see docs/UBUNTU_DEPS.md for the annotated list)"
