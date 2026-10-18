#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

rm -rf build-bench
mkdir -p build-bench
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

echo ""
echo "========================================"
echo "  MMR Benchmark — old (no cache)"
echo "========================================"
./build-bench/bench_mmr_old 10000 5000

echo ""
echo "========================================"
echo "  MMR Benchmark — new (cached)"
echo "========================================"
./build-bench/bench_mmr_new 10000 5000

echo ""
echo "========================================"
echo "  SQLite Benchmark — default"
echo "========================================"
./build-bench/bench_sqlite 100000 0

echo ""
echo "========================================"
echo "  SQLite Benchmark — tuned"
echo "========================================"
./build-bench/bench_sqlite 100000 1
