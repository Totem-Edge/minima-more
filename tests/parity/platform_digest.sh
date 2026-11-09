#!/usr/bin/env bash
# Level F — platform/compiler determinism digest.
#
# Runs the consensus-facing test binaries and hashes their exact output. The
# same digest must be produced on every supported platform/compiler. The header
# (platform/compiler) is metadata and is NOT part of payload_digest.
#
# Usage: tests/parity/platform_digest.sh [build-dir]
set -euo pipefail

BUILD="${1:-build}"
TARGETS=(
    test_consensus_vectors
    test_serialization
    test_script_conformance
    test_txpowtree_weight
)

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

for t in "${TARGETS[@]}"; do
    bin="$BUILD/$t"
    if [[ -x "$bin" ]]; then
        echo "== $t ==" >> "$TMP"
        "$bin" 2>&1 | tr -d '\r' >> "$TMP"
        echo "== end $t ==" >> "$TMP"
    else
        echo "== $t MISSING ==" >> "$TMP"
    fi
done

if command -v sha256sum >/dev/null 2>&1; then
    DIGEST="$(sha256sum "$TMP" | awk '{print $1}')"
else
    DIGEST="$(shasum -a 256 "$TMP" | awk '{print $1}')"
fi

echo "platform: $(uname -s)/$(uname -m)"
echo "compiler: $("${CXX:-c++}" --version 2>/dev/null | head -1 || echo unknown)"
echo "payload_digest: $DIGEST"
echo "--- payload ---"
cat "$TMP"
