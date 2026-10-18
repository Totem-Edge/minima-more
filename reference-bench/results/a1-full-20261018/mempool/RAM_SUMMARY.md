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
