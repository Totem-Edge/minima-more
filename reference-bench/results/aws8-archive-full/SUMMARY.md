# Full archive — populated import findings

Host: AWS `c6g.xlarge` — 4 vCPU / 8 GB / Neoverse-N1 (aarch64).

Sources:
- `https://eurobuddha.com/archive.raw.dat` (1,511,412,727 bytes; 2,374,319 blocks)
- Live archive-node resync: `archive action:resync host:eurobuddha.com:9001`

## Measured

| Metric | Value |
|---|---|
| Import RSS (steady, after fixes) | **~495 MB** |
| Import throughput (bounded) | **~570 blocks/s** (692k blocks / ~20 min) |
| Archive DB size at 692k blocks | **1.8 GB** |
| Projected full-chain DB (uncompressed) | **~6 GB** |
| Raw export size (gzip) | 1.41 GB |
| Live resync (recent range 2,332,915→2,376,483) | **149 MB DB**, completed |
| Disk available on the 8 GB box | ~2.7 GB |

## Fixes that made the import memory-safe
- Historical blocks are **not** added to the RAM/SQL TxPoW DB (`processSyncBlock`).
- The archive resync has **queue backpressure** (`getSize() > 4`).
- Tree recalculation is **batched** (every 2048 blocks, plus end-of-stream).
- Archive WAL is **checkpointed** periodically and bounded (`wal_autocheckpoint`,
  `journal_size_limit=64MB`).

Before these, the node OOM-killed at ~7.5 GB anon RSS; now it holds ~495 MB.

## Conclusions
- **Do not prune a complete archive** — completeness is its purpose. Pruning is
  only appropriate for a deliberately “recent” archive; keep it opt-in.
- A **complete** archive on an 8 GB box needs **compressed block storage** (or a
  bigger disk). Uncompressed SQLite is ~4× the gzip raw (≈6 GB).
- Throughput is limited by per-block `INSERT`+WAL; **batch archive writes**
  (transaction every N blocks) is the next speed/IO win.
- A full archive is **not phone-class** on storage; a full node (59 MB) is.
  R1 should move to a compressed embedded engine (LMDB/RocksDB with block
  compression) per RFC-0001/0003.
