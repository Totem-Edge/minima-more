# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 60s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 17.57 | 8.12 |
| tip block | 2373778 | 2373971 |
| peak RSS during sync (kB) | 91144 | 436316 |
| idle RSS avg (kB) | 196451 | 759095 |
| CPU-seconds over 60s settle | 32.690 | 76.240 |
| threads | 25 | 62 |
| data dir (kB) | 66180 | 738024 |
| P2P peers reached | 2 | 3 |
| P2P known peers | 3 | 10 |
| P2P time to first peer (s) | 5.98 | 5.43 |
| P2P time to 5 peers (s) | - | - |
