# Disk writes (window 180s)

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| storage writes (B/h) | 3684967 | 5978167 |
| storage writes (B/day) | 88439208 | 143476008 |
| datadir growth (kB) | 124 | 68 |
| WAL max (kB) | 300 | 0 |
| WAL checkpoints | 0 | 0 |

storage writes = /proc/<pid>/io write_bytes (bytes dirtied to storage).
macOS has no equivalent and reports 0. Single run, idle node.
