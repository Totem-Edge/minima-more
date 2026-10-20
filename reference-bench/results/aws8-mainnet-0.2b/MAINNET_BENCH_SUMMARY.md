# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 60s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 18.47 | 8.55 |
| tip block | 2375185 | 2375499 |
| peak RSS during sync (kB) | 93356 | 489476 |
| idle RSS avg (kB) | 191112 | 728422 |
| CPU-seconds over 60s settle | 36.530 | 66.560 |
| threads | 25 | 61 |
| data dir (kB) | 60848 | 584244 |
| P2P peers reached | 3 | 2 |
| P2P known peers | 3 | 8 |
| P2P time to first peer (s) | 4.77 | 5.32 |
| P2P time to 5 peers (s) | - | - |
