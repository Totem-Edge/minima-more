# AWS 8 GB full archive + record battery

End-to-end validation of the chunked archive on the 8 GB host (the phone-RAM
profile), plus record-level accuracy/availability tests across the whole chain.

- **Host:** AWS `c6g.xlarge` — 4 vCPU, **8 GB RAM**, 6.8 GB root, aarch64.
- **Code:** `c2c614c` + archive fixes (first/last order, orphan-chunk GC,
  record harness). Raw input **streamed through a FIFO** (no 1.5 GB temp file).
- **Source:** live `https://eurobuddha.com/archive.raw.dat`. The stream ended at
  **2,371,584**; its header advertises 2,374,319 (stale). Online IBD backfilled
  **+3,450** to **2,375,034**; a peer reported top 2,377,085 but further IBD was
  rate-limited for 30 minutes (`Already sent an IBD ... in last 30 mins`).

## Import

| Metric | Value |
|---|---:|
| Blocks imported from stream | 2,371,584 |
| IBD backfill | +3,450 |
| Final blocks | **2,375,034** |
| Import time / throughput | 4,762 s / **498 blocks/s** |
| Peak RSS | **918.1 MB** (8 GB host) |
| Free disk at finish | ~182 MB (tight; ~3.0 GB data) |

## Storage

| Metric | Value |
|---|---:|
| Serialised payload (`sum(len)`) | 6,610,372,208 B (6.61 GB) |
| Chunks | 9,264 |
| Compressed chunk payload | 1,726,313,928 B (1.726 GB) |
| **Compression ratio** | **3.83x** |
| Archive DB (incl. indexes) | 2,453,934,080 B (2.454 GB) |
| On-chain DB (deleted after tests) | 495,091,712 B (495 MB) |
| Chaintree / cascade | 13.3 MB / 2.3 MB |
| **Total data on disk** | **~2.96 GB** |
| Legacy per-row blobs remaining | 0 |

## Integrity

```
"blocks":2375034
"errors":0,
"recommend":"Your ArchiveDB is correct and has no errors."
```

## Record battery (260 sampled heights)

Samples: fixed heights (1, 2, 3, 10, 128, 255/256/257, 1k…2.3M), 200+ seeded
random heights, chain boundaries, and the last four blocks. For each sample:

- `loadBlockFromNumber(n)` returns block `n`;
- `loadBlock(txpowid)` returns the same block;
- child `parentID` equals the previous block's `TxPoWID`;
- serialize → deserialize round-trip preserves the `TxPoWID`;
- range loads across chunk boundaries (exclusive bounds) return the exact sequence;
- `loadFirstBlock` = block 1, `loadLastBlock` = tip.

```
db count=2375034 min=1 max=2375034 distinct_ids=2375034 gaps=0 missing_chunks=0
first block=1 time=1673613003689 (2023-01-13T12:30:03Z)
last block=2375034 time=1792433932641 (2026-10-19T18:18:52Z)
WARN timestamp out of order at 2142113 (delta -6390 ms)
sample times: min=2023-01-13 median=2024-10-07 max=2026-10-19
lookups=260 ok=260 fail=0 avg_us=2233.7 max_us=142916.4
maxrss=108656 (KB)
RESULT PASS
```

- **Availability:** every block 1…2,375,034 present exactly once; **0 gaps**;
  **0 missing chunks**; 2,375,034 distinct TxPoW ids.
- **Accuracy:** 260/260 ID lookups, parent linkages and serialization
  round-trips pass; full `archive action:integrity` = 0 errors.
- **Latency:** 2.23 ms average random lookup, 142.9 ms worst cold outlier
  (chunk decompression + cache miss).
- **Timestamp note:** one **6.39 s** backward step at block 2,142,113 — miner
  clock skew, not missing/corrupt data.
- **Fresh-DB validation:** a 32 MiB slice imported locally gave 116,736 blocks
  and 239/239 checks PASS, confirming the fixed first/last and range paths.

## Bugs found and fixed

- `loadFirstBlock()` / `loadLastBlock()` were inverted (`ORDER BY block DESC/ASC`
  swapped). This also made integrity/resync start at the wrong end.
- `cleanDB()` now deletes **orphaned `blockchunks`** after pruning
  (`DELETE ... WHERE id NOT IN (SELECT chunkid ...)`) plus incremental vacuum.
- IBD log used `ConvertMilliToTime`, which prints the absolute Unix timestamp as
  “54 Years…” since 1970; now logs ISO-8601 (`2024-10-19T…Z`).
- Record harness range checks corrected for exclusive bounds.

## Phone verdict

- **Storage:** ~3.0 GB total vs 256 GB free — ample.
- **RAM:** 0.6–0.9 GB working set on an 8 GB host; fits with ~7 GB to spare.
- **Caveats:** the ~80 min initial import is CPU/IO heavy; Android may kill
  background work, so run it in the foreground/Termux with a wake lock. After
  import, only new blocks are appended.
