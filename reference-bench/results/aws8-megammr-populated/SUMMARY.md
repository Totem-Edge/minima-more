# MegaMMR — populated (real `mega.mmr`)

Host: AWS `c6g.xlarge` — 4 vCPU / 8 GB / Neoverse-N1 (aarch64).

Source: `https://eurobuddha.com/mega.mmr` (504,893,547 bytes), imported with
`megammr action:import` (no file → mode default URL), then restarted `-megammr`.

| Metric | Value |
|---|---|
| MegaMMR coins | **465,588** |
| RSS (loaded, idle) | **1,304,016 kB (~1.24 GB)** |
| Anonymous RSS | 1,287,432 kB (~1.23 GB) |
| File-backed RSS | 8,187 kB |
| Data dir | **477 MB** (`megammr.mmr` 476 MB) |
| DB/MMR permissions | **0600 (owner-only)** |
| Threads | 25* |

\*same node baseline; the populated MMR is the dominant memory cost.

Notes:
- This is the real populated cost — the earlier 112 MB mode-run number was an
  **empty** MegaMMR (a normal `-megammr` sync does not load the UTXO set).
- glibc in-use ~1.21 GB, negligible retained free — the MMR/coins are live data.
- Role lockdown verified: `megammr.mmr`, wallet and WAL/SHM are mode 0600.

## R2 (LMDB coin store) update

| Metric | In-memory map | LMDB store |
|---|---|---|
| Anonymous RSS | ~1.23 GB | **~573 MB** |
| Total RSS (idle) | ~1.24 GB | ~1.17 GB (~617 MB mmap page cache) |
| Warm-restart idle RSS | — | **~522 MB** (coins not touched) |
| Coin store on disk | — | 608 MB (`coins.lmdb`, 0600) |
| Startup | ~87 s | **22 s first load / 10 s warm restart** |

Warm restart skips the `megammr.mmr` coin payload when a sidecar fingerprint
(`megammr.mmr.meta`: raw size/mtime + coin count) matches the populated LMDB
store; only the MMR is read. `megammr action:info` reports `coin_store:lmdb`.
