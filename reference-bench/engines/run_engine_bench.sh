#!/bin/sh
# RFC-0003 — build and run the embedded storage-engine benchmark.
#
# Compares candidate engines for the node's two hot storage roles (R1 archive,
# R2 coin store) over one deterministic mainnet-shaped dataset. Engines whose
# library is absent at build time are skipped (their factory returns nullptr).
#
# Usage:
#   SCALE=1 ./run_engine_bench.sh                 # default: 200k blocks / 120k coins
#   BLOCKS=2400000 COINS=1700000 ./run_engine_bench.sh   # mainnet-scale (needs RAM, A1)
#   ./run_engine_bench.sh /path/to/out
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/../scripts/_env.sh"

ENG=$HERE
SCALE=${SCALE:-1}
BLOCKS=${BLOCKS:-200000}
COINS=${COINS:-120000}
LOOKUPS=${LOOKUPS:-2000}
SPAN=${SPAN:-1000}
OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-engines-$(uname -m)}
mkdir -p "$OUT"
echo "Results dir: $OUT"
echo "Dataset: BLOCKS=$BLOCKS COINS=$COINS LOOKUPS=$LOOKUPS SPAN=$SPAN"

case "$(uname -s)" in
  Darwin) MACLIB="-L/opt/homebrew/lib" ;;
  *)      MACLIB="" ;;
esac

# LMDB: enable only if its headers/lib are usable on this host.
LMDB_FLAGS=""
printf '#include <lmdb.h>\nint main(){return 0;}\n' > /tmp/_lmdb_probe.cpp
if $CXX -std=c++17 $MACLIB $OPENSSL_INCLUDE /tmp/_lmdb_probe.cpp -o /tmp/_lmdb_probe -llmdb >/dev/null 2>&1; then
  LMDB_FLAGS="-DHAVE_LMDB -llmdb"
  echo "LMDB: enabled"
else
  echo "LMDB: not found -> driver will report unavailable"
fi

echo "== building =="
$CXX -std=c++17 -O3 -DNDEBUG -I "$ENG" $OPENSSL_INCLUDE \
    -o "$OUT/bench_engines" \
    "$ENG/bench_engines.cpp" "$ENG/driver_sqlite.cpp" "$ENG/driver_lmdb.cpp" \
    "$ENG/driver_rocksdb.cpp" "$ENG/driver_duckdb.cpp" \
    $MACLIB $SQLITE_LIB $LMDB_FLAGS -lpthread

echo "== running =="
"$OUT/bench_engines" --blocks "$BLOCKS" --coins "$COINS" --lookups "$LOOKUPS" \
    --scan-span "$SPAN" --out "$OUT"

echo ""
cat "$OUT/ENGINE_BENCH_SUMMARY.md"
echo ""
echo "DONE -> $OUT"
