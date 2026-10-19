# Reference sweep

Dir: /tmp/aws8-sweep
arch: aarch64   cores: 4

## MMR RAM / CPU (C++ vs Java)
# RAM-scaling comparison (MMR, N=300000 entries)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/MMR entry | 634.95 | 622.46696 (heap) |
| delta for 300000 entries (kB) | 186020 | 182363 (heap) |
| process peak RSS (kB) | 190856 | 598976 |

Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. Single run, indicative only.

## Mempool RAM + checkForCoinID lookup
# Mempool RAM-scaling comparison (N=100000 txns, 2 coins/txn)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| mempool txns | 100000 | 100000 |
| bytes/txpow | 7589.81 (RSS) | 3679.42544 (heap) |
| bytes/coin | 3794.90 (RSS) | 1839.71272 (heap) |
| build (ms) | 5590.4 | 5080.747766 |
| checkForCoinID (us/lookup) | 0.97 | 20417.454044 |
| process peak RSS (kB) | 746224 | 1367576 |

Notes: C++ rows are process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. The C++ node keeps an O(1)
coin->TxPoW index (extra RAM) which is why its lookup latency is far lower;
Java scans the pool linearly. Single run, indicative only.

## SQLite write throughput (batched vs per-row)
# TxPoWSqlDB write throughput (N=25000 TxPoWs)

| | time | inserts/s |
|---|---|---|
| one txn per row (old) | 11453.1 ms | 2183 |
| batched (new) | 3254.7 ms | 7681 |

Speedup: **3.52x** (TxPoW build time excluded; both paths insert the same objects).

## Microbenchmarks (C++ vs Java)
# Reference benchmark summary

Run: /tmp/aws8-sweep/micro
Java: minima-core `c2a54a2` (performance reference only — NOT byte-parity).

| Metric | Java (minima-core) | C++ (this node) |
|---|---|---|
| WOTS pubkey (ms/op) | 9.920168 | 2.231226 |
| WOTS sign (ms/op, batched) | 5.240560 | 1.072981 |
| WOTS verify (ms/op) | 4.624736 | 1.164362 |
| SHA3-256 224B (h/s, 1 thr) | 283025.992917 | 967125.566394 |
| MMR addEntry+getRoot (entries/s) | 121643.513344 | 214359.526692 |

C++ WOTS sign scalar vs batched:
    scalar   3.89389 ms/sign  (1946.94 ms)
    batched  1.24704 ms/sign  (623.519 ms)
    speedup  3.12251x
    identical yes

Raw files: environment.txt, java_bench.txt, cpp_sha3.txt, cpp_wots_signature.txt, cpp_bench_core.txt

## Initial sync + synced RAM
# Initial sync + fully-synced RAM (chain height 7/9)

Miner built the chain for 120s; each follower is a **fresh** node
that IBDs the same chain from the same miner.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| sync time (s) | 5.10 | 4.87 |
| sync complete | yes | yes |
| chain height | 7 | 9 |
| peak RSS during sync (kB) | 20040 | 212168 |
| idle RSS after sync, avg (kB) | 21707 | 154892 |
| idle RSS after sync, max (kB) | 21940 | 166164 |
| threads | 25 | 58 |

Single run, shared host; automine keeps the chain small (~1 block / 40-50 s).

## CPU at idle
# CPU at idle (window 120s)

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| avg CPU utilisation (%) | 1.857 | 2.847 |
| CPU-seconds in window | 2.230 | 3.420 |
| CPU-seconds per hour | 66.84 | 102.51 |
| RSS avg (kB) | 20005 | 221140 |
| threads | 23 | 57 |

CPU% = CPU-seconds / wall-seconds. Single run, shared host, idle (no
workload) after startup.

## CPU under transaction load
# CPU under transaction load (N=3 sends, rate cap 1/s)

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| sends ok / failed | 3 / 0 | 2 / 1 |
| achieved throughput (tx/min) | 1.88 | 1.84 |
| CPU-seconds during load | 6.210 | 5.710 |
| CPU milliseconds per tx | 2070 | 2855 |
| send latency avg (ms) | 568.8 | 2298.3 |
| peak RSS (kB) | 24440 | 246320 |

Each send = sign + TxPoW mine + mempool/cascade/MMR. Achieved rate is
bounded by solo automine cadence (spendable coin). Single run.

## Disk writes
# Disk writes (window 120s)

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| storage writes (B/h) | 4176528 | 5896275 |
| storage writes (B/day) | 100236672 | 141510600 |
| datadir growth (kB) | 96 | 60 |
| WAL max (kB) | 272 | 0 |
| WAL checkpoints | 0 | 0 |

storage writes = /proc/<pid>/io write_bytes (bytes dirtied to storage).
macOS has no equivalent and reports 0. Single run, idle node.
