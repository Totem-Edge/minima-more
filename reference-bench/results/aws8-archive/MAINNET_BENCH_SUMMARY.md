# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 60s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 20.25 |  |
| tip block | 2375015 |  |
| peak RSS during sync (kB) | 108632 |  |
| idle RSS avg (kB) | 214601 |  |
| CPU-seconds over 60s settle | 33.130 |  |
| threads | 25 |  |
| data dir (kB) | 58188 |  |
| P2P peers reached | 3 |  |
| P2P known peers | 3 |  |
| P2P time to first peer (s) | 4.98 |  |
| P2P time to 5 peers (s) | - |  |
