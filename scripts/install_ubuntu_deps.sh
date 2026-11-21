#!/bin/sh
# Ubuntu 24.04 dependency installer for the Linux port (plan 2026-11-21,
# change C08). Idempotent: safe to re-run; everything already satisfied is a
# no-op. Run WITHOUT sudo; it elevates only for apt.
#
# Layers:
#   1. build tools + platform libs (X11/Xtst hotkey+injection, ncurses TUI,
#      libnotify-bin for notify-send toasts, clipboard helpers)
#   2. ONNX Runtime (prebuilt, never fetched by cmake) into
#      third_party/onnxruntime for the Silero VAD
#   3. silero_vad.onnx model into the user data dir

set -eu

NEED_SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    NEED_SUDO="sudo"
fi

echo "== [1/3] apt packages =="
$NEED_SUDO apt-get update -qq
$NEED_SUDO apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git \
    libx11-dev libxtst-dev libxi-dev \
    libncurses-dev \
    libnotify-bin \
    wl-clipboard xclip

echo "== [2/3] ONNX Runtime (for the Silero VAD) =="
ONNXRUNTIME_VERSION=1.20.1
ONNXRUNTIME_DIR="third_party/onnxruntime"
ONNXRUNTIME_TARBALL="/tmp/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz"
ONNXRUNTIME_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz"

if [ -f "${ONNXRUNTIME_DIR}/include/onnxruntime_cxx_api.h" ]; then
    echo "    already present: ${ONNXRUNTIME_DIR}"
else
    if [ ! -f "${ONNXRUNTIME_TARBALL}" ]; then
        echo "    downloading ${ONNXRUNTIME_URL}"
        curl -fSL "${ONNXRUNTIME_URL}" -o "${ONNXRUNTIME_TARBALL}"
    fi
    mkdir -p "${ONNXRUNTIME_DIR}"
    tar -xzf "${ONNXRUNTIME_TARBALL}" -C "${ONNXRUNTIME_DIR}" --strip-components=1
    echo "    installed into ${ONNXRUNTIME_DIR}"
fi

echo "== [3/3] silero_vad.onnx =="
DATA_DIR="${LWF_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/LocalWisperFlow}"
MODELS_DIR="${DATA_DIR}/models"
SILERO_URL="https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx"
if [ -f "${MODELS_DIR}/silero_vad.onnx" ]; then
    echo "    already present: ${MODELS_DIR}/silero_vad.onnx"
else
    mkdir -p "${MODELS_DIR}"
    curl -fSL "${SILERO_URL}" -o "${MODELS_DIR}/silero_vad.onnx"
    echo "    installed into ${MODELS_DIR}/silero_vad.onnx"
fi

echo
echo "All dependencies satisfied. Build with:"
echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLWF_BUILD_FULL=ON"
echo "  cmake --build build -j\$(nproc)"
echo "  ctest --test-dir build"
echo
echo "Wayland hotkey (optional, needs re-login once):"
echo "  sudo usermod -aG input \$USER"
