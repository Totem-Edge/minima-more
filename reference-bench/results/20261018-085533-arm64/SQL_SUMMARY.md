# TxPoWSqlDB write throughput (N=5000 TxPoWs)

| | time | inserts/s |
|---|---|---|
| one txn per row (old) | 293.8 ms | 17019 |
| batched (new) | 56.2 ms | 88924 |

Speedup: **5.22x** (TxPoW build time excluded; both paths insert the same objects).
