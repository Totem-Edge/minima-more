# RAM-scaling comparison (MMR, N=100000 entries)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/MMR entry | 737.28 | 28.58 (heap) |
| delta for 100000 entries (kB) | 72000 | 2791 (heap) |
| process peak RSS (kB) | 73952 | 206256 |

Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. Single run, indicative only.
