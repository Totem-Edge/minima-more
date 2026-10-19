# TxPoWSqlDB write throughput (N=25000 TxPoWs)

| | time | inserts/s |
|---|---|---|
| one txn per row (old) | 11453.1 ms | 2183 |
| batched (new) | 3254.7 ms | 7681 |

Speedup: **3.52x** (TxPoW build time excluded; both paths insert the same objects).
