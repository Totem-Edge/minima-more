# RAM-scaling comparison (MMR, N=300000 entries)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/MMR entry | 634.95 | 622.46696 (heap) |
| delta for 300000 entries (kB) | 186020 | 182363 (heap) |
| process peak RSS (kB) | 190856 | 598976 |

Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. Single run, indicative only.
