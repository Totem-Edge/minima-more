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
