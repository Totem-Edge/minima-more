# Live-mainnet node benchmark (fresh sync from real peers)

Peers: 141.148.226.161:9001,65.109.31.226:9001,93.93.105.231:9001  ·  settle window: 60s  ·  single run, shared host.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| time to tip (s) | 10.01 | 6.16 |
| tip block | 2373134 | 2372878 |
| peak RSS during sync (kB) | 92448 | 406840 |
| idle RSS avg (kB) | 210162 | 1392086 |
| CPU-seconds over 60s settle | 34.290 | 67.790 |
| threads | 25 | 60 |
| data dir (kB) | 219800 | 881456 |
