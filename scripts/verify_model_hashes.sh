#!/bin/sh
# Model catalog verification (release gate, fix for "hashes aren't verified").
#
#   sh scripts/verify_model_hashes.sh --lint   # no network: catalog sanity
#   sh scripts/verify_model_hashes.sh          # downloads every file, sha256
#
# --lint fails when any entry has a non-https URL, a URL not pinned to a
# 40-hex commit revision (resolve/main would silently break users when the
# upstream repo replaces a file), an empty/non-hex sha256, or a non-positive
# size_bytes. Full mode downloads each file (resumable, .part + curl -C -)
# and fails unless the sha256 of the downloaded bytes matches the catalog.
# Run the full mode before every release (docs/RELEASE.md step 2).

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CATALOG="$ROOT/models.json"
MODE="full"
VERIFY_DIR="${TMPDIR:-/tmp}/lwf-model-verify"

while [ $# -gt 0 ]; do
    case "$1" in
        --lint) MODE="lint" ;;
        --dir) VERIFY_DIR="$2"; shift ;;
        *) echo "usage: $0 [--lint] [--dir DIR]"; exit 2 ;;
    esac
    shift
done

if [ ! -f "$CATALOG" ]; then
    echo "FAIL: $CATALOG not found"
    exit 1
fi

# "group|name|url|sha256|size_bytes" per entry; python3 is a CI assumption
# (ci.yml installs cmake with it). No jq dependency on purpose.
ENTRIES=$(python3 - "$CATALOG" <<'EOF'
import json
import sys

with open(sys.argv[1]) as f:
    catalog = json.load(f)
for group in ("whisper", "llm"):
    for entry in catalog.get(group, []):
        print("|".join((
            group,
            entry.get("name", ""),
            entry.get("url", ""),
            entry.get("sha256", ""),
            str(entry.get("size_bytes", 0)),
        )))
EOF
)
if [ -z "$ENTRIES" ]; then
    echo "FAIL: no entries parsed from $CATALOG"
    exit 1
fi

status=0

if [ "$MODE" = "lint" ]; then
    echo "-- lint: catalog sanity (no network)"
    echo "$ENTRIES" | while IFS='|' read -r group name url sha size; do
        [ -n "$group" ] || continue
        problem=""
        case "$url" in
            https://huggingface.co/*/resolve/*) ;;
            *) problem="url is not a HuggingFace https URL" ;;
        esac
        rev=$(printf '%s' "$url" | sed -n 's#.*/resolve/\([^/]*\)/.*#\1#p')
        if ! printf '%s' "$rev" | grep -Eq '^[0-9a-f]{40}$'; then
            problem="${problem:+$problem; }url revision '$rev' is not a 40-hex commit pin (moving branch/tag)"
        fi
        if ! printf '%s' "$sha" | grep -Eq '^[0-9a-f]{64}$'; then
            problem="${problem:+$problem; }sha256 is empty or not 64-hex"
        fi
        case "$size" in
            ''|*[!0-9]*|0) problem="${problem:+$problem; }size_bytes must be a positive integer" ;;
        esac
        if [ -n "$problem" ]; then
            echo "FAIL: [$group] $name: $problem"
        else
            echo "ok:   [$group] $name (pinned $rev)"
        fi
    done
    # The while runs in a subshell over the pipe; re-check for the exit code.
    bad=0
    echo "$ENTRIES" | while IFS='|' read -r group name url sha size; do
        [ -n "$group" ] || continue
        rev=$(printf '%s' "$url" | sed -n 's#.*/resolve/\([^/]*\)/.*#\1#p')
        case "$url" in
            https://huggingface.co/*/resolve/*) ;;
            *) echo bad; exit 0 ;;
        esac
        printf '%s' "$rev" | grep -Eq '^[0-9a-f]{40}$' || { echo bad; exit 0; }
        printf '%s' "$sha" | grep -Eq '^[0-9a-f]{64}$' || { echo bad; exit 0; }
        case "$size" in ''|*[!0-9]*|0) echo bad; exit 0 ;; esac
    done | grep -q bad && status=1
    if [ "$status" -eq 0 ]; then
        echo "model catalog lint: OK"
    else
        echo "model catalog lint: FAILED"
    fi
    exit $status
fi

# Full mode: download and hash-verify every entry.
command -v curl >/dev/null 2>&1 || { echo "FAIL: curl not found"; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "FAIL: sha256sum not found"; exit 1; }
mkdir -p "$VERIFY_DIR"

echo "-- full: downloading every catalog entry to $VERIFY_DIR"
failures="$VERIFY_DIR/.failures"
rm -f "$failures"
echo "$ENTRIES" | while IFS='|' read -r group name url sha size; do
    [ -n "$group" ] || continue
    target="$VERIFY_DIR/$name"
    part="$target.part"
    if [ ! -f "$target" ]; then
        echo "get: $name ($size bytes)"
        if ! curl -fL --connect-timeout 20 --retry 5 --retry-delay 5 --retry-all-errors -C - -o "$part" "$url"; then
            echo "FAIL: [$group] $name: download failed"
            echo "$name" >> "$failures"
            continue
        fi
        mv "$part" "$target"
    fi
    actual=$(sha256sum "$target" | cut -d' ' -f1)
    if [ "$actual" != "$sha" ]; then
        echo "FAIL: [$group] $name: sha256 mismatch (catalog $sha, download $actual)"
        rm -f "$target"
        echo "$name" >> "$failures"
    else
        echo "ok:  [$group] $name: sha256 verified"
    fi
done
if [ -f "$failures" ]; then
    echo "model hash verification: FAILED for $(wc -l < "$failures" | tr -d ' ') entry/entries"
    echo "re-run to retry; verified files are kept in $VERIFY_DIR"
    exit 1
fi
echo "model hash verification: OK (all entries downloaded and sha256-verified)"
