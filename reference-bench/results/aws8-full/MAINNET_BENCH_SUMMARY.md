# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 60s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 22.74 |  |
| tip block | 2375299 |  |
| peak RSS during sync (kB) | 117568 |  |
| idle RSS avg (kB) | 197495 |  |
| CPU-seconds over 60s settle | 34.200 |  |
| threads | 25 |  |
| data dir (kB) | 54644 |  |
| P2P peers reached | 2 |  |
| P2P known peers | 3 |  |
| P2P time to first peer (s) | 5.90 |  |
| P2P time to 5 peers (s) | - |  |
