#!/bin/sh
# Fails when source hygiene rules are broken.
# 1. windows.h outside src/os/*.cpp
# 2. comments in C++ sources (the closing "// namespace" marker is allowed)
# 3. TODO/FIXME markers left in code

set -eu

status=0

violations=$(grep -rn --include='*.cpp' --include='*.h' -E '^[[:space:]]*#include[[:space:]]*<windows\.h>' src/ | grep -v '^src/os/' || true)
if [ -n "$violations" ]; then
    echo "FAIL: <windows.h> outside src/os:"
    echo "$violations"
    status=1
fi

violations=$(grep -rn --include='*.cpp' --include='*.h' -E '//|/\*' src/ | grep -v -E '//[[:space:]]*namespace([[:space:]].*)?$' || true)
if [ -n "$violations" ]; then
    echo "FAIL: comments found in sources (project rule: no comments):"
    echo "$violations"
    status=1
fi

violations=$(grep -rn --include='*.cpp' --include='*.h' --include='*.py' -E 'TODO|FIXME' src/ server/ tests/ || true)
if [ -n "$violations" ]; then
    echo "FAIL: TODO/FIXME left in code:"
    echo "$violations"
    status=1
fi

violations=$(grep -rn --include='*.cpp' --include='*.h' -E '#include[[:space:]]*<winhttp\.h>|WinHttpOpen\(|WinHttpSendRequest\(' src/ | grep -v '^src/os/WindowsHttpClient\.' || true)
if [ -n "$violations" ]; then
    echo "FAIL: WinHTTP usage outside src/os/WindowsHttpClient.*:"
    echo "$violations"
    status=1
fi

violations=$(grep -rn --include='*.cpp' --include='*.h' -E 'api\.vachana\.ai' src/ | grep -v '^src/stt/cloud/' || true)
if [ -n "$violations" ]; then
    echo "FAIL: cloud endpoints outside src/stt/cloud/:"
    echo "$violations"
    status=1
fi

violations=$(grep -rn --include='*.py' -E 'api\.vachana\.ai' scripts/ server/ | grep -v '^scripts/gnani_smoke\.py' || true)
if [ -n "$violations" ]; then
    echo "FAIL: cloud endpoints outside the smoke script:"
    echo "$violations"
    status=1
fi

if [ "$status" -eq 0 ]; then
    echo "hygiene: OK"
fi
exit "$status"
