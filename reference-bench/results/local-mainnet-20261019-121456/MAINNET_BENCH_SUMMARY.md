# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 45s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 20.22 | 6.12 |
| tip block | 2374365 | 2373791 |
| peak RSS during sync (kB) | 147472 | 374176 |
| idle RSS avg (kB) | 161725 | 704952 |
| CPU-seconds over 45s settle | 13.960 | 19.300 |
| threads | 29 | 66 |
| data dir (kB) | 29768 | 167844 |
| P2P peers reached | 2 | 3 |
| P2P known peers | 3 | 3 |
| P2P time to first peer (s) | 1.26 | 2.86 |
| P2P time to 5 peers (s) | - | - |
