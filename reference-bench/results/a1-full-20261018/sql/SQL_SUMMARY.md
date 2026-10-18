# TxPoWSqlDB write throughput (N=50000 TxPoWs)

| | time | inserts/s |
|---|---|---|
| one txn per row (old) | 32927.1 ms | 1519 |
| batched (new) | 15516.7 ms | 3222 |

Speedup: **2.12x** (TxPoW build time excluded; both paths insert the same objects).
