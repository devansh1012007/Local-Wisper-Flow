#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cache="$root/_deps-cache"
mkdir -p "$cache"

GRPC_TAG="v1.71.0"
LLAMA_TAG="b10603"
LLAMA_URL="${LLAMA_URL:-https://github.com/ggml-org/llama.cpp/archive/refs/tags/${LLAMA_TAG}.tar.gz}"
GRPC_URL="${GRPC_URL:-https://github.com/grpc/grpc/archive/refs/tags/${GRPC_TAG}.tar.gz}"

DEPS=(
    "abseil-cpp-4447c7562e3bc702ade25105912dce503f0c4010.tar.gz|https://github.com/abseil/abseil-cpp/archive/4447c7562e3bc702ade25105912dce503f0c4010.tar.gz|abseil-cpp"
    "boringssl-c64b8fe.tar.gz|https://github.com/google/boringssl/archive/c64b8fe.tar.gz|boringssl-with-bazel"
    "cares-6360e96.tar.gz|https://github.com/c-ares/c-ares/archive/6360e96.tar.gz|cares/cares"
    "protobuf-2d4414f384dc499af113b5991ce3eaa9df6dd931.tar.gz|https://github.com/protocolbuffers/protobuf/archive/2d4414f384dc499af113b5991ce3eaa9df6dd931.tar.gz|protobuf"
    "re2-0c5616df9c0aaa44c9440d87422012423d91c7d1.tar.gz|https://github.com/google/re2/archive/0c5616df9c0aaa44c9440d87422012423d91c7d1.tar.gz|re2"
    "zlib-09155eaa2f9270dc4ed1fa13e2b4b2613e6e4851.tar.gz|https://github.com/madler/zlib/archive/09155eaa2f9270dc4ed1fa13e2b4b2613e6e4851.tar.gz|zlib"
)

fetch() {
    local name="$1" url="$2"
    if [ -f "$cache/$name" ]; then
        echo "have $name"
        return 0
    fi
    echo "downloading $name"
    curl -fL --retry 3 -o "$cache/$name" "$url"
}

fetch "llama-${LLAMA_TAG}.tar.gz" "$LLAMA_URL"
fetch "grpc-${GRPC_TAG}.tar.gz" "$GRPC_URL"
for entry in "${DEPS[@]}"; do
    IFS='|' read -r name url _ <<< "$entry"
    fetch "$name" "$url"
done

if [ -f "$cache/grpc-${GRPC_TAG}-full.tar.gz" ]; then
    echo "have grpc-${GRPC_TAG}-full.tar.gz"
    exit 0
fi

echo "building grpc-${GRPC_TAG}-full.tar.gz"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
tar -xzf "$cache/grpc-${GRPC_TAG}.tar.gz" -C "$work"
grpc_dir="$work/grpc-${GRPC_TAG#v}"
for entry in "${DEPS[@]}"; do
    IFS='|' read -r name _ target <<< "$entry"
    tar -xzf "$cache/$name" -C "$work"
    src_dir="$(tar -tzf "$cache/$name" 2>/dev/null | head -1 | cut -d/ -f1 || true)"
    mkdir -p "$grpc_dir/third_party/$(dirname "$target")"
    rm -rf "$grpc_dir/third_party/$target"
    mv "$work/$src_dir" "$grpc_dir/third_party/$target"
done
tar -czf "$cache/grpc-${GRPC_TAG}-full.tar.gz" -C "$grpc_dir" .
echo "wrote $cache/grpc-${GRPC_TAG}-full.tar.gz"
