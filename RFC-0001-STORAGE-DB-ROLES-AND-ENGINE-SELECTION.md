# RFC-0001 — Node Storage Roles, Database Engine Selection & Security

- **Status:** Draft / Research
- **Scope:** Research (no implementation yet)
- **Related:** RFC-0002 (node-mode benchmarking), RFC-0003 (engine benchmark harness)
- **Author:** Minima More / PureMinima C++

---

## 1. Summary

The node currently uses **SQLite** for all persistent storage. As we add **Archive**
(full TxBlock history) and **MegaMMR** (all coin proofs / UTXO set) node modes, the
storage workload splits into distinct **roles** with very different access patterns.
This RFC defines those roles, surveys candidate engines (with a strong bias to
*embedded* engines for attack-surface reasons), records what the totem-node already
designed (DuckDB + InfluxDB + Grafana + ZVEC), and recommends a role→engine matrix to
take into RFC-0003 for measurement.

The motivation includes **security**: MySQL/H2 are *server* databases (network
listeners, auth, TLS, connection surface). The C++ port already removed the MDS/MySQL
surface in favour of embedded SQLite. Any new engine should preserve that property
where possible.

---

## 2. Context

### 2.1 Current storage

| Store | Engine | Notes |
|---|---|---|
| Wallet / UserDB / P2PDB | SQLite (embedded) | small, transactional |
| TxPoWDB (recent) | SQLite (embedded) | 3-day retention, batched writes (see RESULTS_LOG) |
| ArchiveDB | SQLite (embedded) | block BLOBs, grows with chain |
| ChainTree / Cascade | SQLite (embedded) | active-chain state |
| MegaMMR | custom in-memory MMR + coin map | proofs; snapshotted to `megammr.mmr` |
| (Java reference) | H2 / MySQL / InnoDB | server DB; heavier, larger attack surface |

### 2.2 Precedent — the totem-node mode system

`totem-node-new-crypto` (sibling project) already implements a 9-mode system
(`NODE_MODES.md`) with a DB matrix:

- **SQLite** — core state incl. **ArchiveDB**
- **DuckDB** — analytical archive: 8 tables (`syncblock`, `cascadedata`, `txpow`,
  `coins`, `transactions`, `address_balance`, `daily_stats`, `watchlist`); commands
  `duckdb`, `duckdbcoins`; `duckdb action:syncgenesis` genesis backfill
- **InfluxDB** — time-series metrics (height, peers, mempool, RAM, UTXOs, supply)
- **Grafana** — dashboards (a *visualisation* layer, not a database)
- **MegaMMR** — coin proofs
- **ZVEC/ONNX** — `all-MiniLM-L6-v2` embeddings for semantic search
- **MCP / `capabilities`** — agent surface (RFC-0011/0013 in that repo)

Our goal is to bring this discipline to Minima More, with **measurement** behind each
choice.

---

## 3. Storage roles

| Role | Data | Access pattern | Size (mainnet-scale) | Durability |
|---|---|---|---|---|
| **R0 Core state** | wallet, user, p2p, txpow, chaintree, cascade | tiny rows, read/write, transactional | MBs | high |
| **R1 Archive store** | every TxBlock | bulk **append**, point get by id, **range scan**, last/first, count | ~2.4 M rows, GBs | high |
| **R2 Coin store (MegaMMR)** | UTXO set + MMR | **point get by coinID**, prefix scan, MMR proof reads, prune, snapshot | ~1.7 M coins, sub-GB→GBs | high |
| **R3 Analytics / explorer** | derived: daily stats, balances, token supply | **aggregations**, joins, scans | derived | rebuildable |
| **R4 Agentic** | embeddings, graph, full-text | **KNN/ANN**, BM25, graph hops, RRF fusion | derived | rebuildable |

Key property: **R1/R2 are hot-path, low-latency, embedded, must be secure; R3/R4 are
derived/rebuildable and can use richer engines**.

---

## 4. Engine landscape (2026)

### 4.1 Embedded — row / column / KV

