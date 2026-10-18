#!/bin/sh
# Full reference-bench sweep: run every component bench into one results dir and
# emit a combined SWEEP_SUMMARY.md. C++ = this node; Java = minima-core where a
# mirror exists (microbench + MMR-RAM + mempool).
#
#   run_sweep.sh [OUT dir]
#
# Env: N_MMR (1e6), N_MEMPOOL (2e5), N_SQL (5e4), CPP_BIN (enables node benches),
#      NODE (cpp|java|both for the node benches).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/_env.sh"

OUT=${1:-$_BENCH_DIR/results/$(date +%Y%m%d-%H%M%S)-$(uname -m)}
mkdir -p "$OUT"
echo "Sweep results dir: $OUT"

echo "### microbenchmarks (WOTS / SHA3 / MMR) ###"
sh "$HERE/run_all.sh" "$OUT/micro" 2>&1 | tail -4 || true

echo "### MMR RAM + build time ###"
N=${N_MMR:-1000000} sh "$HERE/run_ram_bench.sh" "$OUT/mmr" 2>&1 | tail -3 || true

echo "### mempool RAM + coin lookup ###"
N=${N_MEMPOOL:-200000} sh "$HERE/run_mempool_bench.sh" "$OUT/mempool" 2>&1 | tail -3 || true

echo "### SQLite write throughput ###"
N=${N_SQL:-50000} sh "$HERE/run_sql_bench.sh" "$OUT/sql" 2>&1 | tail -3 || true

if [ -n "${CPP_BIN:-}" ]; then
    echo "### full-node comparison ###"
    T=${T:-30} sh "$HERE/run_node_bench.sh" "$OUT/node" 2>&1 | tail -5 || true
    echo "### full-node RAM soak ###"
    T=${T:-30} sh "$HERE/run_node_ram.sh" "$OUT/node_ram" 2>&1 | tail -5 || true
fi

{
    echo "# Reference sweep"
    echo
    echo "Dir: $OUT"
    echo "arch: $(uname -m)   cores: $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo '?')"
    echo
    echo "## MMR RAM / CPU (C++ vs Java)"
    cat "$OUT/mmr/RAM_SUMMARY.md" 2>/dev/null || echo "(missing)"
    echo
    echo "## Mempool RAM + checkForCoinID lookup"
    cat "$OUT/mempool/RAM_SUMMARY.md" 2>/dev/null || echo "(missing)"
    echo
    echo "## SQLite write throughput (batched vs per-row)"
    cat "$OUT/sql/SQL_SUMMARY.md" 2>/dev/null || echo "(missing)"
    echo
    echo "## Microbenchmarks (C++ vs Java)"
    cat "$OUT/micro/SUMMARY.md" 2>/dev/null || echo "(missing)"
    if [ -f "$OUT/node/SUMMARY.md" ]; then
        echo
        echo "## Full node"
        cat "$OUT/node/SUMMARY.md"
    fi
} | tee "$OUT/SWEEP_SUMMARY.md"

echo "DONE -> $OUT"
