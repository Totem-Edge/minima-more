#!/usr/bin/env bash
# Level C robustness runner.
#
# Works with both build modes of the fuzz targets:
#   - libFuzzer (when the toolchain ships the runtime)
#   - the portable deterministic driver (fuzz_driver.cpp) otherwise
#
# Usage: tests/fuzz/run_fuzz.sh [build-dir] [runs-per-seed]
#
# Build first:
#   cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=RelWithDebInfo \
#         -DENABLE_SANITIZERS=ON -DMINIMA_BUILD_FUZZERS=ON
#   cmake --build build-fuzz --target fuzz_mini_data fuzz_json_parser \
#         fuzz_mini_number fuzz_txpow -j
set -euo pipefail

BUILD="${1:-build-fuzz}"
RUNS="${2:-2000}"
TARGETS=(fuzz_mini_data fuzz_json_parser fuzz_mini_number fuzz_txpow)

CORPUS="$(mktemp -d)"
trap 'rm -rf "$CORPUS"' EXIT

# Small deterministic seed corpus. Kept out of the source tree on purpose.
: > "$CORPUS/empty"
printf '\x00\x00\x00\x20' > "$CORPUS/len32"
printf '\x00\x00\x00\x00' > "$CORPUS/len0"
printf '\xff\xff\xff\xff' > "$CORPUS/lenmax"
printf '0' > "$CORPUS/num0"
printf '1.5' > "$CORPUS/num15"
printf '1e10' > "$CORPUS/nume10"
printf '123456789012345678901234567890123456789012345678901234567890' > "$CORPUS/numbig"
printf '0.000000000000000000000000000000000000000000001' > "$CORPUS/numscale45"
printf '{"a":[1,2,3],"b":"x"}' > "$CORPUS/json1"
printf '[1,2,3,4,5]' > "$CORPUS/json2"
head -c 512 /dev/zero > "$CORPUS/zeros"

status=0
for t in "${TARGETS[@]}"; do
    bin="$BUILD/$t"
    if [[ ! -x "$bin" ]]; then
        echo "missing $bin (build with -DMINIMA_BUILD_FUZZERS=ON)"
        exit 1
    fi
    echo "== $t =="
    # Both libFuzzer and fuzz_driver accept -runs=N and a corpus directory.
    if ! "$bin" -runs="$RUNS" "$CORPUS"; then
        echo "FAIL $t"
        status=1
    fi
done

if [[ $status -ne 0 ]]; then
    echo "FUZZ FAILURES"
    exit $status
fi
echo "FUZZ PASS"
