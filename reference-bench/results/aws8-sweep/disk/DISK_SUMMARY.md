# Disk writes (window 120s)

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| storage writes (B/h) | 4176528 | 5896275 |
| storage writes (B/day) | 100236672 | 141510600 |
| datadir growth (kB) | 96 | 60 |
| WAL max (kB) | 272 | 0 |
| WAL checkpoints | 0 | 0 |

storage writes = /proc/<pid>/io write_bytes (bytes dirtied to storage).
macOS has no equivalent and reports 0. Single run, idle node.
