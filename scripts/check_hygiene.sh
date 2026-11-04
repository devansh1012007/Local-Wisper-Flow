#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."

status=0
report() {
    echo "HYGIENE FAIL: $1"
    status=1
}

if [ "${1:-}" = "--self-test" ]; then
    offender="src/core/selftest_offender.h"
    printf '#pragma once\nstatic const char* kEp = "api.elevenlabs.io";\n' > "$offender"
    if sh scripts/check_hygiene.sh > /dev/null 2>&1; then
        echo "self-test FAIL: endpoint offender was not caught"
        rm -f "$offender"
        exit 1
    fi
    rm -f "$offender"
    offender="src/core/selftest_offender.cpp"
    printf '#include <winhttp.h>\n' > "$offender"
    if sh scripts/check_hygiene.sh > /dev/null 2>&1; then
        echo "self-test FAIL: network offender was not caught"
        rm -f "$offender"
        exit 1
    fi
    rm -f "$offender"
    if ! sh scripts/check_hygiene.sh > /dev/null 2>&1; then
        echo "self-test FAIL: clean tree did not pass"
        exit 1
    fi
    echo "hygiene self-test: OK"
    exit 0
fi

for f in $(grep -rln 'windows\.h' src 2>/dev/null); do
    case "$f" in
        src/os/*) ;;
        *) report "<windows.h> outside src/os: $f" ;;
    esac
done

if [ "$#" -ge 1 ]; then
    added=$(git diff -U0 "$1" -- src tests | grep '^+' | grep -v '^+++' || true)
    bad=$(printf '%s\n' "$added" | grep -E '//|/\*' | grep -v '://' || true)
    if [ -n "$bad" ]; then
        report "comments added in changed lines:"
        echo "$bad"
        status=1
    fi
else
    bad=$(grep -rn '//\|/\*' src tests 2>/dev/null | grep -v '://' | grep -v '\}  // namespace' || true)
    if [ -n "$bad" ]; then
        report "comments present in tree (baseline namespace enders excluded):"
        echo "$bad"
        status=1
    fi
fi

todo=$(grep -rn 'TODO\|FIXME' src tests 2>/dev/null || true)
if [ -n "$todo" ]; then
    report "TODO/FIXME left in code:"
    echo "$todo"
    status=1
fi

network=$(grep -rniE 'winhttp|wininet|<curl|sys/socket\.h|ws2_32|getaddrinfo' src 2>/dev/null \
    | grep -v '^src/stt/GrpcInferenceProvider' \
    | grep -v '^src/os/WindowsDownloader' || true)
if [ -n "$network" ]; then
    report "network API usage outside allowlist (src/stt/GrpcInferenceProvider, src/os/WindowsDownloader):"
    echo "$network"
    status=1
fi

if [ "$status" -eq 0 ]; then
    echo "hygiene: OK"
fi
exit "$status"
