# Minima Analytics Node — Product & API Contract (v0.1)

**Product:** one node binary/container that exposes a verifiable, mode-aware analytics API
and console. Archive answers *history*, MegaMMR answers *current UTXO state*, the full node
answers *the live tip*. The API never pretends a mode has data it does not have.

## 1. Modes and coverage

| Mode | Owns | Cannot answer |
|---|---|---|
| `full` | tip, mempool, recent chain | deep history, historical UTXO |
| `archive` | all finalized blocks + transaction detail | current UTXO/address balances |
| `megammr` | current UTXO/coin set, address balances, supply | historical flows/balances |
| `analytics` | DuckDB index + aggregates | raw data outside archive/MMR coverage |

One node may run `-archive -megammr -analytics`; flags are independent. Initial data loads
are separate (`archive action:importraw`, `megammr action:import`).

**MegaMMR-only** (`-megammr` without `-archive`) is a supported recent/unpruned
profile: the API reports `megammr` coverage and `archive_incomplete`, and the
index contains `mmr_coins`/`mmr_snapshots` with archive-backed metrics marked
unavailable. Historical flows and archived transaction detail require
`-archive`.

### Deployment profiles

| Profile | Runs | Footprint |
|---|---|---|
| Lite (phone) | archive or MMR + aggregate index | ~4 GB data, 1–2 GB RAM |
| Pro | archive + MMR + full index + API | ~4 GB data + index, 16 GB RAM |
| Federated | archive node + MMR node + gateway | scale/HA; API same |

## 2. Capabilities endpoint

`GET /v1/capabilities` — the front door. Clients cache it and check it before querying.

```json
{
  "node": {"version": "0.3.0", "modes": ["archive","megammr","analytics"]},
  "coverage": {
    "archive": {"from": 1, "to": 2376119, "tx_details": true, "tx_detail_from": 1},
    "megammr": {"as_of": 2376119, "coins": 465588, "coin_store": "lmdb"},
    "live": {"tip": 2377896, "finality_depth": 2048}
  },
  "index": {"status": "ready", "schema": 1, "lag_blocks": 0, "last_height": 2376119},
  "consistency": {"archive_mmr": "ok", "checked_at_height": 2376119},
  "warnings": [],
  "limits": {"max_range_blocks": 100000, "max_page": 1000, "rate_limit_rps": 20}
}
```

## 3. API surface

| Endpoint | Purpose |
|---|---|
| `GET /v1/capabilities` | modes, ranges, freshness, limits |
| `GET /v1/catalog` | metric catalogue + definitions |
| `GET /v1/metrics/{name}?from=&to=&interval=` | time series |
| `GET /v1/blocks/{height}` / `?from=&to=` | block records |
| `GET /v1/txs/{txpowid}` | transaction + inputs/outputs |
| `GET /v1/addresses/{addr}` / `/balance?height=` | address record/balance |
| `GET /v1/utxo/distribution?at=tip\|height&bands=` | UTXO value/age distribution |
| `GET /v1/cohorts/{name}?at=` | holder cohorts |
| `GET /v1/consistency?height=` | archive vs MMR snapshot check |
| `GET /v1/export?dataset=&format=parquet` | async export (v1 limited) |
| `GET /v1/stream` (SSE) | height/metric updates |
| MCP tools | same operations for agents |

Auth: read-only bearer token or localhost/no-auth. No wallet keys, no signing, no writes.

## 4. Response envelope

```json
{
  "data": {},
  "meta": {
    "source": "archive|megammr|live|index",
    "as_of": {"height": 2376119, "hash": "0x...", "finalized": true},
    "interval": "1d",
    "complete": true,
    "coverage": {"from": 1, "to": 2376119},
    "warnings": ["timestamp_anomaly"],
    "proof": null
  }
}
```

`proof` is an optional MMR/coin proof for snapshot responses (v3).

## 5. Metric catalogue (v1)

