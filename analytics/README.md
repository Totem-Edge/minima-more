# minima-analytics (optional sidecar)

Build (off by default). Requires DuckDB headers + `libduckdb`:

```bash
cmake -S . -B build-analytics -DCMAKE_BUILD_TYPE=Release \
  -DMINIMA_BUILD_ANALYTICS=ON \
  -DDUCKDB_INCLUDE_DIR=/path/to/duckdb/include \
  -DDUCKDB_LIBRARY=/path/to/libduckdb.so
cmake --build build-analytics --target minima-analytics -j8
```

macOS: `brew install duckdb` (headers/lib are found automatically).
Ubuntu: install the DuckDB headers and `libduckdb.so`, or point the two CMake
variables above at them.

Build the index (reads the archive **read-only**; never touches wallet/keys):

```bash
./build-analytics/analytics/minima-analytics \
  -data ~/.minima -index -indexdb ~/.minima/analytics.duckdb
```

Serve the API:

```bash
./build-analytics/analytics/minima-analytics \
  -data ~/.minima -indexdb ~/.minima/analytics.duckdb -apiport 8080

curl -s http://127.0.0.1:8080/v1/capabilities | jq
curl -s http://127.0.0.1:8080/v1/catalog | jq '.metrics | length'
curl -s http://127.0.0.1:8080/v1/metrics/block_count | jq '.points[-1]'
curl -s http://127.0.0.1:8080/v1/blocks/2376119 | jq
curl -s http://127.0.0.1:8080/v1/txs/0x... | jq
curl -s http://127.0.0.1:8080/v1/addresses/0x... | jq
curl -s http://127.0.0.1:8080/v1/utxo/distribution | jq
curl -s http://127.0.0.1:8080/v1/cohorts/holder | jq
```

Snapshot tables (`utxo_snapshots`, `address_balances`, age/value bands, holder
cohorts) are **native-token only** in v1; token-specific breakdowns come later.

Overrides: `-archivepath`, `-mmrpath`, `-archive`, `-megammr`, `-rebuild`,
`-indexonly`.

`-index` is incremental: it starts from `MAX(height)` in the DuckDB index.
`-rebuild` recreates the index. Full 2.37M-block index measured at 12.5 min,
2.7 GB DuckDB, 2.86 GB peak RSS on a 16 GB / 8 vCPU host.
