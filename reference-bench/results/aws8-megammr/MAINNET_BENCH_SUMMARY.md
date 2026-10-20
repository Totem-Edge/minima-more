# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 60s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 25.98 |  |
| tip block | 2375015 |  |
| peak RSS during sync (kB) | 114392 |  |
| idle RSS avg (kB) | 197744 |  |
| CPU-seconds over 60s settle | 32.280 |  |
| threads | 25 |  |
| data dir (kB) | 58816 |  |
| P2P peers reached | 2 |  |
| P2P known peers | 3 |  |
| P2P time to first peer (s) | 9.66 |  |
| P2P time to 5 peers (s) | - |  |
