# Initial sync + fully-synced RAM (chain height 11/15)

Miner built the chain for 240s; each follower is a **fresh** node
that IBDs the same chain from the same miner.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| sync time (s) | 5.89 | 4.09 |
| sync complete | yes | yes |
| chain height | 11 | 15 |
| peak RSS during sync (kB) | 20108 | 248220 |
| idle RSS after sync, avg (kB) | 21731 | 177590 |
| idle RSS after sync, max (kB) | 21964 | 239764 |
| threads | 25 | 58 |

Single run, shared host; automine keeps the chain small (~1 block / 40-50 s).
