#!/bin/sh
# Hygiene lint (gate G4): OS headers, TODO markers and cloud endpoint names
# are confined to the files that are supposed to carry them. --self-test
# plants offenders and verifies the run catches them, then verifies the
# clean tree still passes.

status=0
report() {
    echo "FAIL: $1"
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
        src/stt/cloud/WindowsHttpClient.cpp) ;;
        *) report "windows.h outside src/os/ and the cloud HTTP client: $f" ;;
    esac
done

for f in $(grep -rln 'winhttp\.h' src 2>/dev/null); do
    case "$f" in
        src/os/WindowsDownloader.cpp) ;;
        src/stt/cloud/WindowsHttpClient.cpp) ;;
        *) report "winhttp.h outside the two WinHTTP clients: $f" ;;
    esac
done

violations=$(grep -rn --include='*.cpp' --include='*.h' -E '\bTODO\b|\bFIXME\b|\bXXX\b' src/ 2>/dev/null || true)
if [ -n "$violations" ]; then
    echo "FAIL: TODO/FIXME/XXX markers left in src/:"
    echo "$violations"
    status=1
fi

violations=$(grep -rn --include='*.cpp' --include='*.h' -E 'api\.(vachana\.ai|elevenlabs\.io|meta\.ai)' src/ 2>/dev/null | grep -v '^src/stt/cloud/' || true)
if [ -n "$violations" ]; then
    echo "FAIL: cloud endpoints outside src/stt/cloud/:"
    echo "$violations"
    status=1
fi

violations=$(grep -rn --include='*.py' -E 'api\.(vachana\.ai|elevenlabs\.io|meta\.ai)' scripts/ server/ 2>/dev/null | grep -v '^scripts/gnani_smoke\.py' || true)
if [ -n "$violations" ]; then
    echo "FAIL: cloud endpoints outside the smoke script:"
    echo "$violations"
    status=1
fi

if [ "$status" -eq 0 ]; then
    echo "hygiene: OK"
fi
exit $status
