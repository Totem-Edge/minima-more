# Reference sweep

Dir: /home/ubuntu/pm/src/reference-bench/results/a1-full-20261018-061142
arch: aarch64   cores: 4

## MMR RAM / CPU (C++ vs Java)
# RAM-scaling comparison (MMR, N=1000000 entries)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/MMR entry | 633.89 | 601.859424 (heap) |
| delta for 1000000 entries (kB) | 619036 | 587753 (heap) |
| process peak RSS (kB) | 623872 | 1897856 |

Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. Single run, indicative only.

## Mempool RAM + checkForCoinID lookup
# Mempool RAM-scaling comparison (N=200000 txns, 2 coins/txn)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| mempool txns | 200000 | 200000 |
| bytes/txpow | 7575.12 (RSS) | 3662.37168 (heap) |
| bytes/coin | 3787.56 (RSS) | 1831.18584 (heap) |
| build (ms) | 9207.2 | 7663.452749 |
| checkForCoinID (us/lookup) | 0.89 | 43732.610542 |
| process peak RSS (kB) | 1484536 | 1961992 |

Notes: C++ rows are process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. The C++ node keeps an O(1)
coin->TxPoW index (extra RAM) which is why its lookup latency is far lower;
Java scans the pool linearly. Single run, indicative only.

## SQLite write throughput (batched vs per-row)
# TxPoWSqlDB write throughput (N=50000 TxPoWs)

| | time | inserts/s |
|---|---|---|
| one txn per row (old) | 32927.1 ms | 1519 |
| batched (new) | 15516.7 ms | 3222 |

Speedup: **2.12x** (TxPoW build time excluded; both paths insert the same objects).

## Microbenchmarks (C++ vs Java)
# Reference benchmark summary

Run: /home/ubuntu/pm/src/reference-bench/results/a1-full-20261018-061142/micro
Java: minima-core `c2a54a2` (performance reference only — NOT byte-parity).

| Metric | Java (minima-core) | C++ (this node) |
|---|---|---|
| WOTS pubkey (ms/op) | 8.127999 | 1.904691 |
| WOTS sign (ms/op, batched) | 4.298728 | 0.919870 |
| WOTS verify (ms/op) | 3.782524 | 1.027661 |
| SHA3-256 224B (h/s, 1 thr) | 346289.413291 | 1158839.287472 |
| MMR addEntry+getRoot (entries/s) | 139109.074403 | 259839.258498 |

C++ WOTS sign scalar vs batched:
    scalar   3.26273 ms/sign  (1631.36 ms)
    batched  1.06938 ms/sign  (534.688 ms)
    speedup  3.05106x
    identical yes

Raw files: environment.txt, java_bench.txt, cpp_sha3.txt, cpp_wots_signature.txt, cpp_bench_core.txt
