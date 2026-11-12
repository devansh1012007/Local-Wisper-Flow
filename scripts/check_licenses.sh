#!/bin/sh
# Licence check (docs/RELEASE.md step 2): verifies the shipping-critical
# licence artifacts exist, that every model in models.json carries a
# VERIFIED url (allowlisted host) and sha256 (the downloader refuses
# unhashed downloads, and unverified URLs are exactly how a malicious
# mirror slips in), and that every pinned build dependency is named in
# NOTICE.md. Fails loudly so a release candidate cannot go out unlicensed.
#
# --self-test plants offenders and verifies they are caught.

status=0
fail() {
    echo "FAIL: $1"
    status=1
}

ROOT=$(cd "$(dirname "$0")/.." && pwd)
MODE="${1:-}"

[ -f "$ROOT/LICENSE" ] || fail "LICENSE missing at the repo root"
[ -f "$ROOT/NOTICE.md" ] || fail "NOTICE.md missing at the repo root"
[ -f "$ROOT/SECURITY.md" ] || fail "SECURITY.md missing at the repo root"
[ -f "$ROOT/PRIVACY.md" ] || fail "PRIVACY.md missing at the repo root"

if [ "$MODE" = "--self-test" ]; then
    cp "$ROOT/models.json" "$ROOT/models.json.bak"
    printf '{"whisper":[{"name":"x.bin","url":"","sha256":"","size_bytes":1}],"llm":[]}' > "$ROOT/models.json"
    if sh "$0" > /dev/null 2>&1; then
        echo "self-test FAIL: empty url/sha256 offender was not caught"
        mv "$ROOT/models.json.bak" "$ROOT/models.json"
        exit 1
    fi
    printf '{"whisper":[{"name":"x.bin","url":"https://evil.example/x.bin","sha256":"a8d302e2f6c11b2b52b43c4b9e1d0b0f","size_bytes":1}],"llm":[]}' > "$ROOT/models.json"
    if sh "$0" > /dev/null 2>&1; then
        echo "self-test FAIL: non-allowlisted host offender was not caught"
        mv "$ROOT/models.json.bak" "$ROOT/models.json"
        exit 1
    fi
    printf '{"whisper":[{"name":"x.bin","url":"https://huggingface.co/a/b/resolve/main/x.bin","sha256":"nothex","size_bytes":1}],"llm":[]}' > "$ROOT/models.json"
    if sh "$0" > /dev/null 2>&1; then
        echo "self-test FAIL: malformed sha256 offender was not caught"
        mv "$ROOT/models.json.bak" "$ROOT/models.json"
        exit 1
    fi
    mv "$ROOT/models.json.bak" "$ROOT/models.json"
    if ! sh "$0" > /dev/null 2>&1; then
        echo "self-test FAIL: clean tree did not pass"
        exit 1
    fi
    echo "licence self-test: OK"
    exit 0
fi

# --- models.json: every entry needs a verified url on an allowlisted host
# and a 64-hex sha256 (AGENTS.md: never invent model URLs or hashes).
if [ -f "$ROOT/models.json" ]; then
    if command -v python3 >/dev/null 2>&1; then
        python3 - "$ROOT/models.json" << 'PYEOF' || status=1
import json
import re
import sys

ALLOWED_HOSTS = ("https://huggingface.co/", "https://github.com/")
path = sys.argv[1]
with open(path, encoding="utf-8") as fh:
    catalog = json.load(fh)

problems = []
for kind, entries in catalog.items():
    if kind.startswith("//"):
        continue
    if not isinstance(entries, list):
        problems.append(f"{kind}: not a list")
        continue
    for entry in entries:
        name = entry.get("name", "?")
        url = entry.get("url", "")
        sha = entry.get("sha256", "")
        if not url:
            problems.append(f"{name}: empty url")
        elif not url.startswith(ALLOWED_HOSTS):
            problems.append(f"{name}: url host not in the allowlist {ALLOWED_HOSTS}: {url}")
        if not re.fullmatch(r"[0-9a-f]{64}", sha):
            problems.append(f"{name}: sha256 must be 64 lowercase hex chars (verify it, never invent it)")

for problem in problems:
    print(f"FAIL: models.json {problem}")
sys.exit(1 if problems else 0)
PYEOF
    else
        fail "python3 not found - cannot validate models.json"
    fi
else
    fail "models.json missing at the repo root"
fi

# --- every pinned dependency is named in NOTICE.md
NOTICE="$ROOT/NOTICE.md"
for dep in whisper.cpp llama.cpp miniaudio grpc Dear\ ImGui GLFW nlohmann doctest "ONNX Runtime"; do
    if ! grep -qi "$dep" "$NOTICE"; then
        fail "NOTICE.md does not mention pinned dependency: $dep"
    fi
done

if [ "$status" -eq 0 ]; then
    echo "licences: OK"
fi
exit $status
