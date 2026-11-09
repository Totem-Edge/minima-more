#!/usr/bin/env bash
# Level A kernel differential: build the pinned Java oracle and compare it with
# the C++ kernel digest, line for line.
#
# Required:
#   MINIMA_CORE  path to a minima-core checkout (src/ + lib/*.jar), pinned
#   JDK          directory containing javac/java (defaults to PATH)
# Optional:
#   MINIMA_CORE_BUILD  Java classes cache (default /tmp/minima-core-build)
#   FORCE_JAVA_BUILD=1 rebuild the Java core even if cached
#
# Usage: tests/parity/kernel/run_kernel.sh [cmake-build-dir]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${1:-$HERE/../../../build}"

if [[ -z "${MINIMA_CORE:-}" || ! -d "$MINIMA_CORE/src" ]]; then
    echo "error: set MINIMA_CORE to a minima-core checkout (src/ + lib/*.jar)" >&2
    exit 2
fi
if [[ -z "${JDK:-}" ]]; then
    if command -v javac >/dev/null 2>&1; then
        JDK="$(dirname "$(command -v javac)")"
    else
        echo "error: no JDK found (set JDK=/path/to/jdk/bin)" >&2
        exit 2
    fi
fi
if [[ ! -x "$BUILD/kernel_digest" ]]; then
    echo "error: $BUILD/kernel_digest not built (cmake --build $BUILD --target kernel_digest)" >&2
    exit 2
fi

JBUILD="${MINIMA_CORE_BUILD:-/tmp/minima-core-build}"
LIBS="$(ls "$MINIMA_CORE"/lib/*.jar | tr '\n' ':')"
CP="${LIBS}${JBUILD}"
mkdir -p "$JBUILD"

if [[ "${FORCE_JAVA_BUILD:-0}" == "1" || ! -f "$JBUILD/org/minima/objects/TxHeader.class" ]]; then
    find "$MINIMA_CORE/src" -name '*.java' > "$JBUILD/sources.txt"
    "$JDK/javac" -nowarn -cp "$LIBS" -d "$JBUILD" @"$JBUILD/sources.txt" >/dev/null
fi

"$JDK/javac" -cp "$CP" -d "$JBUILD" "$HERE/KernelOracle.java"

JOUT="$(mktemp)"
COUT="$(mktemp)"
trap 'rm -f "$JOUT" "$COUT"' EXIT

"$JDK/java" -cp "$CP" KernelOracle > "$JOUT"
"$BUILD/kernel_digest" > "$COUT"

if ! diff -u "$JOUT" "$COUT"; then
    echo "KERNEL PARITY FAIL" >&2
    exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
    DIGEST="$(sha256sum "$JOUT" | awk '{print $1}')"
else
    DIGEST="$(shasum -a 256 "$JOUT" | awk '{print $1}')"
fi

echo "kernel_digest: $DIGEST"
echo "oracle_commit: $(git -C "$MINIMA_CORE" rev-parse HEAD 2>/dev/null || echo unknown)"
echo "PASS"
