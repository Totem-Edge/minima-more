# Minima More — Rill (live / operational)

Apache-2.0 Rill project for live node/analytics operations.

1. Install Rill: <https://docs.rilldata.com>
2. Edit `connectors/duckdb.yaml` and replace
   `/ABSOLUTE/PATH/TO/analytics.duckdb` with your index path.
3. `rill start .`

Models provided:

- `live_status` — height, hash, lag, status
- `live_blocks` — recent 500 blocks
- `live_burn` — daily burn
- `live_magic_changes` — network-capacity changes
- `live_scripts` — active scripts + known labels
- `live_cohorts` — native holder cohorts
- `history_block_count` — daily blocks

Use Rill Developer to create metrics views/explores and alerts from these
models.

### Omnia live chain events

`models/omnia_chain_events.yaml` and `models/omnia_chain_blocks.yaml` read the
Canary `observe.jsonl` journal produced by the Minima More SSE source
(`chain.block`, `chain.txpow.mined`, `chain.coin.created/spent`). Set the
absolute path at the top of both models. They join to
`analytics.chain_blocks` by `txpow_id` when an archive is present; in
MegaMMR-only mode the block join is empty, but the coin/transaction events stay
queryable for live Omnia correlation. Keep deep history (HODL waves, CDD, token charts) in the Evidence
template; this project is intentionally live/ops-focused.

Rill Cloud/embedding is paid; local `rill start` is free. The DuckDB attach is
read-only from Rill's side; do not expose it publicly without auth.
