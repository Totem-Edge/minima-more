# RAM-scaling comparison (MMR, N=1000000 entries)

C++ = this node | Java = minima-core `c2a54a2` (performance reference only).

| Metric | C++ (this node) | Java (minima-core) |
|---|---|---|
| bytes/MMR entry | 698.34 | 601.86888 (heap) |
| delta for 1000000 entries (kB) | 681968 | 587762 (heap) |
| process peak RSS (kB) | 683904 | 1936608 |

Caveats: C++ is process RSS; Java rows are JVM heap-in-use (plus the peak
RSS row which is the whole JVM). Heap != RSS. Single run, indicative only.
