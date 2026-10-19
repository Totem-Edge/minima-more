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
