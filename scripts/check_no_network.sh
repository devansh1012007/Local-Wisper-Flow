#!/usr/bin/env bash
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
fail=0

network_allowlist=(
    "^src/stt/GrpcInferenceProvider\.(h|cpp)$"
    "^src/os/WindowsDownloader\.(h|cpp)$"
)

network_patterns='winhttp|wininet|WinHttp|WinInet|curl_|curl\.h|socket\(|WSASocket|getaddrinfo|gethostbyname|grpc::CreateChannel|grpc_create_channel|InetOpenUrl|InternetOpen|URLRequest|httplib'

file_write_allowlist=(
    "^src/core/(Log|SettingsStore|HistoryStore|CrashReporter|SnippetStore|PersonalDictionary|Onboarding|ConfigManager)\.(h|cpp)$"
    "^src/core/(Sha256|IClock|Languages|ErrorCode)\.(h|cpp)$"
    "^tests/.*$"
    "^bench/.*$"
    "^scripts/.*$"
)

matches_in_allowlist() {
    local file="$1"
    shift
    for pattern in "$@"; do
        if echo "$file" | grep -qE "$pattern"; then
            return 0
        fi
    done
    return 1
}

while IFS= read -r file; do
    rel="${file#"$root"/}"
    if matches_in_allowlist "$rel" "${network_allowlist[@]}"; then
        continue
    fi
    if grep -InE "$network_patterns" "$file" >/dev/null 2>&1; then
        echo "network API outside allowlist: $rel"
        grep -InE "$network_patterns" "$file" | head -3
        fail=1
    fi
done < <(find "$root/src" -name "*.h" -o -name "*.cpp" | sort)

while IFS= read -r file; do
    rel="${file#"$root"/}"
    if matches_in_allowlist "$rel" "${file_write_allowlist[@]}"; then
        continue
    fi
    if grep -InE "ofstream|fopen|CreateFile\(" "$file" >/dev/null 2>&1; then
        echo "file write API outside allowlist (audio must never touch disk): $rel"
        grep -InE "ofstream|fopen|CreateFile\(" "$file" | head -3
        fail=1
    fi
done < <(find "$root/src/core" "$root/src/audio" "$root/src/stt" "$root/src/vad" "$root/src/llm" -name "*.h" -o -name "*.cpp" 2>/dev/null | sort)

if [ "$fail" -ne 0 ]; then
    echo "privacy check: FAIL"
    exit 1
fi
echo "privacy: OK"
