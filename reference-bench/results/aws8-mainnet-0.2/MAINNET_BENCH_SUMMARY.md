# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 60s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 20.91 | 8.02 |
| tip block | 2375441 | 2375185 |
| peak RSS during sync (kB) | 103056 | 467516 |
| idle RSS avg (kB) | 186004 | 715082 |
| CPU-seconds over 60s settle | 33.740 | 72.090 |
| threads | 25 | 61 |
| data dir (kB) | 65356 | 699236 |
| P2P peers reached | 3 | 3 |
| P2P known peers | 3 | 33 |
| P2P time to first peer (s) | 7.22 | 5.32 |
| P2P time to 5 peers (s) | - | - |
