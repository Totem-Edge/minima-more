# RFC-0003 — Embedded Storage Engine Benchmark Harness & Methodology

- **Status:** Draft / Benchmarking plan
- **Scope:** A pluggable harness that measures candidate **embedded** engines against
  the node's **Archive (R1)** and **Coin store (R2)** roles, at mainnet scale, so the
  role→engine choice in RFC-0001 is decided by data, not opinion.
- **Related:** RFC-0001 (roles/engines), RFC-0002 (node-mode benchmarking)

---

## 1. Objective

Provide a **fair, reproducible** comparison of embedded engines for the two hot
storage roles, on **real mainnet-shaped data**, covering both **latency** (point
lookups) and **throughput/analytics** (range scans, aggregations) — the exact split
identified in RFC-0001 (SQLite wins point lookups; DuckDB wins analytics).

---

## 2. Candidates

| Engine | Model | Role focus |
|---|---|---|
| **SQLite** | row, embedded | baseline (current) |
| **LMDB** | mmap B+tree | R1/R2 point/range, zero-copy |
| **RocksDB** | LSM KV | R1/R2 append-heavy + point |
| **DuckDB** | columnar | R1 cold analytics + `duckdbcoins` |

Optional (server, localhost-only, for completeness): **PostgreSQL**. Excluded by
default for attack-surface reasons (RFC-0001 §5).

---

## 3. Workloads

**Archive store (R1)** — records keyed by block number and by txpowid:
1. **Bulk load** N blocks (from a real `archive.raw.dat`).
2. **Point get** by block number and by id.
3. **Range scan** `[a,b]` (sequential) and descending.
4. **Last/first block**, **count**.
5. **Analytics**: aggregate blocks/day, txns/day, avg size (R3 preview).

**Coin store (R2)** — records keyed by coinID:
1. **Bulk load** N coins (from a real `mega.mmr`).
2. **Point get** by coinID.
3. **Prefix scan** / range by address.
4. **Proof read** (MMR entry + sibling path) — latency.
5. **Snapshot** write + restore (export/import).

**Scale:** mirror mainnet — **~2.4 M blocks**, **~1.7 M coins**; also report a 10×
synthetic sweep to model growth.

---

## 4. Methodology

- **Same data** for every engine (ingest from the same parsed source records).
- **Warm** (OS cache) and **cold** (drop caches) reported separately.
- **Single-thread latency** (p50/p95/p99) and **multi-thread QPS**.
- **Build/ingest** time, **peak RSS**, **on-disk size**.
- **Codec:** keys/values hashed and sizes matched across engines (no unfair
  compression wins); block/coin payloads identical byte arrays.
- **Determinism:** fixed seeds, pinned engine versions, pinned dataset snapshot
  (store SHA-256 of inputs).
- **Fairness statement** published with results (à la the hybrid-recall benchmark).

---

## 5. Harness

```
reference-bench/engines/
  interface.hpp        // ArchiveStore / CoinStore abstract API
  driver_sqlite.cpp
  driver_lmdb.cpp
  driver_rocksdb.cpp
  driver_duckdb.cpp
  bench_engines.cpp    // runs the workloads, emits machine-readable + markdown
  run_engine_bench.sh  // builds, runs each driver, summarizes
```

- One **driver per engine** implementing the same interface.
- Output: JSON + a `ENGINE_BENCH_SUMMARY.md` table (per workload × engine).
- Reuse the existing results layout (`reference-bench/results/`).

---

## 6. Metrics table

| Metric | Archive (R1) | Coin (R2) |
|---|---|---|
| bulk-load (rows/s) | ✓ | ✓ |
| point get p50/p95 (µs) | ✓ | ✓ |
| range/prefix scan (ms) | ✓ | ✓ |
| analytics query (ms) | ✓ | — |
| snapshot write/restore (s) | ✓ | ✓ |
| peak RSS (MB) | ✓ | ✓ |
| on-disk size (MB) | ✓ | ✓ |

---

## 7. Deliverables

- Working, reproducible engine harness (`reference-bench/engines/`).
- `ENGINE_BENCH_SUMMARY.md` + raw results committed.
- A recommendation update in RFC-0001 (which engine per role).
- Follow-up: integrate the winner behind the node's `ArchiveManager`/coin-index
  interfaces and re-run RFC-0002's node-mode benchmark with it.

---

## 8. Risks / caveats

- Real archive import time dominates setup — cache a parsed snapshot once, reuse for
  all engines.
- LMDB map size / RocksDB compaction tuning can swing results — pin a sane config and
  document it.
- DuckDB is expected to lose point lookups and win analytics; the harness should make
  that split explicit rather than declare one "winner".

## 9. Open questions

1. LMDB vs RocksDB — both, or pick one KV to keep the matrix tight?
2. Include the analytics/vector (LanceDB/VSS) roles in this harness or a separate
   RFC-0004 (Agentic/Analytics layers)?
3. Snapshot format parity with `megammr.mmr` / `archive.raw.dat` for round-trip tests?