| Engine | Model | Strengths | Weaknesses |
|---|---|---|---|
| **SQLite** | row, embedded | point lookups (~µs), SQL, ubiquitous, encrypted; single-writer | weak at large scans/aggregations; single-threaded |
| **DuckDB** | columnar, embedded | 11–97× faster analytics; ~2× smaller; Parquet/Arrow; VSS; SQLite scanner | not for many tiny point queries (per-statement overhead ~ms; ~250 µs vs SQLite ~4 µs) |
| **LMDB** | mmap B+tree, embedded | **zero-copy** point lookups, MVCC readers, snapshot, tiny footprint | no SQL; write-map churn |
| **RocksDB** | LSM KV, embedded | high write throughput (append-heavy), tunable, prefix iteration | memory/compaction tuning; binary size |

### 4.2 Analytics / time-series

- **DuckDB** (embedded OLAP) — primary candidate for R3.
- **ClickHouse / TimescaleDB** — server OLAP/TS; strong but add a network service.
- **InfluxDB** — TS metrics (matches totem-node); server.
- **Parquet / Arrow** — columnar *file* + memory formats (not DBs) for archival copies.

### 4.3 Embedded vector / graph / full-text (R4, agentic)

| Engine | Type | Notes |
|---|---|---|
| **LanceDB** | vector (Rust, Lance format) | disk-first IVF-PQ, MVCC/versioning, larger-than-RAM, **mutable** — best for churning agent memory |
| **DuckDB-VSS** | vector (HNSW) | rides inside DuckDB; persistence experimental, poor incremental writes |
| **sqlite-vec** | vector (brute-force SIMD) | exact, tiny, no ANN yet |
| **Ladybug / lance-graph** | graph | ⚠️ **Kùzu was archived Oct 2025** — do **not** adopt Kùzu |
| **Tantivy / Meilisearch** | full-text (BM25) | for keyword/ID search alongside vectors |

### 4.4 Server DBs (for reference)

MySQL, PostgreSQL, Neo4j, ClickHouse, InfluxDB — capable, but each adds a **network
listener + auth + TLS + ops surface**. Prefer embedded for R0–R2.

---

## 5. Security analysis

1. **Attack surface.** Embedded engines (SQLite, DuckDB, LMDB, RocksDB, LanceDB) run
   in-process — **no listening port**, no DB auth to misconfigure, no network
   exposure. This is the largest single security win vs MySQL/H2.
2. **MySQL specifics.** Broad historical CVE surface, client/server auth, and the
   MDS/MySQL path was already removed from this C++ port. Re-introducing a networked
   DB for the archive/coin store would re-expand the surface — avoid unless a role
   genuinely needs it.
3. **Where a server DB is acceptable:** R3 analytics / R4 agentic *if* bound to
   localhost, authenticated, and never internet-exposed (totem-node's InfluxDB/Grafana
   are localhost in compose).
4. **Encoding safety.** `archive.raw.dat` / `megammr.mmr` are untrusted inputs;
   import paths must be treated as parsers of untrusted data (bounds, versioning).

---

## 6. Recommendation (to be validated by RFC-0003)

| Role | Keep / baseline | Candidate(s) to benchmark | Rationale |
|---|---|---|---|
| R0 core | **SQLite** | — | embedded, transactional, encrypted |
| R1 archive | SQLite | **LMDB**, **RocksDB**, **DuckDB** | LMDB/RocksDB for hot point/range/append; DuckDB for cold analytics |
| R2 coin store | custom + SQLite | **LMDB**, **RocksDB**, **DuckDB** (`duckdbcoins`) | LMDB zero-copy point lookups + snapshot |
| R3 analytics | SQLite | **DuckDB** (+Parquet), *InfluxDB* for metrics, **Grafana** for view | column store wins aggregations |
| R4 agentic | — | **LanceDB**, DuckDB-VSS, sqlite-vec, FTS, MCP | mutable vector memory + keyword + graph |

**Design principle:** introduce a thin **storage interface** per role (ArchiveStore,
CoinStore, AnalyticsStore, MemoryStore) with pluggable embedded engines, default
SQLite, selected via a `-mode`/param. Measure before committing (RFC-0003).

---

## 7. Non-goals

- No consensus or wire-format change. All of this is **local storage**.
- No networked DB in the default/edge profile.

## 8. Open questions

1. Is the coin store better served by **LMDB** (point-perfect) or by extending the
   custom in-memory MegaMMR with an embedded backing (mmap) store?
2. Archive: **split hot (LMDB/RocksDB) + cold (DuckDB/Parquet)**, or single engine?
3. Do we mirror totem-node's full mode system, or a lighter `archive`/`lookup`/`full`
   subset?
4. Agentic scope now (LanceDB + FTS + MCP) or defer to a later RFC?
