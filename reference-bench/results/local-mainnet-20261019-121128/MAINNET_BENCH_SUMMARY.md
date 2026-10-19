# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 45s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 22.71 | 5.77 |
| tip block | 2374085 | 2373791 |
| peak RSS during sync (kB) | 139760 | 491808 |
| idle RSS avg (kB) | 136638 | 655887 |
| CPU-seconds over 45s settle | 15.180 | 21.300 |
| threads | 29 | 68 |
| data dir (kB) | 24180 | 135052 |
| P2P peers reached | 0 | 0 |
| P2P known peers | 3 | 3 |
| P2P time to first peer (s) | - | - |
| P2P time to 5 peers (s) | - | - |
