# RAM-scaling comparison (MMR, N=1000000 entries)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/MMR entry | 686.92 | 602.009552 (heap) |
| delta for 1000000 entries (kB) | 670816 | 587899 (heap) |
| process peak RSS (kB) | 672752 | 1753904 |

Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. Single run, indicative only.