| Metric | Unit | Granularity | Source | Definition |
|---|---|---|---|---|
| `block_height` | count | 1m/1h/1d | live/archive | max height at bucket end |
| `block_count` | count | 1m/1h/1d | archive | blocks in bucket |
| `tx_count` | count | 1m/1h/1d | archive | transactions in bucket |
| `tx_rate` | tx/s | 1m/1h | archive | tx_count / seconds |
| `fees_total_raw` | raw | 1h/1d | archive | sum of transaction fees |
| `coinbase_reward_raw` | raw | 1d | archive | sum of coinbase outputs |
| `output_volume_raw` | raw | 1h/1d | archive | sum of non-coinbase outputs |
| `input_volume_raw` | raw | 1h/1d | archive | sum of spent inputs |
| `active_addresses` | count | 1h/1d | archive | unique addresses in inputs or outputs |
| `new_addresses` | count | 1d | archive | first-seen output addresses |
| `sender_addresses` | count | 1d | archive | unique input addresses |
| `receiver_addresses` | count | 1d | archive | unique output addresses |
| `utxo_count` | count | snapshot | megammr | unspent coins at as_of |
| `supply_raw` | raw | snapshot | megammr | sum of unspent native coins |
| `spent_coin_count` | count | 1d | archive | coins first spent in bucket |
| `utxo_age_band_count` | count | snapshot | archive replay | unspent coins per age band |
| `utxo_age_band_value_raw` | raw | snapshot | archive replay | value per age band |
| `utxo_value_band_count` | count | snapshot | megammr | coins per value band |
| `holder_cohort_count` | count | snapshot | megammr | addresses per balance cohort |
| `holder_cohort_value_raw` | raw | snapshot | megammr | balance per cohort |
| `address_inflow_raw` | raw | 1d | archive | value received by address |
| `address_outflow_raw` | raw | 1d | archive | value spent by address |
| `blocks_mined` | count | 1d | archive | blocks by minter |
| `index_lag_blocks` | count | 1m | index | live tip − last indexed |
| `archive_mmr_divergence` | count | snapshot | index | UTXO mismatches at height |
| `reorg_depth` | count | event | live/index | deepest recent reorg |

Age bands: `0–1d, 1d–1w, 1w–1m, 1m–3m, 3m–6m, 6m–1y, 1y–2y, 2y–5y, 5y+`.
Value/holder bands are configurable; defaults are documented in `/v1/catalog`.
Price-derived metrics (`realized_cap`, `mvrv`) are **v4** and require an external price feed.

## 6. Freshness and consistency rules

1. **Height is authoritative; time is advisory.** Timestamps come from block headers and
   may have miner clock skew. Time-series buckets use UTC block time and carry
   `warnings:["timestamp_anomaly"]` when skew is detected.
2. **Archive is finalized history.** `archive.to` is the last fully imported height.
   `tx_detail_from` states the oldest height with full transaction detail.
3. **Live tip is volatile.** Use `finalized=false` for heights above
   `tip − finality_depth`; reorgs are reflected in `reorg_events` and index rollback.
4. **MMR snapshots are point-in-time.** Every UTXO response carries
   `as_of.height + hash`; if the MMR is behind the index beyond `stale_after_blocks`,
   `index.status=stale` and strict queries fail with 409.
5. **Strict joins.** Metrics combining archive and MMR must use the same `height + hash`.
   `join=nearest` is opt-in and returns `meta.warnings:["join_tolerance"]`.
6. **Completeness is explicit.** `meta.complete=false` plus `warnings` for partial ranges;
   strict clients set `require_complete=true` and receive 409 if unavailable.
7. **Idempotent ingestion.** Index rows are keyed by `(height, hash)`; on hash mismatch,
   roll back the affected range before re-indexing.

## 7. 409 semantics

409 = **state/capability conflict**, never bad syntax. Error envelope:

```json
{
  "error": {
    "code": "mode_unavailable",
    "message": "Historical transaction detail requires an archive node",
    "requires": ["archive"],
    "available_modes": ["megammr"],
    "coverage": {"archive": null, "megammr": {"as_of": 2376119}},
    "remediation": "Query /v1/capabilities; use an archive or federated endpoint"
  }
}
```

| Code | When |
|---|---|
| `mode_unavailable` | source mode not running |
| `history_not_retained` | height precedes `tx_detail_from` |
| `height_out_of_range` | outside archive/MMR coverage |
| `index_not_ready` | index building/rebuilding |
| `snapshot_not_ready` | MMR/index lacks requested height |
| `snapshot_mismatch` | archive hash ≠ MMR hash at height |
| `coverage_partial` | `require_complete=true` and range has gaps |
| `index_stale` | lag exceeds `stale_after_blocks` |

`400` invalid parameters; `404` unknown block/tx/address; `429` rate limit; `206 Partial
Content` for non-strict partial ranges.

## 8. v1 acceptance

- `capabilities` accurately reflects running modes/coverage.
- Archive index covers 1…tip with 0 gaps; MMR snapshot matches archive replay at the
  checked height (or reports divergence explicitly).
- All catalogue metrics available at `1h/1d` where applicable.
- 409s are stable and include `requires` + remediation.
- No wallet keys or write paths exposed.

**Non-goals v1:** custody/signing, exchange labels, price oracles, full Glassnode parity,
real-time mempool analytics.
