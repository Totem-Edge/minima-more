# Performance Impact

This document contains both the **measured** micro-benchmark results comparing the pre-optimisation backup with the current code, and the earlier **estimated** ranges we used before the benchmarks were run.

---

## 1. Measured Micro-Benchmarks

Benchmarks were run on macOS (Apple Silicon, AppleClang 16). The pre-optimisation code came from `/Users/peter.cheek/Downloads/CRYSTAL_LABS/SIDE_Projects/pureminima_cpp-pre-optimisations`. Both versions were built in Release mode from the same benchmark harness (`benchmarks/`).

### 1.1 MMR Caching (`benchmarks/bench_mmr.cpp`)

Configuration: 10,000 MMR entries, 5,000 repeated calls.

| Operation | Pre-optimisation | Current (cached) | Speedup |
|-----------|----------------|------------------|---------|
| `getRoot()` | **30.48 µs/call** | **0.22 µs/call** | **~137×** |
| `getPeaks()` / `getPeaksRef()` | **2.46 µs/call** | **0.002 µs/call** | **~1,200×** |

The `getPeaks()` improvement in the current code comes from a new `getPeaksRef()` accessor that returns a `const` reference to the cached peak vector, avoiding a deep copy of `MMREntry` objects on every call. `getPeaks()` still returns a copy for external callers that expect value semantics.

MMR build time was unchanged (≈ 56–70 ms for 10k entries), confirming the cache invalidation overhead is negligible.

### 1.2 SQLite Tuning (`benchmarks/bench_sqlite.cpp`)

Configuration: 100,000 single-row `INSERT` statements into a table with one BLOB column and an index.

| Mode | Time | Throughput | Speedup |
|------|------|-----------|---------|
| SQLite defaults | **37.4 s** | **2,674 rows/sec** | 1× (baseline) |
| Tuned (WAL + NORMAL + cache + mmap + busy timeout) | **4.1 s** | **24,594 rows/sec** | **~9.2×** |

This confirms the SQLite tuning is by far the largest single win for write-heavy workloads.

---

## 2. Original Estimates (Kept for Context)

Before the backup was available, we estimated the following ranges. The measured results above land inside or above these ranges.

### 2.1 SQLite Tuning

| Workload | Estimated effect | Caveats |
|----------|------------------|---------|
| Heavy sync / bulk insert | **1.5× – 5×** throughput | Measured: ~9.2× |
| Concurrent read + write | **2× – 10×** latency improvement | Depends on query pattern |
| Steady-state node | **1× – 1.3×** | Cache already warm |
| DB larger than RAM | **1× – 1.5×** | Disk I/O dominates |

### 2.2 MMR Peak / Root Caching

| Scenario | Estimated per-call saving | Plausible cumulative effect |
|----------|---------------------------|----------------------------|
| Single `getRoot()` on large MMR | **10× – 1000×** | Measured ~137× |
| Full IBD sync with frequent root checks | Thousands of redundant recomputations eliminated | Minutes to hours saved on very large syncs |
| Steady-state block validation | Modest per-block saving | Measurable over time |

---

## 3. What Changed

### 3.1 SQLite (`org/minima/utils/sql_d_b.cpp/.hpp`)

`SqlDB::applySQLiteTuning()` now applies:

- `PRAGMA journal_mode=WAL`
- `PRAGMA synchronous=NORMAL`
- `PRAGMA cache_size=-16384` (64 MiB)
- `PRAGMA busy_timeout=30000`
- `PRAGMA mmap_size=268435456`
- `PRAGMA temp_store=MEMORY`
- `PRAGMA page_size=4096`

Applied to all SQLite connections opened by the node.

### 3.2 MMR Caching (`org/minima/objects/mmr/m_m_r.cpp/.hpp`)

- Added mutable caches for the peak list and root hash.
- Caches are invalidated on every mutating operation (`addEntry`, `finalizeSet`, `updateEntry`, etc.).
- Added `getPeaksRef()` for internal hot paths that only read the cached peaks.
- `getPeaks()` still returns by value for backwards compatibility.

Consensus impact: **none**. The cached root and peaks are identical to the values computed before caching.

---

## 4. Combined Real-World Estimate

The micro-benchmarks isolate the two hot paths. In a full node sync the gains will be lower because other work (network, signature verification, block validation, disk I/O) still dominates.

A realistic aggregate expectation for a **write-heavy sync workload** is now:

> **1.3× – 3.0× faster IBD/sync**, with the upper end when the node is committing many small DB transactions and repeatedly querying MMR roots.

For a **steady-state synced node**:

> **1.0× – 1.3× overall**, because the hot paths are networking and RPC rather than DB/MMR recomputation.

---

## 5. Other Recent Changes (Neutral Performance)

| Change | Performance impact |
|--------|--------------------|
| IBD weight-contest skip | Negligible / path removed |
| P2P peer persistence | Neutral (startup/reconnection robustness) |
| P2P use-after-free fix | Neutral (crash fix) |
| OpenSSL EVP migration | Neutral to slightly slower per op; necessary for OpenSSL 3.x |
| `help` command rewrite | Neutral |
| `override` / warning cleanup | Neutral |

---

## 6. How to Measure the Full-Node Gain

To get the final answer on overall sync speed:

1. **IBD sync benchmark**
   - Wipe the data directory.
   - Time sync from a known peer to a fixed block height with the pre-optimisation binary.
   - Repeat with the current binary.

2. **Runtime profiling**
   - Use `perf` (Linux) or Instruments (macOS) during a full sync.
   - Confirm `MMR::getRoot` / `MMR::getPeaks` and SQLite commit time are no longer near the top of the flame graph.

---

## 7. Recommendation

The performance changes are **low risk** and now have measured backing:

- SQLite pragmas are standard and reversible.
- MMR caches are invalidated on every mutation and do not change consensus output.

The next step to confirm real-world impact is the full-node IBD sync benchmark.
