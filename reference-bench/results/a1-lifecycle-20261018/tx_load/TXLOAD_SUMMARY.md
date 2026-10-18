# CPU under transaction load (N=4 sends, rate cap 1/s)

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| sends ok / failed | 4 / 0 | 3 / 1 |
| achieved throughput (tx/min) | 1.56 | 2.62 |
| CPU-seconds during load | 6.540 | 7.090 |
| CPU milliseconds per tx | 1635 | 2363 |
| send latency avg (ms) | 447.2 | 1964.5 |
| peak RSS (kB) | 24032 | 306532 |

Each send = sign + TxPoW mine + mempool/cascade/MMR. Achieved rate is
bounded by solo automine cadence (spendable coin). Single run.
