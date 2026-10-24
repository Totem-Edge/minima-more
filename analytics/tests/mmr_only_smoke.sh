#!/usr/bin/env bash
# Live smoke: MegaMMR-only analytics (no archive).
# Usage: analytics/tests/mmr_only_smoke.sh <data-dir> [index.duckdb]
# Requires <data-dir>/1.0/databases/megammr.mmr (or -mmrpath support in the binary).
set -euo pipefail

DATA="${1:?usage: $0 <data-dir> [index.duckdb]}"
IDX="${2:-$DATA/minima-mmr-only.$$.duckdb}"
BIN="${MINIMA_ANALYTICS_BIN:-./build-analytics/analytics/minima-analytics}"
# Sidecar binaries are often deployed next to libduckdb.so.
export LD_LIBRARY_PATH="$(cd "$(dirname "$BIN")" && pwd):${LD_LIBRARY_PATH:-}"

"$BIN" -data "$DATA" -indexdb "$IDX" -megammr -mmr-enrich -indexonly
test -s "$IDX"
echo "MegaMMR-only index OK: $IDX"
