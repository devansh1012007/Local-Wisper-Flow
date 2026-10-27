#!/bin/sh
# Rebuilds the _deps-cache tarballs that are no longer committed to the repo.
# Requires: curl, tar, sha-aware GitHub access. Run from the repo root.

set -eu

CACHE=_deps-cache
mkdir -p "$CACHE"

LLAMA_TAG=b10603
GRPC_TAG=v1.71.0

ABSEIL_SHA=4447c7562e3bc702ade25105912dce503f0c4010
BORINGSSL_SHA=c64b8fe
CARES_SHA=6360e96
PROTOBUF_SHA=2d4414f384dc499af113b5991ce3eaa9df6dd931
RE2_SHA=0c5616df9c0aaa44c9440d87422012423d91c7d1
ZLIB_SHA=09155eaa2f9270dc4ed1fa13e2b4b2613e6e4851

fetch() {
    url=$1
    out=$2
    if [ -f "$CACHE/$out" ]; then
        echo "have $out"
        return
    fi
    echo "fetch $out"
    curl -fL --retry 3 -o "$CACHE/$out.part" "$url"
    mv "$CACHE/$out.part" "$CACHE/$out"
}

fetch "https://github.com/ggml-org/llama.cpp/archive/refs/tags/${LLAMA_TAG}.tar.gz" \
    "llama-${LLAMA_TAG}.tar.gz"

fetch "https://github.com/grpc/grpc/archive/refs/tags/${GRPC_TAG}.tar.gz" \
    "grpc-${GRPC_TAG}.tar.gz"

fetch "https://github.com/abseil/abseil-cpp/archive/${ABSEIL_SHA}.tar.gz" \
    "abseil-cpp-${ABSEIL_SHA}.tar.gz"
fetch "https://github.com/google/boringssl/archive/${BORINGSSL_SHA}.tar.gz" \
    "boringssl-${BORINGSSL_SHA}.tar.gz"
fetch "https://github.com/c-ares/c-ares/archive/${CARES_SHA}.tar.gz" \
    "cares-${CARES_SHA}.tar.gz"
fetch "https://github.com/protocolbuffers/protobuf/archive/${PROTOBUF_SHA}.tar.gz" \
    "protobuf-${PROTOBUF_SHA}.tar.gz"
fetch "https://github.com/google/re2/archive/${RE2_SHA}.tar.gz" \
    "re2-${RE2_SHA}.tar.gz"
fetch "https://github.com/madler/zlib/archive/${ZLIB_SHA}.tar.gz" \
    "zlib-${ZLIB_SHA}.tar.gz"

if [ -f "$CACHE/grpc-${GRPC_TAG}-full.tar.gz" ]; then
    echo "have grpc-${GRPC_TAG}-full.tar.gz"
    exit 0
fi

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

echo "assembling grpc-${GRPC_TAG}-full.tar.gz"
tar -xzf "$CACHE/grpc-${GRPC_TAG}.tar.gz" -C "$STAGE" --strip-components=1

mkdir -p "$STAGE/third_party/abseil-cpp"
tar -xzf "$CACHE/abseil-cpp-${ABSEIL_SHA}.tar.gz" -C "$STAGE/third_party/abseil-cpp" --strip-components=1

mkdir -p "$STAGE/third_party/boringssl-with-bazel"
tar -xzf "$CACHE/boringssl-${BORINGSSL_SHA}.tar.gz" -C "$STAGE/third_party/boringssl-with-bazel" --strip-components=1

mkdir -p "$STAGE/third_party/cares/cares"
tar -xzf "$CACHE/cares-${CARES_SHA}.tar.gz" -C "$STAGE/third_party/cares/cares" --strip-components=1

mkdir -p "$STAGE/third_party/protobuf"
tar -xzf "$CACHE/protobuf-${PROTOBUF_SHA}.tar.gz" -C "$STAGE/third_party/protobuf" --strip-components=1

mkdir -p "$STAGE/third_party/re2"
tar -xzf "$CACHE/re2-${RE2_SHA}.tar.gz" -C "$STAGE/third_party/re2" --strip-components=1

mkdir -p "$STAGE/third_party/zlib"
tar -xzf "$CACHE/zlib-${ZLIB_SHA}.tar.gz" -C "$STAGE/third_party/zlib" --strip-components=1

tar -czf "$CACHE/grpc-${GRPC_TAG}-full.tar.gz.part" -C "$STAGE" .
mv "$CACHE/grpc-${GRPC_TAG}-full.tar.gz.part" "$CACHE/grpc-${GRPC_TAG}-full.tar.gz"
echo "done: $CACHE/grpc-${GRPC_TAG}-full.tar.gz"
