# Mempool RAM-scaling comparison (N=100000 txns, 2 coins/txn)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| mempool txns | 100000 | 100000 |
| bytes/txpow | 3433.59 (RSS) | 3677.87904 (heap) |
| bytes/coin | 1716.80 (RSS) | 1838.93952 (heap) |
| build (ms) | 2509.8 | 2575.590125 |
| checkForCoinID (us/lookup) | 3.41 | 13631.221 |
| process peak RSS (kB) | 337376 | 1289136 |

Notes: C++ rows are process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. The C++ node keeps an O(1)
coin->TxPoW index (extra RAM) which is why its lookup latency is far lower;
Java scans the pool linearly. Single run, indicative only.
