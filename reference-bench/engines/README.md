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
| SQLite | **built** | baseline; **the node's tuned+batched config** (WAL, NORMAL, mmap, one bulk txn) |
| SQLite (plain) | **built** | stock defaults + per-row commits — the untuned reference point |
| LMDB | **built** | mmap B+tree; secondary-index DBI for getById / prefix scan |
| RocksDB | **built** | LSM KV; WriteBatch bulk load; Checkpoint snapshot |
| DuckDB | **built** | columnar; Appender bulk load; loses point lookups, wins analytics |

## First results (M1, 200k blocks / 120k coins, single run)

| Metric | SQLite | LMDB | RocksDB | DuckDB |
|---|---|---|---|---|
| archive bulk load (rows/s) | 163,971 | **467,569** | 377,822 | 136,040 |
| archive get-by-num p50 (µs) | 2.71 | **1.92** | 3.79 | 312.62 |
| archive get-by-id p50 (µs) | 2.67 | 3.75 | 5.46 | 107.92 |
| archive analytics (ms) | 23 | 14 | 35 | **2** |
| coin bulk load (rows/s) | 261,733 | 319,176 | 179,001 | **328,214** |
| coin get p50 (µs) | 3.29 | **1.75** | 2.08 | 578.75 |
| coin on-disk (MB) | 44 | 56 | **38** | 42 |

**Tuning is included, and it matters.** "SQLite" above is the node's tuned+batched
config; an untuned `sqlite-plain` baseline (stock pragmas, per-row commits) loads
**~150× slower** (archive 1.9k vs 302k rows/s; coin 2.6k vs 339k) with ~3× slower
point gets. So the comparison pits the *optimised* node config against the other
engines, not stock SQLite.

As RFC-0001 predicted: **LMDB** leads point lookups (zero-copy mmap), **DuckDB**
wins analytics (~7–17×) but is ~100–300× slower per point get, SQLite sits in the
middle, RocksDB is write-optimised (fast bulk, middling gets). Raw:
`../results/engines-local-20261019/`.

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
