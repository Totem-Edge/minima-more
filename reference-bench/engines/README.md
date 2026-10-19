# RFC-0003 — embedded storage-engine benchmark

Measures candidate **embedded** engines against the node's two hot storage roles,
so the role→engine choice in [RFC-0001](../../RFC-0001-STORAGE-DB-ROLES-AND-ENGINE-SELECTION.md)
is decided by data:

- **R1 archive** — one record per TxBlock, keyed by height and by txpowid.
- **R2 coin store** — one record per UTXO, keyed by coinID (prefix scan by address).

## Run

```sh
./run_engine_bench.sh                                   # 200k blocks / 120k coins
BLOCKS=2400000 COINS=1700000 ./run_engine_bench.sh       # mainnet-scale (A1)
```

Output lands in `../results/<ts>-engines-<arch>/`: `ENGINE_BENCH_SUMMARY.md`
(per-role table per engine), `raw.tsv`, and the stores themselves.

## Engines

| Engine | Status | Notes |
|---|---|---|
| SQLite | **built** | baseline; matches the node's current embedded usage |
| LMDB | **built** | mmap B+tree; secondary-index DBI for getById / prefix scan |
| RocksDB | scaffold | driver present, returns unavailable until built on the lab host |
| DuckDB | scaffold | expected to lose point lookups, win analytics (RFC-0001 §4.1) |

A driver whose library is absent at build time returns `nullptr`; the runner skips
it (`run_engine_bench.sh` enables LMDB only when its headers link).

## Methodology / fairness statement

- **Same data:** every engine ingests one deterministic, mainnet-shaped dataset
  (`dataset.hpp`, fixed seed) — byte-identical keys/values, so no engine wins on a
  codec. Swap in `archive.raw.dat` / `mega.mmr` on the lab host later.
- **Same work:** point gets and range/prefix scans use the same sampled keys;
  analytics is the same aggregate (`Σ txns`, `Σ payload bytes`).
- **Bulk discipline:** each engine batches per its idiom (one SQLite transaction,
  one LMDB write txn, …) so bulk-load compares best-effort, not per-row autocommit.
- **Reported separately:** single-thread latency (p50/p95) *and* bulk throughput;
  peak RSS and on-disk size per engine.
- **Expected split:** SQLite/LMDB lead point lookups; DuckDB leads analytics. The
  harness reports both rather than declaring one winner.

**Caveats:** single run on a shared host (indicative); `peak_rss_mb` includes the
dataset held in RAM (identical across engines, so relative deltas are meaningful);
the checked-in `results/engines-local-*` is macOS/M1 — re-run on the A1 for
device-representative numbers.
