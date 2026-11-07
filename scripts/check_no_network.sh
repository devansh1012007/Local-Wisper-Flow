#!/bin/sh
# Privacy gate (E4): the app is offline by default. Network APIs live in
# exactly three places (the optional gRPC inference client, the explicit
# model downloader and the cloud STT HTTP client); cloud endpoint names live
# only under src/stt/cloud/; file-write APIs are banned in the audio/stt/
# vad/llm code and in all of core except the text stores.

status=0
fail() {
    echo "FAIL: $1"
    echo "$2"
    status=1
}

network_hits=$(grep -rniE 'winhttp|wininet|wsastartup|wsasocket|internetopen|getaddrinfo|curl_(easy|multi)_(init|setopt)|urlopen' src --include='*.cpp' --include='*.h' 2>/dev/null | grep -vE '^(src/stt/GrpcInferenceProvider\.(cpp|h):|src/os/WindowsDownloader\.(cpp|h):|src/stt/cloud/WindowsHttpClient\.(cpp|h):)' || true)
if [ -n "$network_hits" ]; then
    fail "network APIs outside the allowlist (GrpcInferenceProvider, WindowsDownloader, WindowsHttpClient)" "$network_hits"
fi

endpoint_hits=$(grep -rnE 'api\.(vachana\.ai|elevenlabs\.io|meta\.ai)' src --include='*.cpp' --include='*.h' 2>/dev/null | grep -v '^src/stt/cloud/' || true)
if [ -n "$endpoint_hits" ]; then
    fail "cloud endpoints outside src/stt/cloud/" "$endpoint_hits"
fi

write_hits=$(grep -rnE 'ofstream|fopen|freopen|CreateFile\(' src/audio src/vad src/llm --include='*.cpp' --include='*.h' 2>/dev/null || true)
if [ -n "$write_hits" ]; then
    fail "file-write APIs in audio/vad/llm (audio must never touch disk)" "$write_hits"
fi

core_write_hits=$(grep -rnE 'ofstream|fopen|freopen|CreateFile\(' src/core src/stt --include='*.cpp' --include='*.h' 2>/dev/null | grep -vE '^(src/core/((AtomicFile\.(cpp|h))|(CrashReporter|HistoryStore|Log|SettingsStore)\.cpp):)' || true)
if [ -n "$core_write_hits" ]; then
    fail "file-write APIs outside the text stores and log" "$core_write_hits"
fi

if [ "$status" -eq 0 ]; then
    echo "no-network: OK"
fi
exit $status
