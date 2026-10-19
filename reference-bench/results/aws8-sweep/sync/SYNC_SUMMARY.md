# Initial sync + fully-synced RAM (chain height 7/9)

Miner built the chain for 120s; each follower is a **fresh** node
that IBDs the same chain from the same miner.

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| sync time (s) | 5.10 | 4.87 |
| sync complete | yes | yes |
| chain height | 7 | 9 |
| peak RSS during sync (kB) | 20040 | 212168 |
| idle RSS after sync, avg (kB) | 21707 | 154892 |
| idle RSS after sync, max (kB) | 21940 | 166164 |
| threads | 25 | 58 |

Single run, shared host; automine keeps the chain small (~1 block / 40-50 s).
