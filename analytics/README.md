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

### MegaMMR-only mode (recent, unpruned)

For a node that runs `-megammr` without `-archive`, index the unpruned coin set
and MMR root directly. Archive-backed `chain_blocks` and historical metrics are
not available in this mode; the indexer skips that rebuild instead of failing
with a NULL `metric_series.bucket_start_ms`.

```bash
./build-analytics/analytics/minima-analytics \
  -data ~/.minima -indexdb ~/.minima/minima.duckdb \
  -megammr -mmr-enrich -indexonly
```

- `-megammr` implies `-mmr-enrich` in the CLI.
- Writes `mmr_coins`, `mmr_snapshots` and `index_state.status = 'ready'`.
- `/v1/capabilities` reports `megammr` and `archive_incomplete`; `index` is ready.
- Historical flows/balances still require `-archive` (long-term Omnia analytics).
- Use a DB filename other than `analytics.duckdb`: DuckDB's catalog then has the
  same name as the `analytics` schema, and `analytics.table` is ambiguous. The
  examples above use `minima.duckdb` for that reason.

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
curl -s http://127.0.0.1:8080/v1/tokens | jq
curl -s 'http://127.0.0.1:8080/v1/utxo/distribution?token=0x...' | jq
curl -s http://127.0.0.1:8080/v1/snapshots | jq
curl -s http://127.0.0.1:8080/v1/consistency/check | jq
curl -s http://127.0.0.1:8080/v1/scripts | jq
curl -s http://127.0.0.1:8080/v1/scripts/known | jq
```

Script/address hashes are indexed in `scripts`; `known_scripts.csv` is loaded from
the build tree (or `-knownscripts FILE`). Compute an address for a KISSVM source
with `-hashscript FILE`. The current MiniDAPP labels are template hashes from
`minima-global/MiniDAPP@e79c8c1`; parameterized/older templates may not match any
live coin.

Snapshots exist in two forms: the native-token tables used by the default
endpoints, and `asset_*` tables keyed by `(snapshot_day_ms, token_id)` for
per-token history. `asset_*` snapshots are retained for
`snapshot_retention_days` (default 365) and pruned on each index pass.

Backup / fast bootstrap:

```bash
# portable Parquet snapshot + manifest, exits after writing
./build-analytics/analytics/minima-analytics -data ~/.minima \
  -indexdb ~/.minima/analytics.duckdb -backup /backups/2026-10-21

# restore into a NEW index, then resume from the snapshot height
./build-analytics/analytics/minima-analytics -data ~/.minima \
  -indexdb ~/.minima/analytics.duckdb.new -restore /backups/2026-10-21 -index

mv ~/.minima/analytics.duckdb.new ~/.minima/analytics.duckdb
```

Backups use DuckDB `EXPORT DATABASE` (Parquet) and include
`analytics_manifest.json` with schema/tip/MMR metadata. `-restore` refuses to
overwrite an existing non-empty index.

MegaMMR enrichment (optional):

```bash
./build-analytics/analytics/minima-analytics -data ~/.minima -indexonly \
  -indexdb ~/.minima/analytics.duckdb -mmr-enrich \
  -mmrpath ~/.minima/1.0/databases/megammr.mmr
```

Use `-mmrstore none` to load coins into memory; otherwise point at
`coins.lmdb`. This writes `mmr_snapshots`, `mmr_coins`, and a
`consistency_checks` row at the archive tip. If the MMR snapshot is older than
the archive tip, `matched=false` means “not comparable at this height,” not
corruption.

Exact amounts and address balances:

`MiniNumber::getAsLong()` returns only the integer part of a 44-decimal value,
so it cannot represent 0.8 MIN. `mmr_coins.amount_atoms` stores the exact
10^-44 value; `/v1/addresses/<0x|Mx>` reads the MegaMMR unspent set and returns
`balance` (decimal), `balance_raw` (exact atoms), `coin_count` and a `tokens[]`
list with name/decimals/is_nft/image_url.

Refresh those tables without replaying the archive:

```bash
systemctl stop minima-node minima-analytics
./build-analytics/analytics/minima-analytics -data ~/.minima \
  -indexdb ~/.minima/analytics.duckdb -mmrcoins
systemctl start minima-node minima-analytics
```

`-mmrcoins` rewrites `mmr_coins`/`mmr_snapshots`/`consistency_checks` and exits.
It must not race the node for `megammr.mmr`/`coins.lmdb` (LMDB), so quiesce the
node or run it against a snapshot/clone. Archive-only `-index` passes skip
`-mmr-enrich` for the same reason, so `mmr_coins` is only as fresh as the last
quiesced enrichment.

Overrides: `-archivepath`, `-mmrpath`, `-archive`, `-megammr`, `-rebuild`,
`-indexonly`, `-mmrcoins`.

BI templates: `integrations/evidence/` (history, MIT) and
`integrations/rill/` (live/ops, Apache-2.0), sharing
`integrations/common/metrics.yaml`.

`-index` is incremental: it starts from `MAX(height)` in the DuckDB index.
`-rebuild` recreates the index. Full 2.37M-block index measured at 12.5 min,
2.7 GB DuckDB, 2.86 GB peak RSS on a 16 GB / 8 vCPU host.
