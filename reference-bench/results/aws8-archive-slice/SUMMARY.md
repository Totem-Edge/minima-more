# AWS 8 GB archive slice — phone-RAM proof

This directory records the evidence that the archive node's **working set fits an
8 GB RAM / 256 GB storage phone**. The full-archive run supplies disk/size and
integrity numbers; **peak RSS on this 8 GB host** is the phone-RAM proof.

- **Host:** AWS `c6g.xlarge` — 4 vCPU, **8 GB RAM**, 6.8 GB root, aarch64
  (Neoverse-N1), same ISA as a modern Android phone.
- **Commit:** `c2c614c` (chunked archive storage + prepared statements).
- **Input:** first **256 MiB** (`Range: bytes=0-268435455`) of the live
  `https://eurobuddha.com/archive.raw.dat` (zlib raw stream), imported into a
  fresh data dir. The slice ends mid-record, so the importer logs an expected
  read error at EOF; committed blocks and integrity are unaffected.

## Command

```bash
curl -s -r 0-268435455 https://eurobuddha.com/archive.raw.dat -o /tmp/archive-slice.raw.dat
printf 'archive action:import file:/tmp/archive-slice.raw.dat\nquit\n' \
  | ./minima -data /tmp/archslice-proof -archive -nosyncibd -noconnect -port 19910
```

## Results

| Metric | Value |
|---|---:|
| Blocks imported | **618,496** |
| Serialised payload (`sum(len)`) | 1,151,359,414 B (1.151 GB) |
| Chunks (`blockchunks` rows) | 2,416 |
| Compressed chunk payload | 291,243,964 B (291 MB) |
| **Compression ratio** | **3.95x** |
| Archive DB file (incl. indexes) | 480,616,448 B (480.6 MB) |
| Rows still using legacy per-row blobs | 0 |
| Missing chunk references | 0 |
| `archive action:integrity` | **618,496 blocks, 0 errors** |
| Import throughput | **~610 blocks/s** (1,013 s) |
| **Peak RSS (8 GB host)** | **627.6 MB** |

Integrity was run on the recreated slice DB with the same binary:

```
"blocks":618496
"errors":0,
"recommend":"Your ArchiveDB is correct and has no errors."
```

## Full-archive result (measured, supersedes the early projection)

The full run (see `reference-bench/results/aws8-archive-full/SUMMARY.md`) measured:
**2,375,034 blocks**, 6.61 GB serialised → 1.73 GB compressed (**3.83×**),
archive DB **2.45 GB** + on-chain **0.50 GB ≈ 3.0 GB total**, peak RSS **918 MB**.
The early slice-based projection (1.85 GB) under-estimated because later blocks
are larger than the first 618k. The raw `.dat` is streamed via a FIFO for the
full run, so it never lands on disk at all.

## Phone verdict

- **Storage:** ~2.3 GB total for a complete archive vs 256 GB free — ample.
- **RAM:** peak RSS ~0.63 GB on an 8 GB host; the full import is memory-bounded
  by queue backpressure, so the working set does not scale with block count.
- **Caveats:** Android may kill long-running background apps; run it in the
  foreground or under Termux with a wake lock, and expect thermal/battery cost
  during the one-time ~65 min import. The ongoing archive write path is a few
  hundred blocks/s and only appends new blocks after the initial import.
